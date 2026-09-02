# QEMU setup / Налаштування QEMU

Tuned to the owner's real machine from the start, per
`docs/OWNER-HARDWARE-PROFILE.md`, rather than QEMU's generic defaults.
Налаштовано під реальне залізо власника з самого початку, за
`docs/OWNER-HARDWARE-PROFILE.md`, а не типовими значеннями QEMU.

## What is actually matched / Що справді відповідає

| Real hardware (profile doc) | QEMU flag | Note |
|---|---|---|
| Intel Core i5-6400, Skylake, family 6 model 94 | `-cpu Skylake-Client-v1` | closest QEMU CPU model name to this exact microarchitecture; still an emulated approximation, not the real silicon |
| 1 socket, 4 cores, 4 logical processors, no SMT | `-smp 4,sockets=1,cores=4,threads=1` | topology matched exactly: 4 cores, 1 thread/core |
| x86_64 little-endian | (implicit: `qemu-system-x86_64`) | — |
| chipset target | `-machine q35` | modern, UEFI-friendly; matches the prior lab's own choice, not owner-specific hardware |
| M1/M2 "deliberately small fixed heap" guidance | `-m 128M` (default, overridable via `WSM_OS_QEMU_MEM`) | VM RAM, not guest heap size — kept small on purpose at this stage |

## What is honestly NOT matched / Що чесно НЕ відповідає

- **KVM hardware passthrough is not available to this agent process,
  and this is a real limit, not a gap that could be installed away.**
  `/dev/kvm` exists; the real error, reproduced directly (2026-09-02):
  `qemu-system-x86_64: -accel kvm: Could not access KVM kernel module:
  Permission denied`. The current agent user is not in the `kvm` group
  (`groups` → `agents users ollama`). This host does grant the agent
  user full `sudo (ALL : ALL) ALL` (confirmed via `sudo -l`) — but sudo
  here requires an interactive password this agent does not have and
  will not attempt to obtain or bypass; a permission granted in chat
  does not supply that password. Fixing this requires the owner (or
  whoever holds that password) to run
  `sudo usermod -aG kvm agents` on the host directly, followed by a new
  login session for the group change to take effect. Both scripts
  auto-detect KVM access (`[[ -r /dev/kvm && -w /dev/kvm ]]`) and fall
  back to `-accel tcg` rather than failing or silently claiming
  acceleration they don't have.
- **TCG cannot emulate every Skylake-Client-v1 CPUID feature.**
  Empirically confirmed running `scripts/qemu-selftest.sh` (local run,
  2026-09-02, QEMU 10.2.1): TCG prints warnings for `pcid`,
  `tsc-deadline`, `hle`, `invpcid`, `rtm`, `xsavec` — these bits are part
  of the requested Skylake-Client-v1 model but TCG doesn't implement
  them. QEMU still starts and runs; nothing crashes. Any wsm-os code that
  ends up depending on one of these specific features would misbehave
  under this configuration and must be caught by a real boot witness, not
  assumed away.
- **AVX2/BMI2** are real, available features on the owner's actual CPU
  (see the profile doc's ISA feature list) but the profile's own
  architecture decision #5 defers using them until measured — this QEMU
  setup does not change that decision either way.
- **This is still QEMU, not the physical machine.** Per the profile doc's
  own consequence section: "the first boot remains QEMU-only." Physical
  hardware execution is separate future work requiring its own
  authorization, same as it was for the prior lab.

## UEFI firmware / OVMF

The prior lab (`wsm-os-lisp`) never had OVMF firmware available in its
Guix profile (`find` over its store profile came up empty). This repo's
`manifest.scm` now declares `ovmf-x86-64` (a real Guix package,
confirmed by `guix build ovmf-x86-64` — outputs
`ovmf_code_x64.bin`/`ovmf_vars_x64.bin`/`Shell_x64.efi`), so UEFI boot
work does not start from that same gap.

`scripts/qemu-uefi-selftest.sh` runs the same hardware-tuned flags
against real OVMF with no boot image. Empirically confirmed (2026-09-02,
local run, corrected from an earlier false "no output" read that was an
artifact of output buffering in a manual test, not a real silence): OVMF
does render console output over the serial chardev — ANSI/VT100 escape
sequences followed by an automatic `>>Start PXE over IPv4.` network-boot
attempt, since no bootable device is attached. This confirms firmware
load and console reachability; it does not exercise boot-menu
interaction or an actual OS boot, since neither exists yet.

## Usage

No wsm-os boot image exists in this repo yet (see the repo README).
Three scripts exist so the QEMU configuration itself can be verified now,
independent of that:

```bash
# Validates the hardware-tuned flags without any boot image.
scripts/qemu-selftest.sh

# Validates the same flags against real OVMF UEFI firmware.
scripts/qemu-uefi-selftest.sh

# Once a real image exists:
scripts/run-qemu.sh path/to/image.raw
```

All three read `QEMU_SYSTEM_X86_64`, `WSM_OS_QEMU_CPU`,
`WSM_OS_QEMU_SMP`, `WSM_OS_QEMU_MEM` as overridable environment
variables (`qemu-uefi-selftest.sh` also reads `OVMF_CODE`/`OVMF_VARS`,
defaulting to the `ovmf-x86-64` Guix package's own output); defaults are
the hardware-matched values in the table above. `manifest.scm` declares
`qemu` and `ovmf-x86-64` as this repo's reproducibility-boundary
dependencies (Guix), per the ecosystem's own convention (root
`CLAUDE.md` §9a) — it does not yet declare a Rust/bare-metal toolchain,
since there is no kernel target in this repo yet.
