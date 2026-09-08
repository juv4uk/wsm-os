# WSM-OS

Апаратний майданчик виконання для [wsm](https://github.com/juv4uk/wsm).

`wsm` не успадковує історію Lisp/Rust. `wsm-os` теж не успадковує:
це не продовження старої лабораторії `wsm-os-lisp`, нічого звідти не
переноситься, окрім самої назви.

Обсяг поза цим ще не визначений. Те, що встановлює `wsm`, тут
доводиться на реальному залізі — нічого не обіцяється наперед.

## Правило, за яким працює цей репозиторій

**`wsm-os` не винаходить можливості. Він лише перевіряє можливості,
які вже сформульовані в `wsm`.** Ніякого scheduler, allocator,
SMP/interrupt framework, driver model, filesystem, heap, runtime чи
ABI — доки `wsm` не створить реальну семантичну потребу в цьому. Те,
що `x86` має якусь інструкцію, регістр чи механізм платформи, саме по
собі не є причиною будувати щось у цьому напрямку. Симетрична частина
цього правила живе в самому `wsm`: він не імпортує апаратне поняття
як семантику лише тому, що машина його має.

```text
wsm:      "нам потрібна операція X"
              ↓
wsm-os:   "ось чи можна її реалізувати і якою ціною"

НЕ:
x86 має ADD
    ↓
    => WSM має +
```

Усе, що зараз є в цьому репозиторії (структурний аналіз BIOS/firmware,
інструментарій QEMU/OVMF, проби handoff-state), існує, щоб відповісти
на питання "чи можна досягти й спостерегти цю можливість на реальному
залізі" — а не для того, щоб вирости в операційну систему раніше, ніж
`wsm` про це попросить.

## Current evidence (x86 arithmetic milestones)

| Milestone | Scope | CML commit | Evidence class |
|---|---|---|---|
| M4 | FIRST-QEMU-PARITY: UEFI boot, `(cons (quote A) (quote B))` -> `(A . B)` | -- | QEMU-BOOT-PARITY (GH run 33280152276) |
| M5A | Fixnum preflight, `cond` branching, truth/identity | a52b690 | oracle->CML->hosted/QEMU witness |
| M5B | Checked fixnum arithmetic (inline overflow) | b526cd6 | oracle->CML->hosted/QEMU witness |
| M5C | 100k-deep self-tail-call, bounded native stack | dd5382f | oracle->CML->hosted/QEMU witness |
| M5D | Definition capsule: stable definition ID, source digest, contract revisions, byte-identical regeneration | -- | QEMU (runs 33281082120, 33280964422) |

*Per Audit 2026-08-31: M5A/B/C have committed oracle->CML->hosted/QEMU witnesses. No additional unclosed non-closure obligation found; general closure gated by closure admission audit.*

---
## WSM-OS (English)

The hardware-execution counterpart to `wsm`. Carries no inherited
Lisp/Rust history — not a continuation of the old `wsm-os-lisp` lab,
nothing kept beyond the name.

**Standing rule**: `wsm-os` invents no capabilities; it only tests
capabilities `wsm` has already formulated (no scheduler, allocator,
SMP/interrupt framework, driver model, filesystem, heap, runtime, or
ABI ahead of an actual semantic need). Symmetrically, `wsm` never
imports a hardware concept as semantics just because `x86` happens to
have it. Everything in this repo exists to answer "can a given
capability be reached and observed on real hardware," not to grow into
an operating system ahead of `wsm` asking for one.

## Ліцензія

Цей твір поширюється під [ВОЛЬНІСТЮ](LICENSE) — простим словом про свободу творити, пам'ятаючи про волю іншого.
