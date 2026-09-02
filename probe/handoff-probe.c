/*
 * handoff-probe.c -- minimal UEFI application that reads real CPU/
 * platform state and prints it before ExitBootServices(), per the
 * owner's own 10-step BIOS research program (step 9): "не Rust
 * runtime, не великий loader. Маленький stub, який перед
 * ExitBootServices() записує у serial: memory map, GOP framebuffer,
 * ACPI root, SMBIOS, CPU state, loaded microcode revision."
 *
 * Self-contained: no gnu-efi runtime library (libefi.a/libgnuefi.a),
 * no InitializeLib/Print. Everything talks to SystemTable's protocols
 * directly. This is a deliberate build-tooling fix, not a style
 * choice: the gnu-efi 4.0.4 Guix package's own gcc+ld+objcopy
 * ELF->PE conversion pipeline produced a binary that loaded under
 * OVMF (BdsDxe: starting Boot0001 ...) but then emitted an endless
 * repeating 3-byte garbage pattern (0xBE 0xAF 0xEA ...) instead of
 * running -- reproduced identically across three different disk
 * backends (QEMU's fat:rw: driver, an unpartitioned raw FAT image, a
 * properly GPT-partitioned image), ruling out the disk layout, and
 * with QEMU's own -d int,guest_errors log showing no CPU exceptions
 * at all (only the expected periodic timer IRQ), ruling out a genuine
 * CPU fault. Isolated by bisection to the gnu-efi build itself (even
 * a bare InitializeLib+one Print()+Stall() loop reproduced it) --
 * most likely an ABI/codegen mismatch between this environment's very
 * recent GCC 16 and gnu-efi's own long-unmaintained ELF-to-PE
 * conversion assumptions. Building instead with clang's native
 * x86_64-unknown-windows target and lld-link (direct COFF/PE output,
 * no ELF conversion step) produced a working binary on the first
 * try -- confirmed with a minimal hello-world before porting this
 * file over. See build.sh for the exact working recipe.
 *
 * Everything read here runs before ExitBootServices() -- UEFI code at
 * this stage executes in a flat, unrestricted ring 0, so control-
 * register and MSR reads that would fault from an ordinary OS
 * userspace process work directly here. This is real, LIVE-CONFIRMED
 * evidence when it runs -- not STATIC-CONFIRMED (it is not reading
 * firmware bytes) and not INFERRED.
 */

#include <efi.h>

static EFI_SYSTEM_TABLE *gST;

static void pstr(CHAR16 *s) { gST->ConOut->OutputString(gST->ConOut, s); }

/* Minimal hex/decimal printers -- no libc, no gnu-efi PrintLib. */
static void phex64(UINT64 v) {
  CHAR16 buf[19];
  const CHAR16 *digits = L"0123456789ABCDEF";
  buf[0] = L'0';
  buf[1] = L'x';
  for (int i = 0; i < 16; i++) {
    buf[2 + i] = digits[(v >> ((15 - i) * 4)) & 0xF];
  }
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
    while (v > 0 && i > 0) {
      buf[--i] = L'0' + (CHAR16)(v % 10);
      v /= 10;
    }
  }
  pstr(&buf[i]);
}
static void pbit(UINT64 v, int bit) { pdec((v >> bit) & 1); }
static void pchar(CHAR16 c) {
  CHAR16 buf[2] = {c, 0};
  pstr(buf);
}
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
/* RDTSC -- raw cycle count since the last CPU reset. A single read is
 * a bare tally value, not yet meaningful on its own (see
 * wsm/research/boot-time-extraction-attacked.md, Candidate 2: this is
 * REPEAT already implemented in silicon). This probe reads it twice,
 * separated by a measured Stall(), specifically so the delta -- not
 * either raw value alone -- is what gets reported: turning the
 * document's structural claim ("a raw count means nothing without a
 * second reference point and a subtraction") into something actually
 * measured on this hardware, not just argued. */
static inline UINT64 read_tsc(void) {
  UINT32 lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((UINT64)hi << 32) | lo;
}
typedef struct { UINT16 limit; UINT64 base; } __attribute__((packed)) DtReg;
static inline void read_gdtr(DtReg *r) { __asm__ __volatile__("sgdt %0" : "=m"(*r)); }
static inline void read_idtr(DtReg *r) { __asm__ __volatile__("sidt %0" : "=m"(*r)); }

