#!/usr/bin/env python3
"""Build the host-layout fixture and its C tables.

The raw sources under tools/host_layouts/sources/ describe what each key of a
layout types. This tool derives, for every character a layout can type, the
one canonical key sequence that types it, writes the versioned fixture
tests/fixtures/host_layouts_v1.json and generates the firmware's C tables
from it. See docs/architecture/host-layouts-v1.md.

  python3 tools/host_layouts/build.py --write          # sources -> fixture -> C
  python3 tools/host_layouts/build.py --check          # fail if either is stale
  python3 tools/host_layouts/build.py --import-qmk QMK # refresh sources/qmk.json

--check and --write need only the committed sources; refreshing the sources
needs QMK (Windows layouts), a Mac (extract_macos.swift) or libxkbcommon
(extract_xkb.sh).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ROOT / "tools" / "host_layouts" / "sources"
FIXTURE = ROOT / "tests" / "fixtures" / "host_layouts_v1.json"
TABLES = ROOT / "users" / "noah" / "lib" / "macro" / "host_layout_tables.c"

# Basic keycodes a layout is read for, with their HID usage values.
KEYCODES = {
    **{f"KC_{chr(ord('A') + i)}": 0x04 + i for i in range(26)},
    **{f"KC_{(i + 1) % 10}": 0x1E + i for i in range(10)},
    "KC_ENT": 0x28, "KC_TAB": 0x2B, "KC_SPC": 0x2C, "KC_MINS": 0x2D, "KC_EQL": 0x2E,
    "KC_LBRC": 0x2F, "KC_RBRC": 0x30, "KC_BSLS": 0x31, "KC_NUHS": 0x32, "KC_SCLN": 0x33,
    "KC_QUOT": 0x34, "KC_GRV": 0x35, "KC_COMM": 0x36, "KC_DOT": 0x37, "KC_SLSH": 0x38,
    "KC_NUBS": 0x64,
}
LAYOUT_KEYS = [name for name in KEYCODES if name not in ("KC_ENT", "KC_TAB")]
LAYERS = ("{}", "S({})", "ALGR({})", "S(ALGR({}))")
SHIFT, ALTGR = 0x0100, 0x0200

# The catalogue. IDs are stored in the keyboard's settings and never reused.
CATALOGUE = [
    {"id": 0, "slug": "us", "name": "US", "os": "any", "source": "qmk"},
    {"id": 1, "slug": "macos-abc", "name": "ABC", "os": "macos", "source": "macos"},
    {"id": 2, "slug": "macos-dutch", "name": "Dutch", "os": "macos", "source": "macos"},
    {"id": 3, "slug": "macos-unicode-hex-input", "name": "Unicode Hex Input", "os": "macos", "source": "macos", "unicodeHexInput": True},
    {"id": 4, "slug": "macos-british", "name": "British", "os": "macos", "source": "macos"},
    {"id": 5, "slug": "macos-german", "name": "German", "os": "macos", "source": "macos"},
    {"id": 6, "slug": "macos-french", "name": "French", "os": "macos", "source": "macos"},
    {"id": 7, "slug": "windows-us-international", "name": "US International", "os": "windows", "source": "qmk"},
    {"id": 8, "slug": "windows-uk", "name": "United Kingdom", "os": "windows", "source": "qmk"},
    {"id": 9, "slug": "windows-german", "name": "German", "os": "windows", "source": "qmk"},
    {"id": 10, "slug": "windows-french", "name": "French", "os": "windows", "source": "qmk"},
    {"id": 11, "slug": "linux-us-international", "name": "US International", "os": "linux", "source": "linux"},
    {"id": 12, "slug": "linux-uk", "name": "United Kingdom", "os": "linux", "source": "linux"},
    {"id": 13, "slug": "linux-german", "name": "German", "os": "linux", "source": "linux"},
    {"id": 14, "slug": "linux-french", "name": "French", "os": "linux", "source": "linux"},
]

# US is the layout QMK's send_string assumes; tests/host/host_layout_test.c
# checks it against QMK's own tables.
US_KEYS = dict(zip(
    [f"KC_{chr(ord('A') + i)}" for i in range(26)] + [f"KC_{(i + 1) % 10}" for i in range(10)]
    + ["KC_SPC", "KC_MINS", "KC_EQL", "KC_LBRC", "KC_RBRC", "KC_BSLS", "KC_SCLN", "KC_QUOT", "KC_GRV", "KC_COMM", "KC_DOT", "KC_SLSH"],
    [(c, c.upper()) for c in "abcdefghijklmnopqrstuvwxyz"]
    + list(zip("1234567890", "!@#$%^&*()"))
    + [(" ", " "), ("-", "_"), ("=", "+"), ("[", "{"), ("]", "}"), ("\\", "|"), (";", ":"), ("'", '"'), ("`", "~"), (",", "<"), (".", ">"), ("/", "?")],
))

# Windows layouts come from QMK's keycodes_<layout> data, which marks dead keys
# but not what they compose; see the spec's derivation rules.
QMK_WINDOWS = {
    "windows-us-international": "keycodes_us_international_0.0.1.hjson",
    "windows-uk": "keycodes_uk_0.0.1.hjson",
    "windows-german": "keycodes_german_0.0.1.hjson",
    "windows-french": "keycodes_french_0.0.1.hjson",
}
WINDOWS_COMBINING = {"´": "́", "`": "̀", "^": "̂", "~": "̃", "¨": "̈", "¸": "̧"}
# On US International the dead acute and diaeresis keys are the apostrophe and
# quote keys, and type those characters before a space.
WINDOWS_DEAD_SPACE = {"windows-us-international": {"KC_QUOT": "'", "S(KC_QUOT)": '"'}}


def fail(message: str) -> None:
    print(f"host_layouts: {message}", file=sys.stderr)
    sys.exit(1)


def stroke(key: str, layer: int) -> str:
    return LAYERS[layer].format(key)


def parse_stroke(text: str) -> tuple[int, int]:
    match = re.fullmatch(r"(S\()?(ALGR\()?(KC_\w+?)\)*", text)
    if not match or match.group(3) not in KEYCODES:
        fail(f"bad stroke {text!r}")
    shift, altgr, key = bool(match.group(1)), bool(match.group(2)), match.group(3)
    if text != (f"S({'ALGR(' + key + ')' if altgr else key})" if shift else (f"ALGR({key})" if altgr else key)):
        fail(f"noncanonical stroke {text!r}")
    return KEYCODES[key], (SHIFT if shift else 0) | (ALTGR if altgr else 0)


def encode_stroke(text: str) -> int:
    value, mods = parse_stroke(text)
    return value | mods


def printable(text: str) -> bool:
    return len(text) == 1 and not (ord(text) < 0x20 or 0x7F <= ord(text) < 0xA0)


# --- QMK import ------------------------------------------------------------

def qmk_entries(path: Path) -> dict[str, tuple[str, str]]:
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(r'"([^"]+)":\s*\{\s*"key":\s*"([^"]+)",\s*"label":\s*"((?:[^"\\]|\\.)*)"')
    return {expr: (name, json.loads(f'"{label}"')) for expr, name, label in pattern.findall(text)}


def import_windows(path: Path, slug: str) -> dict:
    entries = qmk_entries(path)
    alias = {name: expr for expr, (name, _) in entries.items() if expr.startswith("KC_")}
    keys: dict[str, list] = {key: [None, None, None, None] for key in LAYOUT_KEYS}
    for expr, (_, label) in entries.items():
        match = re.fullmatch(r"(S\()?(ALGR\()?(\w+)\)*", expr)
        base = match.group(3)
        base = alias.get(base, base)
        if base not in keys:
            continue
        layer = (1 if match.group(1) else 0) | (2 if match.group(2) else 0)
        dead = re.fullmatch(r"(.) \(dead\)", label)
        keys[base][layer] = {"dead": stroke(base, layer), "accent": dead.group(1)} if dead else label
    for key, row in keys.items():
        # QMK names letters, AltGr letters included, by their capital; the
        # layout types the small letter, and the capital with Shift.
        for plain in (0, 2):
            cell = row[plain]
            if isinstance(cell, str) and len(cell) == 1 and cell.isupper() and cell.lower() != cell:
                row[plain], row[plain + 1] = cell.lower(), row[plain + 1] if row[plain + 1] is not None else cell
    keys["KC_SPC"][:2] = [" ", " "]
    for one, other in (("KC_BSLS", "KC_NUHS"), ("KC_NUHS", "KC_BSLS")):
        # Windows sends both usages as the same scan code.
        if all(cell is None for cell in keys[one]):
            keys[one] = [dict(cell, dead=stroke(one, i)) if isinstance(cell, dict) else cell for i, cell in enumerate(keys[other])]
    dead: dict[str, dict] = {}
    for key, row in keys.items():
        for layer, cell in enumerate(row):
            if not isinstance(cell, dict):
                continue
            accent = cell.pop("accent")
            compose = {}
            combining = WINDOWS_COMBINING.get(accent)
            for base_key, base_row in keys.items():
                for base_layer in (0, 1):
                    char = base_row[base_layer]
                    if combining and isinstance(char, str) and char.isalpha():
                        result = unicodedata.normalize("NFC", char + combining)
                        # Only Latin-1 letters, which every Windows layout here composes.
                        if len(result) == 1 and 0xC0 <= ord(result) <= 0xFF:
                            compose[stroke(base_key, base_layer)] = result
            space = WINDOWS_DEAD_SPACE.get(slug, {}).get(cell["dead"], accent)
            dead[cell["dead"]] = {"space": space, "compose": compose}
    return {"source": f"QMK data/constants/keycodes/extras/{path.name}", "keys": keys, "dead": dead}


def import_qmk(qmk_root: Path) -> None:
    extras = qmk_root / "data" / "constants" / "keycodes" / "extras"
    layouts = {"us": {"source": "US, as QMK send_string assumes", "keys": {k: [*US_KEYS.get(k, (None, None)), None, None] for k in LAYOUT_KEYS}, "dead": {}}}
    for slug, name in QMK_WINDOWS.items():
        layouts[slug] = import_windows(extras / name, slug)
    write_json(SOURCES / "qmk.json", {"generator": "tools/host_layouts/build.py --import-qmk", "layouts": layouts})


# --- Derivation ------------------------------------------------------------

def stroke_cost(text: str) -> tuple:
    value, mods = parse_stroke(text)
    return (bin(mods).count("1"), value, mods)


def derive_chars(raw: dict) -> dict[str, list[str]]:
    candidates: dict[str, list[list[str]]] = {"\t": [["KC_TAB"]], "\n": [["KC_ENT"]]}
    for key, row in raw["keys"].items():
        for layer, cell in enumerate(row):
            if isinstance(cell, str) and printable(cell):
                candidates.setdefault(cell, []).append([stroke(key, layer)])
    for dead_stroke, entry in raw["dead"].items():
        if entry["space"] and printable(entry["space"]):
            candidates.setdefault(entry["space"], []).append([dead_stroke, "KC_SPC"])
        for base, char in entry["compose"].items():
            if printable(char):
                candidates.setdefault(char, []).append([dead_stroke, base])

    def cost(sequence: list[str]) -> tuple:
        return (len(sequence), sum(stroke_cost(s)[0] for s in sequence), [stroke_cost(s)[1:] for s in sequence])

    return {char: min(options, key=cost) for char, options in sorted(candidates.items(), key=lambda item: ord(item[0]))}


def build_fixture() -> dict:
    sources = {name: read_json(SOURCES / f"{name}.json") for name in ("qmk", "macos", "linux")}
    layouts = []
    for entry in CATALOGUE:
        raw = sources[entry["source"]]["layouts"][entry["slug"]]
        keys = {key: raw["keys"][key] for key in LAYOUT_KEYS}
        dead = {stroke_text: raw["dead"][stroke_text] for stroke_text in sorted(raw["dead"], key=lambda s: stroke_cost(s)[1:])}
        layout = {k: v for k, v in entry.items() if k != "source"}
        layout["source"] = raw["source"] + (f"; {sources[entry['source']]['system']}" if "system" in sources[entry["source"]] else "")
        layout["keys"] = keys
        layout["dead"] = dead
        layout["chars"] = derive_chars({"keys": keys, "dead": dead})
        layouts.append(layout)
    return {
        "format": 1,
        "generator": "tools/host_layouts/build.py",
        "spec": "docs/architecture/host-layouts-v1.md",
        "layouts": layouts,
    }


# --- Output ----------------------------------------------------------------

def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def render_json(value: dict) -> str:
    return json.dumps(value, ensure_ascii=False, indent=1) + "\n"


def write_json(path: Path, value: dict) -> None:
    path.write_text(render_json(value), encoding="utf-8")


def render_tables(fixture: dict) -> str:
    lines = [
        "// Generated by tools/host_layouts/build.py from tests/fixtures/host_layouts_v1.json.",
        "// Do not edit; see docs/architecture/host-layouts-v1.md.",
        "",
        '#include "host_layout.h"',
        "",
    ]
    for layout in fixture["layouts"]:
        lines.append(f"// {layout['id']}: {layout['slug']}")
        lines.append(f"static const host_layout_char_t host_layout_{layout['id']}_chars[] = {{")
        for char, sequence in layout["chars"].items():
            strokes = [encode_stroke(s) for s in sequence] + [0]
            lines.append(f"    {{0x{ord(char):06X}u, {{0x{strokes[0]:04X}u, 0x{strokes[1]:04X}u}}}},")
        lines.append("};")
        lines.append("")
    lines.append("const host_layout_t host_layouts[] = {")
    for layout in fixture["layouts"]:
        flags = " | ".join(name for name, on in (("HOST_LAYOUT_FLAG_UNICODE_HEX_INPUT", layout.get("unicodeHexInput")), ("HOST_LAYOUT_FLAG_MACOS", layout["os"] == "macos")) if on) or "0u"
        count = len(layout["chars"])
        lines.append(f"    {{.id = {layout['id']}u, .flags = {flags}, .count = {count}u, .chars = host_layout_{layout['id']}_chars}},")
    lines.append("};")
    lines.append("")
    lines.append(f"const uint8_t host_layout_table_count = {len(fixture['layouts'])}u;")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", action="store_true")
    group.add_argument("--check", action="store_true")
    group.add_argument("--import-qmk", type=Path, metavar="QMK_ROOT")
    args = parser.parse_args()
    if args.import_qmk:
        import_qmk(args.import_qmk)
        return
    fixture = build_fixture()
    outputs = {FIXTURE: render_json(fixture), TABLES: render_tables(fixture)}
    if args.write:
        for path, text in outputs.items():
            path.write_text(text, encoding="utf-8")
        return
    stale = [str(path.relative_to(ROOT)) for path, text in outputs.items() if not path.exists() or path.read_text(encoding="utf-8") != text]
    if stale:
        fail(f"stale generated files: {', '.join(stale)}; run python3 tools/host_layouts/build.py --write")


if __name__ == "__main__":
    main()
