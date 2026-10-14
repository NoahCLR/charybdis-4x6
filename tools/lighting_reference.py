#!/usr/bin/env python3
"""Reference model of RGB domain 0x10 version 5 (lighting rules).

An independent implementation of docs/architecture/lighting-rules-v1.md: the
payload encoder, the strict validator, the version-4 conversion and the walk
that renders a rule list. It writes tests/fixtures/lighting_vectors_v1.json,
which the firmware's C implementation must pass byte for byte and colour for
colour, and which any other implementation of the contract can share.

    python3 tools/lighting_reference.py --write   # regenerate the vectors
    python3 tools/lighting_reference.py --check   # fail if they drifted
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
VECTORS = REPO_ROOT / "tests" / "fixtures" / "lighting_vectors_v1.json"
CONTRACT = REPO_ROOT / "tests" / "fixtures" / "lighting_contract_v1.json"
V4_VECTORS = REPO_ROOT / "tests" / "fixtures" / "rgb_domain_v4.json"

VERSION = 5
LED_COUNT = 58
LEFT_LED_COUNT = 29
BUDGET = 8192
PALETTE_MAX = 128
CONDITIONS_MAX = 4
PARAM_MAX = 16
NONE = 0xFF
INHERIT = 0xFE
ANY = 0xFF

# The Charybdis 4x6: 10 matrix rows of 6 columns, rows 0..4 left. Key index is
# row * 6 + column; four positions have no LED and LEDs 56..57 have no key.
MATRIX_ROWS = 10
MATRIX_COLS = 6
KEY_LEDS = [None] * (MATRIX_ROWS * MATRIX_COLS)
for _led, (_row, _col) in enumerate([
    (0, 0), (1, 0), (2, 0), (3, 0), (3, 1), (2, 1), (1, 1), (0, 1), (0, 2), (1, 2), (2, 2), (3, 2), (3, 3), (2, 3), (1, 3), (0, 3),
    (0, 4), (1, 4), (2, 4), (3, 4), (0, 5), (1, 5), (2, 5), (3, 5), (4, 2), (4, 5), (4, 3), (4, 4), (4, 1),
    (5, 0), (6, 0), (7, 0), (8, 0), (8, 1), (7, 1), (6, 1), (5, 1), (5, 2), (6, 2), (7, 2), (8, 2), (8, 3), (7, 3), (6, 3), (5, 3),
    (5, 4), (6, 4), (7, 4), (8, 4), (5, 5), (6, 5), (7, 5), (8, 5), (9, 1), (9, 3), (9, 2),
]):
    KEY_LEDS[_row * MATRIX_COLS + _col] = _led
LEFT, RIGHT = "left", "right"
ALL_LEDS = frozenset(range(LED_COUNT))
HALF_LEDS = {LEFT: frozenset(range(LEFT_LED_COUNT)), RIGHT: frozenset(range(LEFT_LED_COUNT, LED_COUNT))}


def key_half(key: int) -> str:
    return LEFT if key // MATRIX_COLS < MATRIX_ROWS // 2 else RIGHT


def key_leds(keys) -> frozenset:
    return frozenset(KEY_LEDS[k] for k in keys if KEY_LEDS[k] is not None)


# ─── Registries ─────────────────────────────────────────────────────────────
# Parameter types, by name, as the contract defines them.
FACTS = {1: ("layer active", ["layerSelector"]), 2: ("layer previewing", ["layerSelector"]), 3: ("pointing mode active", ["modeSelector"]), 4: ("combos held", ["comboRole"]), 5: ("behaviour state", ["behaviourStates", "behaviourScope"])}
FACTS.update({7: ("combo held", ["thingSelector", "comboRole"]), 8: ("behaviour", ["thingSelector", "behaviourStates"])})
FACTS.update({9: ('layer status', ['layerSelector', 'layerStatus']), 10: ('mode status', ['modeSelector', 'modeStatus']), 11: ('combo layer', ['thingSelector', 'layerSelector']), 12: ('behaviour phase', ['thingSelector', 'behaviourPhase', 'tapSelector']), 13: ('macro playing', ['thingSelector']), 14: ('key pressed', ['keySelector']), 15: ('modifier', ['modifier', 'side', 'modifierState']), 16: ('host lock', ['hostLock']), 17: ('pointer', ['pointerState']), 18: ('idle', [])})
FACT_THING = {1: 1, 2: 1, 3: 2, 4: 3, 5: 4, 7: 3, 8: 4}
FACT_THING.update({9: 1, 10: 2, 11: 3, 12: 4, 13: 5, 14: 7, 15: 6, 16: 0, 17: 0, 18: 0})
TARGETS = {0: [], 1: [], 2: [], 3: ["keySet"], 4: [], 5: [], 6: [], 7: [], 8: ["keyQuery", "thingSelector"]}
PLACE_TARGETS = {0, 1, 2, 4, 5, 6}
PAINTS = {0: [], 1: [], 2: ["paletteIndex"], 3: [], 4: ["map"], 5: ["level"], 6: ["paletteIndex", "level"]}
PAINTS[8] = []
PAINTS[9] = []
PAINT_READS_BELOW = {5, 6}
TIMINGS = {1: ["milliseconds"]}
RULE_EXTENSIONS: dict = {}
DOMAIN_EXTENSIONS = {1: 4, 2: 1}
THING_KINDS = {1: 16, 2: 32, 3: 128, 4: 128, 5: 128, 6: 8, 7: 60}
TYPE_BYTES = {"milliseconds": 2}

SLOT_CONDITION, SLOT_NEGATED, SLOT_TARGET, SLOT_PAINT, SLOT_BACKGROUND, SLOT_TIMING, SLOT_EXTENSION = range(7)
SLOT_RANK = {0: 0, 1: 0, 2: 1, 3: 2, 4: 3, 5: 4, 6: 5}


def param_length(types) -> int:
    return sum(TYPE_BYTES.get(t, 1) for t in types)


class Limits:
    """What the keyboard supports: the capability bitmaps and compiled facts."""

    def __init__(self, tap_depth=5, layers=16, brightness=200, modes=0xFFFFFFFF, facts=None, targets=None, paints=None, timings=None, rule_extensions=None, domain_extensions=None, things=None):
        self.tap_depth = tap_depth
        self.layers = layers
        self.brightness = brightness
        self.modes = modes
        self.facts = set(FACTS) if facts is None else set(facts)
        self.targets = set(TARGETS) if targets is None else set(targets)
        self.paints = set(PAINTS) if paints is None else set(paints)
        self.timings = set(TIMINGS) if timings is None else set(timings)
        self.rule_extensions = set(RULE_EXTENSIONS) if rule_extensions is None else set(rule_extensions)
        self.domain_extensions = set(DOMAIN_EXTENSIONS) if domain_extensions is None else set(domain_extensions)
        self.things = set(THING_KINDS) if things is None else set(things)

    def colour_slots(self, kind: int) -> int:
        return 3 + (self.tap_depth - 1) if kind == 4 else 1

    def record_size(self, kind: int) -> int:
        return 1 + self.colour_slots(kind)

    def thing_count(self, kind: int) -> int:
        return self.layers if kind == 1 else THING_KINDS[kind]

    def as_json(self) -> dict:
        return {"tapDepth": self.tap_depth, "logicalLayers": self.layers, "maximumBrightness": self.brightness, "supportedModeMask": self.modes,
                "facts": sorted(self.facts), "targets": sorted(self.targets), "paints": sorted(self.paints), "timings": sorted(self.timings),
                "ruleExtensions": sorted(self.rule_extensions), "domainExtensions": sorted(self.domain_extensions), "things": sorted(self.things)}

    @staticmethod
    def from_json(data: dict) -> "Limits":
        return Limits(data["tapDepth"], data["logicalLayers"], data["maximumBrightness"], data["supportedModeMask"], data["facts"], data["targets"], data["paints"], data["timings"], data["ruleExtensions"], data["domainExtensions"], data["things"])


# ─── Encoding ───────────────────────────────────────────────────────────────
# A domain is a dict: palette [[h, s, v]], things [{kind, default, records}],
# keySets [[led...]], maps [{led: value}], rules [{flags, brightness, parts:
# [[slot, kind, [bytes]]]}], extensions [[kind, [bytes]]].

def part(slot: int, kind: int, *params: int) -> list:
    return [slot, kind, list(params)]


def rule(*parts, flags=0, brightness=255) -> dict:
    return {"flags": flags, "brightness": brightness, "parts": list(parts)}


def encode_map(values: dict) -> bytes:
    painted = {led: v for led, v in sorted(values.items()) if v != 0x80}
    if len(painted) <= 28:
        out = bytearray([1, len(painted)])
        for led, value in painted.items():
            out += bytes([led, value])
        return bytes(out)
    return bytes([0] + [values.get(led, 0x80) for led in range(LED_COUNT)])


def encode(domain: dict) -> bytes:
    palette = domain.get("palette", [])
    things = domain.get("things", [])
    key_sets = domain.get("keySets", [])
    maps = domain.get("maps", [])
    rules = domain.get("rules", [])
    out = bytearray([VERSION, 0, LED_COUNT, len(palette), len(things), len(key_sets), len(maps), len(rules)])
    for colour in palette:
        out += bytes(colour)
    for table in things:
        out += bytes([table["kind"], len(table["records"])]) + bytes(table["default"])
        for record in table["records"]:
            out += bytes(record)
    for leds in key_sets:
        bitmap = bytearray(8)
        for led in leds:
            bitmap[led // 8] |= 1 << (led % 8)
        out += bitmap
    for values in maps:
        out += encode_map(values)
    for item in rules:
        body = bytearray([item["flags"], item["brightness"]])
        for slot, kind, params in item["parts"]:
            body += bytes([(slot << 5) | len(params), kind]) + bytes(params)
        out += bytes([len(body)]) + body
    extensions = domain.get("extensions", [])
    out += bytes([len(extensions)])
    for kind, body in extensions:
        out += bytes([kind, len(body)]) + bytes(body)
    return bytes(out)


# ─── Validation ─────────────────────────────────────────────────────────────

class Invalid(Exception):
    def __init__(self, code: str, offset: int):
        super().__init__(f"{code} at {offset}")
        self.code = code
        self.offset = offset


def check_param(kind_type: str, data: bytes, offset: int, limits: Limits, counts: dict) -> None:
    value = data[offset]
    if kind_type == "layerSelector":
        if value != ANY and value >= limits.layers:
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "modeSelector":
        if value != ANY and (value >= 32 or not (limits.modes >> value) & 1):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "thingSelector":
        if value != ANY and value >= 128:
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "comboRole":
        if value not in (0, 1, ANY):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "behaviourStates":
        if value == 0 or value & 0xF0:
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "behaviourScope":
        if value > 3:
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "keySet":
        if value >= counts["K"]:
            raise Invalid("INVALID_REFERENCE", offset)
    elif kind_type == "paletteIndex":
        if value >= counts["P"]:
            raise Invalid("INVALID_REFERENCE", offset)
    elif kind_type == "map":
        if value >= counts["M"]:
            raise Invalid("INVALID_REFERENCE", offset)
    elif kind_type == "milliseconds":
        if data[offset] | data[offset + 1] << 8 == 0:
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "keyQuery":
        if value >= 16:
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "layerStatus":
        if not (value <= 3):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "modeStatus":
        if not (value <= 1):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "behaviourPhase":
        if not (value <= 4):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "tapSelector":
        if not (value == ANY or 1 <= value <= limits.tap_depth):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "keySelector":
        if not (value == ANY or value < 60):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "modifier":
        if not (value < 4):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "side":
        if not (value < 3):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "modifierState":
        if not (value < 2):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "hostLock":
        if not (value < 3):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type == "pointerState":
        if not (value < 4):
            raise Invalid("INVALID_PARAMETER", offset)
    elif kind_type != "level":
        raise AssertionError(kind_type)


def check_map_value(value: int, offset: int, limits: Limits, counts: dict) -> None:
    if value < 0x80:
        if value >= counts["P"]:
            raise Invalid("INVALID_REFERENCE", offset)
        return
    kind = value - 0x80
    if kind not in limits.paints or PAINTS.get(kind) != [] or kind == 4:
        raise Invalid("INVALID_REFERENCE", offset)


def validate(data: bytes, limits: Limits) -> dict:
    """Return the parsed domain, or raise Invalid with the reference offset."""
    if len(data) > BUDGET:
        raise Invalid("CAPACITY_EXCEEDED", BUDGET)
    if len(data) < 8:
        raise Invalid("TRUNCATED", 0)
    if data[0] != VERSION:
        raise Invalid("INVALID_VERSION", 0)
    if data[1] != 0:
        raise Invalid("RESERVED_BITS", 1)
    if data[2] != LED_COUNT:
        raise Invalid("INCOMPATIBLE_GEOMETRY", 2)
    counts = {"P": data[3], "T": data[4], "K": data[5], "M": data[6], "R": data[7]}
    if counts["P"] > PALETTE_MAX:
        raise Invalid("CAPACITY_EXCEEDED", 3)
    offset = 8
    for _ in range(counts["P"]):
        if offset + 3 > len(data):
            raise Invalid("TRUNCATED", offset)
        if data[offset + 2] > limits.brightness:
            raise Invalid("BRIGHTNESS_EXCEEDED", offset + 2)
        offset += 3
    previous_kind = -1
    for _ in range(counts["T"]):
        if offset + 2 > len(data):
            raise Invalid("TRUNCATED", offset)
        kind = data[offset]
        if kind not in limits.things:
            raise Invalid("UNSUPPORTED_KIND", offset)
        if kind <= previous_kind:
            raise Invalid("NONCANONICAL_ORDER", offset)
        previous_kind = kind
        if data[offset + 1] > limits.thing_count(kind):
            raise Invalid("CAPACITY_EXCEEDED", offset + 1)
        size = limits.record_size(kind)
        records = data[offset + 1]
        offset += 2
        for index in range(records + 1):
            if offset + size > len(data):
                raise Invalid("TRUNCATED", offset)
            default = index == 0
            place = data[offset]
            if not (place in PLACE_TARGETS and place in limits.targets) and not (place == INHERIT and not default):
                raise Invalid("INVALID_REFERENCE", offset)
            for field in range(1, size):
                colour = data[offset + field]
                if colour == NONE or (colour == INHERIT and not default) or colour < counts["P"]:
                    continue
                raise Invalid("INVALID_REFERENCE", offset + field)
            offset += size
    for _ in range(counts["K"]):
        if offset + 8 > len(data):
            raise Invalid("TRUNCATED", offset)
        if data[offset + 7] & 0xFC:
            raise Invalid("RESERVED_BITS", offset + 7)
        offset += 8
    for _ in range(counts["M"]):
        if offset + 1 > len(data):
            raise Invalid("TRUNCATED", offset)
        encoding = data[offset]
        if encoding == 0:
            if offset + 1 + LED_COUNT > len(data):
                raise Invalid("TRUNCATED", offset)
            painted = 0
            for led in range(LED_COUNT):
                value = data[offset + 1 + led]
                check_map_value(value, offset + 1 + led, limits, counts)
                painted += value != 0x80
            if painted <= 28:
                raise Invalid("NONCANONICAL_ENCODING", offset)
            offset += 1 + LED_COUNT
        elif encoding == 1:
            if offset + 2 > len(data):
                raise Invalid("TRUNCATED", offset)
            entries = data[offset + 1]
            if entries > 28:
                raise Invalid("NONCANONICAL_ENCODING", offset + 1)
            if offset + 2 + 2 * entries > len(data):
                raise Invalid("TRUNCATED", offset)
            previous_led = -1
            for entry in range(entries):
                at = offset + 2 + 2 * entry
                led, value = data[at], data[at + 1]
                if led >= LED_COUNT:
                    raise Invalid("INVALID_REFERENCE", at)
                if led <= previous_led:
                    raise Invalid("NONCANONICAL_ORDER", at)
                previous_led = led
                if value == 0x80:
                    raise Invalid("NONCANONICAL_ENCODING", at + 1)
                check_map_value(value, at + 1, limits, counts)
            offset += 2 + 2 * entries
        else:
            raise Invalid("INVALID_ENUM", offset)
    for _ in range(counts["R"]):
        if offset + 1 > len(data):
            raise Invalid("TRUNCATED", offset)
        size = data[offset]
        if size < 6:
            raise Invalid("INVALID_LENGTH", offset)
        if offset + 1 + size > len(data):
            raise Invalid("TRUNCATED", offset)
        if data[offset + 1] & 0xFE:
            raise Invalid("RESERVED_BITS", offset + 1)
        end = offset + 1 + size
        at = offset + 3
        rank, conditions, timings, singles, previous = 0, 0, 0, set(), {SLOT_TIMING: -1, SLOT_EXTENSION: -1}
        while at < end:
            if at + 2 > end:
                raise Invalid("INVALID_LENGTH", at)
            slot, length, kind = data[at] >> 5, data[at] & 31, data[at + 1]
            if slot == 7:
                raise Invalid("INVALID_STRUCTURE", at)
            if SLOT_RANK[slot] < rank:
                raise Invalid("NONCANONICAL_ORDER", at)
            rank = SLOT_RANK[slot]
            if slot in (SLOT_CONDITION, SLOT_NEGATED):
                conditions += 1
                if conditions > CONDITIONS_MAX:
                    raise Invalid("CAPACITY_EXCEEDED", at)
            elif slot in (SLOT_TARGET, SLOT_PAINT, SLOT_BACKGROUND):
                if slot in singles:
                    raise Invalid("INVALID_STRUCTURE", at)
                singles.add(slot)
            else:
                if slot == SLOT_TIMING:
                    timings += 1
                    if timings > 4:
                        raise Invalid("CAPACITY_EXCEEDED", at)
                if kind <= previous[slot]:
                    raise Invalid("NONCANONICAL_ORDER", at + 1)
                previous[slot] = kind
            registry, supported = {
                SLOT_CONDITION: (FACTS, limits.facts), SLOT_NEGATED: (FACTS, limits.facts), SLOT_TARGET: (TARGETS, limits.targets),
                SLOT_PAINT: (PAINTS, limits.paints), SLOT_BACKGROUND: (PAINTS, limits.paints), SLOT_TIMING: (TIMINGS, limits.timings),
                SLOT_EXTENSION: (RULE_EXTENSIONS, limits.rule_extensions),
            }[slot]
            if kind not in supported or kind not in registry:
                raise Invalid("UNSUPPORTED_KIND", at + 1)
            types = registry[kind][1] if slot in (SLOT_CONDITION, SLOT_NEGATED) else registry[kind]
            if length != param_length(types):
                raise Invalid("INVALID_LENGTH", at)
            if at + 2 + length > end:
                raise Invalid("INVALID_LENGTH", at)
            param_at = at + 2
            for kind_type in types:
                check_param(kind_type, data, param_at, limits, counts)
                param_at += TYPE_BYTES.get(kind_type, 1)
            at += 2 + length
        if SLOT_TARGET not in singles or SLOT_PAINT not in singles:
            raise Invalid("INVALID_STRUCTURE", offset)
        offset = end
    if offset + 1 > len(data):
        raise Invalid("TRUNCATED", offset)
    extensions = data[offset]
    offset += 1
    previous_kind = -1
    for _ in range(extensions):
        if offset + 2 > len(data):
            raise Invalid("TRUNCATED", offset)
        kind, length = data[offset], data[offset + 1]
        if kind not in limits.domain_extensions or kind not in DOMAIN_EXTENSIONS:
            raise Invalid("UNSUPPORTED_KIND", offset)
        if kind <= previous_kind:
            raise Invalid("NONCANONICAL_ORDER", offset)
        previous_kind = kind
        if length != DOMAIN_EXTENSIONS[kind]:
            raise Invalid("INVALID_LENGTH", offset + 1)
        if offset + 2 + length > len(data):
            raise Invalid("TRUNCATED", offset)
        body = offset + 2
        if kind == 1:
            if data[body] & 0xFE:
                raise Invalid("RESERVED_BITS", body)
            if data[body + 1] > 2:
                raise Invalid("INVALID_PARAMETER", body + 1)
            if data[body + 2] >= counts["P"]:
                raise Invalid("INVALID_REFERENCE", body + 2)
            if data[body + 3] > counts["R"]:
                raise Invalid("INVALID_REFERENCE", body + 3)
        elif kind == 2:
            if data[body] != 1:
                raise Invalid("INVALID_PARAMETER", body)
        offset = body + length
    if offset != len(data):
        raise Invalid("TRAILING_BYTES", offset)
    return decode(data, limits)


def decode(data: bytes, limits: Limits) -> dict:
    """Parse a payload already known to be valid."""
    counts = list(data[3:8])
    offset = 8
    palette = [list(data[offset + 3 * i: offset + 3 * i + 3]) for i in range(counts[0])]
    offset += 3 * counts[0]
    things = {}
    for _ in range(counts[1]):
        kind, records = data[offset], data[offset + 1]
        size = limits.record_size(kind)
        offset += 2
        default = list(data[offset: offset + size])
        offset += size
        rows = []
        for _ in range(records):
            rows.append(list(data[offset: offset + size]))
            offset += size
        things[kind] = {"default": default, "records": rows}
    key_sets = []
    for _ in range(counts[2]):
        key_sets.append(frozenset(led for led in range(LED_COUNT) if data[offset + led // 8] >> (led % 8) & 1))
        offset += 8
    maps = []
    for _ in range(counts[3]):
        values = {}
        if data[offset] == 0:
            for led in range(LED_COUNT):
                values[led] = data[offset + 1 + led]
            offset += 1 + LED_COUNT
        else:
            entries = data[offset + 1]
            for entry in range(entries):
                values[data[offset + 2 + 2 * entry]] = data[offset + 3 + 2 * entry]
            offset += 2 + 2 * entries
        maps.append(values)
    rules = []
    for _ in range(counts[4]):
        size = data[offset]
        end = offset + 1 + size
        item = {"flags": data[offset + 1], "brightness": data[offset + 2], "conditions": [], "target": None, "paint": None, "background": None, "timings": []}
        at = offset + 3
        while at < end:
            slot, length, kind = data[at] >> 5, data[at] & 31, data[at + 1]
            params = list(data[at + 2: at + 2 + length])
            if slot in (SLOT_CONDITION, SLOT_NEGATED):
                item["conditions"].append((slot == SLOT_NEGATED, kind, params))
            elif slot == SLOT_TARGET:
                item["target"] = (kind, params)
            elif slot == SLOT_PAINT:
                item["paint"] = (kind, params)
            elif slot == SLOT_BACKGROUND:
                item["background"] = (kind, params)
            elif slot == SLOT_TIMING:
                item["timings"].append((kind, params))
            at += 2 + length
        rules.append(item)
        offset = end
    extensions = {}
    for _ in range(data[offset]):
        kind, length = data[offset + 1], data[offset + 2]
        extensions[kind] = list(data[offset + 3: offset + 3 + length])
        offset += 2 + length
    return {"palette": palette, "things": things, "keySets": key_sets, "maps": maps, "rules": rules, "extensions": extensions}


# ─── Rendering ──────────────────────────────────────────────────────────────

def hsv_to_rgb(h: int, s: int, v: int) -> tuple:
    """QMK's hsv_to_rgb without the CIE curve."""
    if s == 0:
        return (v, v, v)
    region = h * 6 // 255
    remainder = (h * 2 - region * 85) * 3
    p = (v * (255 - s)) >> 8
    q = (v * (255 - ((s * remainder) >> 8))) >> 8
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8
    if region in (0, 6):
        return (v, t, p)
    if region == 1:
        return (q, v, p)
    if region == 2:
        return (p, v, t)
    if region == 3:
        return (p, q, v)
    if region == 4:
        return (t, p, v)
    return (v, p, q)


