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
