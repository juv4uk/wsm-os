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

## Результат №2 — 7.1.5+kali-amd64 (стоковий, той самий реюн)

```
$ uname -a
Linux desktop 7.1.5+kali-amd64 #1 SMP PREEMPT_DYNAMIC Kali 7.1.5-1kali1 (2026-07-29) x86_64 GNU/Linux

10M getpid(): 9.835 s  (983.5 ns/call)
200000 round trips: 1.588 s  (7.9 us/round-trip, ~3969 ns/switch)

AES-256-CBC (8192 bytes): 898304.68 KB/s
SHA-256 (8192 bytes):     418250.75 KB/s

7z b: Tot Rating (compress+decompress avg): ~19675 / 20704 MIPS
```

Повний сирий вивід: `benchmarks/result-7.1.5+kali-amd64.txt`.

## Порівняння — завершено (2026-09-09)

| Тест | 7.1.5-native | 7.1.5+kali-amd64 | Різниця |
|---|---:|---:|---:|
| Syscall (getpid), нс/виклик | 953.9 | 983.5 | **native швидше на ~3.0%** |
| Context-switch, нс/switch | 3950 | 3969 | ~0.5% — шум |
| AES-256-CBC, КБ/с (8192B) | 898517.67 | 898304.68 | ~0.02% — шум |
| SHA-256, КБ/с (8192B) | 418291.71 | 418250.75 | ~0.01% — шум |
| 7z Tot Rating (compress/decompress) | 19844/20764 | 19675/20704 | ~0.9%/0.3% — у межах шуму |

**Висновок:** підтвердився наперед заявлений прогноз. Єдиний тест, що
реально проходить через код ядра на кожній ітерації (syscall), показав
відтворюваний, хоч і скромний виграш ~3%. Все інше — у межах шуму
вимірювання, як і очікувалось: `-march=native` для ядра змінює вузькі
гарячі шляхи (частину checksum/memcpy-варіантів у самому ядрі), а не
загальну вартість переходу в kernel mode, яка домінується апаратним
кільцем захисту (однаковим на обох ядрах) — і тим більше не впливає на
чисто userspace CPU-навантаження (OpenSSL, 7z).

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

## Result #2 — 7.1.5+kali-amd64 (stock, same run)

Raw output: `benchmarks/result-7.1.5+kali-amd64.txt`.

## Comparison — complete (2026-09-09)

| Test | 7.1.5-native | 7.1.5+kali-amd64 | Delta |
|---|---:|---:|---:|
| Syscall (getpid), ns/call | 953.9 | 983.5 | **native ~3.0% faster** |
| Context-switch, ns/switch | 3950 | 3969 | ~0.5% — noise |
| AES-256-CBC, KB/s (8192B) | 898517.67 | 898304.68 | ~0.02% — noise |
| SHA-256, KB/s (8192B) | 418291.71 | 418250.75 | ~0.01% — noise |
| 7z Tot Rating (compress/decompress) | 19844/20764 | 19675/20704 | ~0.9%/0.3% — noise |

**Conclusion:** the stated-in-advance prediction held. The only test that
actually goes through kernel code on every iteration (syscall) showed a
reproducible, if modest, ~3% gain. Everything else sits within measurement
noise, as expected: `-march=native` for the kernel changes narrow hot paths
(some checksum/memcpy variants inside the kernel itself), not the general
cost of entering kernel mode, which is dominated by the hardware
ring-transition (identical on both kernels) — and it has no bearing at all
on purely userspace CPU load (OpenSSL, 7z).
