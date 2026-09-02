# entropy-source-probe: real, empirical results

Per the owner's direct instruction to stop discussing and start
researching what an "empty system" on real hardware can learn about
reality. Run 2026-09-02, QEMU/OVMF/TCG (`-cpu Skylake-Client-v1,
+rdrand,+rdseed`).

## Real captured output

```text
CPUID reports RDRAND=1 RDSEED=1
Samples requested: 256
Successful reads:  256
Total bits set across successful 64-bit samples: 8177 of 16384 (expect ~50% if genuinely uniform)
Retries-before-success: sum=0 mean_x1000=0 max=0
```

## Honest interpretation (SINGULAR EXTERNAL confirmation of the probe's PLURAL INTERNAL data)

- **Bit balance: 49.9% ones.** Close to the 50% expected of uniform
  random data. This alone does not distinguish real hardware entropy
  from a well-seeded software PRNG — both would pass this test.
- **Zero retries across 256 calls — the real finding.** On genuine
  Intel silicon, RDSEED is documented to occasionally return failure
  (CF=0) because the on-die entropy conditioner has not accumulated
  enough fresh entropy since the last draw; this is expected, normal
  behavior, not an error. Getting **zero** failures across 256 calls is
  itself evidence: it strongly suggests QEMU/TCG's RDSEED emulation
  does not model the real hardware's entropy-exhaustion/retry behavior
  at all — most likely it draws from the host OS's own random source
  (ultimately seeded from real hardware entropy somewhere on this WSL2
  host, but via the OS, not a faithful emulation of the guest CPU's own
  DRNG under load) and always reports success.
- **Consequence for the original question**: the "retries-before-success
  behaves like a geometric/Poisson-shaped process" idea from
  `wsm/research/hardware-native-constants.md` is **not testable in this
  QEMU/TCG environment** — there is no retry behavior to measure here.
  This is a real, structural limit of the current lab setup, not a dead
  end for the idea itself: it would need either real physical hardware
  execution (separate, owner-authorized future work, same boundary
  already stated in `docs/QEMU-SETUP.md`) or a QEMU CPU/accelerator
  configuration that actually models entropy exhaustion, which was not
  attempted here.

## What this probe actually demonstrates, regardless of the entropy question

- `RDRAND`/`RDSEED` are real, present, callable instructions in this
  lab environment (`CPUID` confirms both bits), matching the owner's
  real CPU's own documented ISA features
  (`OWNER-HARDWARE-PROFILE.md`).
- The probe itself is a working, reusable instrument: it collects real
  samples, computes real statistics (bit balance, retry counts), and
  prints raw data for external inspection rather than asserting its own
  conclusions — exactly the `PLURAL INTERNAL` (candidate generation) /
  `SINGULAR EXTERNAL` (confirmation, done here, by a party other than
  the probe itself) split `wsm/research/the-observer-gap.md` requires.
- A `RDTSC` timing delta over a fixed spin loop was also captured
  (`0x7624D84` cycles) and deliberately left uninterpreted in the probe
  itself — a second real hardware-adjacent quantity available for
  future analysis, not yet analyzed here.

## Honest status

This is one run, on one virtual machine, under one software emulator.
Per this project's own evidence-strength ladder (`local run < clean CI
run < reproducible CI test < independent external reproduction`), this
sits at the lowest rung — a real, empirically confirmed local result,
not yet reproduced, not yet run on physical hardware. The specific,
useful finding (TCG's RDSEED doesn't model real entropy exhaustion) is
solid at this rung; anything about the *real* CPU's own entropy
statistics remains unmeasured.