def mix(below, colour, amount):
    return tuple((b * (255 - amount) + c * amount + 127) // 255 for b, c in zip(below, colour))


def scale(colour, level):
    return tuple((c * level + 127) // 255 for c in colour)


STATE_SLOT = {"hold": 0, "long": 1, "tap": 2}
STATE_BIT = {"branch": 0, "tap": 1, "hold": 2, "long": 3}


def subject(kind, thing, slot=0, keys=None, halves=None):
    return {"kind": kind, "thing": thing, "slot": slot, "keys": frozenset(keys or ()), "halves": frozenset(halves or ())}


def native_identity(code, layers=16):
    """Direct native action only; no authored branch or host-character guess."""
    for lo, hi in ((0x5220, 0x523f), (0x5200, 0x521f), (0x5260, 0x527f), (0x5280, 0x529f), (0x52c0, 0x52df)):
        if lo <= code <= hi:
            return (1, code-lo) if code-lo < layers else None
    if 0x4000 <= code <= 0x4fff:
        return (1, (code >> 8) & 15)
    if 0x5000 <= code <= 0x51ff:
        return (1, (code >> 5) & 15)
    if 0x7ec0 <= code < 0x7ec0+layers:
        return (1, code-0x7ec0)
    if 0x7e80 <= code <= 0x7ebf:
        return (2, (code-0x7e80) % 32)
    if 0x7700 <= code <= 0x777f:
        return (5, code-0x7700)
    return None


def key_query(code, behaviour, query, selector, layer, combos):
    basic = code & 255 if 0x100 <= code <= 0x1fff else code
    if query == 0: return code not in (0, 1)
    if query == 1: return code == 1
    if query == 2: return code == 0
    if query == 3: return 0xe0 <= basic <= 0xe7 or 0x100 <= code <= 0x3fff or 0x5000 <= code <= 0x51ff or 0x52a0 <= code <= 0x52bf
    if query == 4: return 4 <= basic <= 0x1d
    if query == 5: return 0x1e <= basic <= 0x27 or 0x59 <= basic <= 0x62
    if query == 6: return 0x2d <= basic <= 0x38 or 0x54 <= basic <= 0x57 or basic in (0x64, 0x67, 0x85)
    if query == 7: return 0x49 <= basic <= 0x52 or basic in (0x2a, 0x2b, 0x28, 0x29, 0x58)
    if query == 8: return 0xa5 <= basic <= 0xbe
    if query == 9: return 0xcd <= basic <= 0xdf
    if query in (10, 11, 12):
        identity = native_identity(code)
        return bool(identity and identity[0] == (1, 2, 5)[query-10] and (selector == ANY or selector == identity[1]))
    if query == 13: return 0x7f00 <= code <= 0x7f7f
    if query == 14: return behaviour is not None and (selector == ANY or selector == behaviour)
    if query == 15:
        return any((selector == ANY or int(row) == selector) and combo.get("enabled", True) and layer in combo.get("layers", list(range(16))) and code in combo["inputs"] for row, combo in combos.items())
    raise AssertionError(query)


class Situation:
    """The facts for one render, in the contract's terms."""

    def __init__(self, data: dict, limits: Limits):
        self.limits = limits
        self.data = data
        self.layers = set(data.get("layers", [])) | {0}
        self.mapped = {int(layer): frozenset(keys) for layer, keys in data.get("mapped", {}).items()}
        self.preview = data.get("preview")
        self.mode = data.get("mode")
        self.combos = data.get("combos", {})
        self.behaviour = data.get("behaviour", {"keys": {}, "owners": {}})
        self.effect = {int(led): tuple(bytes.fromhex(colour)) for led, colour in data.get("effect", {}).items()}
        self.time = data.get("time", 0)
        self.fade = data.get("fade")

    def without_layer(self, layer: int) -> "Situation":
        other = Situation.__new__(Situation)
        other.__dict__.update(self.__dict__)
        other.layers = (self.layers - {layer}) | {0}
        return other

    def shown(self, key):
        entry = self.behaviour["keys"].get(str(key))
        if not entry or (entry.get("flashing") and not entry.get("lit")):
            return None
        return entry

    def branch_slot(self, branch: int) -> int:
        index = 0 if branch <= 2 else min(branch - 2, self.limits.tap_depth - 2)
        return 3 + index

    def behaviour_subject(self, key, entry, halves=None):
        slot = self.branch_slot(entry.get("branch", 0)) if entry["state"] == "branch" else STATE_SLOT[entry["state"]]
        return subject(4, ANY, slot, {key}, halves if halves is not None else {key_half(key)})

    def subjects(self, kind: int, params) -> list:
        if kind == 9:
            owners = {int(k): layer for k, layer in self.data.get("layerOwners", {}).items()}
            matches = [max(self.layers)] if params[1] == 0 else set(owners.values()) if params[1] == 1 else self.data.get("lockedLayers", []) if params[1] == 2 else [self.data.get("oneshotLayer")]
            return [subject(1, layer, 0, [key for key, owned in owners.items() if owned == layer], {key_half(key) for key, owned in owners.items() if owned == layer}) for layer in sorted((l for l in matches if l is not None), reverse=True) if params[0] in (ANY, layer)]
        if kind == 10:
            mode = self.data.get("modeStatus")
            if not mode or (params[1] == 1) != bool(mode.get("locked")) or params[0] not in (ANY, mode["id"]): return []
            return [subject(2, mode["id"], 0, mode.get("keys", []), mode.get("halves", []))]
        if kind == 11:
            return [subject(3, item["id"], 0, item["keys"], {key_half(key) for key in item["keys"]}) for item in self.data.get("comboLayers", []) if params[0] in (ANY, item["id"]) and params[1] in (ANY, item["layer"])]
        if kind == 12:
            found = []
            for key, item in sorted(self.data.get("behaviourPhases", {}).items(), key=lambda kv: int(kv[0])):
                if params[0] not in (ANY, item["id"]) or params[1] not in item["phases"] or params[2] not in (ANY, item["branch"]): continue
                slot = 1 if params[1] == 4 else 0 if params[1] in (2, 3) else 2 if params[1] == 1 else self.branch_slot(item["branch"])
                found.append(subject(4, item["id"], slot, [int(key)], [key_half(int(key))]))
            return found
        if kind == 13:
            slot = self.data.get("macroPlaying")
            return [subject(5, slot)] if slot is not None and params[0] in (ANY, slot) else []
        if kind == 14:
            return [subject(7, key, 0, [key], [key_half(key)]) for key in sorted(self.data.get("pressedKeys", [])) if params[0] in (ANY, key)]
        if kind == 15:
            mods = self.data.get("modifiers", [0, 0])[params[2]]
            return [subject(6, params[0] + side * 4) for side in (0, 1) if params[1] in (2, side) and mods & (1 << (params[0] + side * 4))]
        if kind in (16, 17, 18):
            holds = self.data.get("hostLocks", 0) & (1 << params[0]) if kind == 16 else self.data.get("pointer", 0) & (1 << params[0]) if kind == 17 else self.data.get("idle", False)
            return [subject(0, ANY)] if holds else []
        if kind == 1:
            return [subject(1, layer) for layer in sorted(self.layers, reverse=True) if params[0] in (ANY, layer)]
        if kind == 2:
            return [subject(1, self.preview)] if self.preview is not None and params[0] in (ANY, self.preview) else []
        if kind == 3:
            if not self.mode or params[0] not in (ANY, self.mode["id"]):
                return []
            return [subject(2, self.mode["id"], 0, self.mode.get("keys", []), self.mode.get("halves", []))]
        if kind == 4:
            keys = set()
            if params[0] in (0, ANY):
                keys |= set(self.combos.get("owning", []))
            if params[0] in (1, ANY):
                keys |= set(self.combos.get("others", []))
            return [subject(3, ANY, 0, keys, {key_half(k) for k in keys})] if keys else []
        if kind == 7:
            return [subject(3, item["id"], 0, item["keys"], {key_half(k) for k in item["keys"]}) for item in self.combos.get("instances", []) if params[0] in (ANY, item["id"]) and params[1] in (ANY, item["role"])]
        if kind == 8:
            found = []
            covered = set(self.combos.get("owning", [])) | set(self.combos.get("others", []))
            for key in sorted(int(k) for k in self.behaviour["keys"]):
                entry = self.shown(key)
                if not entry or "id" not in entry or params[0] not in (ANY, entry["id"]) or not (params[1] >> STATE_BIT[entry["state"]] & 1):
                    continue
                if entry["state"] == "branch" and key in covered:
                    continue
                subj = self.behaviour_subject(key, entry)
                subj["thing"] = entry["id"]
                subj["outcome"] = entry.get("outcome", 0)
                found.append(subj)
            return found
        if kind == 5:
            mask, scope = params
            owners = self.behaviour.get("owners", {})
            found = []
            if scope == 0:
                for key in sorted(int(k) for k in self.behaviour["keys"]):
                    entry = self.shown(key)
                    if entry and mask >> STATE_BIT[entry["state"]] & 1:
                        found.append(self.behaviour_subject(key, entry))
            elif scope == 1:
                for half in (LEFT, RIGHT):
                    key = owners.get(half)
                    entry = self.shown(key) if key is not None else None
                    if entry and mask >> STATE_BIT[entry["state"]] & 1:
                        found.append(self.behaviour_subject(key, entry, {half}))
            elif scope == 2:
                key = owners.get("board")
                entry = self.shown(key) if key is not None else None
                if entry and mask >> STATE_BIT[entry["state"]] & 1:
                    found.append(self.behaviour_subject(key, entry))
            else:
                for state in ("long", "hold", "tap", "branch"):
                    key = owners.get(state)
                    entry = self.shown(key) if key is not None else None
                    if mask >> STATE_BIT[state] & 1 and entry and entry["state"] == state:
                        found.append(self.behaviour_subject(key, entry))
            return found
        raise AssertionError(kind)


class Renderer:
    def __init__(self, domain: dict, limits: Limits):
        self.domain = domain
        self.limits = limits
        self.rgb = [hsv_to_rgb(*colour) for colour in domain["palette"]]
        self.gates: dict = {}
        self.seen: set = set()
        self.observed_time = 0
        self.clock_sample = None

    def record_field(self, subj, field: int):
        table = self.domain["things"].get(subj["kind"])
        if table is None:
            return None
        record = table["records"][subj["thing"]] if subj["thing"] != ANY and subj["thing"] < len(table["records"]) else table["default"]
        value = record[field]
        return table["default"][field] if value == INHERIT else value

    def target(self, kind: int, params, subj, situation) -> frozenset:
        if kind == 0:
            return ALL_LEDS
        if kind in (1, 2):
            return HALF_LEDS[LEFT if kind == 1 else RIGHT]
        if kind == 3:
            return self.domain["keySets"][params[0]]
        if kind == 4:
            return key_leds(subj["keys"]) if subj["keys"] else ALL_LEDS
        if kind == 5:
            return frozenset().union(*(HALF_LEDS[h] for h in subj["halves"])) if subj["halves"] else ALL_LEDS
        if kind == 6:
            return key_leds(situation.mapped.get(subj["thing"], ())) if subj["kind"] == 1 else frozenset()
        if kind == 8:
            layer = subj["thing"] if subj["kind"] == 1 else max(situation.layers)
            keymap = situation.data.get("keymap", {}).get(str(layer), {})
            return key_leds(key for key in range(MATRIX_ROWS*MATRIX_COLS) if key_query(keymap.get(str(key), {}).get("code", 4 if key in situation.mapped.get(layer, ()) else 0), keymap.get(str(key), {}).get("behaviour"), params[0], params[1], layer, situation.data.get("queryCombos", {})))
        if kind == 7:
            place = self.record_field(subj, 0)
            return frozenset() if place is None else self.target(place, [], subj, situation)
        raise AssertionError(kind)

    def paint(self, kind: int, params, subj, led: int):
        """Return None (clear), ('colour', rgb) or ('below', f)."""
        if kind == 0:
            return None
        if kind == 1:
            return ("colour", (0, 0, 0))
        if kind == 2:
            return ("colour", self.rgb[params[0]])
        if kind == 3:
            ref = self.record_field(subj, 1 + subj["slot"])
            return None if ref is None or ref == NONE else ("colour", self.rgb[ref])
        if kind == 9:
            layer = subj["thing"] if subj["kind"] == 1 else max(self.situation.layers)
            key = next((i for i, value in enumerate(KEY_LEDS) if value == led), None)
            item = self.situation.data.get("keymap", {}).get(str(layer), {}).get(str(key), {})
            identity = native_identity(item.get("code", 0))
            if identity is None: return None
            ref = self.record_field(subject(*identity), 1)
            return None if ref is None or ref == NONE else ("colour", self.rgb[ref])
        if kind == 8:
            outcome = subj.get("outcome", 0)
            if subj["kind"] != 4 or subj["slot"] > 1:
                return None
            kind, thing = (1, outcome - 1) if 1 <= outcome <= 16 else (2, outcome - 33) if 33 <= outcome <= 64 else (0, 0)
            if kind == 0:
                return None
            ref = self.record_field(subject(kind, thing), 1)
            return None if ref is None or ref == NONE else ("colour", self.rgb[ref])
        if kind == 4:
            value = self.domain["maps"][params[0]].get(led, 0x80)
            return ("colour", self.rgb[value]) if value < 0x80 else self.paint(value - 0x80, [], subj, led)
        if kind == 5:
            return ("below", lambda below: scale(below, params[0]))
        if kind == 6:
            return ("below", lambda below: mix(below, self.rgb[params[0]], params[1]))
        raise AssertionError(kind)

    def plan(self, situation, first: int, end: int, timed: bool) -> list:
        """Each enabled rule's subjects with their targets and gate amounts.

        Gate state is kept per (rule, subject thing) while the subject exists;
        the fade's end scene reads it without starting any."""
        entries = []
        for index in range(first, end):
            item = self.domain["rules"][index]
            if item["flags"] & 1:
                continue
            conditions = item["conditions"]
            subjects = [subject(0, ANY)]
            filters = conditions
            if conditions and not conditions[0][0]:
                subjects = situation.subjects(conditions[0][1], conditions[0][2])
                filters = conditions[1:]
            if any(bool(situation.subjects(kind, params)) == negated for negated, kind, params in filters):
                subjects = []
            for subj in subjects:
                amount = 255
                for kind, params in item["timings"]:
                    key = (index, subj["kind"], subj["thing"])
                    since = self.gates.setdefault(key, self.observed_time) if timed else self.gates.get(key, self.observed_time)
                    if timed:
                        self.seen.add(key)
                    if kind == 1:
                        amount = min(amount, 255 if self.observed_time - since >= (params[0] | params[1] << 8) else 0)
                entries.append((index, item, subj, amount, self.target(item["target"][0], item["target"][1], subj, situation)))
        return entries

    def walk(self, entries, at: int, led: int, situation, depth: int, bottom):
        while at < len(entries):
            index, item, subj, amount, target = entries[at]
            at += 1
            if amount == 0:
                continue
            self.situation = situation
            value = self.paint(*item["paint"], subj, led) if led in target else None
            if value is None and item["background"] is not None:
                value = self.paint(*item["background"], subj, led)
            if value is None:
                continue
            needs_below = value[0] == "below" or amount < 255
            if needs_below:
                below = self.walk(entries, at, led, situation, depth + 1, bottom)
                below = below if below is not None else situation.effect.get(led, (0, 0, 0))
            colour = value[1](below) if value[0] == "below" else value[1]
            colour = scale(colour, item["brightness"]) if item["brightness"] != 255 else colour
            if amount < 255:
                colour = mix(below, colour, amount)
            return colour
        return bottom(led) if bottom else None

    def render(self, situation) -> list:
        self.situation = situation
        # Gate time is a per-half local observation clock. Defensively ignore
        # backward samples; a 32-bit wrap advances by its modular delta.
        now = situation.time & 0xffffffff
        if self.clock_sample is None:
            self.clock_sample = now
        elif now >= self.clock_sample or self.clock_sample - now >= 0x7fffffff:
            self.observed_time += (now - self.clock_sample) & 0xffffffff
            self.clock_sample = now
        rules = self.domain["rules"]
        fade = self.domain["extensions"].get(1)
        fading = bool(fade and fade[0] & 1 and situation.fade and situation.fade["autoMouseLayer"] in situation.layers and situation.fade["progress"] != 0)
        scene_first = len(rules) - fade[3] if fading else len(rules)
        self.seen = set()
        upper = self.plan(situation, 0, scene_first, True)
        bottom = None
        if fading:
            start_plan = self.plan(situation, scene_first, len(rules), True)
            end_plan = self.plan(situation.without_layer(situation.fade["autoMouseLayer"]), scene_first, len(rules), False)
            span = situation.fade["timeout"] - situation.fade["deadTime"]
            progress = min(situation.fade["progress"], span)
            blend = 255 if progress >= span else progress * 255 // span
            end_colour = self.rgb[fade[2]]

            def bottom(led):
                start = self.walk(start_plan, 0, led, situation, 0, None)
                if fade[1] == 2:
                    end = end_colour
                else:
                    end = self.walk(end_plan, 0, led, situation, 0, None)
                    if end is None:
                        end = end_colour if fade[1] == 1 else situation.effect.get(led)
                if start is not None and end is not None:
                    return mix(start, end, blend)
                if start is not None:
                    return None if blend == 255 else start
                if end is not None and blend != 0:
                    return end
                return None
        frame = [self.walk(upper, 0, led, situation, 0, bottom) for led in range(LED_COUNT)]
        self.gates = {key: since for key, since in self.gates.items() if key in self.seen}
        return frame


def frame_text(frame) -> list:
    return ["-" if colour is None else "%02x%02x%02x" % colour for colour in frame]


# ─── Version 4 → version 5 ──────────────────────────────────────────────────

def parse_v4(data: bytes) -> dict:
    counts = {"groups": data[4], "layers": data[5], "layerRows": data[6], "pd": data[7], "pdRows": data[8], "comboRows": data[9], "branches": data[10], "keyRows": data[11]}
    offset = 16
    out = {"stages": data[2] | data[3] << 8}
    out["groups"] = []
    for _ in range(counts["groups"]):
        out["groups"].append(frozenset(led for led in range(LED_COUNT) if data[offset + 1 + led // 8] >> (led % 8) & 1))
        offset += 9

    def rows(count, size):
        nonlocal offset
        found = []
        for _ in range(count):
            found.append(list(data[offset: offset + size]))
            offset += size
        return found
    out["layers"] = rows(counts["layers"], 5)
    out["layerRows"] = rows(counts["layerRows"], 5)
    out["automouse"] = list(data[offset: offset + 4])
    offset += 4
    out["pd"] = rows(counts["pd"], 5)
    out["pdRows"] = rows(counts["pdRows"], 5)
    out["combo"] = list(data[offset: offset + 4])
    offset += 4
    out["comboRows"] = rows(counts["comboRows"], 4)
    out["branches"] = rows(counts["branches"], 3)
    out["key"] = list(data[offset: offset + 11])
    offset += 11
    out["keyRows"] = rows(counts["keyRows"], 5)
    assert offset == len(data), "version-4 payload length"
    return out


LOCALITY_PLACE = {0: 0, 1: 1, 2: 2, 3: 5, 4: 4}


def convert_v4(data: bytes, limits: Limits) -> bytes:
    v4 = parse_v4(data)
    stages = v4["stages"]
    palette: list = []
    rules: list = []
    things: list = []

    def supported(entry):
        slot, kind = (SLOT_PAINT, 2) if entry[0] == "colour" else (entry[0], entry[1])
        registry = {0: limits.facts, 1: limits.facts, 2: limits.targets, 3: limits.paints, 4: limits.paints, 5: limits.timings, 6: limits.rule_extensions}[slot]
        return kind in registry

    def colour_part(hsv):
        # A group row's 0,0,0 inherits the stage colour: the subject's identity.
        return part(SLOT_PAINT, 3) if hsv == [0, 0, 0] else ["colour", hsv]

    def add(condition, target_part, paint_part, disabled):
        parts = [condition, target_part, paint_part]
        if all(supported(entry) for entry in parts):
            rules.append({"flags": 1 if disabled else 0, "brightness": 255, "parts": parts})

    key_present = len(v4["branches"]) > 0
    if key_present:
        scope = 0 if v4["key"][10] == 4 else 1 if v4["key"][10] == 3 else 2
        off = not stages & 0x10
        specific = [r for r in v4["keyRows"] if r[0] != 0xFF]
        everything = [r for r in v4["keyRows"] if r[0] == 0xFF]
        for row in reversed(specific):
            add(part(SLOT_CONDITION, 5, 1 << row[0], 3), part(SLOT_TARGET, 3, row[4]), colour_part(row[1:4]), off)
        for row in reversed(everything):
            add(part(SLOT_CONDITION, 5, 0x0F, 3), part(SLOT_TARGET, 3, row[4]), colour_part(row[1:4]), off)
        add(part(SLOT_CONDITION, 5, 0x0F, scope), part(SLOT_TARGET, 7), part(SLOT_PAINT, 3), off)

    def combo_rules(role):
        off = not stages & 0x08
        for row in reversed(v4["comboRows"]):
            add(part(SLOT_CONDITION, 4, role), part(SLOT_TARGET, 3, row[3]), colour_part(row[0:3]), off)
        add(part(SLOT_CONDITION, 4, role), part(SLOT_TARGET, 7), part(SLOT_PAINT, 3), off)

    combo_rules(1)
    if v4["pd"]:
        off = not stages & 0x04
        for row in reversed(v4["pdRows"]):
            add(part(SLOT_CONDITION, 3, row[0]), part(SLOT_TARGET, 3, row[4]), colour_part(row[1:4]), off)
        add(part(SLOT_CONDITION, 3, ANY), part(SLOT_TARGET, 7), part(SLOT_PAINT, 3), off)
    if v4["layers"]:
        off = not stages & 0x01
        for row in reversed(v4["layerRows"]):
            add(part(SLOT_CONDITION, 2, row[0]), part(SLOT_TARGET, 3, row[4]), colour_part(row[1:4]), off)
        add(part(SLOT_CONDITION, 2, ANY), part(SLOT_TARGET, 7), part(SLOT_PAINT, 3), off)
    combo_rules(0)
    base = 0
    if v4["layers"]:
        off = not stages & 0x01
        before = len(rules)
        for row in reversed(v4["layerRows"]):
            add(part(SLOT_CONDITION, 1, row[0]), part(SLOT_TARGET, 3, row[4]), colour_part(row[1:4]), off)
        add(part(SLOT_CONDITION, 1, ANY), part(SLOT_TARGET, 7), part(SLOT_PAINT, 3), off)
        base = len(rules) - before

    # Thing tables, colours as HSV until the palette is assigned.
    if v4["layers"] and 1 in limits.things:
        records = [[0 if row[4] == 0 else 6, NONE if row[2] == 0 and row[3] == 0 else row[1:4]] for row in v4["layers"]]
        things.append({"kind": 1, "default": [6, NONE], "records": records})
    if v4["pd"] and 2 in limits.things:
        things.append({"kind": 2, "default": [0, NONE], "records": [[LOCALITY_PLACE[row[4]], row[1:4]] for row in v4["pd"]]})
    if 3 in limits.things:
        things.append({"kind": 3, "default": [LOCALITY_PLACE[v4["combo"][3]], v4["combo"][0:3]], "records": []})
    if key_present and 4 in limits.things:
        if len(v4["branches"]) != limits.tap_depth - 1:
            raise ValueError("tap-branch colours do not match the target's tap depth")
        key = v4["key"]
        things.append({"kind": 4, "default": [LOCALITY_PLACE[key[10]], key[3:6], key[6:9], key[0:3]] + [b for b in v4["branches"]], "records": []})

    extensions = []
    if 1 in limits.domain_extensions:
        extensions.append([1, ["flags", stages >> 1 & 1, v4["automouse"][0], v4["automouse"][1:4], base]])
    if 2 in limits.domain_extensions and key_present and stages & 0x10 and v4["key"][9] == 1:
        extensions.append([2, [1]])

    def pal(hsv):
        if hsv == NONE:
            return NONE
        if hsv not in palette:
            palette.append(list(hsv))
        return palette.index(list(hsv))

    for table in things:
        table["default"] = [table["default"][0]] + [pal(c) for c in table["default"][1:]]
        table["records"] = [[r[0]] + [pal(c) for c in r[1:]] for r in table["records"]]
    for item in rules:
        resolved = []
        for entry in item["parts"]:
            resolved.append(part(SLOT_PAINT, 2, pal(entry[1])) if entry[0] == "colour" else entry)
        item["parts"] = resolved
    for extension in extensions:
        if extension[0] == 1:
            flags, mode, end, base_count = extension[1][1:]
            extension[1] = [flags, mode, pal(end), base_count]
    domain = {"palette": palette, "things": things, "keySets": [sorted(g) for g in v4["groups"]], "maps": [], "rules": rules, "extensions": extensions}
    return encode(domain)


def encode_v4(stages=0x1F, groups=(), layers=(), layer_rows=(), automouse=(0, 0, 0, 0), pd=(), pd_rows=(), combo=(0, 0, 0, 0), combo_rows=(), branches=(), key=(0,) * 11, key_rows=()) -> bytes:
    """A version-4 payload from its rows (rgb-domain-v1.md), for conversion vectors."""
    header = bytes([4, 0, stages & 0xFF, stages >> 8, len(groups), len(layers), len(layer_rows), len(pd), len(pd_rows), len(combo_rows), len(branches), len(key_rows), LED_COUNT, 8, 0, 0])
    out = bytearray(header)
    for index, leds in enumerate(groups):
        bitmap = bytearray(8)
        for led in leds:
            bitmap[led // 8] |= 1 << (led % 8)
        out += bytes([index]) + bitmap
    for rows in (layers, layer_rows):
        for row in rows:
            out += bytes(row)
    out += bytes(automouse)
    for rows in (pd, pd_rows):
        for row in rows:
            out += bytes(row)
    out += bytes(combo)
    for rows in (combo_rows, branches):
        for row in rows:
            out += bytes(row)
    out += bytes(key)
    for row in key_rows:
        out += bytes(row)
    return bytes(out)


# ─── Vectors ────────────────────────────────────────────────────────────────

RED, GREEN, BLUE, WHITE, YELLOW, PURPLE, CYAN, ORANGE, BLACK = range(9)
PALETTE = [[0, 255, 200], [85, 255, 200], [170, 255, 200], [0, 0, 150], [43, 255, 200], [200, 255, 200], [127, 255, 200], [21, 255, 200], [0, 0, 0]]
THUMBS = [24, 25, 26, 27, 28, 53, 54, 55]
C = SLOT_CONDITION
N = SLOT_NEGATED
T = SLOT_TARGET
P = SLOT_PAINT
B = SLOT_BACKGROUND
G = SLOT_TIMING


def layer_table(records, default=(6, NONE)):
    return {"kind": 1, "default": list(default), "records": [list(r) for r in records]}


BEHAVIOUR_TABLE = {"kind": 4, "default": [5, ORANGE, CYAN, WHITE, BLUE, PURPLE, GREEN, YELLOW], "records": []}


def render_case(name, entry, domain, situations, limits):
    """One render vector: a domain and its situations (a sequence when timed)."""
    payload = encode(domain)
    renderer = Renderer(validate(payload, limits), limits)
    steps = []
    for situation in situations:
        frame = renderer.render(Situation(situation, limits))
        steps.append({"situation": situation, "expect": frame_text(frame)})
    return {"name": name, "entry": entry, "hex": payload.hex(), "steps": steps}


def render_vectors(limits):
    cases = []
    codes = [0,1,4,0x1d,0x1e,0x62,0x2d,0x49,0xa8,0xcd,0xe7,0x204,0x2104,0x52a1,0x522f,0x5263,0x5282,0x52c1,0x5021,0x4304,0x7ecf,0x7e9f,0x7ebf,0x777f,0x7f7f,0x7e00]
    keymap = {"3": {str(key): {"code": code, **({"behaviour": 127} if key in (2,24) else {})} for key,code in enumerate(codes)}}
    qs = {"layers": [3], "keymap": keymap, "queryCombos": {"127": {"inputs": [4,0x7f7f], "layers": [3]}}}
    changed = {**qs, "keymap": {"3": {"31": {"code": 0x7e9f, "behaviour": 0}}}, "queryCombos": {"127": {"inputs": [0x7e9f], "layers": [2]}}}
    for query in range(16):
        selectors = [ANY, 127, 0, 15, 31] if query in (10,11,12,14,15) else [ANY]
        for selector in selectors:
            cases.append(render_case(f"keys-that-{query}-{selector}", "target:8", {"palette": PALETTE, "rules": [rule(part(C,1,3),part(T,8,query,selector),part(P,2,GREEN))]}, [qs,changed], limits))
    identity_tables = [{"kind": kind, "default": [0,colour], "records": []} for kind,colour in ((1,RED),(2,GREEN),(5,WHITE))]
    cases.append(render_case("what-it-does", "paint:9", {"palette": PALETTE, "things": identity_tables, "rules": [rule(part(T,0),part(P,2,BLUE)),rule(part(C,1,3),part(T,0),part(P,9))]}, [qs,changed], limits))
    cases.append(render_case("phase3-fact-9", "fact:9", {"palette": PALETTE, "rules": [rule(part(C, 9, *[255, 1]), part(T, 4), part(P, 2, RED))]}, [{}, {'layers': [1, 3], 'layerOwners': {'0': 1, '31': 3}, 'lockedLayers': [1, 3], 'oneshotLayer': 3}], limits))
    cases.append(render_case("phase3-negated-9", "fact:9", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 9, *[255, 1]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'layers': [1, 3], 'layerOwners': {'0': 1, '31': 3}, 'lockedLayers': [1, 3], 'oneshotLayer': 3}], limits))
    cases.append(render_case("phase3-fact-10", "fact:10", {"palette": PALETTE, "rules": [rule(part(C, 10, *[255, 0]), part(T, 4), part(P, 2, RED))]}, [{}, {'modeStatus': {'id': 2, 'locked': False, 'keys': [0, 31], 'halves': ['left', 'right']}}], limits))
    cases.append(render_case("phase3-negated-10", "fact:10", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 10, *[255, 0]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'modeStatus': {'id': 2, 'locked': False, 'keys': [0, 31], 'halves': ['left', 'right']}}], limits))
    cases.append(render_case("phase3-fact-11", "fact:11", {"palette": PALETTE, "rules": [rule(part(C, 11, *[255, 255]), part(T, 4), part(P, 2, RED))]}, [{}, {'comboLayers': [{'id': 2, 'layer': 3, 'keys': [0, 31]}]}], limits))
    cases.append(render_case("phase3-negated-11", "fact:11", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 11, *[255, 255]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'comboLayers': [{'id': 2, 'layer': 3, 'keys': [0, 31]}]}], limits))
    cases.append(render_case("phase3-fact-12", "fact:12", {"palette": PALETTE, "rules": [rule(part(C, 12, *[255, 0, 255]), part(T, 4), part(P, 2, RED))]}, [{}, {'behaviourPhases': {'0': {'id': 2, 'phases': [0, 1], 'branch': 3}, '31': {'id': 3, 'phases': [2, 3, 4], 'branch': 2}}}], limits))
    cases.append(render_case("phase3-negated-12", "fact:12", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 12, *[255, 0, 255]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'behaviourPhases': {'0': {'id': 2, 'phases': [0, 1], 'branch': 3}, '31': {'id': 3, 'phases': [2, 3, 4], 'branch': 2}}}], limits))
    cases.append(render_case("phase3-fact-13", "fact:13", {"palette": PALETTE, "rules": [rule(part(C, 13, *[255]), part(T, 4), part(P, 2, RED))]}, [{}, {'macroPlaying': 127}], limits))
    cases.append(render_case("phase3-negated-13", "fact:13", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 13, *[255]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'macroPlaying': 127}], limits))
    cases.append(render_case("phase3-fact-14", "fact:14", {"palette": PALETTE, "rules": [rule(part(C, 14, *[255]), part(T, 4), part(P, 2, RED))]}, [{}, {'pressedKeys': [0, 31]}], limits))
    cases.append(render_case("phase3-negated-14", "fact:14", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 14, *[255]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'pressedKeys': [0, 31]}], limits))
    cases.append(render_case("phase3-fact-15", "fact:15", {"palette": PALETTE, "rules": [rule(part(C, 15, *[1, 2, 0]), part(T, 4), part(P, 2, RED))]}, [{}, {'modifiers': [34, 68]}], limits))
    cases.append(render_case("phase3-negated-15", "fact:15", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 15, *[1, 2, 0]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'modifiers': [34, 68]}], limits))
    cases.append(render_case("phase3-fact-16", "fact:16", {"palette": PALETTE, "rules": [rule(part(C, 16, *[0]), part(T, 4), part(P, 2, RED))]}, [{}, {'hostLocks': 7}], limits))
    cases.append(render_case("phase3-negated-16", "fact:16", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 16, *[0]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'hostLocks': 7}], limits))
    cases.append(render_case("phase3-fact-17", "fact:17", {"palette": PALETTE, "rules": [rule(part(C, 17, *[0]), part(T, 4), part(P, 2, RED))]}, [{}, {'pointer': 15}], limits))
    cases.append(render_case("phase3-negated-17", "fact:17", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 17, *[0]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'pointer': 15}], limits))
    cases.append(render_case("phase3-fact-18", "fact:18", {"palette": PALETTE, "rules": [rule(part(C, 18, *[]), part(T, 4), part(P, 2, RED))]}, [{}, {'idle': True}], limits))
    cases.append(render_case("phase3-negated-18", "fact:18", {"palette": PALETTE, "rules": [rule(part(SLOT_NEGATED, 18, *[]), part(T, 0), part(P, 2, GREEN))]}, [{}, {'idle': True}], limits))
    cases.append(render_case("layer-status-0", "fact:9", {"palette": PALETTE, "rules": [rule(part(C, 9, 255, 0), part(T, 4), part(P, 2, RED))]}, [{'layers': [1, 3], 'layerOwners': {'0': 1, '31': 3}, 'lockedLayers': [1, 3], 'oneshotLayer': 3}], limits))
    cases.append(render_case("layer-status-1", "fact:9", {"palette": PALETTE, "rules": [rule(part(C, 9, 255, 1), part(T, 4), part(P, 2, RED))]}, [{'layers': [1, 3], 'layerOwners': {'0': 1, '31': 3}, 'lockedLayers': [1, 3], 'oneshotLayer': 3}], limits))
    cases.append(render_case("layer-status-2", "fact:9", {"palette": PALETTE, "rules": [rule(part(C, 9, 255, 2), part(T, 4), part(P, 2, RED))]}, [{'layers': [1, 3], 'layerOwners': {'0': 1, '31': 3}, 'lockedLayers': [1, 3], 'oneshotLayer': 3}], limits))
    cases.append(render_case("layer-status-3", "fact:9", {"palette": PALETTE, "rules": [rule(part(C, 9, 255, 3), part(T, 4), part(P, 2, RED))]}, [{'layers': [1, 3], 'layerOwners': {'0': 1, '31': 3}, 'lockedLayers': [1, 3], 'oneshotLayer': 3}], limits))
    cases.append(render_case("behaviour-phase-0", "fact:12", {"palette": PALETTE, "rules": [rule(part(C, 12, 255, 0, 255), part(T, 4), part(P, 2, RED))]}, [{'behaviourPhases': {'0': {'id': 2, 'phases': [0, 1], 'branch': 3}, '31': {'id': 3, 'phases': [2, 3, 4], 'branch': 2}}}], limits))
    cases.append(render_case("behaviour-phase-1", "fact:12", {"palette": PALETTE, "rules": [rule(part(C, 12, 255, 1, 255), part(T, 4), part(P, 2, RED))]}, [{'behaviourPhases': {'0': {'id': 2, 'phases': [0, 1], 'branch': 3}, '31': {'id': 3, 'phases': [2, 3, 4], 'branch': 2}}}], limits))
    cases.append(render_case("behaviour-phase-2", "fact:12", {"palette": PALETTE, "rules": [rule(part(C, 12, 255, 2, 255), part(T, 4), part(P, 2, RED))]}, [{'behaviourPhases': {'0': {'id': 2, 'phases': [0, 1], 'branch': 3}, '31': {'id': 3, 'phases': [2, 3, 4], 'branch': 2}}}], limits))
    cases.append(render_case("behaviour-phase-3", "fact:12", {"palette": PALETTE, "rules": [rule(part(C, 12, 255, 3, 255), part(T, 4), part(P, 2, RED))]}, [{'behaviourPhases': {'0': {'id': 2, 'phases': [0, 1], 'branch': 3}, '31': {'id': 3, 'phases': [2, 3, 4], 'branch': 2}}}], limits))
    cases.append(render_case("behaviour-phase-4", "fact:12", {"palette": PALETTE, "rules": [rule(part(C, 12, 255, 4, 255), part(T, 4), part(P, 2, RED))]}, [{'behaviourPhases': {'0': {'id': 2, 'phases': [0, 1], 'branch': 3}, '31': {'id': 3, 'phases': [2, 3, 4], 'branch': 2}}}], limits))
    layers = layer_table([[0, NONE], [6, RED], [0, GREEN], [6, BLUE]])
    every_layer = rule(part(C, 1, ANY), part(T, 7), part(P, 3))
    mapped = {"1": [0, 1, 2, 30], "2": [5], "3": [1, 31, 32]}
    cases.append(render_case("layer-active-any", "fact:1", {"palette": PALETTE, "things": [layers], "rules": [every_layer]}, [
        {"layers": [], "mapped": mapped},
        {"layers": [1], "mapped": mapped},
        {"layers": [1, 3], "mapped": mapped},
        {"layers": [1, 2, 3], "mapped": mapped},
    ], limits))
    cases.append(render_case("layer-active-one", "fact:1", {"palette": PALETTE, "rules": [rule(part(C, 1, 1), part(T, 0), part(P, 2, YELLOW))]}, [
        {"layers": [2]}, {"layers": [1, 2]},
    ], limits))
    cases.append(render_case("layer-active-base-layer", "fact:1", {"palette": PALETTE, "rules": [rule(part(C, 1, 0), part(T, 1), part(P, 2, GREEN))]}, [{"layers": []}], limits))
    cases.append(render_case("layer-previewing", "fact:2", {"palette": PALETTE, "things": [layers], "rules": [rule(part(C, 2, ANY), part(T, 7), part(P, 3)), rule(part(C, 2, 1), part(T, 3, 0), part(P, 2, WHITE))], "keySets": [THUMBS]}, [
        {"mapped": mapped}, {"preview": 3, "mapped": mapped}, {"preview": 1, "mapped": mapped},
    ], limits))
    modes = {"kind": 2, "default": [0, NONE], "records": [[5, PURPLE], [4, CYAN], [2, BLACK]]}
    cases.append(render_case("pointing-mode-active", "fact:3", {"palette": PALETTE, "things": [modes], "rules": [rule(part(C, 3, ANY), part(T, 7), part(P, 3))]}, [
        {}, {"mode": {"id": 0, "keys": [], "halves": ["left"]}}, {"mode": {"id": 0, "keys": [], "halves": []}},
        {"mode": {"id": 1, "keys": [0, 35], "halves": ["left", "right"]}}, {"mode": {"id": 1, "keys": [], "halves": []}},
        {"mode": {"id": 2, "keys": [30], "halves": ["right"]}}, {"mode": {"id": 5, "keys": [], "halves": []}},
    ], limits))
    cases.append(render_case("pointing-mode-one", "fact:3", {"palette": PALETTE, "rules": [rule(part(C, 3, 1), part(T, 0), part(P, 2, RED))]}, [
        {"mode": {"id": 0, "keys": [], "halves": []}}, {"mode": {"id": 1, "keys": [], "halves": []}},
    ], limits))
    combos = {"kind": 3, "default": [5, YELLOW], "records": []}
    cases.append(render_case("combos-held", "fact:4", {"palette": PALETTE, "things": [combos], "keySets": [THUMBS], "rules": [
        rule(part(C, 4, 1), part(T, 4), part(P, 2, GREEN)),
        rule(part(C, 4, 0), part(T, 7), part(P, 3)),
        rule(part(C, 4, ANY), part(T, 3, 0), part(P, 3)),
    ]}, [
        {}, {"combos": {"others": [0, 1]}}, {"combos": {"owning": [30]}}, {"combos": {"owning": [0], "others": [31]}},
    ], limits))
    behaviour_keys = {"0": {"state": "hold"}, "35": {"state": "branch", "branch": 3}, "12": {"state": "long", "flashing": True, "lit": True},
                      "13": {"state": "long", "flashing": True, "lit": False}, "14": {"state": "tap"}, "15": {"state": "branch", "branch": 7}, "16": {"state": "branch", "branch": 1}}
    owners = {"board": 35, "left": 0, "right": 35, "hold": 0, "branch": 35, "long": 13, "tap": 14}
    for scope, place in ((0, 4), (1, 5), (2, 5), (3, 4)):
        cases.append(render_case(f"behaviour-state-scope-{scope}", "fact:5", {"palette": PALETTE, "things": [dict(BEHAVIOUR_TABLE, default=[place] + BEHAVIOUR_TABLE["default"][1:])], "rules": [rule(part(C, 5, 0x0F, scope), part(T, 7), part(P, 3))]}, [
            {}, {"behaviour": {"keys": behaviour_keys, "owners": owners}},
            {"behaviour": {"keys": {"0": {"state": "hold", "flashing": True, "lit": False}, "35": {"state": "tap"}}, "owners": {"board": 0, "left": 0, "right": 35, "hold": 0, "tap": 35}}},
        ], limits))
    cases.append(render_case("behaviour-state-mask", "fact:5", {"palette": PALETTE, "things": [BEHAVIOUR_TABLE], "keySets": [THUMBS], "rules": [
        rule(part(C, 5, 0x04, 0), part(T, 4), part(P, 3)),
        rule(part(C, 5, 0x03, 3), part(T, 3, 0), part(P, 3)),
    ]}, [{"behaviour": {"keys": behaviour_keys, "owners": owners}}], limits))

    board_colour = lambda target, colour=RED: rule(part(T, *target), part(P, 2, colour))
    for target, name in (((0,), "whole-board"), ((1,), "left-half"), ((2,), "right-half"), ((3, 0), "key-set"), ((4,), "subject-keys"), ((5,), "subject-halves"), ((6,), "subject-layer-mapped-keys"), ((7,), "subject-place")):
        cases.append(render_case(f"target-{name}", f"target:{target[0]}", {"palette": PALETTE, "things": [layers], "keySets": [[3, 40, 57]], "rules": [
            rule(part(C, 1, 1), part(T, *target), part(P, 2, GREEN)),
            board_colour(target),
        ]}, [{"mapped": mapped}, {"layers": [1], "mapped": mapped}], limits))

    under = rule(part(T, 0), part(P, 2, RED))
    cases.append(render_case("paint-clear", "paint:0", {"palette": PALETTE, "rules": [rule(part(T, 1), part(P, 0)), under]}, [{}], limits))
    cases.append(render_case("paint-off", "paint:1", {"palette": PALETTE, "rules": [rule(part(T, 1), part(P, 1)), under]}, [{}], limits))
    cases.append(render_case("paint-colour", "paint:2", {"palette": PALETTE, "rules": [rule(part(T, 1), part(P, 2, CYAN)), under]}, [{}], limits))
    cases.append(render_case("paint-identity", "paint:3", {"palette": PALETTE, "things": [layers], "rules": [rule(part(C, 1, ANY), part(T, 0), part(P, 3)), under]}, [{"layers": []}, {"layers": [3]}], limits))
    sparse = {0: RED, 5: 0x81, 6: 0x83, 40: BLUE}
    dense = {led: (GREEN if led % 2 else 0x81) for led in range(40)}
    dense.update({led: 0x83 for led in range(40, 50)})
    cases.append(render_case("paint-map", "paint:4", {"palette": PALETTE, "things": [layers], "maps": [sparse, dense], "rules": [
        rule(part(C, 1, 3), part(T, 0), part(P, 4, 1)),
        rule(part(C, 1, ANY), part(T, 0), part(P, 4, 0)),
        rule(part(T, 0), part(P, 2, YELLOW)),
    ]}, [{"layers": [1]}, {"layers": [3]}, {"layers": []}], limits))
    cases.append(render_case("paint-dim", "paint:5", {"palette": PALETTE, "rules": [rule(part(T, 0), part(P, 5, 64)), rule(part(T, 1), part(P, 2, WHITE))]}, [
        {"effect": {"40": "804020"}},
    ], limits))
    cases.append(render_case("paint-tint", "paint:6", {"palette": PALETTE, "rules": [rule(part(T, 0), part(P, 6, BLUE, 100)), rule(part(T, 1), part(P, 2, RED))]}, [
        {"effect": {"40": "102030"}},
    ], limits))
    cases.append(render_case("rule-brightness-background", "engine", {"palette": PALETTE, "keySets": [[0, 1, 2]], "rules": [
        rule(part(C, 1, 1), part(T, 3, 0), part(P, 2, WHITE), part(B, 1), brightness=128),
        rule(part(C, 1, 2), part(T, 3, 0), part(P, 2, GREEN), part(B, 5, 128)),
        under,
    ]}, [{"layers": [1]}, {"layers": [2]}], limits))
    stack = [rule(part(T, 0), part(P, 5, 200)) for _ in range(5)] + [under]
    cases.append(render_case("below-composition", "engine", {"palette": PALETTE, "rules": stack}, [{}], limits))
    cases.append(render_case("conditions", "engine", {"palette": PALETTE, "rules": [
        rule(part(C, 1, 1), part(SLOT_NEGATED, 1, 2), part(T, 1), part(P, 2, GREEN)),
        rule(part(SLOT_NEGATED, 1, 1), part(T, 2), part(P, 2, BLUE)),
        rule(part(T, 0), part(P, 2, WHITE), flags=1),
    ]}, [{"layers": []}, {"layers": [1]}, {"layers": [1, 2]}], limits))
    cases.append(render_case("timing-show-after", "timing:1", {"palette": PALETTE, "rules": [rule(part(C, 1, 1), part(T, 0), part(P, 2, RED), part(G, 1, 100, 0))]}, [
        {"layers": [1], "time": 1000}, {"layers": [1], "time": 1099}, {"layers": [1], "time": 1100}, {"layers": [], "time": 1200}, {"layers": [1], "time": 1300}, {"layers": [1], "time": 1450},
    ], limits))
    # Accepted-domain capacity and lifetime boundaries. These are render
    # vectors, so every implementation must retain their exact semantics.
    for count in (32, 33, 255):
        delayed = [rule(part(T, 0), part(P, 2, BLUE), part(G, 1, 100, 0))]
        delayed += [rule(part(T, 0), part(P, 2, RED), part(G, 1, 100, 0)) for _ in range(count - 1)]
        cases.append(render_case(f"show-after-{count}-rules", "timing:1", {"palette": PALETTE, "rules": delayed}, [{"time": 0}, {"time": 100}], limits))
    held_rows = {str(key): {"id": key, "state": "hold"} for key in range(60)}
    sixty = {"behaviour": {"keys": held_rows}}
    many = [rule(part(C, 8, ANY, 4), part(T, 4), part(P, 2, BLUE), part(G, 1, 100, 0))]
    many += [rule(part(C, 8, ANY, 4), part(T, 4), part(P, 2, RED), part(G, 1, 100, 0)) for _ in range(254)]
    high_rows = {str(key): {"id": 68 + key, "state": "hold"} for key in range(60)}
    cases.append(render_case("show-after-255-rules-60-subjects", "timing:1", {"palette": PALETTE, "rules": many}, [dict(sixty, time=0), dict(sixty, time=100), {"time": 200, "behaviour": {"keys": high_rows}}, {"time": 300, "behaviour": {"keys": high_rows}}], limits))
    cases.append(render_case("show-after-subject-leaves-and-returns", "timing:1", {"palette": PALETTE, "rules": [rule(part(C, 8, ANY, 4), part(T, 4), part(P, 2, BLUE), part(G, 1, 100, 0))]}, [
        {"time": 0, "behaviour": {"keys": {"0": {"id": 0, "state": "hold"}, "31": {"id": 1, "state": "hold"}}}},
        {"time": 50, "behaviour": {"keys": {"31": {"id": 1, "state": "hold"}}}},
        {"time": 100, "behaviour": {"keys": {"0": {"id": 0, "state": "hold"}, "31": {"id": 1, "state": "hold"}}}},
        {"time": 200, "behaviour": {"keys": {"0": {"id": 0, "state": "hold"}, "31": {"id": 1, "state": "hold"}}}},
    ], limits))
    cases.append(render_case("show-after-filter-loss-and-other-rule", "timing:1", {"palette": PALETTE, "rules": [
        rule(part(C, 1, 1), part(C, 16, 0), part(T, 0), part(P, 2, BLUE), part(G, 1, 100, 0)),
        rule(part(C, 1, 1), part(T, 0), part(P, 2, RED), part(G, 1, 100, 0)),
    ]}, [{"time": 0, "layers": [1], "hostLocks": 1}, {"time": 99, "layers": [1]}, {"time": 100, "layers": [1], "hostLocks": 1}, {"time": 199, "layers": [1], "hostLocks": 1}, {"time": 200, "layers": [1], "hostLocks": 1}], limits))
    cases.append(render_case("show-after-normalized-specific-and-any", "timing:1", {"palette": PALETTE, "rules": [
        rule(part(C, 8, 0, 4), part(T, 4), part(P, 2, BLUE), part(G, 1, 100, 0)),
        rule(part(C, 8, ANY, 4), part(T, 4), part(P, 2, RED), part(G, 1, 100, 0)),
    ]}, [{"time": 0, "behaviour": {"keys": {"0": {"id": 0, "state": "hold"}, "31": {"id": 1, "state": "hold"}}}}, {"time": 100, "behaviour": {"keys": {"0": {"id": 0, "state": "hold"}, "31": {"id": 1, "state": "hold"}}}}], limits))
    cases.append(render_case("show-after-max-delay-and-large-jump", "timing:1", {"palette": PALETTE, "rules": [rule(part(C, 1, 1), part(T, 0), part(P, 2, BLUE), part(G, 1, 255, 255))]}, [{"time": 0, "layers": [1]}, {"time": 65534, "layers": [1]}, {"time": 65535, "layers": [1]}, {"time": 200000, "layers": [1]}, {"time": 300000}, {"time": 400000, "layers": [1]}, {"time": 465535, "layers": [1]}], limits))
    # Select every normalized non-selector signature in one accepted profile.
    # Many simultaneous true signatures and high/low id churn exercise dense
    # storage beyond the former 32 entries and the 255-rule duplication case.
    import itertools
    domains = {"layerSelector": [*range(16), ANY], "layerStatus": range(4), "modeStatus": range(2),
               "comboRole": [0, 1, ANY], "behaviourStates": range(1, 16), "behaviourScope": range(4),
               "behaviourPhase": range(5), "tapSelector": [*range(1, limits.tap_depth + 1), ANY],
               "modifier": range(4), "side": range(3), "modifierState": range(2), "hostLock": range(3), "pointerState": range(4)}
    signatures = []
    for kind, (_, types) in FACTS.items():
        selector = bool(types and types[0] in ("layerSelector", "modeSelector", "thingSelector", "keySelector"))
        values = [[ANY] if i == 0 and selector else domains[t] for i, t in enumerate(types)]
        signatures.extend((kind, list(params)) for params in itertools.product(*values))
    assert len(signatures) == 171
    for probe in (8, 12):
        selected = [(kind, params) for kind, params in signatures if kind == probe] + [(kind, params) for kind, params in signatures if kind != probe]
        full = [rule(part(C, kind, *params), part(T, 4), part(P, 2, BLUE) if kind == probe else part(P, 0), part(G, 1, 100, 0)) for kind, params in selected]
        def populated(high, duplicate=False):
            ids = [127 if duplicate else (68 if high else 0) + k for k in range(60)]
            return {"layers": list(range(16)), "preview": 15, "mode": {"id": 31, "keys": [59]},
                    "combos": {"owning": [0], "others": [59], "instances": [{"id": 124 + k, "role": k % 2, "keys": [k]} for k in range(4)]},
                    "behaviour": {"keys": {str(k): {"id": ids[k], "state": ["branch", "tap", "hold", "long"][k % 4], "branch": 2} for k in range(60)},
                                  "owners": {"board": 0, "left": 0, "right": 31, "branch": 0, "tap": 1, "hold": 2, "long": 3}},
                    "layerOwners": {str(k): k % 16 for k in range(60)}, "lockedLayers": list(range(16)), "oneshotLayer": 15,
                    "modeStatus": {"id": 31, "locked": True, "keys": [59]},
                    "comboLayers": [{"id": 124 + k, "layer": k, "keys": [k]} for k in range(4)],
                    "behaviourPhases": {str(k): {"id": ids[k], "phases": list(range(5)), "branch": 1 + k % limits.tap_depth} for k in range(60)},
                    "macroPlaying": 127, "pressedKeys": list(range(60)), "modifiers": [255, 255], "hostLocks": 7, "pointer": 15, "idle": True}
        scenarios = [dict(populated(True), time=0), dict(populated(True), time=100),
                     dict(populated(False), time=150), dict(populated(False), time=250),
                     dict(populated(True, True), time=300), dict(populated(True, True), time=400),
                     {"time": 500}, dict(populated(True), time=600), dict(populated(True), time=700)]
        cases.append(render_case(f"show-after-all-signatures-probe-{probe}", "timing:1", {"palette": PALETTE, "rules": full}, scenarios, limits))
    cases.append(render_case("show-after-backward-time-regression", "timing:1", {"palette": PALETTE, "rules": [rule(part(C, 1, 1), part(T, 0), part(P, 2, BLUE), part(G, 1, 244, 1))]}, [{"time": t, "layers": [1]} for t in (1000, 1100, 1099, 1200, 1499, 1500)], limits))
    cases.append(render_case("show-after-clock-wrap-and-backward-matured", "timing:1", {"palette": PALETTE, "rules": [rule(part(C, 1, 1), part(T, 0), part(P, 2, BLUE), part(G, 1, 100, 0))]}, [{"time": t, "layers": [1]} for t in (0xfffffff0, 0x20, 0x54, 0x53, 0x60, 200000)], limits))
    cases.append(render_case("show-after-backward-filter-reappearance", "timing:1", {"palette": PALETTE, "rules": [rule(part(C, 1, 1), part(C, 16, 0), part(T, 0), part(P, 2, BLUE), part(G, 1, 255, 255))]}, [{"time": t, "layers": [1], "hostLocks": locks} for t, locks in ((1000, 1), (66534, 1), (66535, 1), (66534, 1), (66536, 0), (66537, 1), (132071, 1), (132072, 1))], limits))
    cases.append(render_case("fade-top-layer-regression", "fact:9", {"palette": PALETTE, "things": [layer_table([[0, GREEN], [0, GREEN], [0, GREEN], [0, GREEN], [0, WHITE]])], "rules": [rule(part(C, 9, ANY, 0), part(T, 0), part(P, 3))], "extensions": [[1, [1, 0, 0, 1]]]}, [{"layers": [4]}, {"layers": [4], "fade": {"progress": 600, "timeout": 900, "deadTime": 300, "autoMouseLayer": 4}}], limits))
    fade_layers = layer_table([[6, NONE], [0, GREEN], [0, NONE], [0, NONE], [0, WHITE]])
    fade_rules = [rule(part(T, 3, 0), part(P, 2, PURPLE)), every_layer]
    for mode in (0, 1, 2):
        cases.append(render_case(f"automouse-fade-mode-{mode}", "extension:1", {"palette": PALETTE, "things": [fade_layers], "keySets": [[10]], "rules": fade_rules, "extensions": [[1, [1, mode, BLUE, 1]]]}, [
            {"layers": [4], "effect": {"0": "302010"}, "fade": {"progress": 0, "timeout": 900, "deadTime": 300, "autoMouseLayer": 4}},
            {"layers": [4], "effect": {"0": "302010"}, "fade": {"progress": 300, "timeout": 900, "deadTime": 300, "autoMouseLayer": 4}},
            {"layers": [1, 4], "effect": {"0": "302010"}, "fade": {"progress": 450, "timeout": 900, "deadTime": 300, "autoMouseLayer": 4}},
            {"layers": [4], "effect": {"0": "302010"}, "fade": {"progress": 600, "timeout": 900, "deadTime": 300, "autoMouseLayer": 4}},
        ], limits))
    identified_combos = {"kind": 3, "default": [5, YELLOW], "records": [[4, RED], [2, GREEN], [INHERIT, INHERIT], [1, NONE]]}
    instances = [{"id": 1, "role": 1, "keys": [30, 31]}, {"id": 0, "role": 0, "keys": [0, 1]}]
    for selector, role in ((ANY, ANY), (0, ANY), (1, 1), (1, 0), (127, ANY)):
        cases.append(render_case(f"combo-identity-{selector}-{role}", "fact:7", {"palette": PALETTE, "things": [identified_combos], "rules": [rule(part(C, 7, selector, role), part(T, 7), part(P, 3))]}, [{}, {"combos": {"instances": instances}}, {"combos": {"instances": list(reversed(instances))}}, {"combos": {"instances": [{"id": 2, "role": 1, "keys": [0]}]}}, {"combos": {"instances": [{"id": 3, "role": 1, "keys": [0]}]}}], limits))
    behaviours = dict(BEHAVIOUR_TABLE, records=[[4, RED, GREEN, BLUE, WHITE, CYAN, PURPLE, YELLOW], [2] + [INHERIT] * 7])
    keys = {"0": {"id": 0, "state": "hold", "outcome": 2}, "31": {"id": 1, "state": "long", "outcome": 34}}
    for selector in (ANY, 0, 1, 127):
        cases.append(render_case(f"behaviour-identity-{selector}", "fact:8", {"palette": PALETTE, "things": [behaviours], "rules": [rule(part(C, 8, selector, 15), part(T, 7), part(P, 3))]}, [{}, {"behaviour": {"keys": keys}}, *[{"behaviour": {"keys": {"0": {"id": 0, "state": "branch", "branch": branch}}}} for branch in (2, 3, 5, 7)], {"behaviour": {"keys": {"0": {"id": 0, "state": "tap"}, "31": {"id": 1, "state": "hold", "flashing": True, "lit": False}}}}], limits))
    outcome_domain = {"palette": PALETTE, "things": [layers, modes, behaviours], "rules": [rule(part(C, 8, ANY, 15), part(T, 4), part(P, 8)), rule(part(T, 0), part(P, 2, PURPLE))]}
    cases.append(render_case("behaviour-outcome", "paint:8", outcome_domain, [{"behaviour": {"keys": keys}}, *[{"behaviour": {"keys": {"0": {"id": 0, "state": state, "outcome": outcome}}}} for state, outcome in (("hold", 0), ("hold", 1), ("hold", 2), ("long", 34), ("tap", 2), ("branch", 2), ("hold", 65))]], limits))
    cases.append(render_case("painting-map-outcome-clear-off", "paint:8", {"palette": PALETTE, "things": [layers, modes, behaviours], "maps": [{0: 0x88, 1: 0x81, 57: 0x83}], "rules": [rule(part(C, 8, ANY, 15), part(T, 0), part(P, 4, 0)), rule(part(T, 0), part(P, 2, PURPLE))]}, [{"behaviour": {"keys": {"0": {"id": 0, "state": "hold", "outcome": 2}}}}, {"behaviour": {"keys": {"0": {"id": 0, "state": "tap"}}}}], limits))
    return cases


