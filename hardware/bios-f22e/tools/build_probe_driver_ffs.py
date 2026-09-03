#!/usr/bin/env python3
"""Wraps wsm-probe-driver.efi (EFI_FV_FILETYPE_DRIVER, subsystem
efi_boot_service_driver -- built by probe/build.sh) as a complete FFS
file for insertion into the DXE volume, replacing the earlier
EFI_FV_FILETYPE_APPLICATION version (GUID d03270ea-...) which never ran
automatically -- DXE only auto-dispatches DRIVER-type files.

State=0xF8, FFS_FIXED_CHECKSUM2=0xAA and the header-checksum formula are
the same values verified against a real file in this exact DXE volume
during the earlier logo/probe-application work (see
hardware/bios-f22e/modified/README.md's $BDR and FfsEngine sections for
how these were derived and confirmed, not guessed).
"""
import struct
import uuid
import sys

def u24(n):
    return struct.pack("<I", n)[:3]

def section_header(size, sec_type):
    return u24(size) + bytes([sec_type])

EFI_SECTION_PE32 = 0x10
EFI_SECTION_USER_INTERFACE = 0x15
EFI_FV_FILETYPE_DRIVER = 0x07
FFS_FIXED_CHECKSUM2 = 0xAA
STATE_VALID_ERASE_POLARITY_1 = 0xF8

def build(efi_path, out_path, file_guid_str, ui_name):
    with open(efi_path, "rb") as f:
        efi_data = f.read()

    pe32_section = section_header(4 + len(efi_data), EFI_SECTION_PE32) + efi_data
    while len(pe32_section) % 4 != 0:
        pe32_section += b"\x00"

    ui_content = (ui_name + "\x00").encode("utf-16-le")
    ui_section = section_header(4 + len(ui_content), EFI_SECTION_USER_INTERFACE) + ui_content

    body = pe32_section + ui_section

    file_guid = uuid.UUID(file_guid_str)
    guid_bytes = file_guid.bytes_le

    header_size = 24
    total_size = header_size + len(body)

    header = bytearray(header_size)
    header[0:16] = guid_bytes
    header[16] = 0x00
    header[17] = 0x00
    header[18] = EFI_FV_FILETYPE_DRIVER
    header[19] = 0x00
    header[20:23] = u24(total_size)
    header[23] = STATE_VALID_ERASE_POLARITY_1

    sum8 = sum(header) & 0xFF
    temp = (sum8 - header[16] - header[17] - header[23]) & 0xFF
    header[16] = (0x100 - temp) & 0xFF
    header[17] = FFS_FIXED_CHECKSUM2

    ffs = bytes(header) + body
    with open(out_path, "wb") as f:
        f.write(ffs)

    print(f"wrote {out_path}: {len(ffs)} bytes (header={header_size}, pe32_section={len(pe32_section)}, ui_section={len(ui_section)})")
    print(f"file GUID: {file_guid}, type=DRIVER(0x07)")
    print(f"header checksum: {hex(header[16])}, data checksum: {hex(header[17])}, state: {hex(header[23])}")

if __name__ == "__main__":
    efi_path = sys.argv[1] if len(sys.argv) > 1 else "wsm-probe-driver.efi"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "wsm-probe-driver.ffs"
    guid = sys.argv[3] if len(sys.argv) > 3 else "d03270ea-2e65-4a37-9c91-e9abc36083e3"
    build(efi_path, out_path, guid, "WSM Probe Driver")
