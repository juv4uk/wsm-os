/*
 * physical-boot-probe.c -- handoff-probe.c's captures, plus a file log
 * written back to real removable media, for booting on the owner's
 * actual physical machine via a Ventoy USB stick.
 *
 * Why a separate file rather than editing handoff-probe.c: that probe
 * is the QEMU/OVMF-validated baseline (its own README documents real,
 * reproduced output under TCG). This one adds one real capability --
 * writing everything to \WSM-PROBE-LOG.TXT on every writable FAT
 * volume it can find at boot, not just printing to the screen -- and
 * is meant specifically for a run this repository cannot itself
 * perform: booting on real silicon, which only the owner can do by
 * physically inserting a USB stick into his own machine.
 *
 * Why write to *every* writable volume found, not just the one this
 * probe booted from: Ventoy typically maps a booted .img file as its
 * own ephemeral block device, and whether writes to that mapping
 * persist back to the .img file on the Ventoy USB stick itself is not
 * confirmed (checked before writing this -- Ventoy's own documentation
 * does not clearly state this for the plain, non-persistence-plugin
 * IMG-boot path). Writing to every EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
 * volume this firmware can see hedges against that uncertainty: the
 * real Ventoy USB partition itself should be one of the discovered
 * volumes, distinct from whatever the booted .img is mapped as, and
 * *that* write should persist normally, since it is real FAT32 on a
 * real USB stick, not an ephemeral disk mapping.
 *
 * Self-contained, same build discipline as every other probe here
 * (see handoff-probe.c's header comment for the full gnu-efi/GCC
 * bisection story): no gnu-efi runtime library, only its headers.
 */

#include <efi.h>

static EFI_SYSTEM_TABLE *gST;
static EFI_BOOT_SERVICES *gBS;

#define MAX_LOG_FILES 8
static EFI_FILE_HANDLE gLogFiles[MAX_LOG_FILES];
static int gLogFileCount = 0;

static UINTN chr16len(const CHAR16 *s) {
  UINTN n = 0;
  while (s[n]) n++;
  return n;
}

/* Writes to the screen AND to every open log file. This is the single
 * choke point every other print helper below routes through, so
 * "logged" and "shown on screen" can never silently drift apart. */
static void pstr(const CHAR16 *s) {
  gST->ConOut->OutputString(gST->ConOut, (CHAR16 *)s);
  UINTN bytes = chr16len(s) * sizeof(CHAR16);
  for (int i = 0; i < gLogFileCount; i++) {
    if (gLogFiles[i] == NULL) continue;
    UINTN sz = bytes;
    gLogFiles[i]->Write(gLogFiles[i], &sz, (VOID *)s);
  }
}

static void phex64(UINT64 v) {
  CHAR16 buf[19];
  const CHAR16 *digits = L"0123456789ABCDEF";
  buf[0] = L'0';
  buf[1] = L'x';
  for (int i = 0; i < 16; i++) buf[2 + i] = digits[(v >> ((15 - i) * 4)) & 0xF];
  buf[18] = 0;
  pstr(buf);
}
static void phex32(UINT32 v) { phex64((UINT64)v); }
static void pdec(UINT64 v) {
  CHAR16 buf[21];
  int i = 20;
  buf[20] = 0;
  if (v == 0) {
    buf[--i] = L'0';
  } else {
    while (v > 0 && i > 0) { buf[--i] = L'0' + (CHAR16)(v % 10); v /= 10; }
  }
  pstr(&buf[i]);
}
static void pbit(UINT64 v, int bit) { pdec((v >> bit) & 1); }
static void pchar(CHAR16 c) { CHAR16 buf[2] = {c, 0}; pstr(buf); }
static void pnl(void) { pstr(L"\r\n"); }

