#!/usr/bin/env python3
"""Write the maximum profile fixture (D-F14), or check it is current.

Every table at its simultaneous maximum in one schema 4.6 blob: RGB v5 with
128 groups, sixteen layer colours and 640 group rows, 255 in one table
(D-F63), 65 of them replacement rows beside a replacement feedback record
(D-F70); 128 behaviour rows of
five full steps (640 steps); 128 combos of sixteen inputs; settings v13 with
a shortcut on every custom key, most of them six keys with layers, and
every one of its 273 names at 32 bytes of UTF-8; and 32 pointing slots (v4), each named with 32
bytes, with every layer starting one of them; the full 1,920-byte matrix and
34,903-byte macro bank; and an autocorrect list (v2) of 1,024 corrections and
1,024 typos, every correction firing both ways and every typo part of a word,
so in both indexes, and one correction of 128 bytes, in 20,480 bytes of text
(36,874 bytes).
Values reach the edges the plan names: layers 7, 8 and
15, combo rows 31, 32, 63, 64 and 127, macros and custom keys 0, 63, 64 and
127. It is test evidence built from the byte specs, not from firmware code.

    python3 tests/host/make_maximum_profile.py          # rewrite the fixture
    python3 tests/host/make_maximum_profile.py --check  # fail if it is stale
"""

import json
import subprocess
import tempfile
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "autocorrect"))
import build as autocorrect_list  # noqa: E402  (the list's one encoder)

FIXTURE = ROOT / "tests/fixtures/maximum_profile_v4.fixture"
LAYERS = 16
DEPTH = 5
def slot_payload_max() -> int:
    with tempfile.TemporaryDirectory() as directory:
        probe = Path(directory) / "slot"
        source = '#include <stdio.h>\n#include "users/noah/lib/profile/storage/profile_storage_layout.h"\nint main(void) { printf("%u", (unsigned)NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX); }\n'
        subprocess.run(["cc", "-x", "c", "-I", str(ROOT), "-o", str(probe), "-"], input=source, text=True, check=True)
        return int(subprocess.check_output([str(probe)], text=True))


PAYLOAD_MAX = slot_payload_max()
ALLOWANCE = 12288  # reserved for future payload (per-key RGB and other additions)

KIND_KEY, KIND_LAYER_MO, KIND_LAYER_LOCK, KIND_PD, KIND_PD_LOCK, KIND_MACRO, KIND_CUSTOM = 1, 2, 3, 4, 5, 6, 7


def action(kind: int, operand: int) -> bytes:
    return struct.pack("<BBH", kind, 0, operand)


def name(label: str) -> bytes:
    """Exactly 32 bytes of UTF-8: the label, then two-byte characters."""
    encoded = label.encode()
    while len(encoded) + 2 <= 32:
        encoded += "é".encode()
    if len(encoded) < 32:
        encoded += b"!"
    assert len(encoded) == 32
    encoded.decode()
    return encoded


def basic_keycode(n: int) -> int:
    return 0x04 + n % 0x60  # KC_A .. inside the basic range


def rgb() -> bytes:
    vectors = json.loads((ROOT / "tests/fixtures/rgb_domain_v5.json").read_text())
    compiled = bytes.fromhex(next(v for v in vectors["valid"] if v["name"] == "compiled")["hex"])
    g, l, lg, p, pg, cg, t, kg = compiled[4:12]
    offset = 16 + 9 * g + 5 * l + 5 * lg
    fade = compiled[offset:offset + 4]
    offset += 4 + 5 * p + 5 * pg
    combo_feedback = compiled[offset:offset + 4]
    offset += 4 + 4 * cg + 3 * t
    key_feedback = compiled[offset:offset + 11]
    offset += 11 + 5 * kg
    replacement_feedback = compiled[offset:offset + 10]
    assert t == DEPTH - 1 and l == LAYERS and p == 32 and offset + 10 + 5 * compiled[14] == len(compiled)

    # D-F63: 128 LED sets; 640 group rows, 255 in each of the layer and
    # pointing tables, the rest shared by behaviour and replacement feedback.
    groups, layer_rows, pd_rows, combo_rows, replacement_rows = 128, 255, 255, 0, 65
    key_rows = 640 - layer_rows - pd_rows - combo_rows - replacement_rows
    out = bytearray([5, 0, 0x3F, 0x00, groups, LAYERS, layer_rows, 32, pd_rows, combo_rows, DEPTH - 1, key_rows, 58, 8, replacement_rows, 0])
    # Distinct bitmaps over LEDs 0..57, already in ascending byte order.
    for i in range(groups):
        bitmap = bytearray(8)
        bitmap[0] = i
        bitmap[7] = 0x03
        out += bytes([i]) + bitmap
    for layer in range(LAYERS):
        out += bytes([layer, (layer * 16) & 0xFF, 255, 200, layer & 1])
    # Row group ids reach 0 and 127; selectors reach layers 7, 8 and 15,
    # slots 0 and 31, every semantic and 0xff.
    for i in range(layer_rows):
        out += bytes([[7, 8, 15, 0xFF][i % 4], (10 * i) & 0xFF, 255, 200, i % groups])
    out += fade
    for slot in range(32):
        out += bytes([slot, 8 * slot, 200, 150, slot % 5])
    for i in range(pd_rows):
        out += bytes([[0, 31, 0xFF][i % 3], (20 * i) & 0xFF, 255, 200, (groups - 1 - i) % groups])
    out += combo_feedback
    for i in range(DEPTH - 1):
        out += bytes([40 * i, 255, 200])
    out += key_feedback
    for i in range(key_rows):
        out += bytes([[0, 1, 2, 3, 0xFF][i % 5], (30 * i) & 0xFF, 255, 200, (i + 4) % groups])
    out += replacement_feedback
    for i in range(replacement_rows):
        out += bytes([[0, 1, 2, 0xFF][i % 4], (40 * i) & 0xFF, 255, 200, (i + 63) % groups])
    assert len(out) == 45 + 9 * groups + 5 * LAYERS + 5 * layer_rows + 5 * 32 + 5 * pd_rows + 4 * combo_rows + 3 * (DEPTH - 1) + 5 * key_rows + 5 * replacement_rows == 4649
    return bytes(out)