def mutate(payload: bytes, offset: int, value: int) -> bytes:
    data = bytearray(payload)
    data[offset] = value
    return bytes(data)


def codec_vectors(limits, standard: bytes):
    valid = [{"name": "empty", "hex": encode({}).hex()}, {"name": "standard", "hex": standard.hex()}]
    full = {
        "palette": PALETTE,
        "things": [layer_table([[0, RED], [INHERIT, INHERIT], [6, GREEN]]), {"kind": 2, "default": [5, NONE], "records": []}, {"kind": 3, "default": [4, YELLOW], "records": []}, BEHAVIOUR_TABLE],
        "keySets": [THUMBS, []],
        "maps": [{0: RED, 57: 0x81}, {led: WHITE for led in range(30)}],
        "rules": [
            rule(part(C, 1, ANY), part(SLOT_NEGATED, 3, 2), part(C, 4, 0xFF), part(C, 5, 0x0F, 3), part(T, 7), part(P, 4, 1), part(B, 6, BLUE, 30), part(G, 1, 0x34, 0x12), brightness=90),
            rule(part(C, 2, 1), part(T, 3, 1), part(P, 5, 0)),
        ],
        "extensions": [[1, [1, 2, BLUE, 2]], [2, [1]]],
    }
    base = encode(full)
    valid.append({"name": "every-section", "hex": base.hex()})
    invalid = []

    def reject(name, data):
        try:
            validate(data, limits)
        except Invalid as error:
            invalid.append({"name": name, "hex": data.hex(), "error": {"code": error.code, "offset": error.offset}})
            return
        raise AssertionError(f"{name} validated")

    def domain_with(**changes):
        return encode(dict(full, **changes))

    reject("budget", bytes(BUDGET + 1))
    reject("header-truncated", base[:7])
    reject("version-4", mutate(base, 0, 4))
    reject("header-reserved", mutate(base, 1, 1))
    reject("led-count", mutate(base, 2, 57))
    reject("palette-count", mutate(base, 3, 129))
    reject("palette-brightness", domain_with(palette=PALETTE + [[0, 0, 201]]))
    reject("palette-truncated", base[:8 + 3 * 4])
    reject("thing-kind-zero", domain_with(things=[{"kind": 0, "default": [0], "records": []}]))
    reject("thing-kind-unknown", domain_with(things=[{"kind": 9, "default": [0, NONE], "records": []}]))
    reject("thing-order", domain_with(things=[{"kind": 2, "default": [0, NONE], "records": []}, layer_table([])]))
    reject("thing-count", domain_with(things=[layer_table([[0, NONE]] * 17)]))
    reject("thing-default-inherits-place", domain_with(things=[layer_table([], default=(INHERIT, NONE))]))
    reject("thing-default-inherits-colour", domain_with(things=[layer_table([], default=(0, INHERIT))]))
    reject("thing-place-not-a-place", domain_with(things=[layer_table([[3, NONE]])]))
    reject("thing-place-subject-place", domain_with(things=[layer_table([[7, NONE]])]))
    reject("thing-colour-reference", domain_with(things=[layer_table([[0, 9]])]))
    reject("key-set-padding", domain_with(keySets=[[0]], maps=[], rules=[]) [:-1] + b"" if False else mutate(encode({"palette": PALETTE, "keySets": [[0]]}), 8 + 27 + 7, 0x04))
    reject("map-dense-noncanonical", encode({"palette": PALETTE}) [:6] and encode_raw_map(bytes([0] + [0x80] * 30 + [RED] * 28)))
    reject("map-sparse-count", encode_raw_map(bytes([1, 29]) + bytes([0, RED]) * 29))
    reject("map-sparse-led", encode_raw_map(bytes([1, 1, 58, RED])))
    reject("map-sparse-order", encode_raw_map(bytes([1, 2, 5, RED, 5, RED])))
    reject("map-sparse-clear", encode_raw_map(bytes([1, 1, 5, 0x80])))
    reject("map-value-map-paint", encode_raw_map(bytes([1, 1, 5, 0x84])))
    reject("map-value-parameterised-paint", encode_raw_map(bytes([1, 1, 5, 0x82])))
    reject("map-value-palette", encode_raw_map(bytes([1, 1, 5, 9])))
    reject("map-encoding", encode_raw_map(bytes([2])))
    simple = lambda *parts, **kw: encode({"palette": PALETTE, "keySets": [THUMBS], "maps": [{0: RED}], "rules": [rule(*parts, **kw)]})
    good = [part(T, 0), part(P, 2, RED)]
    reject("rule-size", mutate(simple(*good), 8 + 27 + 8 + 4, 5))
    reject("rule-truncated", simple(*good)[:-3])
    reject("rule-flags", simple(*good, flags=2))
    reject("rule-reserved-slot", simple(part(7, 1, 1), *good))
    reject("rule-part-order", simple(part(P, 2, RED), part(T, 0)))
    reject("rule-conditions", simple(*[part(C, 1, ANY)] * 5, *good))
    reject("rule-two-targets", simple(part(T, 0), part(T, 1), part(P, 2, RED)))
    reject("rule-no-paint", simple(part(T, 0)))
    reject("rule-unknown-fact", simple(part(C, 31), *good))
    reject("rule-unknown-rule-extension", simple(*good, part(SLOT_EXTENSION, 1)))
    reject("rule-parameter-length", simple(part(C, 1, ANY, 0), *good))
    reject("rule-layer", simple(part(C, 1, 16), *good))
    reject("rule-mode", simple(part(C, 3, 32), *good))
    reject("combo-selector-128", simple(part(C, 7, 128, 255), *good))
    reject("behaviour-selector-254", simple(part(C, 8, 254, 15), *good))
    reject("identified-combo-role", simple(part(C, 7, 0, 2), *good))
    reject("identified-behaviour-states-zero", simple(part(C, 8, 0, 0), *good))
    reject("identified-behaviour-states-high", simple(part(C, 8, 0, 16), *good))
    reject("outcome-has-no-params", simple(part(T, 0), part(P, 8, 1)))
    reject("key-query-out-of-range", simple(part(T,8,16,255),part(P,0)))
    reject("key-query-selector-out-of-range", simple(part(T,8,0,128),part(P,0)))
    reject("key-query-missing-selector", simple(part(T,8,0),part(P,0)))
    reject("key-query-not-a-place", domain_with(things=[layer_table([[8,NONE]])]))
    reject("what-it-does-parameterless", simple(part(T,0),part(P,9,0)))
    reject("rule-combo-role", simple(part(C, 4, 2), *good))
    reject("rule-behaviour-states-zero", simple(part(C, 5, 0, 0), *good))
    reject("rule-behaviour-states-high", simple(part(C, 5, 0x10, 0), *good))
    reject("rule-behaviour-scope", simple(part(C, 5, 1, 4), *good))
    reject("rule-key-set", simple(part(T, 3, 1), part(P, 2, RED)))
    reject("rule-palette", simple(part(T, 0), part(P, 2, 9)))
    reject("rule-map", simple(part(T, 0), part(P, 4, 1)))
    reject("rule-show-after-zero", simple(*good, part(G, 1, 0, 0)))
    reject("rule-part-past-end", mutate(simple(*good), 8 + 27 + 8 + 4, 6))
    reject("extension-unknown", encode({"extensions": [[3, []]]}))
    reject("extension-order", encode({"palette": PALETTE, "extensions": [[2, [1]], [1, [1, 0, 0, 0]]]}))
    reject("extension-length", encode({"palette": PALETTE, "extensions": [[1, [1, 0, 0]]]}))
    reject("fade-flags", encode({"palette": PALETTE, "extensions": [[1, [2, 0, 0, 0]]]}))
    reject("fade-mode", encode({"palette": PALETTE, "extensions": [[1, [1, 3, 0, 0]]]}))
    reject("fade-colour", encode({"palette": PALETTE, "extensions": [[1, [1, 0, 9, 0]]]}))
    reject("fade-base", encode({"palette": PALETTE, "extensions": [[1, [1, 0, 0, 1]]]}))
    reject("tap-flash-mode", encode({"extensions": [[2, [0]]]}))
    reject("extensions-missing", encode({})[:-1])
    reject("trailing", encode({}) + b"\0")
    return {"valid": valid, "invalid": invalid}


