# handoff-probe — a real UEFI application that reads live CPU/platform state

Per the owner's ten-step BIOS research program, step 9: "Не Rust
runtime, не великий loader. Маленький stub, який перед
`ExitBootServices()` записує у serial: memory map, GOP framebuffer,
ACPI root, SMBIOS, CPU state, loaded microcode revision." This is that
stub, real and running, not a design sketch.

## Usage

```bash
./build.sh      # clang + lld-link -> handoff-probe.efi (see below for why not gnu-efi's own toolchain)
./make-esp.sh   # sgdisk + mtools -> esp.img (a real GPT-partitioned FAT32 disk image)
./run-probe.sh  # boots esp.img under the wsm-os hardware-tuned QEMU/OVMF setup
```

All three scripts are idempotent and require no root/loop-mounting —
`make-esp.sh` partitions and formats a plain file directly via `sgdisk`
and `mtools`'s byte-offset addressing (`file@@offset`).

## Real, captured output (2026-09-02)

```text
=== wsm-os handoff-probe: real CPU/platform state, pre-ExitBootServices ===
(LIVE-CONFIRMED when this line is visible -- read from the real running machine, not reconstructed)

CR0            = 0x0000000080010033  (PE=1 PG=1)
CR3            = 0x0000000007C01000  (page table base)
CR4            = 0x0000000000000668  (PAE=1 PGE=0)
EFER (MSR 0xC0000080) = 0x0000000000000D00  (LME=1 LMA=1 NXE=1)
RSP            = 0x0000000007E77680  (16-byte aligned: yes)
RFLAGS         = 0x0000000000000206  (IF=1, interrupts ENABLED)
GDTR           = base 0x0000000000000000 limit 0x0000000000000047
IDTR           = base 0x0000000007E80000 limit 0x0000000000000FFF
CPUID leaf 0   = max_leaf=13 vendor=GenuineIntel
CPUID leaf 1   = signature=0x00000000000506E3 (family 6 model 94 stepping 3) initial_APIC_ID=0
Live microcode revision (RDMSR 0x8B, read directly, pre-OS) = 0x0000000000000001
Memory map     = 121 descriptors, 99232 total pages (387 MiB), 21889 conventional/usable pages (85 MiB)
ACPI RSDP      = present
  address      = 0x0000000007B7E014
SMBIOS         = present
  address      = 0x000000000793F000
Framebuffer    = 0x0000000080000000  1280x800  pixels-per-scanline=1280  mode=0
```

**Scope, stated plainly:** this is `LIVE-CONFIRMED` for the QEMU/OVMF
*virtual* environment this ran in on this host — **not** the owner's
physical i5-6400. It is a real measurement of a real (emulated)
machine's actual boot-time state, not a reconstruction — that distinction
from `BINARY-ANALYSIS.md`'s static reading still holds. But it is not
yet a measurement of the physical hardware. Two concrete places where
QEMU/TCG visibly diverges from what the real silicon would report:

- **Live microcode revision reads back as `0x1`.** TCG does not
  emulate Intel's real microcode-update mechanism; `IA32_BIOS_SIGN_ID`
  under TCG is a stub, not a reflection of any real loaded microcode.
  On the physical machine this exact mechanism would instead read the
  real revision — plausibly `0xD6`, matching the Windows-registry
  reading from `wsm/research/handoff-state.md`, or possibly the F22e-
  embedded `0xC2` if run before Windows' own microcode override lands
  — but that is `predicted`, not measured, until this probe (or an
  equivalent) actually runs on real hardware.
- **CPUID signature reads back as exactly `0x000506E3`.** This is
  QEMU faithfully reporting back the `-cpu Skylake-Client-v1` model it
  was told to emulate — a real read of the *emulated* CPU, correctly
  matching the owner's real signature by construction (that's what the
  hardware-tuned QEMU setup in `docs/QEMU-SETUP.md` is for), not
  independent confirmation of anything.
- **Framebuffer (1280x800, `0x80000000`) and the memory map's specific
  numbers** reflect QEMU's own virtual chipset/GPU, not the owner's
  real Intel HD Graphics 530 or the real machine's actual E820/UEFI
  memory map.

Physical-hardware execution of this exact probe is separate,
owner-authorized future work — same boundary already stated in
`docs/QEMU-SETUP.md` for boot-image execution generally.

## Why clang + lld-link, not gnu-efi's own gcc + ld + objcopy pipeline

The first build attempt used gnu-efi's documented recipe (gcc,
`elf_x86_64_efi.lds`, `crt0-efi-x86_64.o`, then `objcopy
--target=efi-app-x86_64` to convert the resulting ELF shared object to
PE32+). It compiled and linked cleanly, `file` reported a valid PE32+
EFI application, and OVMF's own boot log confirmed it loaded
(`BdsDxe: starting Boot0001 ...`) — but instead of running, it emitted
an endless repeating 3-byte garbage pattern (`0xBE 0xAF 0xEA ...`) over
serial, forever.

Real bisection, not guessing:

1. **Ruled out disk/boot-path issues** by trying three different
   backends for the exact same binary: QEMU's `fat:rw:<dir>`
   on-the-fly driver, an unpartitioned raw FAT image attached as a
   SATA drive, and a properly `sgdisk`-partitioned GPT image with a
   real ESP. All three reproduced the identical garbage byte pattern.
2. **Ruled out a genuine CPU fault** by re-running under
   `-d int,guest_errors -D qemu_debug.log`: the only vector logged was
   `0x20` (a periodic timer IRQ, expected from the probe's own idle
   `Stall()` loop), 688 times over the run — no `#GP`/`#PF`/`#UD`, no
   exception at all.
3. **Ruled out anything probe-specific** by writing a minimal
   hello-world (`InitializeLib` + one `Print()` + an idle loop, no
   register/MSR reads at all) through the exact same gnu-efi
   build pipeline — it reproduced the identical garbage.
4. Rebuilding that same hello-world with clang's native
   `x86_64-unknown-windows` target and `lld-link` (direct COFF/PE
   output, no ELF-to-PE conversion step at all) **worked on the first
   try** — real, visible `HELLO FROM SELF-CONTAINED PROBE` text on
   serial.

The most likely explanation (not itself independently verified further
— the working alternative was adopted instead of chasing gnu-efi's own
failure to its root cause, a reasonable stopping point once a clean
working path existed): this environment's GCC (16.1.0, a very recent
version) produces code or relocations that gnu-efi 4.0.4's
long-unmaintained ELF→PE conversion assumptions don't handle correctly
— a real toolchain-version-skew bug, not a logic error in the probe.

**Consequence for the probe's own code:** `handoff-probe.c` does not
link against gnu-efi's runtime library (`libefi.a`/`libgnuefi.a`) at
all — no `InitializeLib`, no `Print()`. It only uses gnu-efi's headers
(portable C struct definitions) and talks to `SystemTable`'s protocols
directly, with small hand-written hex/decimal print helpers. This is
what actually made the clang/lld-link path self-contained and
buildable without needing gnu-efi's own object files to link cleanly
into a COFF output at all.
