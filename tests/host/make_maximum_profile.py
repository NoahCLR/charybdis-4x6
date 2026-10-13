#!/usr/bin/env python3
"""Write the maximum profile fixture (D-F14), or check it is current.

Every table at its simultaneous maximum in one schema 3.0 blob: lighting (RGB
v5, D-F25) filling its whole 8 KiB budget, with every thing table at its
maximum record count, 128 palette colours, maps both dense and sparse and 255
rules using every kind; 128 behaviour rows of
five full steps (640 steps); 128 combos of sixteen inputs; settings v6 with
every one of its 272 names at 32 bytes of UTF-8; and 32 pointing slots, each
named with 32 bytes. Values reach the edges the plan names: layers 7, 8 and
15, combo rows 31, 32, 63, 64 and 127, macros and custom keys 0, 63, 64 and
127. It is test evidence built from the byte specs, not from firmware code.

    python3 tests/host/make_maximum_profile.py          # rewrite the fixture
    python3 tests/host/make_maximum_profile.py --check  # fail if it is stale
"""

import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests/fixtures/maximum_profile_v3.fixture"
LAYERS = 16
DEPTH = 5
PAYLOAD_MAX = 53216
LIGHTING_BUDGET = 8192
# Reserved for future payload once lighting has its budget (D-F25 took 8 KiB
# of the earlier 12 KiB reserve).
ALLOWANCE = 4096

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
    """A version-5 lighting domain of exactly 8,192 bytes (lighting-rules-v1.md)."""
    sys.path.insert(0, str(ROOT / "tools"))
    import lighting_reference as ref

    part, rule = ref.part, ref.rule
    C, N, T, P, B, G = ref.SLOT_CONDITION, ref.SLOT_NEGATED, ref.SLOT_TARGET, ref.SLOT_PAINT, ref.SLOT_BACKGROUND, ref.SLOT_TIMING
    palette = [[(7 * i) % 256, 255 - i, 200 - (i % 200)] for i in range(128)]
    behaviour_record = [5] + [(3 * i) % 128 for i in range(3 + DEPTH - 1)]
    things = [
        {"kind": 1, "default": [6, 255], "records": [[0 if layer & 1 else 6, layer] for layer in range(LAYERS)]},
        {"kind": 2, "default": [0, 255], "records": [[[0, 1, 2, 4, 5][slot % 5], 16 + slot] for slot in range(32)]},
        {"kind": 3, "default": [5, 50], "records": [[[0xFE, 4, 5][i % 3], [0xFE, 0xFF, 64 + i % 64][i % 3]] for i in range(128)]},
        {"kind": 4, "default": behaviour_record, "records": [[0xFE] + [(i + j) % 128 if (i + j) % 4 else 0xFE for j in range(3 + DEPTH - 1)] for i in range(128)]},
    ]
    key_sets = [[(set_index + led) % 58 for led in range(set_index % 9)] for set_index in range(32)]
    kinds = [
        part(C, 1, 0xFF), part(C, 1, 7), part(C, 2, 8), part(C, 3, 31), part(C, 4, 0), part(C, 4, 0xFF),
        part(C, 5, 0x0F, 0), part(C, 5, 0x04, 1), part(C, 5, 0x01, 2), part(C, 5, 0x0A, 3), part(N, 1, 15), part(N, 3, 0xFF),
    ]
    targets = [part(T, 0), part(T, 1), part(T, 2), part(T, 3, 31), part(T, 4), part(T, 5), part(T, 6), part(T, 7)]
    rules = []
    for index in range(255):
        conditions = [kinds[(index + k) % len(kinds)] for k in range(index % 5)]
        paint = [part(P, 0), part(P, 1), part(P, 2, index % 128), part(P, 3), part(P, 4, index % 4), part(P, 5, index), part(P, 6, 127 - index % 128, index)][index % 7]
        extra = []
        if index % 11 == 0:
            extra.append(part(B, 5, 64))
        if index % 13 == 0:
            extra.append(part(G, 1, index, 1))
        rules.append(rule(*conditions, targets[index % len(targets)], paint, *extra, flags=1 if index % 17 == 0 else 0, brightness=255 - index % 100))
    extensions = [[1, [1, 2, 127, 10]], [2, [1]]]
    maps = [{led: (led * 5 + m) % 128 if (led + m) % 7 else 0x83 for led in range(58) if (led + m) % 3} for m in range(4)]
    maps += [{led: 0x81 for led in range(m % 29)} for m in range(4)]
    domain = {"palette": palette, "things": things, "keySets": key_sets, "maps": maps, "rules": rules, "extensions": extensions}
    # Fill the budget exactly with dense maps and, last, one sparse map.
    while len(ref.encode(domain)) + 59 + 2 <= LIGHTING_BUDGET:
        domain["maps"].append({led: (led + len(domain["maps"])) % 128 for led in range(58)})
    remaining = LIGHTING_BUDGET - len(ref.encode(domain))
    assert remaining >= 2 and remaining % 2 == 0, remaining
    domain["maps"].append({led: led % 128 for led in range((remaining - 2) // 2)})
    out = ref.encode(domain)
    assert len(out) == LIGHTING_BUDGET
    ref.validate(out, ref.Limits(tap_depth=DEPTH, layers=LAYERS))
    return out


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


def settings() -> bytes:
    scalars = [0] * 31
    scalars[0:4] = [200, 220, 600, 180]  # tapping, tap/hold, long hold, multi tap
    scalars[4:8] = [1, 15, 1200, 25]  # auto mouse on layer 15
    scalars[8:10] = [1, 8]  # auto sniping on layer 8
    scalars[15:17] = [250, 300]
    scalars[17] = 600000
    scalars[18:21] = [1600, 200, 1]
    scalars[21] = 1 | (1 << 8) | (128 << 16)
    scalars[22] = 10 | (255 << 8) | (150 << 16)
    scalars[23] = 1
    scalars[24] = 0
    scalars[25:27] = [100, 8]
    scalars[28:31] = [1, 0xFF7F, 0x7FFF]  # behaviours off on layer 7, combos off on 15
    out = bytearray([6, LAYERS, 31, 128, 128, 0, 0, 0])
    out += b"".join(struct.pack("<I", v) for v in scalars)
    for layer in range(LAYERS):
        reference = (layer + 1) % LAYERS
        bypass = (1 << (layer % 60)) | (1 << 59)
        exclusion = (1 << ((3 * layer) % 60)) if layer % 2 else 0
        out += bytes([reference]) + struct.pack("<QQ", bypass, exclusion)
    names = [f"Layer {n} " for n in range(LAYERS)] + [f"Macro {n} " for n in range(128)] + [f"Key {n} " for n in range(128)]
    for label in names:
        encoded = name(label)
        out += bytes([len(encoded)]) + encoded
    assert len(out) == 9380
    return bytes(out)


def pointing() -> bytes:
    vectors = json.loads((ROOT / "tests/fixtures/pd_mode_domain_v3.json").read_text())
    full = bytearray.fromhex(next(v for v in vectors["valid"] if v["name"] == "full")["hex"])
    assert len(full) == 8 + 32 * 128
    for slot in range(32):
        record = 8 + 128 * slot
        encoded = name(f"Slot {slot} ")
        full[record + 8] = len(encoded)
        full[record + 96:record + 128] = encoded
    return bytes(full)


def blob() -> tuple[bytes, list[tuple[int, int, bytes]]]:
    domains = [(0x10, 5, rgb()), (0x20, 2, behaviors()), (0x30, 3, combos()), (0x40, 6, settings()), (0x50, 3, pointing())]
    out = bytearray(b"NLP1" + bytes([3, 0, len(domains), 1]))
    for domain_id, version, payload in domains:
        out += bytes([domain_id, version]) + struct.pack("<H", len(payload)) + payload
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