def behaviors() -> bytes:
    out = bytearray([128, 0]) + struct.pack("<H", 128 * DEPTH)
    for slot in range(128):
        allowed = 0xFFFF if slot % 3 else (1 << 7) | (1 << 8) | (1 << 15)
        flags = (1 if slot % 4 == 1 else 0) | (2 if slot % 5 == 2 else 0)
        body = action(KIND_CUSTOM, slot)
        body += struct.pack("<HHHBBI", 150 + slot, 300 + slot, 120 + slot, flags, DEPTH, allowed)
        for tap in range(DEPTH):
            body += bytes([tap, 0b111])
            body += action(KIND_KEY, basic_keycode(slot + tap))
            body += bytes([1, 0]) + action(KIND_LAYER_MO, [7, 8, 15, 1, 2][tap])
            body += bytes([3, 1 + (slot + tap) % 100]) + action(KIND_MACRO, [0, 63, 64, 127][(slot + tap) % 4])
        out += struct.pack("<H", len(body)) + body
    return bytes(out)


def combos() -> bytes:
    out = bytearray([128, 0, 0, 0]) + struct.pack("<HH", 50, 200)
    for row in range(128):
        flags = [0, 1, 2, 4, 8, 4 | 8][row % 6]
        window = 0 if row % 2 else 30 + row
        allowed = 0xFFFF if row % 7 else 1 << (row % LAYERS)
        if row % 3 == 0:
            output = action(KIND_MACRO, row)
        elif row % 3 == 1:
            output = action(KIND_CUSTOM, 127 - row)
        else:
            output = action(KIND_KEY, basic_keycode(row))
        out += bytes([16, flags]) + struct.pack("<HI", window, allowed) + output
        for member in range(16):
            out += action(KIND_KEY, basic_keycode(3 * row + member))
    return bytes(out)


def shortcuts() -> bytes:
    """Settings v13 (D-F26): a modifier mask, six keys, a layer mask and the
    layer kind for each custom key.

    Slot 0 mixes sides (Left Ctrl + Right Alt + K), 63 holds modifiers alone
    (Hyper), 64 a mouse button alone, 65 layers alone as a One-shot, 127 every
    modifier, six keys ending in the last keyboard, media and mouse keys, and
    every layer but the base as a Tap-toggle; the rest hold six keys (four
    keyboard keys, a media key, then a system or mouse key), cycling through the
    key vocabulary, the layers (never the base, the one default layer here) and
    the five layer kinds, Hold without layers.
    """
    records = []
    for slot in range(128):
        layers = (slot * 0x0203) & 0xFFFE
        keys = [0x04 + (slot * 4 + key) % 0xA1 for key in range(4)]
        keys += [0xA8 + slot % 27, 0xA5 + slot % 3 if slot % 2 else 0xCD + slot % 16]
        records.append(((slot * 37) & 0xFF, keys, layers, slot % 5 if layers else 0))
    records[0] = (0x41, [0x0E], 0x0000, 0)
    records[63] = (0x0F, [], 0x0000, 0)
    records[64] = (0x00, [0xD1], 0x0000, 0)
    records[65] = (0x00, [], 0x8102, 4)
    records[127] = (0xFF, [0x04, 0x52, 0xA7, 0xA4, 0xC2, 0xDC], 0xFFFE, 3)
    out = b""
    for mods, keys, layers, kind in records:
        out += bytes([mods] + keys + [0] * (6 - len(keys))) + struct.pack("<HB", layers, kind)
    return out