def encode_raw_map(raw: bytes) -> bytes:
    """A domain with the palette and one map given as raw bytes."""
    head = bytearray(encode({"palette": PALETTE})[:-1])
    head[6] = 1
    return bytes(head) + raw + b"\0"


def conversion_vectors(limits):
    v4 = json.loads(V4_VECTORS.read_text())
    compiled = bytes.fromhex(next(case["hex"] for case in v4["valid"] if case["name"] == "compiled"))
    cases = [{"name": "standard", "v4": compiled.hex(), "limits": limits.as_json(), "hex": convert_v4(compiled, limits).hex()}]
    reduced = Limits(facts={1, 3, 5}, things={1, 2, 4}, domain_extensions={2})
    cases.append({"name": "standard-without-preview-combos-fade", "v4": compiled.hex(), "limits": reduced.as_json(), "hex": convert_v4(compiled, reduced).hex()})
    layers = [[n, 0, 0, 0, 1] for n in range(16)]
    layers[0] = [0, 10, 255, 200, 0]
    layers[2] = [2, 85, 255, 200, 1]
    layers[3] = [3, 5, 0, 0, 0]
    pd = [[n, 0, 0, 0, n % 5] for n in range(32)]
    pd[1] = [1, 127, 255, 200, 4]
    synthetic = encode_v4(
        stages=0x15, groups=[THUMBS, [3]], layers=layers,
        layer_rows=[[0xFF, 0, 0, 0, 1], [2, 43, 255, 200, 0]], automouse=(2, 167, 255, 199), pd=pd,
        pd_rows=[[1, 0, 0, 0, 0], [0xFF, 200, 255, 200, 1]], combo=(191, 255, 200, 4), combo_rows=[[0, 0, 0, 0], [21, 255, 200, 1]],
        branches=[[169, 255, 200], [222, 255, 200], [85, 255, 200], [25, 255, 200]],
        key=(0, 0, 150, 18, 255, 200, 148, 255, 200, 0, 4), key_rows=[[0xFF, 0, 0, 0, 1], [2, 21, 255, 200, 0], [0, 0, 0, 0, 0]],
    )
    cases.append({"name": "every-row-kind", "v4": synthetic.hex(), "limits": limits.as_json(), "hex": convert_v4(synthetic, limits).hex()})
    for locality, name in ((0, "both"), (1, "left"), (2, "right")):
        variant = encode_v4(stages=0x1F, layers=[[n, 0, 0, 0, 1] for n in range(16)], pd=[[n, 0, 0, 0, 2] for n in range(32)],
                            branches=[[169, 255, 200]] * 4, key=(0, 0, 150, 18, 255, 200, 148, 255, 200, 1, locality))
        cases.append({"name": f"behaviour-locality-{name}", "v4": variant.hex(), "limits": limits.as_json(), "hex": convert_v4(variant, limits).hex()})
    for case in cases:
        validate(bytes.fromhex(case["hex"]), Limits.from_json(case["limits"]))
    return cases


