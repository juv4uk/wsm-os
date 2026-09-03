# Модифікований F22e — логотип завантаження + вбудований DXE-модуль проби

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

- `H170G3-wsm-logo.22e` — образ лише із заміненим логотипом, 8 МіБ,
  SHA-256 `4e3f02ef...` (проміжний крок, лишений для історії)
- `H170G3-wsm-full.22e` — **фінальний, комбінований образ**: заміна
  логотипа + вбудований DXE-модуль проби (нижче), 8 МіБ, SHA-256
  `c9a608e7401839f35ab247d2ea5e9817134653f67c98fd7c98ff87e684181dcd`
- `wsm-boot-logo.jpg` — новий логотип окремо, 809×116, JPEG
- `wsm-probe.ffs` — готовий FFS-файл вбудованого DXE-модуля (заголовок
  + PE32-секція + UI-секція), 12852 байтів, SHA-256
  `92e69766cfe8c48477874dabce8f5d065a06e0441c5a7dc4895e9ae743f3c9b5`,
  той самий бінарник, що й `probe/physical-boot-probe.efi`

## Вбудований DXE-модуль проби

**Статус: вставка зроблена й структурно перевірена, ще НЕ прошита на
реальне залізо.** Та сама межа §8, що й для логотипа.

### Що зроблено

Той самий `probe/physical-boot-probe.efi` (12800 байтів — вже
перевірений на реальному залізі власника через Ventoy, див.
`probe/README.md`), загорнутий у власний FFS-файл (GUID
`d03270ea-2e65-4a37-9c91-e9abc36083e3`, тип `EFI_FV_FILETYPE_
APPLICATION`, PE32-секція + UI-секція з назвою "WSM Probe") і
вставлений безпосередньо у вкладений DXE-том прошивки (GUID
`ee4e5898-3914-4259-9d6e-dc7bd79403cf`, той самий том, де лежить
логотип), замінивши собою мережевий завантажувач `UefiPxeBcDxe`
(GUID `b95e9fda-26de-48d2-8807-1f9107ac5e3a`, ~62.7 КБ — прибраний,
щоб звільнити місце, за прямим дозволом власника: "я дозволяю").
Мета — щоб проба вмикалась автоматично при кожному завантаженні
прямо з самої прошивки, без потреби у флешці з Ventoy, і продовжувала
логувати на флешку так само, як `physical-boot-probe.c` вже вміє.

**Інструмент**: `UEFIInsert` — власний, написаний з нуля інструмент
командного рядка (немає в апстрімі UEFITool; GUI `old_engine` має дію
Insert, але без CLI-еквіваленту), який використовує той самий
`FfsEngine`, що й `UEFIReplace`. Зібраний з того самого дерева джерел,
проти того самого справжнього Qt 5.15.17.

### Дві реальні помилки в `old_engine`, знайдені й обійдені

Це не здогадки — обидві підтверджені прямим читанням джерела
`ffsengine.cpp` і прямою побайтовою перевіркою результату.

**1. `create()` губить перерахований checksum/State.** Функція
`FfsEngine::create()` для типу `File` нарізає `newHeader`/`newBody` з
`newObject` через `QByteArray::left()`/`mid()` — а ці методи не є
copy-on-write "видом" на той самий буфер, вони одразу глибоко
копіюють дані в нову, незалежну пам'ять. Увесь наступний код рахує
правильний checksum/State і пише його через сирий вказівник
`EFI_FFS_FILE_HEADER*` у буфер `newObject` — але `newHeader`/`newBody`
вже давно скопійовані з ним НЕ пов'язані, тож перерахований результат
губиться, і в реконструйований файл потрапляють мої ПОЧАТКОВІ,
непорахoвані байти. Підтверджено емпірично: `parseFile` під час
вставки друкував `invalid header checksum 00h, should be D7h` /
`invalid data checksum 00h, should be AAh`. **Обхід**: рахую
правильні `State` (`0xF8`, підтверджено читанням реального валідного
файлу в тому самому томі), `Header checksum` (за формулою з
`parseFile`, звірено на тому самому реальному файлі — `0xE3` зійшовся
точно) і `Data checksum` (`FFS_FIXED_CHECKSUM2` = `0xAA`) заздалегідь,
у самому Python-скрипті, що будує `wsm-probe.ffs` — тоді "перерахунок"
`create()`, хоч і зламаний, просто нічого не псує.

