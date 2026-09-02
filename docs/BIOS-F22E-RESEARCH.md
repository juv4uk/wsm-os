# BIOS F22e — external research / зовнішнє дослідження

External-web corroboration for the BIOS version already confirmed live
on the owner's board in `docs/OWNER-HARDWARE-PROFILE.md`
(`SMBIOSBIOSVersion F22e`, American Megatrends Inc., live-queried twice:
2026-08-29 and re-confirmed 2026-09-02). This document adds outside
sources; it does not supersede the live system query, which remains the
most authoritative single fact per the ecosystem's own evidence-priority
ordering (accepted evidence > repository/live state > external
documentation).
Зовнішнє web-підтвердження версії BIOS, вже підтвердженої наживо на
платі власника в `docs/OWNER-HARDWARE-PROFILE.md`. Цей документ додає
зовнішні джерела; він не замінює живий системний запит, який лишається
найавторитетнішим окремим фактом за власним порядком пріоритету доказів
екосистеми.

**Method:** `WebSearch` + `WebFetch`, 2026-09-02. Two direct fetches to
`gigabyte.com` and `drivers.softpedia.com` returned HTTP 403 (blocked,
not fetched — their content is not reproduced here, only what search
snippets independently surfaced). A Wayback Machine fetch was attempted
and is not supported by the available tool. No content from a blocked
page is asserted below.

## Board / BIOS identity

- Board: Gigabyte **GA-H170-Gaming 3**, chipset H170, sockets for 6th/7th
  Gen Intel Core, dual-channel DDR4 (4 DIMMs), dual PCIe Gen3 x4 M.2 —
  matches the owner's live-confirmed board (`docs/OWNER-HARDWARE-PROFILE.md`:
  Gigabyte H170-Gaming 3, i5-6400 = 6th Gen). `source-confirmed` (Softpedia
  search snippet).
- F22e exists as a release for **both board revisions**, rev. 1.0 and
  rev. 1.1. The owner's own live query returned `Version: x.x` for
  `Win32_BaseBoard`, which does not disambiguate the revision — **which
  revision the owner's physical board actually is remains
  `not-yet-verified`** from this research; it was not re-queried this
  pass. `source-confirmed` that both variants exist; `unknown` which one
  the owner has.

## Release date — a real discrepancy, not resolved

- The owner's own machine reports `ReleaseDate: 2018-03-09` for this
  exact BIOS (`Win32_BIOS`, queried live twice). `empirically confirmed`,
  local run, WINDOWS-OBSERVED — the strongest evidence available here.
- One WebSearch summary (aggregated, not a single primary source)
  independently stated F22e/F22b were "released on 2018-04-03" — within
  four weeks of the owner's own system's date, consistent enough to be
  the same release cycle. `predicted` (aggregator summary, not a primary
  page read directly).
- `driverscollection.com`'s own listing states **"01 Apr 2021"** for the
  same F22e file. This conflicts with both of the above by roughly three
  years. It is far more likely this is the date that mirror site
  indexed/re-hosted the file than the true firmware release date — a
  common pattern for third-party driver-mirror sites — but this is
  `predicted`, not confirmed; no primary Gigabyte page was successfully
  read to settle it. **Do not treat driverscollection.com's date as
  authoritative; the owner's own live system date is.**

## Changelog

- Two independent mirror-site listings (Softpedia search snippet,
  `driverscollection.com` direct fetch) give the same one-line
  changelog: **"Update CPU Microcode."** `source-confirmed` from two
  independent mirrors, but neither is Gigabyte's own official changelog
  page — both direct fetches to `gigabyte.com` and to Softpedia's page
  itself returned HTTP 403. No official first-party changelog text was
  read.

## Plausible but NOT confirmed: Spectre/Meltdown link