/* Real Intel microcode-revision read: write 0 to IA32_BIOS_SIGN_ID
 * (0x8B), execute CPUID (forces the microcode-update mechanism to
 * refresh the MSR), then read it back -- the high 32 bits are the
 * currently-loaded revision. This is the same MSR the owner's earlier
 * Windows-registry reading reflects (0x8B is what "Update Revision"
 * is sourced from) -- here it is read directly, at the real handoff
 * point, before any OS (and its own microcode loader) has touched the
 * CPU at all. */
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

/* Well-known GUIDs (values are public/stable UEFI spec constants --
 * not redeclaring gnu-efi's externs since this file no longer links
 * against gnu-efi's library at all). */
static EFI_GUID gAcpi20Guid = {0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};
static EFI_GUID gSmbios3Guid = {0xf2fd1544, 0x9794, 0x4a2c, {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94}};
static EFI_GUID gSmbiosGuid = {0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};
static EFI_GUID gGopGuid = {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  (void)ImageHandle;
  gST = SystemTable;
  EFI_BOOT_SERVICES *BS = SystemTable->BootServices;

  pnl();
  pstr(L"=== wsm-os handoff-probe: real CPU/platform state, pre-ExitBootServices ===\r\n");
  pstr(L"(LIVE-CONFIRMED when this line is visible -- read from the real running machine, not reconstructed)\r\n\r\n");

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
  BS->Stall(100000); /* 100ms, measured against Boot Services' own timer */
  UINT64 tsc2 = read_tsc();
  UINT64 tsc_delta = tsc2 - tsc1;
  pstr(L"TSC (RDTSC)    = t1="); phex64(tsc1); pstr(L" t2="); phex64(tsc2);
  pstr(L" delta="); pdec(tsc_delta); pstr(L" cycles / 100ms Stall()\r\n");
  pstr(L"  implied rate = "); pdec(tsc_delta / 100000); pstr(L" cycles/us ("); pdec(tsc_delta / 100000); pstr(L" MHz-equivalent)\r\n");

  UINTN mapSize = 0, mapKey, descSize;
  UINT32 descVer;
  EFI_MEMORY_DESCRIPTOR *map = NULL;
  EFI_STATUS st = BS->GetMemoryMap(&mapSize, map, &mapKey, &descSize, &descVer);
  if (st == EFI_BUFFER_TOO_SMALL) {
    mapSize += 2 * descSize;
    st = BS->AllocatePool(EfiLoaderData, mapSize, (void **)&map);
    if (!EFI_ERROR(st)) {
      st = BS->GetMemoryMap(&mapSize, map, &mapKey, &descSize, &descVer);
    }
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
  st = BS->LocateProtocol(&gGopGuid, NULL, (VOID **)&gop);
  if (!EFI_ERROR(st) && gop != NULL) {
    pstr(L"Framebuffer    = "); phex64(gop->Mode->FrameBufferBase);
    pstr(L"  "); pdec(gop->Mode->Info->HorizontalResolution); pstr(L"x"); pdec(gop->Mode->Info->VerticalResolution);
    pstr(L"  pixels-per-scanline="); pdec(gop->Mode->Info->PixelsPerScanLine);
    pstr(L"  mode="); pdec(gop->Mode->Mode); pnl();
  } else {
    pstr(L"Framebuffer    = NOT AVAILABLE (status "); phex64(st); pstr(L") -- expected under -nographic/serial-only QEMU\r\n");
  }

  pstr(L"\r\n=== end of probe, not calling ExitBootServices() in this pass ===\r\n");
  pstr(L"(deliberately: capturing pre-ExitBootServices state was this run's whole goal;\r\n");
  pstr(L" a later pass can add a post-ExitBootServices marker for comparison)\r\n");

  /* Idle rather than exit, so the transcript above stays on screen/serial
   * long enough for the launching script's timeout to capture it. */
  for (;;) {
    BS->Stall(1000000);
  }
  return EFI_SUCCESS;
}
