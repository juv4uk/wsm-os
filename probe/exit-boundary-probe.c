/*
 * exit-boundary-probe.c -- crosses ExitBootServices() for real and
 * proves liveness on the other side without any UEFI API at all.
 *
 * Per the owner's own next-step framing after handoff-probe.c: step 9
 * proved a probe can observe the machine through UEFI; this is step
 * 10, proving control can survive the one boundary that actually
 * matters -- the moment Boot Services (and everything built on them,
 * including ConOut) stop being valid. The criterion, his own words:
 *
 *   UEFI console: BEFORE_EXIT
 *   ExitBootServices()
 *   [no UEFI used from here on]
 *   raw channel:  AFTER_EXIT
 *   assembly:     ()
 *   raw channel:  WSM_0_REACHED
 *
 * Deliberately not doing: an OS, an allocator, a scheduler, Lisp, a
 * Rust runtime, or even canonical `t`. Reaching a point of pure,
 * UEFI-independent control and proving it -- nothing else.
 *
 * "()" itself is deliberately NOT encoded here. Choosing a bit
 * pattern for () is real design work belonging to wsm's own
 * architecture, not something to rush inside a wsm-os lab probe --
 * see wsm/research/handoff-state.md and the ()-preservation
 * discipline in wsm's own docs/ROADMAP.md. This probe only proves the
 * boundary is crossable and observable; it does not decide anything
 * about what WSM's own machine state should look like once there.
 *
 * ARCHITECTURAL BOUNDARY (per the owner's own diagram): everything
 * above the ExitBootServices() call belongs to wsm-os -- the
 * laboratory, the bootstrap apparatus, UEFI/gnu-efi/clang/COFF/OVMF/
 * QEMU as tools for observing and crossing the boundary. Everything
 * from that call onward is where wsm's own machine would actually
 * begin, were this probe's tail end to become a real entry point
 * rather than a proof-of-crossing. This file stays in wsm-os because
 * it IS the crossing experiment, not wsm's own code -- wsm itself
 * should never need to know gnu-efi, clang's COFF target, or OVMF
 * exist.
 */

#include <efi.h>

static inline void outb(UINT16 port, UINT8 val) {
  __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline UINT8 inb(UINT16 port) {
  UINT8 ret;
  __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

/* Raw 16550 UART (COM1, base 0x3F8) -- no UEFI protocol involved at
 * all, works identically before or after ExitBootServices(). Not
 * reinitializing baud/LCR here: OVMF's own SIO driver already
 * configured this exact UART compatibly (confirmed empirically --
 * every earlier probe's console output already came through this
 * same serial line). A physical-hardware pass would need to verify or
 * set this explicitly, not assume it holds there too. */
#define COM1_BASE 0x3F8
#define COM1_LSR (COM1_BASE + 5)
#define LSR_THR_EMPTY 0x20

static void raw_putc(char c) {
  while ((inb(COM1_LSR) & LSR_THR_EMPTY) == 0) { }
  outb(COM1_BASE, (UINT8)c);
}
static void raw_puts(const char *s) {
  while (*s) {
    if (*s == '\n') raw_putc('\r');
    raw_putc(*s++);
  }
}

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_BOOT_SERVICES *BS = SystemTable->BootServices;

  SystemTable->ConOut->OutputString(SystemTable->ConOut, L"BEFORE_EXIT\r\n");

  UINTN mapSize = 0, mapKey = 0, descSize = 0;
  UINT32 descVer = 0;
  EFI_MEMORY_DESCRIPTOR *map = NULL;
  EFI_STATUS st = EFI_SUCCESS;
  int exited = 0;

  /* GetMemoryMap+ExitBootServices retry loop: any allocation between
   * the two invalidates the map key, so ExitBootServices can
   * legitimately fail once with EFI_INVALID_PARAMETER -- that is not
   * an error to give up on, it is documented UEFI behavior. Boot
   * Services remain valid after a FAILED ExitBootServices call, so
   * re-fetching the map and retrying is safe; they do not remain
   * valid after a successful one, which is exactly the line this
   * whole probe exists to cross. */
  for (int attempt = 0; attempt < 5 && !exited; attempt++) {
    mapSize = 0;
    st = BS->GetMemoryMap(&mapSize, map, &mapKey, &descSize, &descVer);
    if (st == EFI_BUFFER_TOO_SMALL) {
      if (map) BS->FreePool(map);
      mapSize += 4 * descSize;
      st = BS->AllocatePool(EfiLoaderData, mapSize, (void **)&map);
      if (EFI_ERROR(st)) break;
      st = BS->GetMemoryMap(&mapSize, map, &mapKey, &descSize, &descVer);
    }
    if (EFI_ERROR(st)) break;

    st = BS->ExitBootServices(ImageHandle, mapKey);
    if (!EFI_ERROR(st)) {
      exited = 1;
    }
    /* else: loop again, re-fetch a fresh map/key */
  }

  if (!exited) {
    /* Boot Services are still valid here -- this branch never touches
     * the raw UART, only ConOut, on purpose. */
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"ExitBootServices FAILED after retries\r\n");
    for (;;) {
      BS->Stall(1000000);
    }
  }

  /* ==================== THE BOUNDARY ====================
   * Past this point: no Boot Services, no ConOut, no UEFI protocol of
   * any kind. `cli` first, defensively -- any leftover periodic timer
   * interrupt still configured by firmware could otherwise fire and
   * touch now-invalid Boot-Services-era state; real bootloaders mask
   * interrupts around exactly this transition for the same reason. */
  __asm__ __volatile__("cli");

  raw_puts("AFTER_EXIT\n");

  /* "assembly only" -- this is the whole of it. No () encoding chosen
   * here; see the file header. Reaching this halt, with nothing else
   * running and no UEFI left underneath, is the entire claim. */
  raw_puts("WSM_0_REACHED\n");

  for (;;) {
    __asm__ __volatile__("hlt");
  }
  return EFI_SUCCESS;
}
