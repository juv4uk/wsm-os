#!/usr/bin/env python3
"""Parser/builder for EFI_HII_STRING_PACKAGE_HDR + string block stream,
per MdePkg/Include/Uefi/UefiInternalFormRepresentation.h (EDK2 reference).

EFI_HII_PACKAGE_HEADER: Length:24 | Type:8   (4 bytes total)
EFI_HII_STRING_PACKAGE_HDR:
    Header            EFI_HII_PACKAGE_HEADER  (4 bytes)
    HdrSize           UINT32
    StringInfoOffset  UINT32
    LanguageWindow    CHAR16[16]              (32 bytes)
    LanguageName      EFI_STRING_ID (UINT16)
    Language          CHAR8[]  (null-terminated ASCII RFC4646 tag)
then, starting at offset StringInfoOffset from the start of the package
(i.e. from Header, not from end of Language[]): a stream of
EFI_HII_STRING_BLOCK entries, first byte = BlockType:
    0x00 END
    0x14 STRING_UCS2       CHAR16 text[]   (1 implicit id)
    0x15 STRING_UCS2_FONT  UINT8 font; CHAR16 text[]  (1 implicit id)
    0x16 STRINGS_UCS2      UINT16 count; CHAR16 text[][]  (count implicit ids)
    0x17 STRINGS_UCS2_FONT UINT8 font; UINT16 count; CHAR16 text[][]
    0x20 DUPLICATE         UINT16 StringId  (1 implicit id, text = referenced id's text)
    0x21 SKIP2             UINT16 SkipCount (SkipCount implicit ids, no text)
    0x22 SKIP1             UINT8  SkipCount
EXT1/EXT2/EXT4 (0x30/0x31/0x32) are not expected in a plain string package
and are treated as unsupported (parser raises rather than silently mis-handling).

CHAR16 string termination is found by scanning in 2-byte-aligned strides for
a 0x0000 code unit -- NOT a raw byte-level search for b'\\x00\\x00', which can
false-hit on a misaligned pair straddling two adjacent non-terminator bytes.
"""
import struct

def find_char16_end(data, start):
    """Return the offset of the terminating 0x0000 CHAR16 code unit,
    searching only at 2-byte-aligned positions from `start`."""
    pos = start
    n = len(data)
    while pos + 1 < n:
        if data[pos] == 0 and data[pos+1] == 0:
            return pos
        pos += 2
    raise ValueError(f"no CHAR16 terminator found starting at {hex(start)}")