def settings() -> bytes:
    scalars = [0] * 33
    scalars[0:4] = [200, 220, 600, 180]  # tapping, tap/hold, long hold, multi tap
    scalars[4:8] = [1, 15, 1200, 25]  # auto mouse on layer 15
    scalars[8:10] = [1, (1 << 8) | (1 << 15)]  # auto sniping on layers 8 and 15
    scalars[15:17] = [250, 300]
    scalars[17] = 600000
    scalars[18:21] = [1600, 200, 1]
    scalars[21] = 1 | (1 << 8) | (128 << 16)
    scalars[22] = 10 | (255 << 8) | (150 << 16)
    scalars[23] = 1
    scalars[24] = 0
    scalars[25:27] = [100, 8]
    scalars[28:31] = [1, 0xFF7F, 0x7FFF]  # behaviours off on layer 7, combos off on 15
    scalars[31] = 65535  # the longest replacement flash
    scalars[32] = 10000  # the longest leader timeout
    out = bytearray([13, LAYERS, 33, 128, 128, 0, 128, 0])
    out += b"".join(struct.pack("<I", v) for v in scalars)
    for layer in range(LAYERS):
        reference = (layer + 1) % LAYERS
        bypass = (1 << (layer % 60)) | (1 << 59)
        exclusion = (1 << ((3 * layer) % 60)) if layer % 2 else 0
        out += bytes([reference]) + struct.pack("<QQ", bypass, exclusion)
    out += shortcuts()
    names = [f"Layer {n} " for n in range(LAYERS)] + [f"Macro {n} " for n in range(128)] + [f"Key {n} " for n in range(128)] + ["Keyboard "]
    for label in names:
        encoded = name(label)
        out += bytes([len(encoded)]) + encoded
    assert len(out) == 10701
    return bytes(out)


def pointing() -> bytes:
    vectors = json.loads((ROOT / "tests/fixtures/pd_mode_domain_v4.json").read_text())
    full = bytearray.fromhex(next(v for v in vectors["valid"] if v["name"] == "full")["hex"])
    assert len(full) == 8 + 32 * 128
    for slot in range(32):
        record = 8 + 128 * slot
        encoded = name(f"Slot {slot} ")
        full[record + 8] = len(encoded)
        full[record + 96:record + 128] = encoded
    # Every layer starts one mode (D-F62): eight slots link two layers each.
    # Their keys turn on, in turn, their own linked layers, a layer the next
    # slot links, or every layer; what ends them cycles through nothing, a
    # higher layer and any other layer.
    configured = [slot for slot in range(32) if full[8 + 128 * slot + 1] != 0]
    for index, slot in enumerate(configured[:LAYERS // 2]):
        layers = 0b11 << (2 * index)
        turns_on = [layers, 1 << ((2 * index + 2) % LAYERS), 0xFFFF][index % 3]
        record = 8 + 128 * slot
        full[record + 11] = index % 3
        full[record + 12:record + 16] = struct.pack("<HH", layers, turns_on)
    return bytes(full)


def keymap() -> bytes:
    return b"".join(struct.pack("<H", basic_keycode(layer + position)) for layer in range(16) for position in range(60))


def macros() -> bytes:
    # One long stream and all 128 terminators fill today's bank exactly.
    return b"a" * (34903 - 128) + b"\0" * 128


def autocorrect() -> bytes:
    payload = autocorrect_list.encode(autocorrect_list.maximum_entries())
    assert len(payload) == autocorrect_list.MAX_SIZE == 36874
    return payload


def blob() -> tuple[bytes, list[tuple[int, int, bytes]]]:
    domains = [(0x10, 5, rgb()), (0x20, 2, behaviors()), (0x30, 3, combos()), (0x40, 13, settings()), (0x50, 4, pointing()), (0x60, 1, keymap()), (0x70, 1, macros()), (0x80, 2, autocorrect())]
    out = bytearray(b"NLP1" + bytes([4, 6, len(domains), 1]))
    for domain_id, version, payload in domains:
        out += bytes([domain_id, version]) + struct.pack("<I", len(payload)) + payload
    assert len(out) + ALLOWANCE <= PAYLOAD_MAX
    return bytes(out), domains


def fnv1a(data: bytes) -> int:
    value = 0x811C9DC5
    for byte in data:
        value = ((value ^ byte) * 0x01000193) & 0xFFFFFFFF
    return value


def render() -> str:
    data, domains = blob()
    lines = [
        "# The maximum profile (D-F14): every table at its simultaneous maximum.",
        "# Generated by tests/host/make_maximum_profile.py; do not edit.",
        f"profile.byte_length={len(data)}",
        f"profile.headroom={PAYLOAD_MAX - len(data)}",
        f"profile.crc32={zlib.crc32(data):08x}",
        f"profile.digest={fnv1a(data):08x}",
    ]
    for domain_id, version, payload in domains:
        lines.append(f"domain.{domain_id:02x}.version={version}")
        lines.append(f"domain.{domain_id:02x}.length={len(payload)}")
    lines.append(f"profile.hex={data.hex()}")
    return "\n".join(lines) + "\n"


def main() -> int:
    text = render()
    if sys.argv[1:] == ["--check"]:
        if not FIXTURE.exists() or FIXTURE.read_text() != text:
            print(f"{FIXTURE.relative_to(ROOT)} is stale; run python3 tests/host/make_maximum_profile.py", file=sys.stderr)
            return 1
        return 0
    if sys.argv[1:]:
        print(__doc__, file=sys.stderr)
        return 2
    FIXTURE.write_text(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
