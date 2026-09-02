# F22e BIOS firmware — as downloaded / як завантажено

`mb_bios_ga-h170-gaming3_f22e.zip` — downloaded directly by the owner
from Gigabyte's own site on 2026-09-02 (Windows Downloads folder,
`user\Downloads\mb_bios_ga-h170-gaming3_f22e.zip`), copied here byte-
identical. Not re-derived, not re-hosted from a third-party mirror.
Завантажено власником напряму з сайту Gigabyte 2026-09-02, скопійовано
сюди побайтово ідентично. Не з дзеркала третьої сторони.

**SHA-256:** `565154ef1f625557f86a3b3f89a2d106b194b27b73d3441bd87fd508ae3c5b24`
(verified equal between the Windows-side download and this copy).

## Archive contents

```
autoexec.bat        19 bytes   2018-03-15 17:40
Efiflash.exe     81,976 bytes   2017-01-24 17:54
H170G3.22e     8,388,608 bytes  2018-03-09 20:47
```

`H170G3.22e` (exactly 8 MiB) is the actual BIOS ROM image; `Efiflash.exe`
is Gigabyte's own DOS/EFI flashing utility; `autoexec.bat` is a short
script that presumably invokes it. None of these have been run, flashed,
or otherwise executed — this is a research/provenance copy only. Nothing
here is written to any physical device by any script in this repo.

## Resolves the open date question from BIOS-F22E-RESEARCH.md

`docs/BIOS-F22E-RESEARCH.md` flagged a real, unresolved conflict:
the owner's live system reports `ReleaseDate: 2018-03-09` for this BIOS,
while `driverscollection.com`'s listing claimed `01 Apr 2021`, and that
document could not settle which was right from external sources alone.

**This archive settles it.** The ROM file's own internal timestamp
inside the zip — `H170G3.22e`, dated `2018-03-09 20:47` — matches the
owner's live-queried `ReleaseDate` exactly, independent of any
third-party mirror's claim. `driverscollection.com`'s "01 Apr 2021" is
now confirmed to be that site's own indexing/mirroring date, not the
firmware's actual release date. `source-confirmed` (the file itself,
not a description of it).
