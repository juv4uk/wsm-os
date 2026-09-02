# What actually happens when the power turns on / Що насправді стається при вмиканні живлення

The full path from pressing the power button to the point this
repo's own `probe/exit-boundary-probe.c` has already proven reachable
(`ExitBootServices()`, raw serial control). Every phase below cites
what grounds it: `STATIC-CONFIRMED` (read directly from this board's
own F22e flash image, see `hardware/bios-f22e/`), `LIVE-CONFIRMED`
(directly observed in this repo's own QEMU/OVMF runs — real BDS
output, not a description of it), or `general` (well-documented public
Intel/x86 platform architecture, not independently re-verified against
this exact board's silicon fuse state).
Повний шлях від натискання кнопки живлення до точки, яку цей репозиторій
вже реально довів досяжною. Кожна фаза підписана джерелом підтвердження.

## Phase 0 — physical / analog (general, not board-specific)

ATX PSU's 5VSB standby rail is live whenever the machine is plugged in,
independent of the power switch. Pressing the power button pulls
`PWR_BTN#` low; the motherboard's embedded controller / PCH power
sequencing logic drives `PS_ON#` to bring up the main PSU rails, then
sequences its own internal power planes (`RSMRST#`, the `SLP_S3#` /
`SLP_S4#` / `SLP_S5#` state machine). Only once PCH's own power is
stable does it release `PLTRST#` (platform reset), which is what
actually releases the CPU from reset. None of this is verified against
the H170-Gaming 3's own schematic — it is standard ATX/Intel platform
design, cited as `general`, not `STATIC-CONFIRMED`.

## Phase 1 — the Intel Management Engine boots first (general)

Before the main CPU begins executing anything at all, the Intel
Management Engine — a separate x86 core embedded in the PCH, running
its own independent OS — boots on its own. The main CPU's reset
sequence does not proceed until ME signals it is ready. This repo's
own analysis did not parse the ME region's contents (`FIT-AND-STRUCTURE-ANALYSIS.md`
explicitly noted this as out of scope — the ME region's 2,093,056
bytes are `STATIC-CONFIRMED` present by size/offset, but not
internally decoded). That ME exists and gates the main CPU's start is
`general`, well-documented platform architecture, not something this
repo independently confirmed for this exact firmware.

## Phase 2 — CPU reset, the FIT, and real microcode loading — STATIC-CONFIRMED which microcode

