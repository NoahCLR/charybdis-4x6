"""A profile's domains in a logical form two encodings of one profile share.

Each domain decodes to plain JSON values, field by field, following the specs
in docs/spec/: RGB v4 (rgb-domain-v1.md), key behaviours v2, combos v3,
settings v8, keymap v1 and macros v1 (profile-wire-v1/), sparse PD v3
(pd-mode-domain-v1.md). An action is written as the native keycode it stands for, so a
combo naming pointing slot 5 as a semantic action and one naming it as the keycode
0x7e85 read the same; nonzero action flags are kept beside it.
"""

import struct

LAYERS = 16
MATRIX_KEYS = 60
MACRO_SLOTS = 128

DOMAIN_NAMES = {0x10: "rgb", 0x20: "behaviors", 0x30: "combos", 0x40: "settings", 0x50: "pointing", 0x60: "keymap", 0x70: "macros"}

# users/noah/noah_keymap_ids.h and QMK's keycode blocks.
_NATIVE_BASE = {2: 0x5220, 3: 0x7EC0, 4: 0x7E80, 5: 0x7EA0, 6: 0x7700, 7: 0x7F00}


def blob_domains(blob: bytes) -> dict:
    """The domains of a schema-4 NLP1 blob: {id: (version, payload)}."""
    assert blob[:4] == b"NLP1", "not a profile blob"
    offset, domains = 8, {}
    for _ in range(blob[6]):
        length = struct.unpack_from("<I", blob, offset + 2)[0]
        domains[blob[offset]] = (blob[offset + 1], blob[offset + 6 : offset + 6 + length])
        offset += 6 + length
    assert offset == len(blob), "the domains do not end the blob"
    return domains


def _u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def _hsv(data, offset):
    return list(data[offset : offset + 3])


def action(data: bytes, offset: int) -> str:
    kind, flags, operand = data[offset], data[offset + 1], _u16(data, offset + 2)
    native = 0 if kind == 0 else operand if kind == 1 else _NATIVE_BASE[kind] + operand
    return f"0x{native:04x}" + (f"/flags {flags}" if flags else "")


def rgb(data: bytes) -> dict:
    assert data[0] == 4 and data[1] == 0 and data[14:16] == b"\0\0"
    groups, layers, layer_groups, pd, pd_groups, combo_groups, taps, key_groups = data[4], data[5], data[6], data[7], data[8], data[9], data[10], data[11]
    offset = 16

    def take(count, size, decode):
        nonlocal offset
        rows = [decode(offset + index * size) for index in range(count)]
        offset += count * size
        return rows

    value = {"stages": _u16(data, 2), "leds": data[12], "bitmapBytes": data[13]}
    value["groups"] = take(groups, 9, lambda at: {"id": data[at], "bitmap": data[at + 1 : at + 9].hex()})
    value["layerColors"] = take(layers, 5, lambda at: {"layer": data[at], "hsv": _hsv(data, at + 1), "mode": data[at + 4]})
    value["layerGroups"] = take(layer_groups, 5, lambda at: {"selector": data[at], "hsv": _hsv(data, at + 1), "group": data[at + 4]})
    value["autoMouseFade"] = take(1, 4, lambda at: {"mode": data[at], "endHsv": _hsv(data, at + 1)})[0]
    value["pdColors"] = take(pd, 5, lambda at: {"slot": data[at], "hsv": _hsv(data, at + 1), "locality": data[at + 4]})
    value["pdGroups"] = take(pd_groups, 5, lambda at: {"selector": data[at], "hsv": _hsv(data, at + 1), "group": data[at + 4]})
    value["comboFeedback"] = take(1, 4, lambda at: {"hsv": _hsv(data, at), "locality": data[at + 3]})[0]
    value["comboGroups"] = take(combo_groups, 4, lambda at: {"hsv": _hsv(data, at), "group": data[at + 3]})
    value["tapBranchColors"] = take(taps, 3, lambda at: _hsv(data, at))
    value["keyFeedback"] = take(1, 11, lambda at: {"committed": _hsv(data, at), "hold": _hsv(data, at + 3), "longHold": _hsv(data, at + 6), "tapPolicy": data[at + 9], "locality": data[at + 10]})[0]
    value["keyFeedbackGroups"] = take(key_groups, 5, lambda at: {"semantic": data[at], "hsv": _hsv(data, at + 1), "group": data[at + 4]})
    assert offset == len(data), "RGB records do not end the domain"
    return value


def behaviors(data: bytes) -> list:
    assert data[1] == 0
    rows, offset = [], 4
    for _ in range(data[0]):
        body, end = offset + 2, offset + 2 + _u16(data, offset)
        row = {"key": action(data, body), "tapHoldTerm": _u16(data, body + 4), "longerHoldTerm": _u16(data, body + 6), "multiTapTerm": _u16(data, body + 8),
               "flags": data[body + 10], "allowedLayers": _u32(data, body + 12), "steps": []}
        at = body + 16
        for _ in range(data[body + 11]):
            step, mask = {"tap": data[at]}, data[at + 1]
            at += 2
            if mask & 1:
                step["tapAction"] = action(data, at)
                at += 4
            for bit, name in ((2, "hold"), (4, "longHold")):
                if mask & bit:
                    step[name] = {"mode": data[at], "repeatHz": data[at + 1], "action": action(data, at + 2)}
                    at += 6
            row["steps"].append(step)
        assert at == end, "a behaviour row has bytes left over"
        rows.append(row)
        offset = end
    assert offset == len(data) and sum(len(row["steps"]) for row in rows) == _u16(data, 2)
    return rows


