# F22e — flash region map, FIT, and the real microcode path

Direct structural analysis of `H170G3.22e`, 2026-09-02, following the
owner's own ten-step research program (steps 1, 2/3, 4/5, 6 partial, 7
partial below; step 8/9 — live handoff-state probe — is separate work,
see `wsm/research/handoff-state.md`). Every claim below is labeled per
his requested three-tier discipline:

- **STATIC-CONFIRMED** — read directly from the firmware bytes.
- **LIVE-CONFIRMED** — measured on the real, running machine.
- **INFERRED** — a reasoned conclusion, not a direct observation of either.

Tools: a hand-written Python parser for the Intel Flash Descriptor and
the FIT table (the format is public and well-documented; parsing it
directly avoids depending on a GUI-only tool); `uefi_firmware` (a
headless Python FV/FFS parser, `pip install uefi_firmware`) for the
region map cross-check and the full module inventory; `UEFITool`
(installed via Guix) was also tried but is GUI-only in this environment
(no headless CLI companion binary in this Guix package) — it opened a
real window on the owner's desktop via WSLg, but nothing in this
document was extracted from it; everything here comes from the two
scriptable tools instead, so the result is reproducible.

## 1. Flash region map — STATIC-CONFIRMED

```text
image size: 0x800000 (8 MiB)

Descriptor region:  0x000000 - 0x000FFF   (4 KiB, implicit — everything
                                            before the ME region starts)
ME region:           0x001000 - 0x1FFFFF  (0x1FF000 = 2,093,056 bytes)
BIOS region:         0x200000 - 0x7FFFFF  (0x600000 = 6,291,456 bytes)
GbE region:           not populated (size 0 — no GbE PHY firmware present)
PDR region:            not populated (size 0)
```

Cross-checked two ways: a hand-written parser of `FLMAP0` and the
Flash Region array following the `5A A5 F0 0F` descriptor signature at
offset `0x10`, and independently by `uefi_firmware`'s own
`FlashDescriptor.showinfo()` — both agree exactly. `2,093,056 +
6,291,456 = 8,384,512`; plus the 4 KiB descriptor region = `8,388,608`
= the file's exact size. The map is internally consistent, not just
plausible-looking.

The ME (Management Engine) region's own internal contents were not
parsed — Intel ME firmware uses a separate, largely proprietary format
that neither tool here attempts to decode, and that was out of scope
for this pass. Its region boundary is `STATIC-CONFIRMED`; its contents
are `not-yet-verified`.

## 2/3. Firmware Volume / FFS module inventory — STATIC-CONFIRMED

`uefi_firmware` parsed the BIOS region into **12 Firmware Volumes
containing 780 total files**. The library recognizes and names a set of
well-known component GUIDs (not every file has a name — most are
anonymous raw/compressed/GUID-defined data, correctly reported as such
rather than guessed). File-type distribution across all 780 files (FFS
file type byte, most common first): `0x15`(616), `0x14`(606),
`0x10`(500), `0x07`(372), `0x13`(310), `0x19`(182), `0x02`(164, PEIM),
`0x1c`(118), `0x0a`(118), `0x1b`(114), `0x06`(114), `0x12`(110),
`0x18`(32), `0xf0`(28, padding), `0x01`(22, raw), plus a handful of
single-digit-count rarer types. (Note: the parse was run in a way that
double-emitted its own summary once; the 12/780 counts above are the
correct, deduplicated totals from the underlying object graph, not the
doubled printed output.)

Confirmed NVRAM variable stores exist in the BIOS region (three named
stores: `NvramPei`, `NvramDxe`, `NvramSmm`) — this is the real
mechanism boot entries, Secure Boot state, and platform configuration
would live in on the running board; their actual variable *contents*
were not extracted this pass (the parser reported the store structures
and one variable count, not a full variable dump) — `not-yet-verified`
beyond store presence.

## 4/5. The real microcode path — STATIC-CONFIRMED, cross-confirmed two independent ways

### Path A: the Firmware Interface Table (FIT)

The FIT pointer lives at the architecturally fixed flash-mapped address
`0xFFFFFFC0` (file offset `0x7FFFC0` for this 8 MiB image, since the
image maps to the top of the 4 GiB address space). It resolves to a
real FIT table at file offset `0x5D0100`, header signature `_FIT_   `
confirmed byte-for-byte, `7` entries:

```text
[0] FIT Header
[1] Microcode Update  addr=0xFFDD0400 -> offset 0x5D0400
[2] Microcode Update  addr=0xFFDE7C00 -> offset 0x5E7C00
[3] Microcode Update  addr=0xFFE00000 -> offset 0x600000
[4] BIOS Startup Module (ACM)  addr=0xFFFF0000
[5] CSE Secure Boot   addr=0xFFFF9100  size=577
[6] type 0x0C (unrecognized in this parse)  addr=0xFFFF8080  size=735
```

Reading the real 48-byte Intel microcode header at each of the three
Microcode Update entries (all three decode as `hdrver=1, ldrver=1` —
structurally valid):

| Offset | CPU signature | Revision | Date | Platform flags | Total size |
|---|---|---:|---|---|---:|
| `0x5D0400` | `0x000506E8` | `0x34` | 2016-07-10 | `0x22` | 96,256 B |
| `0x5E7C00` | **`0x000506E3`** | **`0xC2`** | **2017-11-16** | `0x36` | 99,328 B |
| `0x600000` | `0x000906E9` | `0x84` | 2018-01-21 | `0x2A` | 98,304 B |

