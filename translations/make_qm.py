#!/usr/bin/env python3
"""
Minimal .ts -> .qm converter.
Generates valid Qt binary message files from Qt Linguist .ts XML files.
The .qm format:
  - Magic: 3CB86418CA EF9C B701 2A00 D4D8A0C2 (16 bytes)
  - Followed by sections, each:
      tag (1 byte) + length (4 bytes, big-endian) + data
  - Tag 0x2A = messages array
  - Tag 0x2E = dependencies array (empty)
  Each message:
      0x04 (source context hash, 4 bytes)
      0x08 (source text CRC16, 2 bytes)
      0x05 (translation, len+1 = null-terminated utf-16be)
"""

import struct
import sys
import xml.etree.ElementTree as ET
import os

QM_MAGIC = bytes([
    0x3C, 0xB8, 0x64, 0x18, 0xCA, 0xEF, 0x9C, 0xB7,
    0x01, 0x2A, 0x00, 0xD4, 0xD8, 0xA0, 0xC2, 0x00
])

def qm_hash(s):
    """Qt's qHash for strings (used for context & source hashing)."""
    h = 5381
    for c in s.encode('utf-8'):
        h = ((h << 5) + h) ^ c
        h &= 0xFFFFFFFF
    return h

def crc16(data):
    """CRC-16/CCITT as used by Qt."""
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc

def encode_message(context, source, translation):
    """Encode a single message entry."""
    msg = bytearray()

    # 0x04: context hash (4 bytes, big-endian)
    ctx_hash = qm_hash(context)
    msg += bytes([0x04]) + struct.pack('>I', ctx_hash)

    # 0x08: source CRC-16 (2 bytes, big-endian)
    src_crc = crc16(source.encode('utf-8') + b'\x00')
    msg += bytes([0x08]) + struct.pack('>H', src_crc)

    # 0x09: source string length check (4 bytes, used internally)
    msg += bytes([0x09]) + struct.pack('>I', len(source))

    # 0x05: translation (utf-16-be null-terminated)
    if translation:
        trans_enc = translation.encode('utf-16-be') + b'\x00\x00'
    else:
        trans_enc = source.encode('utf-16-be') + b'\x00\x00'
    msg += bytes([0x05]) + struct.pack('>I', len(trans_enc)) + trans_enc

    # 0x06: source text (utf-8 null-terminated)
    src_enc = source.encode('utf-8') + b'\x00'
    msg += bytes([0x06]) + struct.pack('>I', len(src_enc)) + src_enc

    # 0x07: context name (utf-8 null-terminated)
    ctx_enc = context.encode('utf-8') + b'\x00'
    msg += bytes([0x07]) + struct.pack('>I', len(ctx_enc)) + ctx_enc

    return bytes(msg)

def ts_to_qm(ts_path, qm_path):
    tree = ET.parse(ts_path)
    root = tree.getroot()

    messages_data = bytearray()

    for ctx in root.findall('context'):
        ctx_name_el = ctx.find('name')
        ctx_name = ctx_name_el.text if ctx_name_el is not None else ''

        for msg in ctx.findall('message'):
            src_el = msg.find('source')
            trans_el = msg.find('translation')
            if src_el is None:
                continue
            source = src_el.text or ''
            translation = ''
            if trans_el is not None and trans_el.get('type') != 'unfinished':
                translation = trans_el.text or ''
            if not translation:
                translation = source  # fallback to source

            messages_data += encode_message(ctx_name, source, translation)

    # Build the .qm file
    out = bytearray(QM_MAGIC)

    # Section 0x2A: Messages
    out += bytes([0x2A])
    out += struct.pack('>I', len(messages_data))
    out += messages_data

    # Section 0x2E: Empty (dependencies)
    out += bytes([0x2E])
    out += struct.pack('>I', 0)

    with open(qm_path, 'wb') as f:
        f.write(out)
    print(f"Generated: {qm_path} ({len(out)} bytes, {len(list(tree.getroot().iter('message')))} messages)")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: make_qm.py input.ts output.qm")
        sys.exit(1)
    ts_to_qm(sys.argv[1], sys.argv[2])