F22e's live-confirmed release timing (2018-03-09) falls inside the
industry-wide peak window (Jan–Apr 2018) for Intel microcode updates
addressing Spectre variant 2 (CVE-2017-5715). Combined with the
"Update CPU Microcode" changelog text, this is a **plausible** but
**not source-confirmed** explanation for what F22e actually changed.
Checked directly: the community-maintained
[`meltdown-spectre-bios-list`](https://github.com/mathse/meltdown-spectre-bios-list)
tracker has no Gigabyte section at all, so it neither confirms nor
denies this for this specific board. **`predicted`, not
`source-confirmed`** — stated here as a hypothesis worth flagging, not
a fact to build on.

## Real forum reports found

### Tom's Hardware — NOT an F22e defect (root cause was bad RAM)

A user reported upgrading this exact board from BIOS F5 to F22e at the
same time as installing a new NVMe SSD, then hitting a boot loop (red
Ambient LED, fans spin, repeated power-cycle, no boot screen). This
could easily be *misread* as "F22e breaks NVMe boot" — it is not that.
The thread's own resolution: after extensive troubleshooting (CMOS
reset, battery swap, PSU swap, Memtest86, component swapping between
machines), the user isolated the cause to **a specific pair of RAM
DIMMs** — removing them fixed the machine, with the other RAM pair
working fine. The BIOS update and the NVMe install were coincidental
timing, not the cause. `source-confirmed` (thread read directly) that
this was a RAM failure, not a firmware defect.
[Tom's Hardware thread](https://forums.tomshardware.com/threads/gigabyte-ga-h170-gaming-3-is-not-working.3850371/)

### AnandTech — different BIOS (F5-era), CSM/legacy quirk, not F22e

A separate user on this board family reported that after updating to an
early BIOS revision (their own numbering: "Revision 5", not F22e) and
installing a Samsung 950 Pro NVMe SSD, none of their drives (including
three legacy SATA drives) were detected. Resolution: switching CSM to
"Legacy only" restored detection of the older drives; the NVMe drive
worked despite not appearing as a listed PCIe device in BIOS. This
report predates F22e by many BIOS revisions and is not about F22e
itself — included only as background on this board family's known
early NVMe/CSM sensitivity, not as evidence about the owner's actual
installed version. `source-confirmed` (thread read directly), but
`not-applicable` to F22e specifically.
[AnandTech thread](https://forums.anandtech.com/threads/gigabyte-h170-gamer-3-and-nvme-issues.2481416/)

## Relevance to wsm-os

No forum evidence found links F22e itself to any defect. Neither report
above is actually about F22e causing a problem (one is a coincidental
RAM failure, the other is a different, much older BIOS). The owner's
live-confirmed NVMe drive (Kingston `SNV2S1000G`, `Status: OK`) has not
shown any symptom matching either report. This does not change any
existing architecture decision in `OWNER-HARDWARE-PROFILE.md` — physical
BIOS-level boot work remains deferred regardless (decision #2, QEMU
first).

## Sources

- [Softpedia: GA-H170-Gaming 3 (rev. 1.1) BIOS F22e](https://drivers.softpedia.com/get/BIOS/Gigabyte/Gigabyte-GA-H170-Gaming-3-rev-1-1-BIOS-F22e.shtml) — fetch blocked (403); title/metadata only, via search snippet
- [Softpedia: GA-H170-Gaming 3 (rev. 1.0) BIOS F22e](https://drivers.softpedia.com/get/BIOS/Gigabyte/Gigabyte-GA-H170-Gaming-3-rev-1-0-BIOS-F22e.shtml) — same
- [driverscollection.com: BIOS v.F22e](https://driverscollection.com/_53541113472cbeef773536c45d4/Download-Gigabyte-GA-H170-Gaming-3-(rev.-1.1)-BIOS-v.F22e-free) — fetched directly
- [GIGABYTE GA-H170-Gaming 3 (rev. 1.1) support page](https://www.gigabyte.com/Motherboard/GA-H170-Gaming-3-rev-11/support) — fetch blocked (403), not read
- [Tom's Hardware: "Gigabyte GA-H170-Gaming 3 is not working?"](https://forums.tomshardware.com/threads/gigabyte-ga-h170-gaming-3-is-not-working.3850371/) — fetched and read directly
- [AnandTech: "Gigabyte H170 Gamer 3 and NVMe issues"](https://forums.anandtech.com/threads/gigabyte-h170-gamer-3-and-nvme-issues.2481416/) — fetched and read directly
- [mathse/meltdown-spectre-bios-list (GitHub)](https://github.com/mathse/meltdown-spectre-bios-list) — fetched and read directly; no Gigabyte entry
