#!/usr/bin/env python3
"""Translate an eight-slot schema-2 profile blob for the current firmware.

The documented import translation (docs/architecture/portable-profile-v1.md):
PD domain 0x50 version 1 becomes sparse version 2 by dropping each disabled
record without a name, and RGB domain 0x10 version 2 becomes version 3 by
adding an uncoloured PD row (h=0, s=0, v=0, right half) for slots 8..31.
Combo v1 becomes v2 using a 60 ms default window and its shared hold
threshold; row hold fields become reserved zero. The blob becomes schema 3
(D-F14): RGB v3 becomes v4 with a black, mapped-keys-only colour row for each
of layers 8..15, and settings v5 becomes v6 (the combo reference nibbles move
to the layer records, layer names become counted, the 64 macro names are
followed by 64 empty ones for macros 64..127, the 64 custom-key names by 64
empty ones for custom keys 64..127, participation allows
everything). Key behaviors v1 become v2: a 16-bit step count, and each row
enabled and allowed on all sixteen layers. Combos v2 become v3: each
28-byte row keeps its count, flags, window, output and its inputs only, with
every layer allowed. PD v2 becomes v3: each 96-byte record keeps its mode
bytes and moves its name to the counted rule (length in byte 8, UTF-8 at 96,
a 128-byte record). Test tooling: the firmware never translates.
"""
import struct
import sys
from pathlib import Path

PD_DOMAIN, RGB_DOMAIN = 0x50, 0x10
RGB_HEADER = 16
RGB_SIZES = {"group": 9, "layer_color": 5, "group_row": 5, "automouse": 4, "pd_color": 5}


def translate_rgb(payload: bytes) -> bytes:
    if payload[0] != 2 or payload[7] != 8:
        raise ValueError("expected an RGB v2 domain with eight PD rows")
    groups, layer_colors, layer_groups = payload[4], payload[5], payload[6]
    pd_end = RGB_HEADER + groups * 9 + layer_colors * 5 + layer_groups * 5 + 4 + 8 * 5
    ids = [payload[pd_end - 40 + 5 * row] for row in range(8)]
    if ids != list(range(8)):
        raise ValueError("expected PD rows 0..7")
    added = b"".join(bytes([slot, 0, 0, 0, 2]) for slot in range(8, 32))
    header = bytearray(payload[:RGB_HEADER])
    header[0], header[7] = 3, 32
    return bytes(header) + payload[RGB_HEADER:pd_end] + added + payload[pd_end:]


def translate_rgb_v4(payload: bytes) -> bytes:
    if payload[0] != 3:
        raise ValueError("expected an RGB v3 domain")
    groups, layer_colors = payload[4], payload[5]
    if layer_colors > 16:
        raise ValueError("more than sixteen layer colours")
    end = RGB_HEADER + groups * 9 + layer_colors * 5
    added = b"".join(bytes([layer, 0, 0, 0, 1]) for layer in range(layer_colors, 16)) if layer_colors else b""
    header = bytearray(payload[:RGB_HEADER])
    header[0] = 4
    if layer_colors:
        header[5] = 16
    return bytes(header) + payload[RGB_HEADER:end] + added + payload[end:]


def translate_settings(payload: bytes) -> bytes:
    if payload[:8] != bytes([5, 8, 28, 64, 64, 0, 0, 0]):
        raise ValueError("expected a settings v5 domain")
    scalars = list(struct.unpack_from("<28I", payload, 8))
    references = scalars[27]
    scalars[27:] = [0, 1, 0xFFFF, 0xFFFF]
    records = b""
    for layer in range(16):
        reference = (references >> (4 * layer)) & 15 if layer < 8 else layer
        records += bytes([reference]) + bytes(16)
    names = b""
    for layer in range(16):
        text = payload[120 + 24 * layer : 120 + 24 * (layer + 1)].split(b"\0")[0] if layer < 8 else b""
        names += bytes([len(text)]) + text
    # The 64 macro names, then 64 empty ones for slots 64..127, then the
    # custom keys' names.
    tail, offset = payload[312:], 0
    for _ in range(64):
        offset += 1 + tail[offset]
    macros, custom = tail[:offset], tail[offset:]
    return bytes([6, 16, 31, 128, 128, 0, 0, 0]) + struct.pack("<31I", *scalars) + records + names + macros + bytes(64) + custom + bytes(64)