**The middle entry (`0x000506E3`) is the owner's exact CPU signature**
(family 6, model 94 `0x5E`, stepping 3 — `lscpu`-confirmed, see
`OWNER-HARDWARE-PROFILE.md`). This BIOS embeds microcode for three
different Skylake/Kaby Lake steppings (backward/forward SKU coverage in
one board image, consistent with the "Skylake DT/Halo/ULT/ULX" +
"KBL PCH H" strings already found in `BINARY-ANALYSIS.md`); only one of
the three matches this exact processor.

*(Note: the earlier raw byte-pattern scan in `BINARY-ANALYSIS.md`
searched for `0x000506E3` and found nothing — that scan was run against
the raw file and the one large decompressed DXE volume, but this
microcode container turns out to live in a *different*, smaller
Firmware Volume the earlier pass didn't decompress. The FIT pointer
resolves directly to the correct location regardless of which volume
holds it, which is exactly why FIT lookup is the stronger method the
owner recommended over raw scanning.)*

### Path B: the named FFS container — independent cross-check

`uefi_firmware`'s module inventory separately found a file with a
**recognized, named GUID**:

```text
17088572-377f-44ef-8f4e-b09fff46a070 (CPU_MICROCODE_FILE_GUID)
type 0x01 (raw), size 0x49C28 (302,120 bytes)
```

302,120 bytes is consistent with holding all three microcode blobs
(96,256 + 99,328 + 98,304 = 293,888 bytes) plus FFS/section header
overhead (~8 KB) — the FIT-located blobs and this named container agree
with each other. Two independently-implemented methods (a hand-rolled
FIT parser reading raw pointers, and a separate library's FFS/GUID
walk) converge on the same real microcode set. This is now
`STATIC-CONFIRMED` at a materially stronger level than the earlier
raw-scan attempt — located by structure, not by searching for a byte
pattern and hoping.

Two other CPU/boot-relevant named files were also found:
`PEI_BIOS_ACM_FILE_GUID` (`2d27c618-7dcd-41f5-bb10-21166be7e143`, the
Authenticated Code Module referenced by FIT entry `[4]`, 184,088 bytes)
and `PEI_AP_STARTUP_FILE_GUID` (`d1e59f50-e8c3-4545-bf61-11f002233c97`,
257 bytes) — the latter is the real Application-Processor bring-up code
path, directly relevant to a future "cores online" handoff-state field.

## This BIOS's own embedded microcode vs. the live machine — the divergence, now fully confirmed both sides

Combined with the prior session's live finding: the currently-loaded
microcode revision on the real machine is `0xD6`
(`LIVE-CONFIRMED`, via Windows registry
`HKLM\HARDWARE\DESCRIPTION\System\CentralProcessor\0\Update Revision`,
cross-referenced against Debian's `intel-microcode` changelog:
`sig 0x000506e3, pf_mask 0x36, 2019-10-03, rev 0x00d6`).

F22e's own embedded microcode for this exact CPU signature is now
`STATIC-CONFIRMED` as **revision `0xC2`, dated 2017-11-16**.
`0xC2 != 0xD6`, and `2017-11-16 < 2019-10-03`. **This is no longer an
inference from timing — both sides of the divergence are now directly
read, not guessed:** the BIOS embeds `0xC2`; the machine currently runs
`0xD6`; the live revision is newer than anything this BIOS image could
have loaded at power-on, so it is confirmed to be a later, OS-supplied
override layered on top of F22e's own firmware-loaded microcode, not a
reflection of it.

## 7. ACPI/SMBIOS table presence — STATIC-CONFIRMED (presence only, not parsed)

None of the standard ACPI/SMBIOS signatures (`RSD PTR `, `FACP`,
`APIC`, `MCFG`, `DMAR`, `HPET`, `DSDT`, `SSDT`, `_SM_`, `_SM3_`) appear
in the raw compressed image (expected — they live inside compressed FV
sections). All of them appear inside the main decompressed DXE volume:
`RSD PTR ` ×1, `FACP` ×23, `APIC` ×5, `MCFG` ×1, `DMAR` ×2, `HPET` ×2,
`DSDT` ×19, `SSDT` ×64, `_SM_`/`_SM3_` ×2 each. The high `SSDT` count
(64) is consistent with per-platform-configuration ACPI template
variants compiled into one shared image, not one single active table —
which SSDT(s) actually get built into the live ACPI namespace depends
on runtime platform detection this pass did not follow. Presence is
confirmed; content (AML disassembly, which SSDTs are actually live) is
`not-yet-verified` — real further work, not attempted here.

## What remains from the owner's ten-step program

Steps 1, 4, 5 are now solidly done; 2/3 done at the structural-inventory
level (not a full per-module hash/dependency-expression walk); 6 is
presence-only for NVRAM; 7 is presence-only for ACPI/SMBIOS. Steps 8-10
— live post-boot register/MSR/memory-map state, a minimal UEFI probe,
and the actual handoff-boundary experiment — are separate, larger work,
tracked in `wsm/research/handoff-state.md`.
