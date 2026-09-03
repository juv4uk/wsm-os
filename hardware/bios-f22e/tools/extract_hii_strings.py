#!/usr/bin/env python3
"""Extracts en-US strings from the 10 real, confirmed HII String Package
files found in this board's F22e firmware (5-language driver components:
en-US/de-DE/ja-JP/ru-RU/ko-KR), for translation into Ukrainian.

File/offset pairs below were located by hand: search the whole extracted
BIOS region for the ASCII language-tag pattern [a-z]{2}-[A-Z]{2}\\x00,
subtract 46 (Header 4 + HdrSize 4 + StringInfoOffset 4 + LanguageWindow 32
+ LanguageName 2) from each hit to get the EFI_HII_PACKAGE_HEADER start,
then parse_package() from hii_string_pkg.py. Some files have a spurious
earlier "en-US" byte match (real text containing that literal substring,
not a real package header) that fails to parse -- the offsets recorded
here are the ones that DO parse correctly (verified via round-trip:
rebuild_package(parse_package(data, base)) == data[base:base+length]).

file-b13edd38-684c-41ed-a305-d7b7e32497df was excluded: its ~20 apparent
language-tag matches are false positives (do not parse as this format --
almost certainly a keyboard layout package, EFI_HII_PACKAGE_KEYBOARD_LAYOUT,
a different binary structure entirely).

Usage: extract from a fresh `uefi-firmware-parser -e -o extracted-orig
H170G3.22e` extraction tree, run from the directory containing it:
    python3 extract_hii_strings.py > en_strings_for_translation.json
"""
import json
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hii_string_pkg import parse_package

BASE_DIR = 'extracted-orig/regions/region-bios/volume-0/file-9e21fd93-9c72-4c15-8c4b-e77f1db2d792/section0/section1/volume-ee4e5898-3914-4259-9d6e-dc7bd79403cf'

FILES = {
    'file-5bedb5cc-d830-4eb2-8742-2d4cc9b54f2c': 0x4fc,     # IPv6 network configuration
    'file-0718ad81-f26a-4850-a6ec-f268e309d707': 0x7770,
    'file-a29a63e3-e4e7-495f-8a6a-07738300cbb3': 0x81d0,
    'file-d20d5c9f-0fa1-42f9-989e-c5c42f66e5b4': 0x11088,
    'file-668706b2-bcfc-4ad4-a185-75e79f3fe169': 0x3911,
    'file-d57c852e-809f-45cf-a377-d77bc0cb78ee': 0x5a9c,
    'file-70e1a818-0be1-4449-bfd4-9ef68c7f02a8': 0x5860,
    'file-cdc1c80d-e6d3-4a42-9229-75f3befcf109': 0x2861,
    'file-899407d7-99fe-43d8-9a21-79ec328cac21': 0x52ce9,   # main AMI Aptio Setup menu -- 8032 entries
    'file-8f4b8f82-9b91-4028-86e6-f4db7d4c1dff': 0x165e8,
}


def main():
    out = {}
    for fn, base in FILES.items():
        path = f'{BASE_DIR}/{fn}/file.obj'
        data = open(path, 'rb').read()
        pkg = parse_package(data, base)
        strings = {}
        for sid, val, btype, off, blen in pkg['entries']:
            if btype == 'STRING_UCS2' and val:
                strings[sid] = val
        out[fn] = strings
    json.dump(out, sys.stdout, ensure_ascii=False, indent=1)


if __name__ == '__main__':
    main()
