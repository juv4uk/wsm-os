# F22e — what the actual binary says / що насправді каже сам бінарник

Direct static examination of `H170G3.22e` (the 8 MiB ROM image inside
`mb_bios_ga-h170-gaming3_f22e.zip`), 2026-09-02. Everything below is
`source-confirmed`: read directly from the file's own bytes, not
inferred from a description of it. Method: `file`, raw hex inspection,
`binwalk` (via `guix shell binwalk`), Python for LZMA extraction and a
targeted Intel-microcode-header scan.
Пряме статичне дослідження самого файлу `H170G3.22e`, 2026-09-02. Усе
нижче — `source-confirmed`: прочитано напряму з байтів файлу, а не з
опису про нього.

**Role of this document, clarified 2026-09-02:** firmware research only
matters to WSM insofar as it helps determine WSM's own initial
conditions, not as archaeology for its own sake. Three levels of such
research exist, and only the third is what WSM actually needs:

```text
1. STATIC IMAGE      -- what physically sits in flash/ROM (this document)
2. BOOT BEHAVIOUR     -- what firmware actually does to the CPU
3. HANDOFF STATE      -- the exact machine state when control passes onward
```

This document is level 1 only — genuine hardware provenance, stays
here as-is, but does not by itself answer what WSM needs. `probe/` in
this same repo does level 3 directly (`handoff-probe.c`,
`exit-boundary-probe.c`). A concrete demonstration of why level 1 and
level 3 can genuinely disagree — this exact BIOS's own microcode
question — is in `FIT-AND-STRUCTURE-ANALYSIS.md`. The sibling `wsm`
repo's `research/handoff-state.md` keeps only the conclusions this
work implies for WSM's own design, not this framework or the evidence
behind it — this document is the source of record for both.

## File classification

`file H170G3.22e` → **"Intel serial flash for PCH ROM"**. Confirmed
structurally: bytes `5A A5 F0 0F` (the real Intel Flash Descriptor
signature) appear at offset `0x10`, exactly where the Flash Descriptor
Signature is required to live in a valid Intel Flash Descriptor region.
This is a genuine full-chip SPI flash image (Flash Descriptor + ME
region + BIOS region), not a bare "BIOS-only" capsule — consistent with
an 8 MiB SPI flash part actually present on this board.

## Internal structure

`binwalk` found five LZMA-compressed streams (classic `LZMA_ALONE`
container, properties byte `0x5D`, 16 MiB dictionary) inside the BIOS
region — typical of an AMI Aptio V UEFI firmware volume's compressed
PEI/DXE sections:

| Offset | Compressed | Uncompressed |
|---|---:|---:|
| `0x297FC8` | 3,261,452 B | **9,130,000 B** (main DXE/driver volume) |
| `0x6E587C` | 22,646 B | 64,618 B |
| `0x6EB188` | 11,988 B | 27,366 B |
| `0x6F9828` | 1,876 B | 2,762 B |
| `0x6F9FFC` | 8,464 B | 14,638 B |

All five decompressed cleanly with Python's `lzma` module
(`FORMAT_ALONE`) to exactly their binwalk-reported uncompressed sizes —
confirms these are genuine, well-formed LZMA streams, not false-positive
signature matches.

## Direct string evidence — triple-confirms the release date

The main decompressed volume contains this literal string:

```
BIOS Date: 03/09/2018 20:42:30 Ver: 1ASOH2225
```

This is now the **third independent confirmation** of the same release
date, each from a different source: the owner's live `Win32_BIOS`
query (`2018-03-09`), the zip archive's own internal file timestamp for
`H170G3.22e` (`2018-03-09 20:47`), and now this embedded build-time
string baked directly into the firmware image itself
(`03/09/2018 20:42:30`) — five minutes before the zip's own file
timestamp, consistent with "build, then package" ordering. `Ver:
1ASOH2225` is AMI's own internal Aptio core build-version string, not
previously seen in any external source checked in
`docs/BIOS-F22E-RESEARCH.md`.

Other direct string hits, also `source-confirmed`:

- `H170-Gaming 3`, `8A19AG04F22e` — the board name and Gigabyte's own
  internal project-code+version string, both embedded literally.
- `$VBT SKYLAKE` — an embedded Video BIOS Table specifically built for
  Skylake graphics, matching the Intel HD Graphics 530 already
  confirmed in `OWNER-HARDWARE-PROFILE.md`.
- `Skylake DT`, `Skylake Halo`, `Skylake ULT`, `Skylake ULX`, plus HSIO
  version strings for both `SKL PCH H/LP` **and** `KBL PCH H` — this
  build's reference code covers the full Skylake SKU family *and* Kaby
  Lake PCH tables. This directly confirms, from the binary itself
  rather than a vendor product page, the "supports 6th and 7th Gen
  Intel Core" claim from `docs/BIOS-F22E-RESEARCH.md`'s external
  research.
- Debug/PDB path strings reveal the network stack driver's real
  origin: `...\RivetLomPkg\...\AthrLomPkg-Bigfoot\LxUndiDxe\...` — an
  Atheros "Bigfoot Networks" UNDI (PXE) driver package. This matches
  the Killer E2200 Gigabit Ethernet Controller already confirmed in
  `OWNER-HARDWARE-PROFILE.md` (Killer/Qualcomm Atheros acquired Bigfoot
  Networks; "Killer" NICs are Bigfoot-lineage silicon).
- `Copyright (C) 2000-2015 Intel Corp.` and UDK2015-era build paths —
  this firmware is built on Intel's UDK2015 UEFI reference codebase.

## Microcode: searched directly, not located — reported as inconclusive, not absent

The owner's exact CPU signature (family 6, model 94 `0x5E`, stepping 3
— `lscpu`-confirmed) packs to the standard Intel CPUID value
`0x000506E3`. Two searches were run for a real embedded microcode
update matching it:

1. A full Intel microcode-header scan (the real 48-byte header format:
   `hdrver`, `rev`, `date`, `sig`, `cksum`, `ldrver`, `pf`, `datasize`,
   `totalsize`, reserved) across the raw 8 MiB file and across the
   fully decompressed main firmware volume (9,130,000 bytes) — **zero**
   plausible headers found anywhere, for any CPU signature, not just
   this one.
2. A bare byte-pattern search for `E3 06 50 00` (the signature alone,
   little-endian) — **zero** occurrences in either the raw file or the
   decompressed volume.

The setup-menu string `uCode Version` **is** present in the firmware
(confirming the BIOS does display/handle a loaded microcode revision at
runtime), but the actual update blob itself was not located by either
method above. The most likely explanation is that it lives inside a
firmware-volume section not reached by this pass — the four smaller
LZMA streams were decompressed and scanned too, all negative, but AMI
Aptio images commonly nest further GUID-addressed sections (a proper
UEFI Firmware-Volume/FFS walk would be needed to be exhaustive, and was
not attempted here — that is real additional work, not done in this
pass). **This is `not-yet-verified`, not `confirmed absent`.**

## What this does not change

None of this alters any existing architecture decision. It is
provenance/corroboration work: confirms the archived file is genuine,
matches the owner's live system on every checked point, and gives three
independent, mutually-consistent dates for the same release instead of
relying on one live query alone.
