# WSM-OS

The hardware-execution counterpart to [wsm](https://github.com/juv4uk/wsm).

wsm carries no inherited Lisp/Rust history. wsm-os carries none either:
it is not a continuation of the old wsm-os-lisp lab, and inherits
nothing from it beyond the name.

Its scope beyond that is not yet decided. What wsm establishes,
wsm-os is where it gets proven on real hardware — nothing is promised
here ahead of that.

## The rule this repo works under

**wsm-os does not invent capabilities. It only tests capabilities
already formulated in `wsm`.** Not scheduler, allocator, SMP/interrupt
framework, driver model, filesystem, heap, runtime, or ABI — none of
that until `wsm` has created an actual semantic need for it. `x86`
having an instruction, a register, or a platform mechanism is not by
itself a reason for this repo to build toward it. The symmetric half of
this rule lives in `wsm` itself: it does not import a hardware concept
as semantics just because the machine happens to have one.

```text
wsm:      "нам потрібна операція X"
              ↓
wsm-os:   "ось чи можна її реалізувати і якою ціною"

НЕ:
x86 has ADD
    ↓
    => WSM has +
```

Everything currently in this repo (BIOS/firmware structural analysis,
QEMU/OVMF tooling, the handoff-state probes) exists to answer "can a
given capability be reached and observed on real hardware" — not to
grow into an operating system ahead of `wsm` asking for one.