def combos(data: bytes) -> dict:
    assert data[1:4] == b"\0\0\0"
    value, offset = {"defaultTermMs": _u16(data, 4), "holdTermMs": _u16(data, 6), "rows": []}, 8
    for _ in range(data[0]):
        count = data[offset]
        value["rows"].append({"output": action(data, offset + 8), "inputs": [action(data, offset + 12 + 4 * slot) for slot in range(count)],
                              "flags": data[offset + 1], "termMs": _u16(data, offset + 2), "allowedLayers": _u32(data, offset + 4)})
        offset += 12 + 4 * count
    assert offset == len(data), "combo rows do not end the domain"
    return value


SETTING_NAMES = [
    "TAPPING_TERM", "TAP_HOLD_TERM", "LONG_HOLD_TERM", "MULTI_TAP_TERM", "AUTO_MOUSE_ENABLED", "AUTO_MOUSE_LAYER", "AUTO_MOUSE_TIMEOUT",
    "AUTO_MOUSE_DEBOUNCE", "AUTO_SNIPING_ENABLED", "AUTO_SNIPING_LAYER", "DRAGSCROLL_DPI", "VOLUME_DPI", "BRIGHTNESS_DPI", "ZOOM_DPI",
    "ARROW_DPI", "FEEDBACK_PERIOD", "AUTO_MOUSE_DEAD_TIME", "RGB_TIMEOUT", "DEFAULT_DPI", "SNIPING_DPI", "COMBOS_ENABLED", "RGB_MODE",
    "RGB_COLOR", "DEFAULT_LAYERS", "KEYMAP_OPTIONS", "AUTO_MOUSE_DELAY", "AUTO_MOUSE_THRESHOLD", "UNICODE_HOST_MODE", "BEHAVIORS_ENABLED",
    "LAYER_BEHAVIORS", "LAYER_COMBOS",
]


def settings(data: bytes) -> dict:
    assert list(data[:8]) == [8, 16, 31, 128, 128, data[5], 128, 0]
    value = {"flags": data[5], "values": {name: _u32(data, 8 + 4 * index) for index, name in enumerate(SETTING_NAMES)}}
    value["layers"] = [{"comboReference": data[at], "bypass": data[at + 1 : at + 9].hex(), "exclusion": data[at + 9 : at + 17].hex()}
                       for at in range(132, 404, 17)]
    value["customKeyShortcuts"] = {slot: [data[404 + 2 * slot], data[405 + 2 * slot]] for slot in range(128) if data[404 + 2 * slot] or data[405 + 2 * slot]}
    names, offset = [], 660
    for _ in range(16 + 128 + 128 + 1):
        names.append(data[offset + 1 : offset + 1 + data[offset]].decode("utf-8"))
        offset += 1 + data[offset]
    assert offset == len(data), "settings names do not end the domain"
    value["layerNames"] = names[:16]
    value["macroNames"] = {slot: name for slot, name in enumerate(names[16:144]) if name}
    value["customKeyNames"] = {slot: name for slot, name in enumerate(names[144:272]) if name}
    value["keyboardName"] = names[272]
    return value


def _tap(record, at):
    return {"keycode": f"0x{_u16(record, at):04x}", "policy": record[at + 2], "mask": record[at + 3]}


def pointing(data: bytes) -> dict:
    assert list(data[:3]) == [3, 32, 128] and data[4:8] == b"\0\0\0\0" and len(data) == 8 + 128 * data[3]
    slots = {}
    for index in range(data[3]):
        record = data[8 + 128 * index : 136 + 128 * index]
        slot = {"kind": record[1], "pointerLayer": record[2], "axis": record[3], "dpi": _u16(record, 4), "heldModifiers": record[6],
                "name": record[96 : 96 + record[8]].decode("utf-8"), "thresholdX": _u16(record, 32), "thresholdY": _u16(record, 34),
                "directions": [_tap(record, 36 + 4 * n) for n in range(4)],
                "buttons": [{"kind": record[52 + 6 * n], "modifiers": record[53 + 6 * n], "tap": _tap(record, 54 + 6 * n)} for n in range(3)]}
        if record[1] == 2:
            slot["scroll"] = [_u16(record, 70 + 2 * n) for n in range(7)] + list(record[84:90])
        else:
            slot["diagonals"] = [_tap(record, 70 + 4 * n) for n in range(4)]
            slot["emptyDirection"], slot["directionOutput"] = record[86], record[87]
        slots[record[0]] = slot
    return slots


def keymap(data: bytes) -> list:
    assert len(data) == LAYERS * MATRIX_KEYS * 2
    return [[f"0x{_u16(data, (layer * MATRIX_KEYS + position) * 2):04x}" for position in range(MATRIX_KEYS)] for layer in range(LAYERS)]


def macros(data: bytes) -> list:
    """The 128 macro streams; bank capacity past the last one is geometry, not content."""
    streams, offset = [], 0
    for _ in range(MACRO_SLOTS):
        end = data.index(0, offset)
        streams.append(data[offset:end].hex())
        offset = end + 1
    return streams


DECODERS = {"rgb": rgb, "behaviors": behaviors, "combos": combos, "settings": settings, "pointing": pointing, "keymap": keymap, "macros": macros}


def logical(domains: dict) -> dict:
    """Every domain present, by name, in logical form, JSON-normalized."""
    import json

    value = {DOMAIN_NAMES[domain]: DECODERS[DOMAIN_NAMES[domain]](payload) for domain, (_, payload) in sorted(domains.items())}
    return json.loads(json.dumps(value))