**2. `CREATE_MODE_APPEND` ставить новий файл ПІСЛЯ вузла FreeSpace —
і `reconstructVolume()` через це просто губить його.** `insert(...,
CREATE_MODE_APPEND)` додає новий файл як останню дитину тому — а в
дереві тому вже є службовий вузол `Types::FreeSpace` в самому кінці.
Цикл `reconstructVolume()`, який серіалізує файли тому, трактує БУДЬ-
ЩО після `FreeSpace` як застарілі "не-UEFI хвостові дані" (реальний,
але вузький випадок: у деяких старих BIOS справді буває сміття
виробника, приклеєне після останнього файлу, перед вільним місцем) —
і для цього випадку бере лише `body()` елемента (БЕЗ 24-байтного
`EFI_FFS_FILE_HEADER`!) і вставляє ці сирі байти як заповнювач.
Підтверджено прямою побайтовою перевіркою: PE32- і UI-секції нашого
файлу (і рядок "WSM Probe") справді опинялись у розпакованому томі,
але GUID файлу — ніде, бо заголовок ніколи не записувався; жоден
FFS-інструмент такий файл не бачив. **Виправлення**: замість `insert
(targetVolume, ffsData, CREATE_MODE_APPEND)` шукаю вузол `FreeSpace`
усередині тому й викликаю `insert(freeSpaceIndex, ffsData,
CREATE_MODE_BEFORE)` — це коректно ставить новий файл ПЕРЕД
`FreeSpace`, туди, де й належить бути звичайному FFS-файлу.

### Структурна перевірка — усе пройдено

```text
Розмір файлу:        8 388 608 байтів (точно 8 МіБ), як в оригіналі
Сигнатура FD:         5A A5 F0 0F, незмінна
Таблиця FIT:           побайтово ідентична оригіналу
Регіони ME/GbE/PDR:    повністю побайтово ідентичні оригіналу
Перелік файлів/секцій: рівно одна заміна в цілому образі —
                       file-b95e9fda... (UefiPxeBcDxe, прибраний)
                       ↔ file-d03270ea... (WSM Probe, доданий);
                       усе інше, включно з логотипом, побайтово
                       незмінне
Вміст нового файлу:    section0.pe == physical-boot-probe.efi точно
                       (12800 байтів), section1.ui == "WSM Probe"
                       (UTF-16LE, 20 байтів)
```

### Межа цієї перевірки

Так само, як і для логотипа: підтверджено структурну коректність
(валідний том, коректний FFS-файл на правильному місці, нічого інше
не зачеплено) — не підтверджено реальне виконання цього DXE-модуля на
живому UEFI. QEMU тут не допомагає (лише OVMF, не ця прошивка);
реальна перевірка — тільки фізичне залізо, з DualBIOS як сіткою
безпеки.

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

## Embedded DXE probe module (English, secondary)

Wrapped the already hardware-verified `physical-boot-probe.efi` in a
hand-built FFS file and inserted it directly into the firmware's
nested DXE volume, replacing `UefiPxeBcDxe` (removed with explicit
owner authorization to free space). Final combined image:
`H170G3-wsm-full.22e` (logo + probe module together).

Two real, source-confirmed and byte-confirmed bugs in UEFITool's
`old_engine` `FfsEngine` were found and worked around while building
this: (1) `create()`'s State/checksum recalculation writes through a
raw struct pointer into a buffer that `QByteArray::left()`/`mid()`
have already deep-copied away from by the time of the write, so the
recalculation never reaches the reconstructed bytes -- worked around
by precomputing correct State/checksums in the FFS blob itself; (2)
`insert(..., CREATE_MODE_APPEND)` places the new file after the
volume's trailing `FreeSpace` tree node, which `reconstructVolume()`
then treats as legacy non-UEFI trailing data and re-emits *without*
its FFS header -- worked around by inserting with `CREATE_MODE_BEFORE`
against the `FreeSpace` node itself. Both were confirmed empirically
(checksum-mismatch warnings; a byte-search of the decompressed volume
finding the probe's raw bytes and "WSM Probe" string but no file GUID)
before either fix was applied, not assumed from reading source alone.

Full structural verification passed on the combined image: identical
size, Flash Descriptor signature, byte-identical FIT table, byte-
identical ME/GbE/PDR regions, and an extraction-based diff across the
*entire* image showing exactly one substitution (`UefiPxeBcDxe` out,
the probe file in) with everything else -- including the logo --
byte-identical. Not yet flashed to real hardware. See the Ukrainian
section above for full detail, including the exact bug mechanics.
