# Модифікований F22e — заміна логотипа завантаження

**Статус: заміна зроблена й структурно перевірена, ще НЕ прошита на
реальне залізо.** Прошивку на фізичну плату не виконано — це окрема,
явно авторизована власником дія (§8 кореневої політики), коли до неї
дійде.

## Що зроблено

Замінено тіло файлу логотипа BIOS (GUID `7bb28b99-61bb-11d5-9a5d-
0090273fc14d`, стандартний `EDKII_LOGO_FILE_GUID`, секція типу
`EFI_SECTION_RAW` 0x19) з оригінального логотипу Gigabyte
("GIGABYTE™ / Insist on Ultra Durable™", 809×116, JPEG) на власний,
тематичний для цього проєкту дизайн — `()` як центральний мотив,
"WSM" бірюзовим, підпис "given, not derived · wsm-os" — той самий
розмір 809×116, JPEG.

**Інструмент**: `UEFIReplace` з гілки `old_engine` LongSoft/UEFITool
(0.28.0) — зібрано з джерела проти справжнього Qt 5.15.17 (не Qt6:
поточний "new engine" явно не підтримує редагування образу, лише
читання — перевірено прямо в самому репозиторії проєкту, не
здогадка). Команда:

```bash
UEFIReplace H170G3.22e 7bb28b99-61bb-11d5-9a5d-0090273fc14d 0x19 \
  wsm-boot-logo.jpg -o H170G3-wsm-logo.22e
```

(без `-asis` — з `-asis` перші 4 байти JPEG обрізались, бо `-asis`
очікує вже готову секцію з власним заголовком, не сирий вміст файлу;
знайдено й виправлено реальним тестуванням, не здогадкою.)

## Структурна перевірка — усе пройдено

```text
Розмір файлу:        8 388 608 байтів (точно 8 МіБ), як в оригіналі
Сигнатура FD:         5A A5 F0 0F, незмінна
Таблиця FIT:           побайтово ідентична оригіналу (мікрокод,
                       Key Manifest, Boot Policy Manifest — усі записи
                       незмінні)
Регіон ME:              повністю побайтово ідентичний оригіналу
                        (0x1000-0x1FFFFF)
Кількість файлів/секцій: 1954 в обох образах, той самий перелік шляхів
                        GUID/секцій, 0 розбіжностей у переліку
Новий логотип:          коректний JPEG 809x116, точно той самий
                        розмір, що й вхідний файл
```

**Чесне застереження**: побайтова різниця всередині самого регіону
BIOS — близько 51% байтів. Це очікувано, не ознака пошкодження: новий
логотип менший за оригінал (~3.8 КБ різниці), тож `UEFIReplace`
перебудував увесь том, змістивши позиції всіх наступних файлів
усередині нього — так само, як зробив би легітимний редактор. Перевірка
вище (перелік файлів, FIT, ME-регіон) підтверджує, що сама структура
лишилась коректною, попри великий побайтовий зсув.

**Межа цієї перевірки, сказана прямо**: це підтверджує структурну
коректність (валідний том, коректні секції, нічого не загублено) — це
**не** те саме, що підтвердження реального завантаження на фізичному
залізі. Жодного способу "завантажити" саме цей образ у QEMU не існує
(QEMU тут завжди використовував OVMF, зовсім іншу прошивку, не
H170G3.22e) — реальна перевірка можлива лише на фізичній платі, з
DualBIOS як реальною сіткою безпеки, не віртуалізацією.

## Файли тут

- `H170G3-wsm-logo.22e` — повний, модифікований образ, 8 МіБ,
  SHA-256 `4e3f02ef...`
- `wsm-boot-logo.jpg` — новий логотип окремо, 809×116, JPEG

---

## Modified F22e — boot logo replacement (English, secondary)

Replaced the BIOS boot logo file body (standard `EDKII_LOGO_FILE_GUID`,
RAW section) with a project-themed design (`()` as the central motif,
"WSM" in teal, "given, not derived · wsm-os" subtitle), same 809x116
JPEG dimensions as the original Gigabyte logo. Built `UEFIReplace` from
LongSoft/UEFITool's `old_engine` branch against real Qt 5.15.17 (the
current "new engine" explicitly does not support image editing, only
reading -- verified directly in the project's own repo, not assumed).
Full structural verification passed: identical file size, identical
Flash Descriptor signature, byte-identical FIT table (microcode, Key
Manifest, Boot Policy Manifest untouched), byte-identical ME region,
identical file/section count and path listing (1954/1954, 0 diffs).
Not yet flashed to real hardware -- that remains a separate,
explicitly-authorized action, and cannot be tested in QEMU (which only
ever runs OVMF here, not this board's actual firmware). See the
Ukrainian version above for full detail.
