/*
 * wsm-probe-driver.c -- embedded, auto-dispatched DXE driver variant of
 * physical-boot-probe.c. Where that probe is a manually-launched
 * EFI_APPLICATION meant for one-off Ventoy USB testing (screen output +
 * a log file written to every writable volume), this one is built to
 * live inside the firmware itself, as EFI_FV_FILETYPE_DRIVER, and run
 * automatically on every boot without anyone invoking it.
 *
 * That difference in role forces two real behavior changes, not just a
 * rename:
 *
 *   1. physical-boot-probe.c ends in `for (;;) gBS->Stall(...)` -- fine
 *      for a manually-launched application the operator expects to sit
 *      there until the USB stick is pulled, but fatal for a DXE driver:
 *      the DXE Dispatcher calls a driver's entry point and expects it
 *      to RETURN so dispatch can continue to the rest of DXE and then
 *      BDS. An infinite loop here would hang every single boot,
 *      permanently, until the driver is removed from the firmware
 *      again. This one returns EFI_SUCCESS immediately after doing its
 *      one-shot capture.
 *
 *   2. physical-boot-probe.c opens \WSM-PROBE-LOG.TXT on *every*
 *      writable volume it can find -- reasonable for a deliberate,
 *      one-time test run, but not something that should happen on
 *      every normal boot of the owner's actual machine (it would leave
 *      a stray file on the Windows/Linux boot drive every single time).
 *      This variant does not touch the filesystem at all. Instead it
 *      writes one EFI_VARIABLE, which is the standard, OS-observable
 *      channel for exactly this kind of boot-time-firmware-to-later-OS
 *      handoff: Linux reads it after boot via efivarfs
 *      (/sys/firmware/efi/efivars/), no custom driver needed on the OS
 *      side. NON_VOLATILE so it survives a power cycle in NVRAM;
 *      RUNTIME_ACCESS so it is still readable once Linux has called
 *      ExitBootServices() and taken over.
 *
 * Same build discipline as every other probe here: gnu-efi headers
 * only, no gnu-efi runtime library linked in (see handoff-probe.c's
 * header comment for why -- the ELF-to-PE gnu-efi pipeline was tried
 * first and produced a binary that loaded under OVMF but then executed
 * garbage instead of running).
 */

#include <efi.h>

#pragma pack(1)
typedef struct {
  UINT32 Magic;           /* 'WSMP' = 0x504D5357, so a reader can sanity-check this is really ours */
  UINT32 StructVersion;   /* bump if this layout ever changes */
  UINT64 Tsc1;
  UINT64 Tsc2;
  UINT64 TscDeltaOver100ms;
  UINT32 LiveMicrocodeRevision; /* from RDMSR 0x8B, read live, pre-OS */
  UINT64 BootGuardMsr;          /* raw MSR 0x13A (BOOTGUARD_SACM_INFO) */
  UINT32 Cpuid1Signature;       /* CPUID leaf 1 EAX */
  UINT32 BootCounter;           /* how many times this driver has run; read-modify-write of its own prior value */
} WSM_PROBE_DATA;
#pragma pack()

#define WSM_PROBE_DATA_VERSION 1
#define WSM_PROBE_MAGIC 0x504D5357u /* 'W' 'S' 'M' 'P' little-endian as UINT32 */

/* Fresh, dedicated GUID for this variable -- not reused from anywhere
 * else in this repository or in any known vendor/spec GUID list. */
static EFI_GUID gWsmProbeVarGuid = {
  0x3a1f9c2e, 0x7b44, 0x4e1a,
  {0x9d, 0x6c, 0x1e, 0x2f, 0x8a, 0x53, 0xc0, 0x77}
};
static CHAR16 gWsmProbeVarName[] = L"WsmProbeData";

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

#define MSR_IA32_BIOS_SIGN_ID 0x8B
static UINT32 read_live_microcode_revision(void) {
  UINT32 a, b, c, d;
  write_msr(MSR_IA32_BIOS_SIGN_ID, 0);
  do_cpuid(1, 0, &a, &b, &c, &d);
  UINT64 sign = read_msr(MSR_IA32_BIOS_SIGN_ID);
  return (UINT32)(sign >> 32);
}

#define MSR_BOOTGUARD 0x13A

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  (void)ImageHandle;
  EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
  EFI_RUNTIME_SERVICES *RT = SystemTable->RuntimeServices;

  WSM_PROBE_DATA data;
  data.Magic = WSM_PROBE_MAGIC;
  data.StructVersion = WSM_PROBE_DATA_VERSION;

  UINT32 a, b, c, d;
  do_cpuid(1, 0, &a, &b, &c, &d);
  data.Cpuid1Signature = a;

  data.LiveMicrocodeRevision = read_live_microcode_revision();
  data.BootGuardMsr = read_msr(MSR_BOOTGUARD);

  data.Tsc1 = read_tsc();
  BS->Stall(100000);
  data.Tsc2 = read_tsc();
  data.TscDeltaOver100ms = data.Tsc2 - data.Tsc1;

  /* Read back any prior value purely to carry the boot counter forward;
   * a failed read (first boot after flashing, variable does not exist
   * yet) is expected and treated as counter == 0, not an error. */
  data.BootCounter = 0;
  {
    WSM_PROBE_DATA prior;
    UINTN size = sizeof(prior);
    EFI_STATUS st = RT->GetVariable(gWsmProbeVarName, &gWsmProbeVarGuid, NULL, &size, &prior);
    if (!EFI_ERROR(st) && size == sizeof(prior) && prior.Magic == WSM_PROBE_MAGIC) {
      data.BootCounter = prior.BootCounter + 1;
    }
  }

  RT->SetVariable(
    gWsmProbeVarName,
    &gWsmProbeVarGuid,
    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
    sizeof(data),
    &data
  );

  /* Must return so DXE dispatch can continue -- see header comment. */
  return EFI_SUCCESS;
}