def capability_pages(limits: Limits) -> list:
    """The capability read (GET 0x0D) a keyboard with these limits answers."""
    registries = [limits.facts, limits.targets, limits.paints, limits.timings, limits.rule_extensions, limits.domain_extensions, limits.things]
    pages = [bytes([1, VERSION, BUDGET & 0xFF, BUDGET >> 8, PALETTE_MAX, CONDITIONS_MAX, PARAM_MAX, limits.tap_depth, LED_COUNT, len(registries)])]
    for index, ids in enumerate(registries):
        for half in range(2):
            bitmap = bytearray(16)
            for i in ids:
                if 128 * half <= i < 128 * half + 128:
                    bitmap[(i - 128 * half) // 8] |= 1 << (i % 8)
            pages.append(bytes([index, 128 * half]) + bytes(bitmap))
    return [page.hex() for page in pages]


def build_vectors() -> dict:
    limits = Limits()
    conversion = conversion_vectors(limits)
    standard = bytes.fromhex(conversion[0]["hex"])
    return {
        "format": 1,
        "description": "Cross-language vectors for RGB domain 0x10 version 5 (docs/architecture/lighting-rules-v1.md), generated by tools/lighting_reference.py. Codec: valid payloads and rejections with the validator's code and payload-relative offset. Conversion: version-4 payloads and the version-5 bytes the conversion produces for the given capabilities. Render: per registry entry, a payload and situations with the 58 LED colours each must produce ('-' is an LED no rule writes). Capability: the GET 0x0D pages, in order, for these limits. Keys are matrix indices row * 6 + column.",
        "limits": limits.as_json(),
        "geometry": {"matrixRows": MATRIX_ROWS, "matrixColumns": MATRIX_COLS, "leftLedCount": LEFT_LED_COUNT, "keyLeds": KEY_LEDS},
        "palette": PALETTE,
        "codec": codec_vectors(limits, standard),
        "conversion": conversion,
        "render": render_vectors(limits),
        "capability": capability_pages(limits),
    }


def encode_event_group(group: dict) -> bytes:
    """Qualified internal split group, independent of profile format version."""
    owners = bytes.fromhex(group["owners"])
    qualified = bytes.fromhex(group["qualified"])
    if len(owners) != 8 or len(qualified) != 32 or not any(qualified) or qualified[-1] & 0x80:
        raise ValueError("event owner/rule mask")
    fields = [group[name] for name in ("thingKind", "thing", "slot", "outcome", "sides", "hasKeys")]
    if not all(0 <= value <= 255 for value in fields) or fields[4] > 3 or fields[5] > 1:
        raise ValueError("event subject")
    return group["order"].to_bytes(8, "little") + bytes(fields) + owners + qualified


def decode_event_group(wire: bytes) -> dict:
    if len(wire) != 54:
        raise ValueError("event group length")
    result = {"order": int.from_bytes(wire[:8], "little"),
              **dict(zip(("thingKind", "thing", "slot", "outcome", "sides", "hasKeys"), wire[8:14])),
              "owners": wire[14:22].hex(), "qualified": wire[22:].hex()}
    if encode_event_group(result) != wire:
        raise ValueError("event group encoding")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="regenerate the vectors file")
    mode.add_argument("--check", action="store_true", help="fail if the vectors file differs from a fresh generation")
    args = parser.parse_args()
    text = json.dumps(build_vectors(), indent=1) + "\n"
    if args.write:
        VECTORS.write_text(text)
        print(f"wrote {VECTORS.relative_to(REPO_ROOT)}")
        return 0
    if not VECTORS.exists() or VECTORS.read_text() != text:
        print(f"{VECTORS.relative_to(REPO_ROOT)} is stale; run tools/lighting_reference.py --write", file=sys.stderr)
        return 1
    print("lighting vectors match the reference model")
    return 0


if __name__ == "__main__":
    sys.exit(main())
