#!/usr/bin/env python3
"""Builds the uk-UA HII String Package for each of the eligible firmware
files and splices it into file.obj in place of the other 6 languages
(zh-cht/zh-chs/de-DE/ja-JP/ru-RU/ko-KR), keeping en-US and the TOTAL
byte span of the whole language-package run unchanged.

Why total size is kept exactly fixed, not shrunk: verified empirically
(pefile relocation-directory check + an exhaustive brute-force scan of
every possible RIP-relative displacement in the .text section, filtered
to real LEA/MOV/CALL opcode encodings) that nothing in the PE32's own
code holds a fixed pointer into the middle of this package run -- it is
walked sequentially at runtime via each package's own self-described
Length field. That means individual package sizes are free to change,
but nothing establishes whether content *after* the whole run is
referenced independent of the run's total size -- so the total span is
left untouched to eliminate that remaining unknown entirely, rather
than assume it away.

Language-name self-reference: each package's own string ID 1 is that
language's OWN name in its OWN language (confirmed: ru-RU's string 1 is
"Русский", not a translation of the English word "English") -- so
uk-UA's string 1 is hardcoded to "Українська" here, not taken from the
generic translation of "English" in uk_strings.json (which is
"Англійська", the wrong word for this specific slot).

Files with less combined non-English budget than en-US's own length
(0718ad81, a29a63e3, 70e1a818, cdc1c80d -- TPM confirmation dialogs,
NVRAM/recovery reset, and PCI resource error messages, all rare/edge
screens, not everyday Setup) are left completely untouched: forcing a
translation into too little space would mean truncating it, which is a
worse outcome than leaving those four screens in English for now.
"""
import json
import struct
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hii_string_pkg import parse_package, rebuild_package

BASE_DIR = 'extracted-orig/regions/region-bios/volume-0/file-9e21fd93-9c72-4c15-8c4b-e77f1db2d792/section0/section1/volume-ee4e5898-3914-4259-9d6e-dc7bd79403cf'
TRANSLATION_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'uk-translation')

ELIGIBLE_FILES = {
    'file-5bedb5cc-d830-4eb2-8742-2d4cc9b54f2c': 0x4fc,
    'file-d20d5c9f-0fa1-42f9-989e-c5c42f66e5b4': 0x11088,
    'file-668706b2-bcfc-4ad4-a185-75e79f3fe169': 0x3911,
    'file-d57c852e-809f-45cf-a377-d77bc0cb78ee': 0x5a9c,
    'file-899407d7-99fe-43d8-9a21-79ec328cac21': 0x52ce9,   # main AMI Aptio Setup menu
    'file-8f4b8f82-9b91-4028-86e6-f4db7d4c1dff': 0x165e8,
}
SKIPPED_TIGHT_BUDGET_FILES = [
    'file-0718ad81-f26a-4850-a6ec-f268e309d707',
    'file-a29a63e3-e4e7-495f-8a6a-07738300cbb3',
    'file-70e1a818-0be1-4449-bfd4-9ef68c7f02a8',
    'file-cdc1c80d-e6d3-4a42-9229-75f3befcf109',
]

UK_LANGUAGE_NAME = 'Українська'


def walk_language_block(data, first_base):
    """Returns list of (language, base_offset, length) for the contiguous
    run of type-0x04 (STRINGS) packages starting at first_base."""
    pos = first_base
    pkgs = []
    while pos < len(data) - 8:
        length_type = struct.unpack_from('<I', data, pos)[0]
        length = length_type & 0xFFFFFF
        ptype = (length_type >> 24) & 0xFF
        if length < 8 or ptype != 0x04:
            break
        pkgs.append((None, pos, length))
        pos += length
    return pkgs


def build_padded_uk_package(en_pkg, uk_text_for_id, total_span):
    """Rebuild en_pkg's structure with uk_text_for_id substituted, tagged
    as uk-UA, then zero-pad (and fix up the outer Length field) so the
    result is EXACTLY total_span bytes -- so the package's own declared
    span, not just its real content, accounts for every padding byte."""
    uk_pkg = dict(en_pkg)
    uk_pkg['language'] = 'uk-UA'
    built = rebuild_package(uk_pkg, id_to_text=uk_text_for_id)
    if len(built) > total_span:
        raise ValueError(f"uk-UA package ({len(built)} bytes) does not fit in budget ({total_span} bytes)")
    pad = total_span - len(built)
    padded = bytearray(built) + bytearray(pad)
    new_length_type = (total_span & 0xFFFFFF) | (uk_pkg['type'] << 24)
    struct.pack_into('<I', padded, 0, new_length_type)
    return bytes(padded)


def main():
    with open(os.path.join(TRANSLATION_DIR, 'uk_strings.json'), encoding='utf-8') as f:
        uk_strings = json.load(f)

    report = []
    for fn, first_base in ELIGIBLE_FILES.items():
        path = f'{BASE_DIR}/{fn}/file.obj'
        data = bytearray(open(path, 'rb').read())

        run = walk_language_block(data, first_base)
        run_start = run[0][1]
        run_end = run[-1][1] + run[-1][2]
        total_span = run_end - run_start

        en_pkg = None
        en_end = None
        for _, base, length in run:
            pkg = parse_package(data, base)
            if pkg['language'] == 'en-US':
                en_pkg = pkg
                en_end = base + length
                break
        assert en_pkg is not None, f"no en-US package found in {fn}"

        uk_text_for_id = dict(uk_strings[fn])
        # JSON object keys are strings; parse_package's sid is int.
        uk_text_for_id = {int(k): v for k, v in uk_text_for_id.items()}
        uk_text_for_id[1] = UK_LANGUAGE_NAME

        remaining_budget = run_end - en_end
        uk_pkg_bytes = build_padded_uk_package(en_pkg, uk_text_for_id, remaining_budget)

        new_file = bytes(data[:en_end]) + uk_pkg_bytes + bytes(data[run_end:])
        assert len(new_file) == len(data), (len(new_file), len(data))

        out_path = f'{BASE_DIR}/{fn}/file.obj.uk'
        with open(out_path, 'wb') as f:
            f.write(new_file)

        report.append((fn, total_span, en_pkg['length'], remaining_budget))
        print(f"{fn}: run [{hex(run_start)},{hex(run_end)}) span={total_span}, en={en_pkg['length']}, "
              f"uk built into {remaining_budget} bytes -> {out_path}")

    print()
    print(f"Built {len(ELIGIBLE_FILES)} files. Skipped (kept all 7 original languages, untouched): {SKIPPED_TIGHT_BUDGET_FILES}")


if __name__ == '__main__':
    main()