static inline UINT64 read_cr0(void) { UINT64 v; __asm__ __volatile__("mov %%cr0, %0" : "=r"(v)); return v; }
static inline UINT64 read_cr3(void) { UINT64 v; __asm__ __volatile__("mov %%cr3, %0" : "=r"(v)); return v; }
static inline UINT64 read_cr4(void) { UINT64 v; __asm__ __volatile__("mov %%cr4, %0" : "=r"(v)); return v; }
static inline UINT64 read_rsp(void) { UINT64 v; __asm__ __volatile__("mov %%rsp, %0" : "=r"(v)); return v; }
static inline UINT64 read_rflags(void) { UINT64 v; __asm__ __volatile__("pushfq\n\tpop %0" : "=r"(v)); return v; }
static inline UINT64 read_msr(UINT32 msr) {
  UINT32 lo, hi;
  __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
  return ((UINT64)hi << 32) | lo;
}
static inline void write_msr(UINT32 msr, UINT64 val) {
  UINT32 lo = (UINT32)val, hi = (UINT32)(val >> 32);
  __asm__ __volatile__("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}
static inline void do_cpuid(UINT32 leaf, UINT32 subleaf, UINT32 *a, UINT32 *b, UINT32 *c, UINT32 *d) {
  __asm__ __volatile__("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(subleaf));
}
static inline UINT64 read_tsc(void) {
  UINT32 lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((UINT64)hi << 32) | lo;
}
typedef struct { UINT16 limit; UINT64 base; } __attribute__((packed)) DtReg;
static inline void read_gdtr(DtReg *r) { __asm__ __volatile__("sgdt %0" : "=m"(*r)); }
static inline void read_idtr(DtReg *r) { __asm__ __volatile__("sidt %0" : "=m"(*r)); }

#define MSR_IA32_BIOS_SIGN_ID 0x8B
static UINT32 read_live_microcode_revision(void) {
  UINT32 a, b, c, d;
  write_msr(MSR_IA32_BIOS_SIGN_ID, 0);
  do_cpuid(1, 0, &a, &b, &c, &d);
  UINT64 sign = read_msr(MSR_IA32_BIOS_SIGN_ID);
  return (UINT32)(sign >> 32);
}

static int guid_eq(EFI_GUID *a, EFI_GUID *b) {
  UINT8 *pa = (UINT8 *)a, *pb = (UINT8 *)b;
  for (int i = 0; i < 16; i++) if (pa[i] != pb[i]) return 0;
  return 1;
}

static EFI_GUID gAcpi20Guid = {0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};
static EFI_GUID gSmbios3Guid = {0xf2fd1544, 0x9794, 0x4a2c, {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94}};
static EFI_GUID gSmbiosGuid = {0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};
static EFI_GUID gGopGuid = {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
static EFI_GUID gSfsGuid = {0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};

/* Opens \WSM-PROBE-LOG.TXT (create-or-truncate) on every discoverable
 * FAT/filesystem volume, keeping every handle that succeeds. Prints,
 * to the screen only (log files aren't open yet for these lines, by
 * definition), which volumes worked -- so the owner can see on the
 * real monitor whether this step succeeded before anything else runs. */
static void open_log_files(void) {
  EFI_HANDLE *handles = NULL;
  UINTN count = 0;
  EFI_STATUS st = gBS->LocateHandleBuffer(ByProtocol, &gSfsGuid, NULL, &count, &handles);
  gST->ConOut->OutputString(gST->ConOut, L"Discovering writable volumes for \\WSM-PROBE-LOG.TXT ...\r\n");
  if (EFI_ERROR(st) || handles == NULL) {
    gST->ConOut->OutputString(gST->ConOut, L"  LocateHandleBuffer(SimpleFileSystem) FAILED -- no log files will be written, screen output only.\r\n");
    return;
  }
  CHAR16 numbuf[8];
  for (UINTN i = 0; i < count && gLogFileCount < MAX_LOG_FILES; i++) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs = NULL;
    if (EFI_ERROR(gBS->HandleProtocol(handles[i], &gSfsGuid, (VOID **)&sfs)) || sfs == NULL) continue;
    EFI_FILE_HANDLE root = NULL;
    if (EFI_ERROR(sfs->OpenVolume(sfs, &root)) || root == NULL) continue;
    EFI_FILE_HANDLE f = NULL;
    EFI_STATUS ost = root->Open(root, &f, L"\\WSM-PROBE-LOG.TXT",
                                 EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    root->Close(root);
    UINTN n = i, di = 7; numbuf[7] = 0;
    if (n == 0) numbuf[--di] = L'0'; else { while (n > 0 && di > 0) { numbuf[--di] = L'0' + (n % 10); n /= 10; } }
    if (!EFI_ERROR(ost) && f != NULL) {
      CHAR16 lo = 0xFEFF; /* UTF-16LE BOM, one CHAR16 write, so text editors decode this correctly */
      UINTN sz = sizeof(CHAR16);
      f->Write(f, &sz, &lo);
      gLogFiles[gLogFileCount++] = f;
      gST->ConOut->OutputString(gST->ConOut, L"  volume "); gST->ConOut->OutputString(gST->ConOut, &numbuf[di]);
      gST->ConOut->OutputString(gST->ConOut, L": opened for write (OK)\r\n");
    } else {
      gST->ConOut->OutputString(gST->ConOut, L"  volume "); gST->ConOut->OutputString(gST->ConOut, &numbuf[di]);
      gST->ConOut->OutputString(gST->ConOut, L": could not open for write (read-only or unformatted)\r\n");
    }
  }
  gBS->FreePool(handles);
  if (gLogFileCount == 0) {
    gST->ConOut->OutputString(gST->ConOut, L"  NO writable volume found -- proceeding with screen output only.\r\n");
  }
}

/* Closes every open log handle and resets gLogFileCount to 0 --
 * critical, not cosmetic: pstr() writes to every entry in gLogFiles[]
 * up to gLogFileCount, and a closed EFI_FILE_HANDLE is not just inert,
 * it can point at firmware pool memory the allocator has since reused
 * or poisoned. Forgetting this reset was a real bug caught by actually
 * running this under QEMU, not by inspection: the two screen-only
 * status lines printed *after* the first version of this function ran
 * still routed through pstr(), which called ->Write() on the just-
 * closed handles and crashed with a #GP fault at RIP=0xAFAFAFAFAFAFAFAF
 * -- EDK2's own freed-pool poison pattern, not a coincidence. */
static void close_log_files(void) {
  for (int i = 0; i < gLogFileCount; i++) {
    if (gLogFiles[i] != NULL) gLogFiles[i]->Close(gLogFiles[i]);
    gLogFiles[i] = NULL;
  }
  gLogFileCount = 0;
}

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  (void)ImageHandle;
  gST = SystemTable;
  gBS = SystemTable->BootServices;

  open_log_files();

  pnl();
  pstr(L"=== wsm-os physical-boot-probe: real CPU/platform state ===\r\n");
  pstr(L"(this text is on-screen AND, if any volume opened above, written to \\WSM-PROBE-LOG.TXT)\r\n\r\n");

  UINT64 cr0 = read_cr0(), cr3 = read_cr3(), cr4 = read_cr4();
  UINT64 efer = read_msr(0xC0000080);
  UINT64 rsp = read_rsp();
  UINT64 rflags = read_rflags();
  DtReg gdtr, idtr;
  read_gdtr(&gdtr);
  read_idtr(&idtr);

  pstr(L"CR0            = "); phex64(cr0); pstr(L"  (PE="); pbit(cr0, 0); pstr(L" PG="); pbit(cr0, 31); pstr(L")\r\n");
  pstr(L"CR3            = "); phex64(cr3); pstr(L"  (page table base)\r\n");
  pstr(L"CR4            = "); phex64(cr4); pstr(L"  (PAE="); pbit(cr4, 5); pstr(L" PGE="); pbit(cr4, 7); pstr(L")\r\n");
  pstr(L"EFER (MSR 0xC0000080) = "); phex64(efer); pstr(L"  (LME="); pbit(efer, 8); pstr(L" LMA="); pbit(efer, 10); pstr(L" NXE="); pbit(efer, 11); pstr(L")\r\n");
  pstr(L"RSP            = "); phex64(rsp); pstr(L"  (16-byte aligned: "); pstr((rsp % 16 == 0) ? L"yes" : L"NO"); pstr(L")\r\n");
  pstr(L"RFLAGS         = "); phex64(rflags); pstr(L"  (IF="); pbit(rflags, 9); pstr(L", interrupts "); pstr(((rflags >> 9) & 1) ? L"ENABLED" : L"disabled"); pstr(L")\r\n");
  pstr(L"GDTR           = base "); phex64(gdtr.base); pstr(L" limit "); phex32(gdtr.limit); pnl();
  pstr(L"IDTR           = base "); phex64(idtr.base); pstr(L" limit "); phex32(idtr.limit); pnl();

  UINT32 a, b, c, d;
  do_cpuid(0, 0, &a, &b, &c, &d);
  pstr(L"CPUID leaf 0   = max_leaf="); pdec(a); pstr(L" vendor=");
  pchar((CHAR16)(b & 0xff)); pchar((CHAR16)((b >> 8) & 0xff)); pchar((CHAR16)((b >> 16) & 0xff)); pchar((CHAR16)((b >> 24) & 0xff));
  pchar((CHAR16)(d & 0xff)); pchar((CHAR16)((d >> 8) & 0xff)); pchar((CHAR16)((d >> 16) & 0xff)); pchar((CHAR16)((d >> 24) & 0xff));
  pchar((CHAR16)(c & 0xff)); pchar((CHAR16)((c >> 8) & 0xff)); pchar((CHAR16)((c >> 16) & 0xff)); pchar((CHAR16)((c >> 24) & 0xff));
  pnl();

  do_cpuid(1, 0, &a, &b, &c, &d);
  UINT32 stepping = a & 0xf, model = (a >> 4) & 0xf, family = (a >> 8) & 0xf;
  UINT32 ext_model = (a >> 16) & 0xf, ext_family = (a >> 20) & 0xff;
  UINT32 disp_family = (family == 0xf) ? (family + ext_family) : family;
  UINT32 disp_model = (family == 6 || family == 0xf) ? ((ext_model << 4) + model) : model;
  UINT32 apic_id = (b >> 24) & 0xff;
  pstr(L"CPUID leaf 1   = signature="); phex32(a);
  pstr(L" (family "); pdec(disp_family); pstr(L" model "); pdec(disp_model); pstr(L" stepping "); pdec(stepping);
  pstr(L") initial_APIC_ID="); pdec(apic_id); pnl();

  UINT32 live_ucode = read_live_microcode_revision();
  pstr(L"Live microcode revision (RDMSR 0x8B, read directly, pre-OS) = "); phex32(live_ucode); pnl();

  UINT64 tsc1 = read_tsc();
  gBS->Stall(100000);
  UINT64 tsc2 = read_tsc();
  UINT64 tsc_delta = tsc2 - tsc1;
  pstr(L"TSC (RDTSC)    = t1="); phex64(tsc1); pstr(L" t2="); phex64(tsc2);
  pstr(L" delta="); pdec(tsc_delta); pstr(L" cycles / 100ms Stall()\r\n");
  pstr(L"  implied rate = "); pdec(tsc_delta / 100000); pstr(L" cycles/us ("); pdec(tsc_delta / 100000); pstr(L" MHz-equivalent)\r\n");

  UINTN mapSize = 0, mapKey, descSize;
  UINT32 descVer;
  EFI_MEMORY_DESCRIPTOR *map = NULL;
  EFI_STATUS st = gBS->GetMemoryMap(&mapSize, map, &mapKey, &descSize, &descVer);
  if (st == EFI_BUFFER_TOO_SMALL) {
    mapSize += 2 * descSize;
    st = gBS->AllocatePool(EfiLoaderData, mapSize, (void **)&map);
    if (!EFI_ERROR(st)) st = gBS->GetMemoryMap(&mapSize, map, &mapKey, &descSize, &descVer);
  }
  if (!EFI_ERROR(st) && map != NULL) {
    UINTN entries = mapSize / descSize;
    UINT64 totalPages = 0, usablePages = 0;
    for (UINTN i = 0; i < entries; i++) {
      EFI_MEMORY_DESCRIPTOR *d2 = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)map + i * descSize);
      totalPages += d2->NumberOfPages;
      if (d2->Type == EfiConventionalMemory) usablePages += d2->NumberOfPages;
    }
    pstr(L"Memory map     = "); pdec(entries); pstr(L" descriptors, "); pdec(totalPages);
    pstr(L" total pages ("); pdec((totalPages * 4096) / (1024 * 1024)); pstr(L" MiB), ");
    pdec(usablePages); pstr(L" conventional/usable pages ("); pdec((usablePages * 4096) / (1024 * 1024)); pstr(L" MiB)\r\n");
  } else {
    pstr(L"Memory map     = FAILED to read (status "); phex64(st); pstr(L")\r\n");
  }

  VOID *acpiRoot = NULL;
  for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++) {
    EFI_GUID *g = &SystemTable->ConfigurationTable[i].VendorGuid;
    if (guid_eq(g, &gAcpi20Guid)) acpiRoot = SystemTable->ConfigurationTable[i].VendorTable;
  }
  pstr(L"ACPI RSDP      = "); pstr(acpiRoot ? L"present" : L"NOT FOUND"); pnl();
  if (acpiRoot) { pstr(L"  address      = "); phex64((UINT64)acpiRoot); pnl(); }

  VOID *smbios = NULL;
  int smbios3 = 0;
  for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++) {
    EFI_GUID *g = &SystemTable->ConfigurationTable[i].VendorGuid;
    if (guid_eq(g, &gSmbios3Guid)) { smbios = SystemTable->ConfigurationTable[i].VendorTable; smbios3 = 1; }
    else if (guid_eq(g, &gSmbiosGuid) && smbios == NULL) smbios = SystemTable->ConfigurationTable[i].VendorTable;
  }
  pstr(L"SMBIOS         = "); pstr(smbios ? L"present" : L"NOT FOUND"); if (smbios3) pstr(L" (SMBIOS3/64-bit)"); pnl();
  if (smbios) { pstr(L"  address      = "); phex64((UINT64)smbios); pnl(); }

  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
  st = gBS->LocateProtocol(&gGopGuid, NULL, (VOID **)&gop);
  if (!EFI_ERROR(st) && gop != NULL) {
    pstr(L"Framebuffer    = "); phex64(gop->Mode->FrameBufferBase);
    pstr(L"  "); pdec(gop->Mode->Info->HorizontalResolution); pstr(L"x"); pdec(gop->Mode->Info->VerticalResolution);
    pstr(L"  pixels-per-scanline="); pdec(gop->Mode->Info->PixelsPerScanLine);
    pstr(L"  mode="); pdec(gop->Mode->Mode); pnl();
  } else {
    pstr(L"Framebuffer    = NOT AVAILABLE (status "); phex64(st); pstr(L")\r\n");
  }

  pstr(L"\r\n=== end of capture ===\r\n");
  close_log_files();
  pstr(L"\r\n=== log file(s) closed and flushed -- SAFE to power off / remove the USB stick now ===\r\n");
  pstr(L"(if a volume above said \"opened for write (OK)\", check that volume's root for WSM-PROBE-LOG.TXT)\r\n");

  for (;;) {
    gBS->Stall(1000000);
  }
  return EFI_SUCCESS;
}
