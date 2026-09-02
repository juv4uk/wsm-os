# Прошивка BIOS F22e — як завантажено

`mb_bios_ga-h170-gaming3_f22e.zip` — завантажено власником напряму з
сайту Gigabyte 2026-09-02 (папка Downloads у Windows,
`user\Downloads\mb_bios_ga-h170-gaming3_f22e.zip`), скопійовано сюди
побайтово ідентично. Не перевиведено, не передзавантажено з дзеркала
третьої сторони.

**SHA-256:** `565154ef1f625557f86a3b3f89a2d106b194b27b73d3441bd87fd508ae3c5b24`
(перевірено — збігається між завантаженням на Windows-стороні і цією
копією).

## Вміст архіву

```
autoexec.bat        19 байтів   2018-03-15 17:40
Efiflash.exe     81 976 байтів  2017-01-24 17:54
H170G3.22e    8 388 608 байтів  2018-03-09 20:47
```

`H170G3.22e` (рівно 8 МіБ) — реальний ROM-образ BIOS; `Efiflash.exe` —
власна DOS/EFI-утиліта прошивки від Gigabyte; `autoexec.bat` — короткий
скрипт, що, ймовірно, її викликає. Жоден із цих файлів не запускався,
не прошивався і жодним іншим способом не виконувався — це лише
дослідницька/provenance-копія. Жоден скрипт цього репозиторію нічого
не записує на реальний фізичний пристрій.

## Закриває відкрите питання про дату з BIOS-F22E-RESEARCH.md

`docs/BIOS-F22E-RESEARCH.md` позначив реальний, тоді ще не вирішений
конфлікт: жива система власника показує `ReleaseDate: 2018-03-09` для
цього BIOS, тоді як лістинг `driverscollection.com` стверджував
`01 Apr 2021`, і той документ не міг вирішити, хто правий, спираючись
лише на зовнішні джерела.

**Цей архів вирішує питання.** Власна внутрішня позначка часу
ROM-файлу всередині zip — `H170G3.22e`, дата `2018-03-09 20:47` —
точно збігається з `ReleaseDate`, отриманим із живого запиту власника,
незалежно від будь-якого твердження стороннього дзеркала.
`driverscollection.com` з його "01 Apr 2021" тепер підтверджено як
дата власного індексування/дзеркалення того сайту, не реальна дата
релізу прошивки. `source-confirmed` (сам файл, а не опис про нього).

---

## F22e BIOS firmware — as downloaded (English)

`mb_bios_ga-h170-gaming3_f22e.zip`, downloaded directly by the owner
from Gigabyte on 2026-09-02, copied here byte-identical (SHA-256
verified). Contains `H170G3.22e` (8 MiB ROM image, internally
timestamped `2018-03-09 20:47`), `Efiflash.exe` (Gigabyte's flashing
utility), `autoexec.bat`. Nothing here has been executed or flashed —
research/provenance copy only.

This archive's internal timestamp resolves the release-date conflict
`docs/BIOS-F22E-RESEARCH.md` left open: it matches the owner's
live-queried `ReleaseDate` exactly, confirming `driverscollection.com`'s
"01 Apr 2021" was that site's own indexing date, not the firmware's
actual release date.
