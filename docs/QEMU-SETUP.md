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

- **KVM hardware passthrough is not available in this environment.**
  `/dev/kvm` exists but the current agent user is not in the `kvm` group
  (confirmed live, 2026-09-02: `groups` → `agents users ollama`, no
  `kvm`). Both scripts auto-detect this (`[[ -r /dev/kvm && -w /dev/kvm ]]`)
  and fall back to `-accel tcg` (software emulation) rather than failing
  or silently claiming hardware acceleration it doesn't have.
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

## Usage

No wsm-os boot image exists in this repo yet (see the repo README). Two
scripts exist so the QEMU configuration itself can be verified now,
independent of that:

```bash
# Validates the hardware-tuned flags without any boot image.
scripts/qemu-selftest.sh

# Once a real image exists:
scripts/run-qemu.sh path/to/image.raw
```

Both read `QEMU_SYSTEM_X86_64`, `WSM_OS_QEMU_CPU`, `WSM_OS_QEMU_SMP`,
`WSM_OS_QEMU_MEM` as overridable environment variables; defaults are the
hardware-matched values in the table above. `manifest.scm` declares
`qemu` as this repo's reproducibility-boundary dependency (Guix), per the
ecosystem's own convention (root `CLAUDE.md` §9a) — it does not yet
declare a Rust/bare-metal toolchain, since there is no kernel target in
this repo yet.