def parse_package(data, base=0):
    length_type = struct.unpack_from('<I', data, base)[0]
    length = length_type & 0xFFFFFF
    ptype = (length_type >> 24) & 0xFF
    hdr_size, string_info_offset = struct.unpack_from('<II', data, base + 4)
    lang_window = data[base+12:base+12+32]
    lang_name_id = struct.unpack_from('<H', data, base+44)[0]
    lang_start = base + 46
    lang_end = data.index(b'\x00', lang_start)
    language = data[lang_start:lang_end].decode('ascii')

    entries = []  # (string_id, value, block_type, offset, block_len)
    pos = base + string_info_offset
    sid = 1
    while True:
        bt = data[pos]
        if bt == 0x00:  # END
            entries.append((None, None, 'END', pos, 1))
            pos += 1
            break
        elif bt == 0x14:  # STRING_UCS2
            start = pos + 1
            end = find_char16_end(data, start)
            text = data[start:end].decode('utf-16-le')
            blk_len = (end + 2) - pos
            entries.append((sid, text, 'STRING_UCS2', pos, blk_len))
            sid += 1
            pos += blk_len
        elif bt == 0x15:  # STRING_UCS2_FONT
            font = data[pos+1]
            start = pos + 2
            end = find_char16_end(data, start)
            text = data[start:end].decode('utf-16-le')
            blk_len = (end + 2) - pos
            entries.append((sid, text, ('STRING_UCS2_FONT', font), pos, blk_len))
            sid += 1
            pos += blk_len
        elif bt == 0x16:  # STRINGS_UCS2
            count = struct.unpack_from('<H', data, pos+1)[0]
            cur = pos + 3
            texts = []
            for _ in range(count):
                start = cur
                end = find_char16_end(data, start)
                texts.append(data[start:end].decode('utf-16-le'))
                cur = end + 2
            blk_len = cur - pos
            entries.append((sid, texts, 'STRINGS_UCS2', pos, blk_len))
            sid += count
            pos += blk_len
        elif bt == 0x17:  # STRINGS_UCS2_FONT
            font = data[pos+1]
            count = struct.unpack_from('<H', data, pos+2)[0]
            cur = pos + 4
            texts = []
            for _ in range(count):
                start = cur
                end = find_char16_end(data, start)
                texts.append(data[start:end].decode('utf-16-le'))
                cur = end + 2
            blk_len = cur - pos
            entries.append((sid, texts, ('STRINGS_UCS2_FONT', font), pos, blk_len))
            sid += count
            pos += blk_len
        elif bt == 0x20:  # DUPLICATE
            ref = struct.unpack_from('<H', data, pos+1)[0]
            entries.append((sid, ref, 'DUPLICATE', pos, 3))
            sid += 1
            pos += 3
        elif bt == 0x21:  # SKIP2
            skip = struct.unpack_from('<H', data, pos+1)[0]
            entries.append((sid, skip, 'SKIP2', pos, 3))
            sid += skip
            pos += 3
        elif bt == 0x22:  # SKIP1
            skip = data[pos+1]
            entries.append((sid, skip, 'SKIP1', pos, 2))
            sid += skip
            pos += 2
        else:
            raise ValueError(f"unsupported block type {hex(bt)} at offset {hex(pos)} (package base {hex(base)}, sid so far {sid})")

    return {
        'length': length, 'type': ptype, 'hdr_size': hdr_size,
        'string_info_offset': string_info_offset,
        'lang_window': lang_window, 'lang_name_id': lang_name_id,
        'language': language, 'entries': entries,
        'total_bytes': (pos - base) + 0,
        'base': base,
    }


def rebuild_package(pkg, id_to_text=None):
    """Rebuild the exact byte sequence for a package, using pkg's header
    fields and block-type pattern. If id_to_text is given, substitute new
    text for each string id (str for single-string blocks, list[str] for
    STRINGS_UCS2/_FONT blocks) instead of the parsed value."""
    if id_to_text is None:
        id_to_text = {}
    body = bytearray()
    body += struct.pack('<II', pkg['hdr_size'], pkg['string_info_offset'])
    body += pkg['lang_window']
    body += struct.pack('<H', pkg['lang_name_id'])
    body += pkg['language'].encode('ascii') + b'\x00'
    while len(body) + 4 < pkg['string_info_offset']:
        body += b'\x00'
    assert len(body) + 4 == pkg['string_info_offset'], (len(body)+4, pkg['string_info_offset'])

    for sid, val, btype, off, blen in pkg['entries']:
        if btype == 'END':
            body += b'\x00'
        elif btype == 'STRING_UCS2':
            text = id_to_text.get(sid, val)
            body += bytes([0x14]) + text.encode('utf-16-le') + b'\x00\x00'
        elif isinstance(btype, tuple) and btype[0] == 'STRING_UCS2_FONT':
            text = id_to_text.get(sid, val)
            body += bytes([0x15, btype[1]]) + text.encode('utf-16-le') + b'\x00\x00'
        elif btype == 'STRINGS_UCS2':
            texts = id_to_text.get(sid, val)
            body += bytes([0x16]) + struct.pack('<H', len(texts))
            for t in texts:
                body += t.encode('utf-16-le') + b'\x00\x00'
        elif isinstance(btype, tuple) and btype[0] == 'STRINGS_UCS2_FONT':
            texts = id_to_text.get(sid, val)
            body += bytes([0x17, btype[1]]) + struct.pack('<H', len(texts))
            for t in texts:
                body += t.encode('utf-16-le') + b'\x00\x00'
        elif btype == 'DUPLICATE':
            body += bytes([0x20]) + struct.pack('<H', val)
        elif btype == 'SKIP2':
            body += bytes([0x21]) + struct.pack('<H', val)
        elif btype == 'SKIP1':
            body += bytes([0x22, val])
        else:
            raise ValueError(f"unhandled block type {btype}")

    total_len = 4 + len(body)
    header = struct.pack('<I', (total_len & 0xFFFFFF) | (pkg['type'] << 24))
    return header + bytes(body)