def translate_behaviors(payload: bytes) -> bytes:
    rows, steps = payload[0], payload[1]
    if payload[2:4] != b"\0\0":
        raise ValueError("expected a key-behavior v1 domain")
    out, offset = bytearray([rows, 0]) + struct.pack("<H", steps), 4
    for _ in range(rows):
        length = struct.unpack_from("<H", payload, offset)[0]
        body = payload[offset + 2 : offset + 2 + length]
        body = body[:12] + struct.pack("<I", 0xFFFF) + body[12:]
        out += struct.pack("<H", len(body)) + body
        offset += 2 + length
    if offset != len(payload):
        raise ValueError("trailing bytes in the key-behavior domain")
    return bytes(out)


def translate_combos_v3(payload: bytes) -> bytes:
    count = payload[0]
    if payload[1:4] != b"\0\0\0" or len(payload) != 8 + count * 28:
        raise ValueError("expected a combo v2 domain")
    out = bytearray(payload[:8])
    for index in range(count):
        row = payload[8 + 28 * index : 8 + 28 * (index + 1)]
        inputs = row[0]
        out += row[:4] + struct.pack("<I", 0xFFFF) + row[8:12] + row[12 : 12 + 4 * inputs]
    return bytes(out)


def translate_pd_v3(payload: bytes) -> bytes:
    count = payload[3]
    if payload[:3] != bytes([2, 32, 96]) or len(payload) != 8 + 96 * count:
        raise ValueError("expected a PD v2 domain")
    out = bytearray([3, 32, 128, count, 0, 0, 0, 0])
    for index in range(count):
        record = payload[8 + 96 * index : 8 + 96 * (index + 1)]
        name = record[8:32].split(b"\0")[0]
        out += record[:8] + bytes([len(name)]) + bytes(23) + record[32:96] + name + bytes(32 - len(name))
    return bytes(out)


def translate_pd(payload: bytes) -> bytes:
    if payload[:8] != bytes([1, 8, 96, 0, 0, 0, 0, 0]) or len(payload) != 776:
        raise ValueError("expected a PD v1 domain")
    records = [payload[8 + 96 * slot : 8 + 96 * (slot + 1)] for slot in range(8)]
    kept = [record for record in records if record[1] != 0 or record[8] != 0]
    return bytes([2, 32, 96, len(kept), 0, 0, 0, 0]) + b"".join(kept)


def translate_combos(payload: bytes) -> bytes:
    count = payload[0]
    if payload[1:4] != b"\0\0\0" or len(payload) != 4 + count * 28:
        raise ValueError("expected a combo v1 domain")
    rows = [bytearray(payload[4 + 28 * i:4 + 28 * (i + 1)]) for i in range(count)]
    hold = struct.unpack_from("<H", rows[0], 4)[0] if rows else 200
    for row in rows:
        if struct.unpack_from("<H", row, 4)[0] != hold:
            raise ValueError("inconsistent combo hold thresholds")
        row[4:6] = b"\0\0"
    return payload[:4] + struct.pack("<HH", 60, hold) + b"".join(rows)


def translate(blob: bytes) -> bytes:
    if blob[:4] != b"NLP1" or blob[4] != 2:
        raise ValueError("expected a schema-2 profile blob")
    # Each step takes a domain one version forward.
    out, offset = bytearray(blob[:8]), 8
    out[4] = 3
    for _ in range(blob[6]):
        domain, version, length = blob[offset], blob[offset + 1], struct.unpack_from("<H", blob, offset + 2)[0]
        payload = blob[offset + 4 : offset + 4 + length]
        if domain == RGB_DOMAIN and version == 2:
            payload, version = translate_rgb(payload), 3
        if domain == RGB_DOMAIN and version == 3:
            payload, version = translate_rgb_v4(payload), 4
        if domain == 0x20 and version == 1:
            payload, version = translate_behaviors(payload), 2
        if domain == PD_DOMAIN and version == 1:
            payload, version = translate_pd(payload), 2
        if domain == PD_DOMAIN and version == 2:
            payload, version = translate_pd_v3(payload), 3
        if domain == 0x30 and version == 1:
            payload, version = translate_combos(payload), 2
        if domain == 0x30 and version == 2:
            payload, version = translate_combos_v3(payload), 3
        if domain == 0x40 and version == 5:
            payload, version = translate_settings(payload), 6
        elif domain == 0x40:
            raise ValueError("expected settings v5")
        out += bytes([domain, version]) + struct.pack("<H", len(payload)) + payload
        offset += 4 + length
    if offset != len(blob):
        raise ValueError("trailing bytes after the last domain")
    return bytes(out)


if __name__ == "__main__":
    Path(sys.argv[2]).write_bytes(translate(Path(sys.argv[1]).read_bytes()))
