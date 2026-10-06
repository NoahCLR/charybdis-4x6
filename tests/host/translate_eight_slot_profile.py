#!/usr/bin/env python3
"""Translate an eight-slot schema-2 profile blob for the 32-slot firmware.

The documented import translation (docs/architecture/portable-profile-v1.md):
PD domain 0x50 version 1 becomes sparse version 2 by dropping each disabled
record without a name, and RGB domain 0x10 version 2 becomes version 3 by
adding an uncoloured PD row (h=0, s=0, v=0, right half) for slots 8..31.
Every other byte is kept. Test tooling: the firmware never translates.
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


def translate_pd(payload: bytes) -> bytes:
    if payload[:8] != bytes([1, 8, 96, 0, 0, 0, 0, 0]) or len(payload) != 776:
        raise ValueError("expected a PD v1 domain")
    records = [payload[8 + 96 * slot : 8 + 96 * (slot + 1)] for slot in range(8)]
    kept = [record for record in records if record[1] != 0 or record[8] != 0]
    return bytes([2, 32, 96, len(kept), 0, 0, 0, 0]) + b"".join(kept)


def translate(blob: bytes) -> bytes:
    if blob[:4] != b"NLP1" or blob[4] != 2:
        raise ValueError("expected a schema-2 profile blob")
    out, offset = bytearray(blob[:8]), 8
    for _ in range(blob[6]):
        domain, version, length = blob[offset], blob[offset + 1], struct.unpack_from("<H", blob, offset + 2)[0]
        payload = blob[offset + 4 : offset + 4 + length]
        if domain == RGB_DOMAIN and version == 2:
            payload, version = translate_rgb(payload), 3
        elif domain == PD_DOMAIN and version == 1:
            payload, version = translate_pd(payload), 2
        out += bytes([domain, version]) + struct.pack("<H", len(payload)) + payload
        offset += 4 + length
    if offset != len(blob):
        raise ValueError("trailing bytes after the last domain")
    return bytes(out)


if __name__ == "__main__":
    Path(sys.argv[2]).write_bytes(translate(Path(sys.argv[1]).read_bytes()))
