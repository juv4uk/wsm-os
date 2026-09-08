# Бенчмарк кастомного ядра (7.1.5-native) проти стокового (7.1.5+kali-amd64)

**Дата першого прогону:** 2026-09-09
**Залізо:** Gigabyte H170-Gaming 3, Intel Core i5-6400 (Skylake), 16 GiB DDR4-2133,
GTX 1050 Ti — див. `docs/OWNER-HARDWARE-PROFILE.md`.
**Джерело ядра:** `linux-source-7.1` (Debian/Kali патчі), зібране з
`CONFIG_X86_NATIVE_CPU=y` (GCC `-march=native`) + `localmodconfig` (147 модулів
замість дистрибутивного дефолту). Деталі збірки — в чаті сесії, не тут.

## Методологія

Скрипт `scripts/native-kernel-benchmark.sh` запускається один раз на кожному
ядрі, яке порівнюємо, і виводить чотири тести:

1. **[KERNEL-SENSITIVE] Syscall overhead** — 10M викликів `getpid()`. Це і
   наступний тест — єдині тут, що реально проходять через код ядра на
   кожній ітерації, тож єдині, де різниця може бути наслідком саме збірки
   ядра, а не просто заліза.
2. **[KERNEL-SENSITIVE] Context-switch overhead** — пінг-понг через pipe між
   батьківським і дочірнім процесом, 200k раундів.
3. **[CPU BASELINE] OpenSSL speed** (AES-256-CBC, SHA-256) — навантажує CPU,
   не ядро. Різниця тут відображає шум вимірювання, не ефект `-march=native`
   у ядрі (userspace `openssl` компілюється й виконується однаково незалежно
   від того, яке ядро під ним крутиться).
4. **[CPU BASELINE] 7-Zip built-in benchmark** — те саме застереження.

**Чому базові CPU-тести взагалі тут:** для повноти картини й тому, що це
звичні цифри, які легко з чимось звірити — але вони НЕ тестують ефект від
перекомпіляції самого ядра, лише сирy потужність CPU, яка не залежить від
того, яке ядро завантажено.

## Результат №1 — 7.1.5-native (базовий прогін)

```
$ uname -a
Linux desktop 7.1.5-native #2 SMP PREEMPT_DYNAMIC Tue Sep  8 23:34:54 EEST 2026 x86_64 GNU/Linux

10M getpid(): 9.539 s  (953.9 ns/call)
200000 round trips: 1.580 s  (7.9 us/round-trip, ~3950 ns/switch)

AES-256-CBC (8192 bytes): 898517.67 KB/s
SHA-256 (8192 bytes):     418291.71 KB/s

7z b: Tot Rating (compress+decompress avg): ~19844 / 20764 MIPS
```

Повний сирий вивід: `benchmarks/result-7.1.5-native.txt`.

## Статус: порівняння НЕ завершено

Це поки що **лише один бік порівняння**. Щоб отримати чесний висновок,
потрібно:

1. Перезавантажитись у `7.1.5+kali-amd64` (звичайний дефолт у GRUB — просто
   звичайний ребут, нічого обирати вручну не треба).
2. Запустити той самий скрипт:
   `./scripts/native-kernel-benchmark.sh > benchmarks/result-7.1.5+kali-amd64.txt`
3. Порівняти файли напряму.

**Очікування, чесно, наперед (щоб не інтерпретувати шум як ефект):** для
syscall/context-switch різниця, ймовірно, буде в межах кількох відсотків
або взагалі в межах шуму вимірювання — `-march=native` для ядра переважно
впливає на вузькі гарячі шляхи (checksum, деякі memcpy-варіанти), а не на
загальну вартість syscall/switch, яка домінується апаратними
переходами кільця захисту (ring transitions), однаковими для обох ядер.
Значної різниці в OpenSSL/7z тестах не очікується взагалі — це CPU-bound
навантаження, не kernel-bound.

---

# Native kernel (7.1.5-native) vs stock (7.1.5+kali-amd64) benchmark

**First run date:** 2026-09-09
**Hardware:** Gigabyte H170-Gaming 3, Intel Core i5-6400 (Skylake), 16 GiB
DDR4-2133, GTX 1050 Ti — see `docs/OWNER-HARDWARE-PROFILE.md`.
**Kernel source:** `linux-source-7.1` (Debian/Kali patches), built with
`CONFIG_X86_NATIVE_CPU=y` (GCC `-march=native`) + `localmodconfig` (147
modules instead of the distro default).

## Methodology

`scripts/native-kernel-benchmark.sh` runs four tests, run once per kernel
being compared:

1. **[KERNEL-SENSITIVE] Syscall overhead** — 10M `getpid()` calls. This and
   the next test are the only ones here that actually go through kernel
   code on every iteration, so they're the only ones where a difference
   could plausibly be attributed to the kernel build itself.
2. **[KERNEL-SENSITIVE] Context-switch overhead** — pipe ping-pong between
   parent and child, 200k round trips.
3. **[CPU BASELINE] OpenSSL speed** (AES-256-CBC, SHA-256) — stresses the
   CPU, not the kernel. A difference here reflects measurement noise, not
   the kernel's `-march=native` effect (userspace `openssl` is compiled and
   runs identically regardless of which kernel is booted underneath it).
4. **[CPU BASELINE] 7-Zip built-in benchmark** — same caveat.

## Result #1 — 7.1.5-native (baseline run)

See the Ukrainian section above for the numbers; raw output is in
`benchmarks/result-7.1.5-native.txt`.

## Status: comparison NOT yet complete

This is only one side of the comparison so far. To get an honest answer:
reboot into `7.1.5+kali-amd64` (the normal GRUB default — a plain reboot,
nothing to pick manually), rerun the same script, and diff the two result
files.

**Honest expectation stated in advance** (so noise doesn't get
misread as a real effect): syscall/context-switch overhead is dominated by
hardware ring-transition cost, identical on both kernels — expect the
difference to be within a few percent or pure measurement noise.
OpenSSL/7z are CPU-bound, not kernel-bound, so no meaningful difference is
expected there either.