The main CPU's reset vector has architecturally lived at
`0xFFFFFFF0` since the 8086 — but on this platform generation, that is
no longer the *first* thing executed. At reset, the CPU's own
microcode reads a pointer at the fixed physical address `0xFFFFFFC0`
(`general`, Intel's own published FIT specification) — **this repo
already parsed that exact pointer on the real F22e image and found a
valid `_FIT_` table there** (`FIT-AND-STRUCTURE-ANALYSIS.md`,
`STATIC-CONFIRMED`). The FIT's Type 1 (Microcode Update) entries are
processed *before* the legacy reset vector runs at all — this board's
FIT has three, one matching the owner's exact CPU signature
(`0x000506E3`): **revision `0xC2`, dated 2017-11-16**
(`STATIC-CONFIRMED`, cross-confirmed by a second, independent tool
finding the same blob set inside a named `CPU_MICROCODE_FILE_GUID`
container). This is not a hypothetical step — it is the specific,
identified microcode blob this exact board's firmware would load into
this exact CPU at this exact phase, on every real cold boot.

## Phase 3 — Startup ACM / Boot Guard, if fused (general architecture; enforcement not verified)

If the platform's write-once fuses require it, the CPU's microcode
also reads the FIT's Type 2 (Startup ACM) entry, copies the
Authenticated Code Module into CPU cache (cache-as-RAM, since no real
DRAM is initialized yet — copying to cache first specifically prevents
a flash-swap attack between verification and execution), and verifies
its signature against a hardcoded Intel key. The ACM then runs in
32-bit protected mode, reads the fused OEM public-key hash, and
verifies the Initial Boot Block's signature before the microcode
switches *back* to 16-bit real mode for legacy compatibility and hands
off to the traditional reset vector.

**This repo already found the concrete FIT entry this would use**:
Type 2 "BIOS Startup Module (ACM)" at flash-mapped address
`0xFFFF0000` (`FIT-AND-STRUCTURE-ANALYSIS.md`), matching a named
`PEI_BIOS_ACM_FILE_GUID` FFS file (184,088 bytes) found independently
by the second tool. **What is honestly `not-yet-verified`: whether
Boot Guard is actually fused/enforced on this specific owner's board at
all.** Its presence in the firmware image only proves the mechanism
exists in the flash — most mainstream desktop boards (this H170 chipset
is a mainstream consumer part, not a managed/enterprise SKU) ship with
Boot Guard fuses left unprogrammed by the OEM, in which case this phase
is present in the image but never actually enforced by the CPU. This is
a real CPU/silicon fuse-state fact, not something readable from the
firmware image alone, and was not checked this pass.

## Phase 4 — SEC (Security) phase (general)

The earliest UEFI PI-spec phase. Runs with no real DRAM available yet
— temporary storage lives in the CPU's own cache, configured as
cache-as-RAM (CAR). Minimal code: locates and verifies the PEI
Foundation, sets up a temporary stack, hands off.

## Phase 5 — PEI (Pre-EFI Initialization) — STATIC-CONFIRMED module presence

PEI Modules (PEIMs) run here, most importantly the Memory Reference
Code that actually trains and initializes the real DDR4 DIMMs (the
owner's real `CT8G4DFS8213.C8FBD1` + `TEAMGROUP-UD4-2133` pair, per
`OWNER-HARDWARE-PROFILE.md`) — this is the first point real system RAM
becomes usable at all. This repo's own FFS inventory
(`FIT-AND-STRUCTURE-ANALYSIS.md`) already found 164 FFS files of type
`0x02` (PEIM) among this image's 780 total files, plus the specifically
named `PEI_AP_STARTUP_FILE_GUID` file (Application-Processor bring-up
— the mechanism that eventually brings the other 3 CPU cores online,
directly relevant to a future "cores online" handoff-state field).
PEI ends by building a HOB (Hand-Off Block) list describing what it
found and handing control to DXE.

## Phase 6 — DXE (Driver Execution Environment) — STATIC-CONFIRMED table presence

The phase that does most of what people mean by "the BIOS": PCI
enumeration, chipset/device driver dispatch, building the ACPI and
SMBIOS tables an OS will later read. This repo already confirmed
(`FIT-AND-STRUCTURE-ANALYSIS.md`) that the decompressed main firmware
volume contains real `RSD PTR `, `FACP`, `APIC`, `MCFG`, `DMAR`,
`HPET`, `DSDT` (×19), `SSDT` (×64), and SMBIOS entry-point signatures —
and separately, `probe/handoff-probe.c`'s own real run
(`probe/README.md`) directly read a live ACPI RSDP pointer and an
SMBIOS pointer out of `SystemTable->ConfigurationTable`, confirming
this phase's output is actually reachable at runtime, not just present
as templates in flash. NVRAM variable stores (`NvramPei`/`NvramDxe`/
`NvramSmm`) are also built/consulted here.

## Phase 7 — BDS (Boot Device Selection) — LIVE-CONFIRMED, directly observed

This is not inferred from documentation — **every single probe run in
this repo's QEMU/OVMF setup, without exception, has printed this phase
by name**:

```text
BdsDxe: loading Boot0001 "UEFI QEMU HARDDISK QM00001 " from PciRoot(0x0)/Pci(0x1F,0x2)/Sata(0x0,0xFFFF,0x0)
BdsDxe: starting Boot0001 "UEFI QEMU HARDDISK QM00001 " from PciRoot(0x0)/Pci(0x1F,0x2)/Sata(0x0,0xFFFF,0x0)
```

BDS walks the boot-option variables (or, absent any, falls back to
`\EFI\BOOT\BOOTX64.EFI` on a connected removable/fixed device, which is
exactly the path every probe in this repo boots through) and transfers
control to the chosen UEFI application — in this repo's case, our own
probes; on the owner's real machine normally an OS's own boot loader.

## Phase 8 — Boot Services era, then the boundary this repo already crossed

From here, a loaded UEFI application (an OS loader, or this repo's own
probes) can freely call Boot Services — console I/O, memory
allocation, protocol location — exactly what `handoff-probe.c` used to
read CR0/CR3/CR4/EFER/CPUID/live-microcode/memory-map/ACPI/SMBIOS/
framebuffer (`probe/README.md`, `LIVE-CONFIRMED` for the QEMU/OVMF
virtual environment). The final step — the one this repo has already
**proven, not merely described** — is `ExitBootServices()`:
`exit-boundary-probe.c` calls it for real, and with Boot Services
(including the console) now genuinely invalid, proves continued
liveness over a raw port-I/O serial channel with zero UEFI API left:

```text
BEFORE_EXIT           <- UEFI ConOut, Boot Services still valid
ExitBootServices()
AFTER_EXIT              <- raw port I/O to the 16550 UART at 0x3F8, no UEFI at all
WSM_0_REACHED
```

Everything from Phase 0 through here is what firmware does *for* a
future WSM entry point, not something WSM itself needs to reimplement,
understand in full, or trust beyond what it can independently verify —
per the standing distinction already recorded in
`wsm/research/handoff-state.md`.

## Sources

- [Intel Firmware Interface Table BIOS Specification, doc 599500](https://www.intel.com/content/dam/develop/external/us/en/documents/firmware-interface-table-bios-specification-r1p2p1.pdf)
- [coreboot: Intel Firmware Interface Table](https://doc.coreboot.org/soc/intel/fit.html)
- [Intel EDC: Startup ACM (Type 2) Rules](https://edc.intel.com/content/www/us/en/design/products-and-solutions/software-and-services/firmware-and-bios/firmware-interface-table/1.4/startup-acm-type-2-rules/)
- [mjg59: Booting modern Intel CPUs](https://mjg59.dreamwidth.org/66109.html)
- [Trammell Hudson: Bootguard](https://trmm.net/Bootguard/)
- [Wikipedia: Reset vector](https://en.wikipedia.org/wiki/Reset_vector)
- This repo's own: `hardware/bios-f22e/BINARY-ANALYSIS.md`, `hardware/bios-f22e/FIT-AND-STRUCTURE-ANALYSIS.md`, `docs/OWNER-HARDWARE-PROFILE.md`, `probe/README.md`
