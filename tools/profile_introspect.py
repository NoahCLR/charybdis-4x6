#!/usr/bin/env python3
"""Generate a diffable authored-profile summary from keymap source data."""

from __future__ import annotations

import argparse
import ast
import difflib
import html
import os
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import NoReturn

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent

KEYMAP_FILE = REPO_ROOT / "keyboards" / "bastardkb" / "charybdis" / "4x6" / "keymaps" / "noah" / "keymap.c"
CONFIG_FILE = REPO_ROOT / "keyboards" / "bastardkb" / "charybdis" / "4x6" / "keymaps" / "noah" / "config.h"
USER_CONFIG_FILE = REPO_ROOT / "users" / "noah" / "config.h"
RGB_CONFIG_FILE = REPO_ROOT / "keyboards" / "bastardkb" / "charybdis" / "4x6" / "keymaps" / "noah" / "rgb_config.c"
PD_MODE_MANIFEST_FILE = REPO_ROOT / "users" / "noah" / "lib" / "pointing" / "defs" / "pd_mode_manifest.h"

DOCS_DIR = REPO_ROOT / "docs"
MEDIA_DIR = DOCS_DIR / "media"
MARKDOWN_OUTPUT = DOCS_DIR / "KEYMAP-OVERVIEW.md"
ASSET_OUTPUT_DIR = MEDIA_DIR / "profile-introspection"
RETIRED_OUTPUT_DIRS = [
    MEDIA_DIR / "generated",
    DOCS_DIR / "generated",
]
RETIRED_MARKDOWN_OUTPUTS = [output_dir / "KEYMAP-OVERVIEW.md" for output_dir in RETIRED_OUTPUT_DIRS]
RETIRED_ASSET_OUTPUT_DIRS = [output_dir / "profile-introspection-assets" for output_dir in RETIRED_OUTPUT_DIRS]
LAYER_IMAGE_PREFIX = "profile-layer-"
COLOR_SWATCH_PREFIX = "profile-color-swatch-"
RETIRED_JSON_OUTPUTS = [output_dir / "profile-summary.json" for output_dir in RETIRED_OUTPUT_DIRS]
RETIRED_JSON_OUTPUTS.extend(asset_dir / "profile-summary.json" for asset_dir in RETIRED_ASSET_OUTPUT_DIRS)

MARKDOWN_HEADER = "<!-- Generated file. Do not edit by hand. -->\n"

DISPLAY_ALIASES = {
    "_______": "TRNS",
    "XXXXXXX": "NO",
    "LEFT_THUMB": "LTHUMB",
    "RIGHT_THUMB": "RTHUMB",
}

KC_DISPLAY_ALIASES = {
    "ESCAPE": "ESC",
    "ESC": "ESC",
    "ENTER": "ENT",
    "ENT": "ENT",
    "INSERT": "INS",
    "INS": "INS",
    "HOME": "HOME",
    "END": "END",
    "PGUP": "PGUP",
    "PGDN": "PGDN",
    "SPACE": "SPC",
    "SPC": "SPC",
    "BACKSPACE": "BSPC",
    "BSPC": "BSPC",
    "DELETE": "DEL",
    "DEL": "DEL",
    "CAPS": "CAPS",
    "TAB": "TAB",
    "APP": "APP",
    "MENU": "MENU",
    "PSCR": "PSCR",
    "PRINT_SCREEN": "PSCR",
    "SCRL": "SCRL",
    "SCROLL_LOCK": "SCRL",
    "PAUS": "PAUSE",
    "PAUSE": "PAUSE",
    "NLCK": "NUM",
    "NUMLOCK": "NUM",
    "LEFT": "LEFT",
    "RIGHT": "RIGHT",
    "RGHT": "RIGHT",
    "UP": "UP",
    "DOWN": "DOWN",
    "MPLY": "PLAY",
    "MNXT": "NEXT",
    "MPRV": "PREV",
    "MUTE": "MUTE",
    "MSTP": "STOP",
    "MFFD": "FFWD",
    "MRWD": "RWD",
    "MSEL": "MEDIA",
    "EJCT": "EJECT",
    "VOLD": "VOL-",
    "VOLU": "VOL+",
    "BRID": "BRI-",
    "BRIU": "BRI+",
    "MS_UP": "MS UP",
    "MS_DOWN": "MS DOWN",
    "MS_LEFT": "MS LEFT",
    "MS_RIGHT": "MS RIGHT",
    "WH_U": "WH UP",
    "WH_D": "WH DOWN",
    "WH_L": "WH LEFT",
    "WH_R": "WH RIGHT",
    "BTN1": "BTN1",
    "BTN2": "BTN2",
    "BTN3": "BTN3",
    "BTN4": "BTN4",
    "BTN5": "BTN5",
    "BTN6": "BTN6",
    "BTN7": "BTN7",
    "BTN8": "BTN8",
    "ACL0": "ACC0",
    "ACL1": "ACC1",
    "ACL2": "ACC2",
    "COMM": ",",
    "DOT": ".",
    "SLSH": "/",
    "BSLS": "\\",
    "SCLN": ";",
    "QUOT": "'",
    "MINS": "-",
    "UNDS": "_",
    "LBRC": "[",
    "RBRC": "]",
    "LCBR": "{",
    "RCBR": "}",
    "LABK": "<",
    "RABK": ">",
    "COLN": ":",
    "PIPE": "|",
    "DQUO": '"',
    "GRV": "`",
    "TILD": "~",
    "EXLM": "!",
    "AT": "@",
    "HASH": "#",
    "DLR": "$",
    "PERC": "%",
    "CIRC": "^",
    "AMPR": "&",
    "ASTR": "*",
    "LPRN": "(",
    "RPRN": ")",
    "EQL": "=",
    "PEQL": "=",
    "PPLS": "P+",
    "PMNS": "P-",
    "PAST": "P*",
    "PSLS": "P/",
    "PDOT": "P.",
    "TRNS": "TRNS",
    "TRANSPARENT": "TRNS",
    "NO": "NO",
}

KC_SIDE_MODIFIERS = {
    "SHIFT": "SFT",
    "CTRL": "CTL",
    "ALT": "ALT",
    "GUI": "GUI",
}

TAP_COUNT_NAMES = {
    0: "single",
    1: "double",
    2: "triple",
    3: "quadruple",
    4: "quintuple",
}

SVG_BACKGROUND = "#2f2f2f"
SVG_CANVAS_WIDTH = 1120
SVG_CANVAS_HEIGHT = 620
SWATCH_WIDTH = 96
SWATCH_HEIGHT = 28
KEY_WIDTH = 58
KEY_HEIGHT = 58
KEY_RADIUS = 7
KEY_TEXT_HORIZONTAL_PADDING = 8
KEYBOARD_Y_OFFSET = 54
TITLE_X = 32
TITLE_Y = 40
SUBTITLE_Y = 68
LEFT_COLUMN_X = [36, 102, 168, 234, 300, 366]
LEFT_COLUMN_TOP_Y = [118, 118, 78, 54, 78, 78]
RIGHT_COLUMN_X = [698, 764, 830, 896, 962, 1028]
RIGHT_COLUMN_TOP_Y = [78, 78, 54, 78, 118, 118]
ROW_Y_STEP = 64
THUMB_VISUALS = {
    48: {"x": 328, "y": 354, "angle": 0},
    49: {"x": 396, "y": 350, "angle": 10},
    50: {"x": 468, "y": 358, "angle": 17},
    51: {"x": 576, "y": 358, "angle": -17},
    52: {"x": 648, "y": 350, "angle": -10},
    53: {"x": 398, "y": 432, "angle": 9},
    54: {"x": 468, "y": 446, "angle": 15},
    55: {"x": 578, "y": 432, "angle": -15},
}
LAYOUT_SLOT_COUNT = 56
MAIN_CLUSTER_KEYS_PER_HALF = 24
ROWS_PER_MAIN_CLUSTER = 4
COLUMNS_PER_MAIN_CLUSTER = 6


@dataclass
class BehaviorAction:
    helper: str
    action: str
    repeat_hz: int | None = None


@dataclass
class BehaviorStep:
    tap_count: int
    tap: BehaviorAction | None = None
    hold: BehaviorAction | None = None
    long_hold: BehaviorAction | None = None


@dataclass
class KeyBehavior:
    keycode: str
    tap_hold_term: int | None
    longer_hold_term: int | None
    multi_tap_term: int | None
    steps: list[BehaviorStep]


@dataclass
class MacroSlot:
    kind: str
    slot: int
    keycode: str
    payload: str
    empty: bool


@dataclass
class Combo:
    output: str
    inputs: list[str]


def die(message: str) -> NoReturn:
    raise SystemExit(message)


def read_text(path: Path) -> str:
    if not path.exists():
        die(f"missing required file: {path}")
    return path.read_text()


def strip_comments(text: str) -> str:
    result: list[str] = []
    i = 0
    quote: str | None = None

    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if quote is not None:
            result.append(ch)
            if ch == "\\" and i + 1 < len(text):
                result.append(text[i + 1])
                i += 2
                continue
            if ch == quote:
                quote = None
            i += 1
            continue

        if ch in {'"', "'"}:
            quote = ch
            result.append(ch)
            i += 1
            continue

        if ch == "/" and nxt == "/":
            i += 2
            while i < len(text) and text[i] != "\n":
                i += 1
            continue

        if ch == "/" and nxt == "*":
            i += 2
            while i + 1 < len(text) and (text[i] != "*" or text[i + 1] != "/"):
                i += 1
            i += 2
            continue

        result.append(ch)
        i += 1

    return "".join(result)


def normalize_expr(text: str) -> str:
    text = text.strip()
    if not text:
        return text

    result: list[str] = []
    i = 0
    quote: str | None = None

    while i < len(text):
        ch = text[i]

        if quote is not None:
            result.append(ch)
            if ch == "\\" and i + 1 < len(text):
                result.append(text[i + 1])
                i += 2
                continue
            if ch == quote:
                quote = None
            i += 1
            continue

        if ch in {'"', "'"}:
            quote = ch
            result.append(ch)
            i += 1
            continue

        if ch.isspace():
            i += 1
            continue

        result.append(ch)
        i += 1

    return "".join(result)


def find_matching(text: str, start_index: int, open_char: str, close_char: str) -> int:
    depth = 0
    quote: str | None = None
    i = start_index

    while i < len(text):
        ch = text[i]

        if quote is not None:
            if ch == "\\":
                i += 2
                continue
            if ch == quote:
                quote = None
            i += 1
            continue

        if ch in {'"', "'"}:
            quote = ch
            i += 1
            continue

        if ch == open_char:
            depth += 1
        elif ch == close_char:
            depth -= 1
            if depth == 0:
                return i

        i += 1

    die(f"unterminated {open_char}{close_char} block")


def split_top_level(text: str, delimiter: str = ",") -> list[str]:
    parts: list[str] = []
    start = 0
    paren_depth = 0
    brace_depth = 0
    bracket_depth = 0
    quote: str | None = None

    for index, ch in enumerate(text):
        if quote is not None:
            if ch == "\\":
                continue
            if ch == quote:
                quote = None
            continue

        if ch in {'"', "'"}:
            quote = ch
            continue

        if ch == "(":
            paren_depth += 1
            continue
        if ch == ")":
            paren_depth -= 1
            continue
        if ch == "{":
            brace_depth += 1
            continue
        if ch == "}":
            brace_depth -= 1
            continue
        if ch == "[":
            bracket_depth += 1
            continue
        if ch == "]":
            bracket_depth -= 1
            continue

        if ch == delimiter and paren_depth == 0 and brace_depth == 0 and bracket_depth == 0:
            part = text[start:index].strip()
            if part:
                parts.append(part)
            start = index + 1

    tail = text[start:].strip()
    if tail:
        parts.append(tail)
    return parts


def parse_designated_fields(body: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for part in split_top_level(body):
        if "=" not in part:
            continue
        field, value = part.split("=", 1)
        fields[field.strip()] = value.strip()
    return fields


def parse_macro_table(text: str, macro_name: str, invocation_name: str) -> list[list[str]]:
    lines = text.splitlines()
    start_index: int | None = None
    buffer: list[str] = []

    for index, line in enumerate(lines):
        if line.lstrip().startswith(f"#define {macro_name}("):
            start_index = index
            break

    if start_index is None:
        die(f"could not find macro block {macro_name}")

    for line in lines[start_index:]:
        buffer.append(line.rstrip())
        if not line.rstrip().endswith("\\"):
            break

    joined = "\n".join(buffer)
    joined = joined.split(")", 1)[1]
    body = joined.replace("\\\n", "\n")

    rows: list[list[str]] = []
    search = 0
    token = f"{invocation_name}("

    while True:
        start = body.find(token, search)
        if start == -1:
            break
        args_start = start + len(invocation_name)
        args_end = find_matching(body, args_start, "(", ")")
        inner = body[args_start + 1 : args_end]
        rows.append([normalize_expr(part) for part in split_top_level(inner)])
        search = args_end + 1

    return rows


def extract_initializer_body(text: str, pattern: str) -> str:
    match = re.search(pattern, text)
    if not match:
        die(f"could not find initializer for pattern: {pattern}")
    brace_start = text.find("{", match.end())
    if brace_start == -1:
        die(f"could not find opening brace for pattern: {pattern}")
    brace_end = find_matching(text, brace_start, "{", "}")
    return text[brace_start + 1 : brace_end]


def parse_keymap_custom_keycodes(text: str) -> list[str]:
    match = re.search(r"enum\s+keymap_custom_keycodes\s*\{(?P<body>.*?)\};", text, re.DOTALL)
    if not match:
        die("could not find enum keymap_custom_keycodes")

    keycodes: list[str] = []
    for entry in split_top_level(match.group("body")):
        token = entry.split("=", 1)[0].strip()
        if not token or token == "KEYMAP_CUSTOM_KEYCODE_SENTINEL":
            continue
        keycodes.append(token)
    return keycodes


def parse_config_layers(text: str) -> list[str]:
    match = re.search(r"enum\s+charybdis_keymap_layers\s*\{(?P<body>.*?)\};", text, re.DOTALL)
    if not match:
        die("could not find enum charybdis_keymap_layers")

    layers: list[str] = []
    for entry in split_top_level(match.group("body")):
        token = entry.split("=", 1)[0].strip()
        if not token or token == "LAYER_COUNT":
            continue
        layers.append(token)
    return layers


def parse_config_macros(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    pattern = re.compile(
        r"^\s*#\s*define\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)(?P<params>\([^)\n]*\))?(?:[ \t]+(?P<value>[^\n]+?))?[ \t]*$",
        re.MULTILINE,
    )
    for match in pattern.finditer(text):
        if match.group("params") is not None:
            continue
        name = match.group("name")
        value = match.group("value")
        values[name] = normalize_expr(value) if value is not None else "defined"
    return values


def merge_config_macros(*macro_sets: dict[str, str]) -> dict[str, str]:
    merged: dict[str, str] = {}
    for macro_set in macro_sets:
        merged.update(macro_set)
    return merged


def eval_numeric_expr(expr: str, known_values: dict[str, str]) -> int:
    normalized = normalize_expr(expr)
    if normalized in known_values:
        normalized = known_values[normalized]

    class ValueResolver(ast.NodeTransformer):
        def visit_Name(self, node: ast.Name) -> ast.AST:
            if node.id not in known_values:
                die(f"unknown numeric identifier in expression: {expr!r}")
            return ast.copy_location(ast.Constant(value=eval_numeric_expr(known_values[node.id], known_values)), node)

    try:
        tree = ast.parse(normalized, mode="eval")
    except SyntaxError as exc:
        die(f"could not parse numeric expression {expr!r}: {exc}")

    tree = ValueResolver().visit(tree)
    ast.fix_missing_locations(tree)

    def _eval(node: ast.AST) -> int:
        if isinstance(node, ast.Expression):
            return _eval(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
            return int(round(node.value))
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
            value = _eval(node.operand)
            return value if isinstance(node.op, ast.UAdd) else -value
        if isinstance(node, ast.BinOp) and isinstance(node.op, (ast.Add, ast.Sub, ast.Mult, ast.Div, ast.FloorDiv)):
            left = _eval(node.left)
            right = _eval(node.right)
            if isinstance(node.op, ast.Add):
                return left + right
            if isinstance(node.op, ast.Sub):
                return left - right
            if isinstance(node.op, ast.Mult):
                return left * right
            if right == 0:
                die(f"division by zero in expression: {expr!r}")
            return left // right
        die(f"unsupported numeric expression: {expr!r}")

    return _eval(tree)


def parse_hsv_expr(expr: str, known_values: dict[str, str]) -> dict[str, object]:
    normalized = normalize_expr(expr)
    if not normalized.startswith("HSV(") or not normalized.endswith(")"):
        die(f"expected HSV(...) color expression, got: {expr!r}")
    parts = split_top_level(normalized[4:-1])
    if len(parts) != 3:
        die(f"HSV(...) expected 3 arguments, got: {expr!r}")
    h = eval_numeric_expr(parts[0], known_values)
    s = eval_numeric_expr(parts[1], known_values)
    v = eval_numeric_expr(parts[2], known_values)
    return {"h": h, "s": s, "v": v, "hex": hsv_to_hex(h, s, v), "enabled": not (h == 0 and s == 0 and v == 0)}


def normalize_color_name(label: str) -> str:
    lowered = label.strip().lower().replace("-", " ")
    lowered = re.sub(r"[^a-z\s]", " ", lowered)
    return " ".join(lowered.split())


def comment_lines_to_text(comment_block: str) -> str:
    lines: list[str] = []
    for line in comment_block.splitlines():
        stripped = line.strip()
        if stripped.startswith("//"):
            lines.append(stripped[2:].strip())
    return " ".join(lines).strip()


def humanize_identifier(identifier: str) -> str:
    parts = [part for part in identifier.strip("_").split("_") if part]
    if not parts:
        return identifier
    return " ".join(part.capitalize() for part in parts)


def extract_color_name_from_comment(comment_text: str, known_names: set[str] | None = None) -> str | None:
    normalized = normalize_color_name(comment_text)
    if not normalized:
        return None

    if known_names:
        for name in sorted(known_names, key=len, reverse=True):
            if re.search(rf"\b{re.escape(name)}\b", normalized):
                return name

    return None


def hsv_to_hex(h: int, s: int, v: int) -> str:
    red, green, blue = qmk_preview_hsv_to_rgb(h, s, v)
    return "#{:02x}{:02x}{:02x}".format(red, green, blue)


def qmk_preview_hsv_to_rgb(h: int, s: int, v: int) -> tuple[int, int, int]:
    h = h % 256
    s = max(0, min(s, 255))
    v = max(0, min(v, 255))

    if v == 0:
        return 0, 0, 0

    preview_v = 255

    if s == 0:
        return preview_v, preview_v, preview_v

    region = (h * 6) // 255
    remainder = (h * 2 - region * 85) * 3
    p = (preview_v * (255 - s)) >> 8
    q = (preview_v * (255 - ((s * remainder) >> 8))) >> 8
    t = (preview_v * (255 - ((s * (255 - remainder)) >> 8))) >> 8

    if region in (0, 6):
        return preview_v, t, p
    if region == 1:
        return q, preview_v, p
    if region == 2:
        return p, preview_v, t
    if region == 3:
        return p, q, preview_v
    if region == 4:
        return t, p, preview_v
    return preview_v, p, q


def parse_pd_mode_colors(raw_text: str, known_values: dict[str, str]) -> list[dict[str, object]]:
    body = extract_initializer_body(raw_text, r"pd_mode_colors\[\]\s*=")
    entry_pattern = re.compile(r"\{(?P<body>.*?)\}\s*,\s*//\s*(?P<label>[^\n]+)", re.DOTALL)
    rows: list[dict[str, object]] = []

    for match in entry_pattern.finditer(body):
        fields = parse_designated_fields(strip_comments(match.group("body")))
        if ".pointing_mode" not in fields or ".color" not in fields or ".locality" not in fields:
            continue
        pointing_mode = normalize_expr(fields[".pointing_mode"])
        color = parse_hsv_expr(fields[".color"], known_values)
        locality = normalize_expr(fields[".locality"])
        rows.append(
            {
                "pointing_mode": pointing_mode,
                "color": color,
                "locality": locality,
                "locality_label": humanize_identifier(locality.removeprefix("RGB_")),
                "locality_meaning": pd_color_locality_description(locality),
                "preview_color": dict(color),
                "comment_color_name": normalize_color_name(match.group("label")) or None,
            }
        )

    return rows


def parse_pd_mode_manifest(text: str) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for row in parse_macro_table(text, "NOAH_PD_MODE_LIST", "PDM"):
        if len(row) != 8:
            die(f"unexpected pd mode manifest row: {row!r}")
        name, mode_keycode, _pointer_handler, _key_handler, _reset_fn, dpi_override, traits, lifecycle = row
        rows.append(
            {
                "name": name,
                "pointing_mode": f"PD_MODE_{name}",
                "mode_keycode": mode_keycode,
                "lock_keycode": f"{mode_keycode}_LOCK",
                "dpi_override": dpi_override,
                "traits": traits,
                "lifecycle": lifecycle,
            }
        )
    return rows


def resolve_preview_color(
    authored_color: dict[str, object],
    semantic_name: str | None,
    color_anchors: dict[str, dict[str, object]],
) -> dict[str, object]:
    if semantic_name is not None and semantic_name in color_anchors:
        return dict(color_anchors[semantic_name])
    return dict(authored_color)


def layer_color_mode_description(mode: str) -> str:
    descriptions = {
        "ALL_KEYS": "Tint every physical key with the authored layer color.",
        "KEYS_MAPPED_ON_THIS_LAYER_ONLY": "Tint only keys with a real mapping on that layer; transparent positions stay neutral so lower layers remain visible underneath.",
    }
    return descriptions.get(mode, "Unknown layer RGB render mode.")


def parse_layer_colors(text: str, known_values: dict[str, str]) -> list[dict[str, object]]:
    body = extract_initializer_body(text, r"layer_colors\[LAYER_COUNT\]\s*=")
    colors: list[dict[str, object]] = []

    for entry in split_top_level(body):
        if "=" not in entry:
            continue
        layer_expr, body_expr = entry.split("=", 1)
        layer_match = re.search(r"\[(?P<name>[A-Z_][A-Z0-9_]*)\]", layer_expr)
        if layer_match is None:
            die(f"could not parse layer color entry: {entry!r}")
        layer_name = layer_match.group("name")
        body_expr = body_expr.strip()
        if not (body_expr.startswith("{") and body_expr.endswith("}")):
            die(f"unexpected layer color body: {body_expr!r}")
        fields = parse_designated_fields(body_expr[1:-1].strip())
        colors.append(
            {
                "layer": layer_name,
                "color": parse_hsv_expr(fields[".color"], known_values),
                "mode": normalize_expr(fields[".mode"]),
            }
        )

    if not colors:
        die("no layer colors parsed from rgb_config.c")

    return colors


def automouse_fade_end_mode_description(mode: str) -> str:
    descriptions = {
        "FOLLOW_REAL_DESTINATION": "Fade to the real rendered board state that remains after the auto-mouse layer drops out.",
        "END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW": "Keep the real destination where layers still paint, but use `end_color` where the base RGB effect would otherwise show through.",
        "END_COLOR_ON_ALL_KEYS": "Use `end_color` as the fade destination on every key while the automouse renderer is active.",
    }
    return descriptions.get(mode, "Unknown auto-mouse fade destination mode.")


def pd_color_locality_description(locality: str) -> str:
    descriptions = {
        "RGB_RIGHT_HALF": "Paint the right half whenever the matching PD mode is active.",
        "RGB_LEFT_HALF": "Paint the left half whenever the matching PD mode is active.",
        "RGB_BOTH_HALVES": "Mirror the PD-mode overlay across both halves.",
        "RGB_KEY_HALF": "Paint the half or halves containing the key footprint that triggered the currently effective PD mode.",
        "RGB_KEYS_ONLY": "Paint only the key footprint that triggered the currently effective PD mode.",
    }
    return descriptions.get(locality, "Unknown PD-mode RGB locality.")


def combo_feedback_locality_description(locality: str) -> str:
    descriptions = {
        "RGB_BOTH_HALVES": "Mirror the steady combo color across both halves while any combo is active.",
        "RGB_KEY_HALF": "Paint the half or halves touched by the live combo footprint.",
        "RGB_KEYS_ONLY": "Paint only the exact keys that formed the currently active combo footprint.",
        "RGB_LEFT_HALF": "Always paint the left half for active combos.",
        "RGB_RIGHT_HALF": "Always paint the right half for active combos.",
    }
    return descriptions.get(locality, "Unknown combo feedback RGB locality.")


def parse_automouse_fade_end_config(raw_text: str, known_values: dict[str, str]) -> dict[str, object] | None:
    try:
        body = extract_initializer_body(raw_text, r"automouse_fade_end_config\s*=")
    except SystemExit:
        return None

    fields = parse_designated_fields(strip_comments(body))
    mode = fields.get(".mode")
    end_color = fields.get(".end_color")
    if mode is None or end_color is None:
        return None

    normalized_mode = normalize_expr(mode)
    parsed_end_color = parse_hsv_expr(end_color, known_values)
    return {
        "mode": normalized_mode,
        "label": humanize_identifier(normalized_mode),
        "meaning": automouse_fade_end_mode_description(normalized_mode),
        "end_color": parsed_end_color,
        "preview_color": dict(parsed_end_color),
    }


def parse_key_behavior_feedback_colors(
    raw_text: str,
    known_values: dict[str, str],
    color_anchors: dict[str, dict[str, object]],
) -> list[dict[str, object]]:
    try:
        body = extract_initializer_body(raw_text, r"key_behavior_feedback_colors\s*=")
    except SystemExit:
        return []

    field_pattern = re.compile(
        r"(?P<comments>(?:\s*//[^\n]*\n)*)\s*\.(?P<field>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?P<expr>HSV\([^)]*\))\s*,",
        re.MULTILINE,
    )

    colors: list[dict[str, object]] = []
    known_anchor_names = set(color_anchors)
    for match in field_pattern.finditer(body):
        field_name = match.group("field")
        if not field_name.endswith("_color"):
            continue
        authored_color = parse_hsv_expr(match.group("expr"), known_values)
        comment_text = comment_lines_to_text(match.group("comments"))
        semantic_name = extract_color_name_from_comment(comment_text, known_anchor_names)
        preview_color = resolve_preview_color(authored_color, semantic_name, color_anchors)
        colors.append(
            {
                "field": field_name,
                "label": humanize_identifier(field_name.removesuffix("_color")),
                "meaning": comment_text or "Authored key-behavior feedback color.",
                "color": authored_color,
                "preview_color": preview_color,
            }
        )

    return colors


def parse_combo_feedback_color(raw_text: str, known_values: dict[str, str]) -> dict[str, object] | None:
    try:
        body = extract_initializer_body(raw_text, r"combo_feedback_colors\s*=")
    except SystemExit:
        return None

    fields = parse_designated_fields(strip_comments(body))
    combo_color = fields.get(".color")
    if combo_color is None:
        return None

    authored_color = parse_hsv_expr(combo_color, known_values)
    return {
        "field": "color",
        "label": "Active Combo",
        "meaning": "Steady combo layer color while a combo chord stays active. Preview- or PD-owning combos can be routed underneath those state indicators, while unrelated combos remain above them.",
        "color": authored_color,
        "preview_color": dict(authored_color),
    }


def parse_combo_feedback_locality(raw_text: str) -> dict[str, object] | None:
    try:
        body = extract_initializer_body(raw_text, r"combo_feedback_colors\s*=")
    except SystemExit:
        return None

    fields = parse_designated_fields(strip_comments(body))
    locality = fields.get(".locality")
    if locality is None:
        return None

    normalized_locality = normalize_expr(locality)
    return {
        "locality": normalized_locality,
        "label": humanize_identifier(normalized_locality.removeprefix("RGB_")),
        "meaning": combo_feedback_locality_description(normalized_locality),
    }


def key_behavior_feedback_locality_description(locality: str) -> str:
    descriptions = {
        "RGB_BOTH_HALVES": "Repaint both halves whenever a key-behavior feedback state is active.",
        "RGB_KEY_HALF": "Repaint only the half that owns the key or tap series currently driving the feedback state.",
        "RGB_KEYS_ONLY": "Repaint only the specific key currently driving the feedback state.",
        "RGB_LEFT_HALF": "Always repaint the left half using the highest-priority active key-behavior feedback state.",
        "RGB_RIGHT_HALF": "Always repaint the right half using the highest-priority active key-behavior feedback state.",
    }
    return descriptions.get(locality, "Unknown key-behavior feedback RGB locality.")


def parse_key_behavior_feedback_locality(raw_text: str) -> dict[str, object] | None:
    try:
        body = extract_initializer_body(raw_text, r"key_behavior_feedback_colors\s*=")
    except SystemExit:
        return None

    fields = parse_designated_fields(strip_comments(body))
    locality = fields.get(".locality")
    if locality is None:
        return None

    normalized_locality = normalize_expr(locality)
    return {
        "locality": normalized_locality,
        "label": humanize_identifier(normalized_locality.removeprefix("RGB_")),
        "meaning": key_behavior_feedback_locality_description(normalized_locality),
    }


def parse_exported_rgb_led_groups(
    raw_text: str,
    export_macro: str,
    known_values: dict[str, str],
    semantic_field: bool = False,
) -> list[dict[str, object]]:
    text = strip_comments(raw_text)
    export_match = re.search(rf"\b{re.escape(export_macro)}\s*\(\s*(?P<table>[A-Za-z_][A-Za-z0-9_]*)\s*\)", text)
    if export_match is None:
        return []

    table_name = export_match.group("table")
    try:
        body = extract_initializer_body(text, rf"\b{re.escape(table_name)}\[\]\s*=")
    except SystemExit:
        return []

    rows: list[dict[str, object]] = []
    for entry in split_top_level(body):
        entry = entry.strip()
        if not (entry.startswith("{") and entry.endswith("}")):
            continue

        fields = parse_designated_fields(entry[1:-1].strip())
        color_expr = fields.get(".color")
        leds_expr = fields.get(".leds")
        count_expr = fields.get(".count")
        if color_expr is None or leds_expr is None or count_expr is None:
            continue

        color = parse_hsv_expr(color_expr, known_values)
        row: dict[str, object] = {
            "color": color,
            "preview_color": dict(color),
            "leds": normalize_expr(leds_expr),
            "count": normalize_expr(count_expr),
        }
        if semantic_field:
            row["semantic"] = normalize_expr(fields.get(".semantic", ""))
            row["label"] = humanize_identifier(row["semantic"].removeprefix("KEY_FEEDBACK_GROUP_"))
        else:
            row["label"] = "Combo Feedback Group"
        rows.append(row)

    return rows


def resolve_rgb_default_color(known_values: dict[str, str]) -> dict[str, object]:
    return {
        "h": eval_numeric_expr(known_values["RGB_MATRIX_DEFAULT_HUE"], known_values),
        "s": eval_numeric_expr(known_values["RGB_MATRIX_DEFAULT_SAT"], known_values),
        "v": eval_numeric_expr(known_values["RGB_MATRIX_DEFAULT_VAL"], known_values),
        "hex": hsv_to_hex(
            eval_numeric_expr(known_values["RGB_MATRIX_DEFAULT_HUE"], known_values),
            eval_numeric_expr(known_values["RGB_MATRIX_DEFAULT_SAT"], known_values),
            eval_numeric_expr(known_values["RGB_MATRIX_DEFAULT_VAL"], known_values),
        ),
        "enabled": True,
    }


def resolve_behavior_timing_defaults(known_values: dict[str, str]) -> dict[str, int]:
    return {
        "tap_hold": eval_numeric_expr(known_values["CUSTOM_TAP_HOLD_TERM"], known_values),
        "tap_hold_lt": eval_numeric_expr(known_values["TAPPING_TERM"], known_values),
        "long_hold": eval_numeric_expr(known_values["CUSTOM_LONGER_HOLD_TERM"], known_values),
        "multi_tap": eval_numeric_expr(known_values["CUSTOM_MULTI_TAP_TERM"], known_values),
    }


def finalize_layer_colors(layer_colors: list[dict[str, object]], rgb_default_color: dict[str, object]) -> list[dict[str, object]]:
    finalized: list[dict[str, object]] = []

    for row in layer_colors:
        color = row["color"]
        if row["layer"] == "LAYER_BASE" and not color["enabled"]:
            preview_color = dict(rgb_default_color)
            preview_source = "rgb_default"
        elif color["enabled"]:
            preview_color = dict(color)
            preview_source = "layer_override"
        else:
            preview_color = None
            preview_source = "no_override"

        finalized.append({**row, "preview_color": preview_color, "preview_source": preview_source})

    return finalized


def parse_macro_slots(rows: list[list[str]], kind: str) -> list[MacroSlot]:
    slots: list[MacroSlot] = []
    for row in rows:
        if len(row) != 2:
            die(f"expected 2 arguments in {kind} macro row, got: {row!r}")
        keycode, payload = row
        normalized_payload = payload.strip()
        if normalized_payload.startswith('"') and normalized_payload.endswith('"'):
            normalized_payload = normalized_payload[1:-1]
        slot_match = re.search(r"_(\d+)$", keycode)
        if slot_match is None:
            die(f"could not determine slot for macro keycode: {keycode}")
        slots.append(
            MacroSlot(
                kind=kind,
                slot=int(slot_match.group(1)),
                keycode=keycode,
                payload=normalized_payload,
                empty=normalized_payload == "",
            )
        )
    return slots


def parse_key_behaviors(text: str) -> list[KeyBehavior]:
    body = extract_initializer_body(text, r"key_behaviors\[\]\s*=")
    behaviors: list[KeyBehavior] = []

    for entry in split_top_level(body):
        if not entry.startswith("{") or not entry.endswith("}"):
            die(f"unexpected key behavior entry: {entry!r}")
        fields = parse_designated_fields(entry[1:-1].strip())
        keycode = normalize_expr(fields[".keycode"])
        tap_counts_body = fields.get(".tap_counts", "{}")
        steps = parse_behavior_steps(tap_counts_body)
        behaviors.append(
            KeyBehavior(
                keycode=keycode,
                tap_hold_term=parse_optional_int(fields.get(".tap_hold_term")),
                longer_hold_term=parse_optional_int(fields.get(".longer_hold_term")),
                multi_tap_term=parse_optional_int(fields.get(".multi_tap_term")),
                steps=steps,
            )
        )

    return behaviors


def parse_optional_int(value: str | None) -> int | None:
    if value is None:
        return None
    normalized = normalize_expr(value)
    if not normalized:
        return None
    try:
        return int(normalized, 10)
    except ValueError:
        return None


def parse_behavior_steps(tap_counts_body: str) -> list[BehaviorStep]:
    normalized = tap_counts_body.strip()
    if not (normalized.startswith("{") and normalized.endswith("}")):
        die(f"unexpected tap_counts initializer: {tap_counts_body!r}")

    steps: list[BehaviorStep] = []
    inner = normalized[1:-1].strip()
    if not inner:
        return steps

    for entry in split_top_level(inner):
        if "=" not in entry:
            continue
        index_expr, body_expr = entry.split("=", 1)
        index_match = re.search(r"\[(\d+)\]", index_expr)
        if index_match is None:
            die(f"could not parse tap count index from: {entry!r}")
        tap_count = int(index_match.group(1))
        body_expr = body_expr.strip()
        if not (body_expr.startswith("{") and body_expr.endswith("}")):
            die(f"unexpected behavior step body: {body_expr!r}")
        fields = parse_designated_fields(body_expr[1:-1].strip())
        steps.append(
            BehaviorStep(
                tap_count=tap_count,
                tap=parse_behavior_action(fields.get(".tap")),
                hold=parse_behavior_action(fields.get(".hold")),
                long_hold=parse_behavior_action(fields.get(".long_hold")),
            )
        )

    return steps


def parse_behavior_action(value: str | None) -> BehaviorAction | None:
    if value is None:
        return None
    normalized = normalize_expr(value)
    if not normalized:
        return None
    open_index = normalized.find("(")
    if open_index == -1 or not normalized.endswith(")"):
        die(f"unexpected behavior helper expression: {value!r}")
    helper = normalized[:open_index]
    inner = normalized[open_index + 1 : -1]
    args = split_top_level(inner)

    if helper == "REPEAT_WHILE_HELD":
        if len(args) != 2:
            die(f"REPEAT_WHILE_HELD expected 2 args, got {args!r}")
        return BehaviorAction(helper=helper, action=normalize_expr(args[0]), repeat_hz=int(normalize_expr(args[1]), 10))

    if len(args) != 1:
        die(f"{helper} expected 1 arg, got {args!r}")
    return BehaviorAction(helper=helper, action=normalize_expr(args[0]))


def parse_combos(text: str) -> list[Combo]:
    combos: list[Combo] = []
    for row in parse_macro_table(text, "COMBOS", "COMBO"):
        if len(row) != 2:
            die(f"unexpected combo row: {row!r}")
        output, inputs_expr = row
        normalized_inputs = inputs_expr.strip()
        if normalized_inputs.startswith("(") and normalized_inputs.endswith(")"):
            normalized_inputs = normalized_inputs[1:-1]
        inputs = [normalize_expr(item) for item in split_top_level(normalized_inputs)]
        combos.append(Combo(output=output, inputs=inputs))
    return combos


def derive_layout_slot(layout_index: int) -> tuple[str, int | None, int | None]:
    main_cluster_key_count = MAIN_CLUSTER_KEYS_PER_HALF * 2
    keys_per_main_row = COLUMNS_PER_MAIN_CLUSTER * 2
    if layout_index < main_cluster_key_count:
        layout_row = layout_index // keys_per_main_row
        row_index = layout_index % keys_per_main_row
        if row_index < COLUMNS_PER_MAIN_CLUSTER:
            return "left", layout_row, row_index
        return "right", layout_row, row_index - COLUMNS_PER_MAIN_CLUSTER

    if layout_index < LAYOUT_SLOT_COUNT:
        return "thumb", None, None

    die(f"unexpected layout slot index: {layout_index}")


def parse_layers(text: str, known_behaviors: set[str]) -> list[dict[str, object]]:
    array_body = extract_initializer_body(text, r"keymaps\[\]\[MATRIX_ROWS\]\[MATRIX_COLS\]\s*=")
    layers: list[dict[str, object]] = []
    search = 0

    while True:
        match = re.search(r"\[(?P<name>[A-Z_][A-Z0-9_]*)\]\s*=\s*LAYOUT\s*\(", array_body[search:])
        if not match:
            break
        layer_name = match.group("name")
        absolute_start = search + match.start()
        open_paren = array_body.find("(", absolute_start)
        close_paren = find_matching(array_body, open_paren, "(", ")")
        items = [normalize_expr(item) for item in split_top_level(array_body[open_paren + 1 : close_paren])]
        if len(items) != LAYOUT_SLOT_COUNT:
            die(f"{layer_name} expected {LAYOUT_SLOT_COUNT} layout entries, got {len(items)}")
        positions: list[dict[str, object]] = []
        for layout_index, keycode in enumerate(items):
            cluster, layout_row, layout_col = derive_layout_slot(layout_index)
            positions.append(
                {
                    "layout_index": layout_index,
                    "cluster": cluster,
                    "layout_row": layout_row,
                    "layout_col": layout_col,
                    "keycode": keycode,
                    "display": display_token(keycode),
                    "has_key_behavior": behavior_lookup_key(keycode) in known_behaviors,
                }
            )
        layers.append({"name": layer_name, "positions": positions})
        search = close_paren + 1

    if not layers:
        die("no layers parsed from keymaps[][]")
    return layers


def short_layer_name(name: str) -> str:
    return name.removeprefix("LAYER_")


def short_mode_name(name: str) -> str:
    return name.removesuffix("_MODE")


def display_kc_suffix(suffix: str) -> str:
    if suffix in KC_DISPLAY_ALIASES:
        return KC_DISPLAY_ALIASES[suffix]

    modifier_match = re.fullmatch(r"(LEFT|RIGHT)_(SHIFT|CTRL|ALT|GUI)", suffix)
    if modifier_match:
        side, modifier = modifier_match.groups()
        return side[0] + KC_SIDE_MODIFIERS[modifier]

    shorthand_modifier_match = re.fullmatch(r"(L|R)(SFT|CTL|ALT|GUI)", suffix)
    if shorthand_modifier_match:
        return suffix

    if re.fullmatch(r"[A-Z0-9]", suffix):
        return suffix

    if re.fullmatch(r"P[0-9]", suffix):
        return suffix

    return suffix


def display_token(token: str) -> str:
    normalized = normalize_expr(token)
    if normalized in DISPLAY_ALIASES:
        return DISPLAY_ALIASES[normalized]
    if normalized.startswith("VIA_MACRO_"):
        return "VIA" + normalized.rsplit("_", 1)[1]
    if normalized.startswith("MACRO_"):
        return "MACRO" + normalized.rsplit("_", 1)[1]
    if normalized.startswith("KC_"):
        return display_kc_suffix(normalized[3:])
    if normalized.startswith("LT(") and normalized.endswith(")"):
        inner = split_top_level(normalized[3:-1])
        if len(inner) == 2:
            inner_label = display_token(inner[1])
            if inner_label == "/":
                inner_label = "SLSH"
            return f"LT[{short_layer_name(inner[0])}]/{inner_label}"
    if normalized.startswith("MO(") and normalized.endswith(")"):
        return f"MO[{short_layer_name(normalized[3:-1])}]"
    if normalized.startswith("LOCK_LAYER(") and normalized.endswith(")"):
        return f"LOCK[{short_layer_name(normalized[11:-1])}]"
    if normalized.startswith("S(") and normalized.endswith(")"):
        return f"S({display_token(normalized[2:-1])})"
    if normalized.startswith("A(") and normalized.endswith(")"):
        return f"A({display_token(normalized[2:-1])})"
    if normalized.startswith("G(") and normalized.endswith(")"):
        return f"G({display_token(normalized[2:-1])})"
    if normalized.startswith("LAG(") and normalized.endswith(")"):
        return f"LAG({display_token(normalized[4:-1])})"
    if normalized.startswith("LSG(") and normalized.endswith(")"):
        return f"LSG({display_token(normalized[4:-1])})"
    if normalized.startswith("LCAG(") and normalized.endswith(")"):
        return f"LCAG({display_token(normalized[5:-1])})"
    if normalized.endswith("_MODE"):
        return short_mode_name(normalized)
    return normalized


def behavior_lookup_key(token: str) -> str:
    return display_token(token)


def collect_macro_usages(
    layers: list[dict[str, object]],
    behaviors: list[KeyBehavior],
    combos: list[Combo],
    macros: list[MacroSlot],
) -> dict[str, list[dict[str, object]]]:
    usage_map = {macro.keycode: [] for macro in macros}

    for layer in layers:
        layer_name = layer["name"]
        for position in layer["positions"]:
            keycode = position["keycode"]
            if keycode in usage_map:
                usage_map[keycode].append(
                    {
                        "kind": "layer",
                        "layer": layer_name,
                        "display": position["display"],
                        "layout_index": position["layout_index"],
                        "cluster": position["cluster"],
                        "layout_row": position["layout_row"],
                        "layout_col": position["layout_col"],
                    }
                )

    for behavior in behaviors:
        for step in behavior.steps:
            for field_name in ("tap", "hold", "long_hold"):
                action = getattr(step, field_name)
                if action is None or action.action not in usage_map:
                    continue
                usage_map[action.action].append(
                    {
                        "kind": "behavior",
                        "owner": behavior.keycode,
                        "tap_count": step.tap_count,
                        "field": field_name,
                    }
                )

    for index, combo in enumerate(combos):
        if combo.output in usage_map:
            usage_map[combo.output].append({"kind": "combo_output", "combo_index": index})
        for input_key in combo.inputs:
            if input_key in usage_map:
                usage_map[input_key].append({"kind": "combo_input", "combo_index": index})

    return usage_map


def build_profile_model() -> dict[str, object]:
    keymap_config_text = strip_comments(read_text(CONFIG_FILE))
    userspace_config_text = strip_comments(read_text(USER_CONFIG_FILE))
    rgb_config_raw_text = read_text(RGB_CONFIG_FILE)
    rgb_config_text = strip_comments(rgb_config_raw_text)
    keymap_text = strip_comments(read_text(KEYMAP_FILE))
    pd_mode_manifest_text = read_text(PD_MODE_MANIFEST_FILE)

    layers = parse_config_layers(keymap_config_text)
    userspace_config_macros = parse_config_macros(userspace_config_text)
    keymap_config_macros = parse_config_macros(keymap_config_text)
    config_macros = merge_config_macros(userspace_config_macros, keymap_config_macros)
    rgb_automouse_gradient_enabled = "RGB_AUTOMOUSE_GRADIENT_ENABLE" in config_macros
    rgb_key_behavior_feedback_enabled = "RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE" in config_macros
    rgb_pd_mode_active_half_enabled = "RGB_PD_MODE_ACTIVE_HALF_ENABLE" in config_macros
    timing_defaults = resolve_behavior_timing_defaults(config_macros)
    layer_colors = finalize_layer_colors(parse_layer_colors(rgb_config_text, config_macros), resolve_rgb_default_color(config_macros))
    keymap_custom_keycodes = parse_keymap_custom_keycodes(keymap_text)
    via_macros = parse_macro_slots(parse_macro_table(keymap_text, "VIA_MACROS", "MACRO"), kind="via")
    hardcoded_macros = parse_macro_slots(parse_macro_table(keymap_text, "HARDCODED_MACROS", "MACRO"), kind="hardcoded")
    behaviors = parse_key_behaviors(keymap_text)
    combos = parse_combos(keymap_text)
    parsed_layers = parse_layers(keymap_text, known_behaviors={behavior_lookup_key(behavior.keycode) for behavior in behaviors})
    pd_modes = parse_pd_mode_manifest(pd_mode_manifest_text)
    pd_mode_colors = parse_pd_mode_colors(rgb_config_raw_text, config_macros)
    pd_mode_color_anchors = {
        row["comment_color_name"]: row["preview_color"]
        for row in pd_mode_colors
        if row["comment_color_name"] is not None
    }
    combo_feedback_color = parse_combo_feedback_color(rgb_config_raw_text, config_macros)
    combo_feedback_locality = parse_combo_feedback_locality(rgb_config_raw_text)
    combo_feedback_led_groups = parse_exported_rgb_led_groups(
        rgb_config_raw_text,
        "EXPORT_COMBO_FEEDBACK_LED_GROUPS",
        config_macros,
    )
    automouse_fade_end_config = (
        parse_automouse_fade_end_config(rgb_config_raw_text, config_macros) if rgb_automouse_gradient_enabled else None
    )
    key_behavior_feedback_colors = (
        parse_key_behavior_feedback_colors(rgb_config_raw_text, config_macros, pd_mode_color_anchors)
        if rgb_key_behavior_feedback_enabled
        else []
    )
    key_behavior_feedback_locality = (
        parse_key_behavior_feedback_locality(rgb_config_raw_text) if rgb_key_behavior_feedback_enabled else None
    )
    key_behavior_feedback_led_groups = (
        parse_exported_rgb_led_groups(
            rgb_config_raw_text,
            "EXPORT_KEY_BEHAVIOR_FEEDBACK_LED_GROUPS",
            config_macros,
            semantic_field=True,
        )
        if rgb_key_behavior_feedback_enabled
        else []
    )

    macro_usages = collect_macro_usages(parsed_layers, behaviors, combos, via_macros + hardcoded_macros)

    via_slots = []
    for slot in via_macros:
        via_slots.append({**asdict(slot), "usages": macro_usages[slot.keycode]})

    hardcoded_slots = []
    for slot in hardcoded_macros:
        hardcoded_slots.append({**asdict(slot), "usages": macro_usages[slot.keycode]})

    behavior_rows = [asdict(behavior) for behavior in behaviors]
    combos_rows = [asdict(combo) for combo in combos]

    summary = {
        "layer_count": len(parsed_layers),
        "layout_key_count": len(parsed_layers[0]["positions"]),
        "key_behavior_count": len(behaviors),
        "key_behavior_step_count": sum(len(behavior.steps) for behavior in behaviors),
        "combo_count": len(combos),
        "via_macro_count": len(via_macros),
        "via_macro_non_empty_count": sum(not slot.empty for slot in via_macros),
        "hardcoded_macro_count": len(hardcoded_macros),
        "hardcoded_macro_non_empty_count": sum(not slot.empty for slot in hardcoded_macros),
        "keymap_custom_keycode_count": len(keymap_custom_keycodes),
        "pd_mode_count": len(pd_modes),
        "pd_mode_color_count": len(pd_mode_colors),
        "combo_feedback_configured": int(combo_feedback_color is not None),
    }

    return {
        "summary": summary,
        "config": {
            "layers": layers,
            "macros": config_macros,
            "keymap_macros": keymap_config_macros,
            "userspace_macros": userspace_config_macros,
            "timing_defaults": timing_defaults,
        },
        "features": {
            "rgb_automouse_gradient_enabled": rgb_automouse_gradient_enabled,
            "rgb_key_behavior_feedback_enabled": rgb_key_behavior_feedback_enabled,
            "rgb_pd_mode_active_half_enabled": rgb_pd_mode_active_half_enabled,
        },
        "pd_modes": pd_modes,
        "rgb": {
            "layer_colors": layer_colors,
            "pd_mode_colors": pd_mode_colors,
            "combo_feedback_color": combo_feedback_color,
            "combo_feedback_locality": combo_feedback_locality,
            "combo_feedback_led_groups": combo_feedback_led_groups,
            "automouse_fade_end_config": automouse_fade_end_config,
            "key_behavior_feedback_locality": key_behavior_feedback_locality,
            "key_behavior_feedback_colors": key_behavior_feedback_colors,
            "key_behavior_feedback_led_groups": key_behavior_feedback_led_groups,
        },
        "keymap_custom_keycodes": keymap_custom_keycodes,
        "via_macros": via_slots,
        "hardcoded_macros": hardcoded_slots,
        "combos": combos_rows,
        "key_behaviors": behavior_rows,
        "layers": parsed_layers,
    }


def render_markdown(profile: dict[str, object]) -> str:
    keymap_link = markdown_path_link(KEYMAP_FILE, "keymap.c")
    config_link = markdown_path_link(CONFIG_FILE, "keymap config.h")
    user_config_link = markdown_path_link(USER_CONFIG_FILE, "users/noah/config.h")
    rgb_link = markdown_path_link(RGB_CONFIG_FILE, "rgb_config.c")
    pd_manifest_link = markdown_path_link(PD_MODE_MANIFEST_FILE, "pd_mode_manifest.h")
    sections = [
        MARKDOWN_HEADER.rstrip(),
        "# Profile Introspection",
        "",
        f"This report is generated from the authored profile files {keymap_link}, {config_link}, {user_config_link}, and {rgb_link}. The renderer is board-specific to the Charybdis 4x6 and derives the current `LAYOUT()` slot order directly from {keymap_link}.",
        "",
        f"PD mode names and bindings in this report stay in sync with the shared definitions in {pd_manifest_link}.",
        "",
        render_quick_legend_section(profile),
        render_layer_maps_section(profile),
        render_pd_mode_color_section(profile),
    ]
    if profile["features"]["rgb_automouse_gradient_enabled"]:
        sections.append(render_automouse_fade_section(profile))
    if profile["rgb"]["combo_feedback_color"] is not None:
        sections.append(render_combo_feedback_section(profile))
    if profile["features"]["rgb_key_behavior_feedback_enabled"]:
        sections.append(render_key_behavior_feedback_section(profile))
    sections.extend(
        [
            render_macro_section(profile),
            render_reference_section(profile),
            render_summary_section(profile),
            render_config_defines_section(profile),
            render_generated_assets_section(),
        ]
    )
    return "\n".join(section for section in sections if section)


def render_quick_legend_section(profile: dict[str, object]) -> str:
    indicator_colors = resolve_behavior_indicator_preview_colors(profile)
    show_behavior_indicator_rows = any(color is not None for color in indicator_colors.values())

    def indicator_preview(kind: str, alt_text: str) -> str:
        color = indicator_colors[kind]
        if color is None:
            return "`not configured`"
        return markdown_color_swatch(color, alt_text)

    lines = [
        "## Quick Legend",
        "",
        "| Where | Marker | Meaning |",
        "| --- | --- | --- |",
        "| Layer image | `C1`, `C2`, ... | Combo badge. Match the badge id to the layer-local combo table below the image. |",
        "| Behavior table | `single`, `double`, `triple`, `quadruple`, `quintuple` | Tap tiers for the same physical key: 1 tap, 2 taps, 3 taps, 4 taps, 5 taps. |",
        "| Behavior table | repeated rows for one key | The same physical key exposes different actions at different tap tiers. |",
        "| Behavior table | `Tap` / `Hold` / `Long Hold` | Actions that fire for that tap tier on tap, hold, or deeper long hold. |",
        "",
    ]
    if show_behavior_indicator_rows:
        lines[5:5] = [
            f"| Layer image | `tap` dot {indicator_preview('tap', 'Tap indicator color')} with optional count | This key has authored tap actions. A plain dot means one authored tap action; a numbered dot means multiple tap tiers on that key define a tap action. Use the behavior table below for `single`, `double`, `triple`, and higher tap counts. |",
            f"| Layer image | `hold` dot {indicator_preview('hold', 'Hold indicator color')} with optional count | This key has authored hold tiers. A plain dot means one hold tier; a numbered dot means multiple tap tiers on that key define a hold action. |",
            f"| Layer image | `long hold` dot {indicator_preview('long_hold', 'Long hold indicator color')} with optional count | This key has authored long-hold tiers. A plain dot means one long-hold tier; a numbered dot means multiple tap tiers on that key define a long-hold action. |",
        ]
    return "\n".join(lines)


def render_reference_section(profile: dict[str, object]) -> str:
    config = profile["config"]
    features = profile["features"]
    rgb = profile["rgb"]
    automouse_fade_end_config = rgb["automouse_fade_end_config"]
    combo_feedback_locality = rgb["combo_feedback_locality"]
    feedback_locality = rgb["key_behavior_feedback_locality"]
    keymap_link = markdown_path_link(KEYMAP_FILE, "keymap.c")
    config_link = markdown_path_link(CONFIG_FILE, "config.h")
    rgb_link = markdown_path_link(RGB_CONFIG_FILE, "rgb_config.c")
    rgb_authored_surfaces = ["layer colors", "pd-mode colors"]
    if features["rgb_automouse_gradient_enabled"]:
        rgb_authored_surfaces.append("auto-mouse fade config")
    if rgb["combo_feedback_color"] is not None:
        rgb_authored_surfaces.append("combo feedback color")
    if features["rgb_key_behavior_feedback_enabled"]:
        rgb_authored_surfaces.append("key-behavior feedback colors")
    lines = [
        "## Reference",
        "",
        "### Authored Sources",
        "",
        "| File | Authored Surface |",
        "| --- | --- |",
        f"| {keymap_link} | custom keycodes, macro tables, combos, key behaviors, and current `LAYOUT()` layer contents |",
        f"| {config_link} | layer enum, timing, RGB defaults, and keymap-facing feature config |",
        f"| {rgb_link} | {', '.join(rgb_authored_surfaces)} |",
        "",
        "### Shared Keycode Surfaces",
        "",
        f"- Layers: {', '.join(f'`{layer}`' for layer in config['layers'])}",
        f"- Keymap-local custom keycodes: {', '.join(f'`{name}`' for name in profile['keymap_custom_keycodes']) or '`none`'}",
        f"- PD color overlays: {', '.join(f'`{row['pointing_mode']}`' for row in rgb['pd_mode_colors']) or '`none`'}",
    ]
    if features["rgb_automouse_gradient_enabled"]:
        lines.append(
            f"- Auto-mouse fade destination mode: `{automouse_fade_end_config['mode']}`"
            if automouse_fade_end_config is not None
            else "- Auto-mouse fade destination mode: `not authored`",
        )
    if features["rgb_key_behavior_feedback_enabled"]:
        lines.append(
            f"- Key-behavior feedback locality: `{feedback_locality['locality']}`"
            if feedback_locality is not None
            else "- Key-behavior feedback locality: `not authored`",
        )
    if rgb["combo_feedback_color"] is not None:
        lines.append(
            f"- Combo feedback locality: `{combo_feedback_locality['locality']}`"
            if combo_feedback_locality is not None
            else "- Combo feedback locality: `not authored`",
        )
    lines.extend(
        [
            "",
            "### Layer RGB Config",
            "",
            "| Layer | RGB Matrix Render Mode | Authored HSV | Preview Color |",
            "| --- | --- | --- | --- |",
        ]
    )
    for row in rgb["layer_colors"]:
        color = row["color"]
        preview_swatch = markdown_color_swatch(row["preview_color"], f"{row['layer']} preview color")
        lines.append(
            f"| `{row['layer']}` | `{row['mode']}` | `HSV({color['h']}, {color['s']}, {color['v']})` | {preview_swatch} |"
        )

    lines.append("")
    return "\n".join(lines)


def render_summary_section(profile: dict[str, object]) -> str:
    lines = [
        "## Summary",
        "",
        "| Field | Value |",
        "| --- | --- |",
    ]

    for key, value in profile["summary"].items():
        lines.append(f"| `{key}` | `{value}` |")

    lines.append("")
    return "\n".join(lines)


def render_config_defines_section(profile: dict[str, object]) -> str:
    keymap_link = markdown_path_link(CONFIG_FILE, "keymap config.h")
    user_config_link = markdown_path_link(USER_CONFIG_FILE, "users/noah/config.h")
    lines = [
        "## Config Defines",
        "",
        "These values come from the keymap config and the shared userspace config. When the same macro is defined in both, the merged evaluation used by this report follows QMK include order and lets the keymap config override the userspace config.",
        "",
        "| Macro | Value | Source |",
        "| --- | --- | --- |",
    ]

    for name, value in profile["config"]["userspace_macros"].items():
        lines.append(f"| `{name}` | `{value}` | {user_config_link} |")

    for name, value in profile["config"]["keymap_macros"].items():
        lines.append(f"| `{name}` | `{value}` | {keymap_link} |")

    lines.append("")
    return "\n".join(lines)


def render_layer_maps_section(profile: dict[str, object]) -> str:
    features = profile["features"]
    layer_color_map = {row["layer"]: row for row in profile["rgb"]["layer_colors"]}
    keymap_link = markdown_path_link(KEYMAP_FILE, "keymap.c")
    config_link = markdown_path_link(CONFIG_FILE, "config.h")
    rgb_link = markdown_path_link(RGB_CONFIG_FILE, "rgb_config.c")
    asset_dir_link = markdown_path_link(ASSET_OUTPUT_DIR, "docs/media/profile-introspection/")
    lines = [
        "## Layer Images",
        "",
        f"These previews are generated as SVG image assets under {asset_dir_link}. The renderer uses the authored `layer_colors[]` config from {rgb_link} and the current `LAYOUT()` slot order from {keymap_link}:",
        "",
        "| Available Layer RGB Mode | Meaning |",
        "| --- | --- |",
        f"| `ALL_KEYS` | {layer_color_mode_description('ALL_KEYS')} |",
        f"| `KEYS_MAPPED_ON_THIS_LAYER_ONLY` | {layer_color_mode_description('KEYS_MAPPED_ON_THIS_LAYER_ONLY')} |",
        "",
        f"- `LAYER_BASE` falls back to the default RGB color from {config_link} when its authored layer color is `HSV(0, 0, 0)`",
        "- Keys that participate in combos on that layer show bottom-edge combo badges such as `C1` and `C2`; those ids match the combo table for the same layer",
        "- Each layer section below also pulls in the authored key behaviors, pd modes that are directly placed or reachable through those behaviors, and combos that are actually present on that layer",
        "",
        "Timing legend for the layer-local behavior tables:",
        "",
        f"- `tap_hold(...)`, `long_hold(...)`, and `multi_tap(...)` use the default timings from {config_link}",
        "- `tap_hold=...`, `long_hold=...`, and `multi_tap=...` are custom timings authored on that key",
        "- `release before tap_hold(...); otherwise normal hold` means the tap fires on a quick release; if you keep holding, the key keeps its normal hold behavior",
        "- Timing is shown per tap count, so each row lists only the timings that matter for that behavior",
        "",
    ]
    if features["rgb_key_behavior_feedback_enabled"] and profile["rgb"]["key_behavior_feedback_colors"]:
        lines.insert(
            10,
            f"- Keys with authored `key_behaviors[]` rows in {keymap_link} show numbered activity dots derived from the authored key-behavior feedback colors in {rgb_link}: white for authored tap actions, orange for authored hold tiers, and cyan for authored long-hold tiers",
        )
    for layer in profile["layers"]:
        color_config = layer_color_map[layer["name"]]
        image_name = layer_image_name(layer["name"])
        preview_swatch = markdown_color_swatch(color_config["preview_color"], f"{layer['name']} preview color")
        combo_badge_map = build_layer_combo_badge_map(layer, profile)
        lines.append(f"### `{layer['name']}`")
        lines.append("")
        lines.append(f"- RGB matrix render mode: `{color_config['mode']}`")
        lines.append(f"- Authored layer color: `HSV({color_config['color']['h']}, {color_config['color']['s']}, {color_config['color']['v']})`")
        lines.append(f"- Preview color: {preview_swatch}")
        if combo_badge_map:
            lines.append(f"- Combo badges on this layer: {', '.join(f'`{badge}`' for badge in sorted({badge for badges in combo_badge_map.values() for badge in badges}))}")
        lines.append("")
        lines.append(f"![{layer['name']}]({markdown_relative_path(ASSET_OUTPUT_DIR / image_name)})")
        lines.append("")
        lines.extend(render_layer_local_key_behaviors(layer, profile))
        lines.extend(render_layer_local_pd_modes(layer, profile))
        lines.extend(render_layer_local_combos(layer, profile))
    return "\n".join(lines)


def render_pd_mode_color_section(profile: dict[str, object]) -> str:
    pd_mode_colors = profile["rgb"]["pd_mode_colors"]
    rgb_link = markdown_path_link(RGB_CONFIG_FILE, "rgb_config.c")
    user_config_link = markdown_path_link(USER_CONFIG_FILE, "users/noah/config.h")
    lines = [
        "## PD Mode Colors",
        "",
    ]

    if not pd_mode_colors:
        lines.extend(
            [
                f"No authored `pd_mode_colors[]` entries were found in {rgb_link}.",
                "",
            ]
        )
        return "\n".join(lines)

    lines.extend(
        [
            f"These overlays come from `pd_mode_colors[]` in {rgb_link}. Each row chooses its own locality and color for the matching pointing mode.",
            f"Key-local PD RGB localities are gated by `RGB_PD_MODE_ACTIVE_HALF_ENABLE` in {user_config_link}; current state: `{ 'defined' if profile['features']['rgb_pd_mode_active_half_enabled'] else 'not defined' }`.",
            "",
            "| PD Locality | Meaning |",
            "| --- | --- |",
            f"| `RGB_BOTH_HALVES` | {pd_color_locality_description('RGB_BOTH_HALVES')} |",
            f"| `RGB_LEFT_HALF` | {pd_color_locality_description('RGB_LEFT_HALF')} |",
            f"| `RGB_RIGHT_HALF` | {pd_color_locality_description('RGB_RIGHT_HALF')} |",
            f"| `RGB_KEY_HALF` | {pd_color_locality_description('RGB_KEY_HALF')} |",
            f"| `RGB_KEYS_ONLY` | {pd_color_locality_description('RGB_KEYS_ONLY')} |",
            "",
            "| Pointing Mode | Locality | Authored HSV | Preview Color |",
            "| --- | --- | --- | --- |",
        ]
    )

    for row in pd_mode_colors:
        color = row["color"]
        preview_swatch = markdown_color_swatch(row["preview_color"], f"{row['pointing_mode']} color")
        lines.append(
            f"| `{row['pointing_mode']}` | `{row['locality']}` | `HSV({color['h']}, {color['s']}, {color['v']})` | {preview_swatch} |"
        )

    lines.append("")
    return "\n".join(lines)


def render_automouse_fade_section(profile: dict[str, object]) -> str:
    automouse_fade_end_config = profile["rgb"]["automouse_fade_end_config"]
    rgb_link = markdown_path_link(RGB_CONFIG_FILE, "rgb_config.c")
    lines = [
        "## Auto-mouse Fade",
        "",
    ]

    if automouse_fade_end_config is None:
        lines.extend(
            [
                f"No authored `automouse_fade_end_config` block was found in {rgb_link}.",
                "",
            ]
        )
        return "\n".join(lines)

    end_color = automouse_fade_end_config["end_color"]
    preview_swatch = markdown_color_swatch(automouse_fade_end_config["preview_color"], "Auto-mouse end color")
    lines.extend(
        [
            f"This fade destination comes from `automouse_fade_end_config` in {rgb_link}. The mode chooses where the timeout fade lands after the auto-mouse layer starts dropping out.",
            "",
            f"Current authored auto-mouse fade mode: `{automouse_fade_end_config['mode']}`.",
            "",
            "| Available Mode | Meaning |",
            "| --- | --- |",
            f"| `FOLLOW_REAL_DESTINATION` | {automouse_fade_end_mode_description('FOLLOW_REAL_DESTINATION')} |",
            f"| `END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW` | {automouse_fade_end_mode_description('END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW')} |",
            f"| `END_COLOR_ON_ALL_KEYS` | {automouse_fade_end_mode_description('END_COLOR_ON_ALL_KEYS')} |",
            "",
            f"Authored `end_color`: `HSV({end_color['h']}, {end_color['s']}, {end_color['v']})`.",
            "",
            f"Preview color: {preview_swatch}",
            "",
            "`end_color` is only visible in the two `END_COLOR_*` modes above; `FOLLOW_REAL_DESTINATION` ignores it and lands on the real rendered board state instead.",
            "",
        ]
    )
    return "\n".join(lines)


def layer_image_name(layer_name: str) -> str:
    return f"{LAYER_IMAGE_PREFIX}{layer_name}.svg"


def color_swatch_image_name(fill_hex: str) -> str:
    return f"{COLOR_SWATCH_PREFIX}{fill_hex.removeprefix('#').lower()}.svg"


def markdown_relative_path(path: Path) -> str:
    return Path(os.path.relpath(path, MARKDOWN_OUTPUT.parent)).as_posix()


def markdown_color_swatch(color: dict[str, object] | None, alt_text: str) -> str:
    if color is None:
        return "no override"
    return (
        f'<img alt="{html.escape(alt_text, quote=True)}" '
        f'src="{markdown_relative_path(ASSET_OUTPUT_DIR / color_swatch_image_name(color["hex"]))}" '
        f'width="{SWATCH_WIDTH}" height="{SWATCH_HEIGHT}" />'
    )


def markdown_path_link(path: Path, label: str | None = None) -> str:
    return f"[{label or path.name}]({markdown_relative_path(path)})"


def layer_positions_by_raw_keycode(layer: dict[str, object]) -> dict[str, list[dict[str, object]]]:
    positions_by_keycode: dict[str, list[dict[str, object]]] = {}
    for position in layer["positions"]:
        positions_by_keycode.setdefault(position["keycode"], []).append(position)
    return positions_by_keycode


def layer_positions_by_lookup_key(layer: dict[str, object]) -> dict[str, list[dict[str, object]]]:
    positions_by_keycode: dict[str, list[dict[str, object]]] = {}
    for position in layer["positions"]:
        positions_by_keycode.setdefault(behavior_lookup_key(position["keycode"]), []).append(position)
    return positions_by_keycode


def format_layer_key_instances(positions: list[dict[str, object]]) -> str:
    counts: dict[str, int] = {}
    ordered_labels: list[str] = []
    for position in positions:
        label = position["display"]
        if label not in counts:
            ordered_labels.append(label)
            counts[label] = 0
        counts[label] += 1

    rendered: list[str] = []
    for label in ordered_labels:
        count = counts[label]
        if count == 1:
            rendered.append(f"`{label}`")
        else:
            rendered.append(f"`{label} x{count}`")
    return ", ".join(rendered)


def format_token_with_raw(token: str) -> str:
    normalized = normalize_expr(token)
    display = display_token(normalized)
    if display == normalized:
        return f"`{normalized}`"
    return f"`{display}` (`{normalized}`)"


def resolve_pd_mode_from_action_expr(
    action_expr: str,
    pd_mode_by_mode_keycode: dict[str, dict[str, object]],
    pd_mode_by_lock_keycode: dict[str, dict[str, object]],
) -> dict[str, object] | None:
    normalized = normalize_expr(action_expr)
    if normalized in pd_mode_by_mode_keycode:
        return pd_mode_by_mode_keycode[normalized]
    if normalized in pd_mode_by_lock_keycode:
        return pd_mode_by_lock_keycode[normalized]
    return None


def format_pd_mode_behavior_source(
    positions: list[dict[str, object]],
    step: dict[str, object],
    field_name: str,
    action_expr: str,
) -> str:
    tap_count = TAP_COUNT_NAMES.get(step["tap_count"], str(step["tap_count"]))
    field_label = field_name.replace("_", " ")
    return f"{format_layer_key_instances(positions)} via `{tap_count} {field_label}` -> {format_token_with_raw(action_expr)}"


def collect_layer_reachable_pd_mode_rows(
    layer: dict[str, object],
    profile: dict[str, object],
) -> list[tuple[dict[str, object], list[str], dict[str, object] | None]]:
    positions_by_raw_keycode = layer_positions_by_raw_keycode(layer)
    positions_by_lookup_key = layer_positions_by_lookup_key(layer)
    behavior_map = {behavior_lookup_key(row["keycode"]): row for row in profile["key_behaviors"]}
    pd_mode_by_mode_keycode = {row["mode_keycode"]: row for row in profile["pd_modes"]}
    pd_mode_by_lock_keycode = {row["lock_keycode"]: row for row in profile["pd_modes"]}
    color_by_mode = {row["pointing_mode"]: row for row in profile["rgb"]["pd_mode_colors"]}
    sources_by_mode: dict[str, list[str]] = {}
    seen_sources_by_mode: dict[str, set[str]] = {}

    def add_source(pd_mode: dict[str, object], source: str) -> None:
        mode_name = pd_mode["pointing_mode"]
        if mode_name not in sources_by_mode:
            sources_by_mode[mode_name] = []
            seen_sources_by_mode[mode_name] = set()
        if source in seen_sources_by_mode[mode_name]:
            return
        seen_sources_by_mode[mode_name].add(source)
        sources_by_mode[mode_name].append(source)

    for pd_mode in profile["pd_modes"]:
        positions = positions_by_raw_keycode.get(pd_mode["mode_keycode"])
        if positions:
            add_source(pd_mode, f"{format_layer_key_instances(positions)} directly on layer")

    seen_behavior_keys: set[str] = set()
    for position in layer["positions"]:
        key = behavior_lookup_key(position["keycode"])
        if key in seen_behavior_keys or key not in behavior_map:
            continue
        seen_behavior_keys.add(key)
        behavior = behavior_map[key]
        positions = positions_by_lookup_key[key]
        for step in behavior["steps"]:
            for field_name in ("tap", "hold", "long_hold"):
                action = step[field_name]
                if action is None:
                    continue
                pd_mode = resolve_pd_mode_from_action_expr(action["action"], pd_mode_by_mode_keycode, pd_mode_by_lock_keycode)
                if pd_mode is None:
                    continue
                add_source(pd_mode, format_pd_mode_behavior_source(positions, step, field_name, action["action"]))

    rows: list[tuple[dict[str, object], list[str], dict[str, object] | None]] = []
    for pd_mode in profile["pd_modes"]:
        sources = sources_by_mode.get(pd_mode["pointing_mode"])
        if not sources:
            continue
        rows.append((pd_mode, sources, color_by_mode.get(pd_mode["pointing_mode"])))
    return rows


def render_layer_local_key_behaviors(layer: dict[str, object], profile: dict[str, object]) -> list[str]:
    timing_defaults = profile["config"]["timing_defaults"]
    behavior_map = {behavior_lookup_key(row["keycode"]): row for row in profile["key_behaviors"]}
    positions_by_keycode = layer_positions_by_lookup_key(layer)
    seen: set[str] = set()
    rows: list[tuple[list[dict[str, object]], dict[str, object]]] = []

    for position in layer["positions"]:
        key = behavior_lookup_key(position["keycode"])
        if key in seen or key not in behavior_map:
            continue
        seen.add(key)
        rows.append((positions_by_keycode[key], behavior_map[key]))

    lines = ["#### Key Behaviors On This Layer", ""]
    if not rows:
        lines.extend(["No authored key-behavior rows are present on this layer.", ""])
        return lines

    lines.extend(
        [
            "| Key On Layer | Behavior Keycode | Tap Count | Tap | Hold | Long Hold | Timing |",
            "| --- | --- | --- | --- | --- | --- | --- |",
        ]
    )
    for positions, behavior in rows:
        visible_steps = [
            step
            for step in behavior["steps"]
            if step["tap"] is not None or step["hold"] is not None or step["long_hold"] is not None
        ]
        for index, step in enumerate(visible_steps):
            key_instances = format_layer_key_instances(positions)
            keycode = format_token_with_raw(behavior["keycode"])
            timing = f"`{format_timing_for_step(behavior, step, visible_steps, timing_defaults)}`"
            tap_count = TAP_COUNT_NAMES.get(step["tap_count"], str(step["tap_count"]))
            lines.append(
                f"| {key_instances} | {keycode} | `{tap_count}` | `{format_behavior_action(step['tap'])}` | `{format_behavior_action(step['hold'])}` | `{format_behavior_action(step['long_hold'])}` | {timing} |"
            )
    lines.append("")
    return lines


def render_layer_local_pd_modes(layer: dict[str, object], profile: dict[str, object]) -> list[str]:
    rows = collect_layer_reachable_pd_mode_rows(layer, profile)

    lines = ["#### PD Modes Reachable On This Layer", ""]
    if not rows:
        lines.extend(["No pd modes are directly placed or reachable through key behaviors on this layer.", ""])
        return lines

    lines.extend(
        [
            "| Reachable Via | Mode Keycode | Pointing Mode | Locality | Authored HSV | Preview Color |",
            "| --- | --- | --- | --- | --- | --- |",
        ]
    )
    for pd_mode, sources, color_row in rows:
        if color_row is None:
            paint_mode = "`none`"
            authored_hsv = "`none`"
            preview_swatch = "no override"
        else:
            color = color_row["color"]
            paint_mode = f"`{color_row['locality']}`"
            authored_hsv = f"`HSV({color['h']}, {color['s']}, {color['v']})`"
            preview_swatch = markdown_color_swatch(color_row["preview_color"], f"{pd_mode['pointing_mode']} color")
        lines.append(
            f"| {'; '.join(sources)} | {format_token_with_raw(pd_mode['mode_keycode'])} | `{pd_mode['pointing_mode']}` | {paint_mode} | {authored_hsv} | {preview_swatch} |"
        )
    lines.append("")
    return lines


def render_layer_local_combos(layer: dict[str, object], profile: dict[str, object]) -> list[str]:
    positions_by_keycode = layer_positions_by_raw_keycode(layer)
    rows = collect_layer_local_combos(layer, profile)

    lines = ["#### Combos Available On This Layer", ""]
    if not rows:
        lines.extend(["No authored combos resolve entirely from keys on this layer.", ""])
        return lines

    lines.extend(
        [
            "| Combo | Inputs On This Layer | Output |",
            "| --- | --- | --- |",
        ]
    )
    for combo in rows:
        input_instances = " + ".join(format_layer_key_instances(positions_by_keycode[input_key]) for input_key in combo["inputs"])
        lines.append(f"| `{combo['badge']}` | {input_instances} | {format_token_with_raw(combo['output'])} |")
    lines.append("")
    return lines


def collect_layer_local_combos(layer: dict[str, object], profile: dict[str, object]) -> list[dict[str, object]]:
    positions_by_keycode = layer_positions_by_raw_keycode(layer)
    rows: list[dict[str, object]] = []

    for combo in profile["combos"]:
        if all(input_key in positions_by_keycode for input_key in combo["inputs"]):
            rows.append(
                {
                    "badge": f"C{len(rows) + 1}",
                    "output": combo["output"],
                    "inputs": combo["inputs"],
                }
            )

    return rows


def build_layer_combo_badge_map(layer: dict[str, object], profile: dict[str, object]) -> dict[str, list[str]]:
    badge_map: dict[str, list[str]] = {}

    for combo in collect_layer_local_combos(layer, profile):
        for input_key in combo["inputs"]:
            badges = badge_map.setdefault(input_key, [])
            if combo["badge"] not in badges:
                badges.append(combo["badge"])

    return badge_map


def resolve_behavior_indicator_preview_colors(profile: dict[str, object]) -> dict[str, dict[str, object] | None]:
    feedback_rows = [
        row
        for row in profile["rgb"]["key_behavior_feedback_colors"]
        if row["preview_color"] is not None
    ]

    def feedback_preview_color(
        required_tokens: set[str],
        forbidden_tokens: set[str] | None = None,
    ) -> dict[str, object] | None:
        blocked = forbidden_tokens or set()
        for row in feedback_rows:
            field_tokens = set(row["field"].removesuffix("_color").split("_"))
            if required_tokens.issubset(field_tokens) and field_tokens.isdisjoint(blocked):
                return row["preview_color"]
        return None

    return {
        "tap": feedback_preview_color({"multi", "tap"}),
        "hold": feedback_preview_color({"hold"}, {"long"}),
        "long_hold": feedback_preview_color({"long", "hold"}),
    }


def count_behavior_tap_actions(behavior: dict[str, object]) -> int:
    return sum(1 for step in behavior["steps"] if step["tap"] is not None)


def build_behavior_indicator_map(profile: dict[str, object]) -> dict[str, list[dict[str, object]]]:
    indicator_map: dict[str, list[dict[str, object]]] = {}
    indicator_colors = resolve_behavior_indicator_preview_colors(profile)
    tap_color = indicator_colors["tap"]["hex"] if indicator_colors["tap"] is not None else None
    hold_color = indicator_colors["hold"]["hex"] if indicator_colors["hold"] is not None else None
    long_hold_color = indicator_colors["long_hold"]["hex"] if indicator_colors["long_hold"] is not None else None

    for behavior in profile["key_behaviors"]:
        tap_count = count_behavior_tap_actions(behavior)
        hold_count = sum(1 for step in behavior["steps"] if step["hold"] is not None)
        long_hold_count = sum(1 for step in behavior["steps"] if step["long_hold"] is not None)
        dots: list[dict[str, object]] = []

        if tap_count > 0 and tap_color is not None:
            dots.append({"color": tap_color, "count": tap_count})
        if hold_count > 0 and hold_color is not None:
            dots.append({"color": hold_color, "count": hold_count})
        if long_hold_count > 0 and long_hold_color is not None:
            dots.append({"color": long_hold_color, "count": long_hold_count})

        if dots:
            indicator_map[behavior_lookup_key(behavior["keycode"])] = dots

    return indicator_map


def build_generated_assets(profile: dict[str, object]) -> dict[Path, str]:
    markdown = render_markdown(profile)
    assets: dict[Path, str] = {
        MARKDOWN_OUTPUT: markdown,
    }

    layer_color_map = {row["layer"]: row for row in profile["rgb"]["layer_colors"]}
    behavior_indicator_map = build_behavior_indicator_map(profile)
    for layer in profile["layers"]:
        image_path = ASSET_OUTPUT_DIR / layer_image_name(layer["name"])
        combo_badge_map = build_layer_combo_badge_map(layer, profile)
        assets[image_path] = render_layer_svg(layer, layer_color_map[layer["name"]], behavior_indicator_map, combo_badge_map)

    swatch_colors: set[str] = set()
    for row in profile["rgb"]["layer_colors"]:
        preview_color = row["preview_color"]
        if preview_color is not None:
            swatch_colors.add(preview_color["hex"])

    for row in profile["rgb"]["pd_mode_colors"]:
        swatch_colors.add(row["preview_color"]["hex"])

    automouse_fade_end_config = profile["rgb"]["automouse_fade_end_config"]
    if automouse_fade_end_config is not None:
        swatch_colors.add(automouse_fade_end_config["preview_color"]["hex"])

    combo_feedback_color = profile["rgb"]["combo_feedback_color"]
    if combo_feedback_color is not None:
        swatch_colors.add(combo_feedback_color["preview_color"]["hex"])

    for row in profile["rgb"]["combo_feedback_led_groups"]:
        swatch_colors.add(row["preview_color"]["hex"])

    for row in profile["rgb"]["key_behavior_feedback_colors"]:
        swatch_colors.add(row["preview_color"]["hex"])

    for row in profile["rgb"]["key_behavior_feedback_led_groups"]:
        swatch_colors.add(row["preview_color"]["hex"])

    for fill_hex in sorted(swatch_colors):
        assets[ASSET_OUTPUT_DIR / color_swatch_image_name(fill_hex)] = render_color_swatch_svg(fill_hex)

    return assets


def render_layer_svg(
    layer: dict[str, object],
    color_config: dict[str, object],
    behavior_indicator_map: dict[str, list[dict[str, object]]],
    combo_badge_map: dict[str, list[str]],
) -> str:
    parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{SVG_CANVAS_WIDTH}" height="{SVG_CANVAS_HEIGHT}" viewBox="0 0 {SVG_CANVAS_WIDTH} {SVG_CANVAS_HEIGHT}" role="img" aria-labelledby="title desc">',
        f"  <title id=\"title\">{layer['name']} layout preview</title>",
        f"  <desc id=\"desc\">Generated layer preview for {layer['name']} using authored RGB layer color, mapped-key render mode, numbered activity dots for keys with key_behaviors[] rows, and combo badges for keys that participate in layer-local combos.</desc>",
        "  <defs>",
        "    <filter id=\"shadow\" x=\"-20%\" y=\"-20%\" width=\"140%\" height=\"140%\">",
        "      <feDropShadow dx=\"0\" dy=\"5\" stdDeviation=\"4\" flood-color=\"#000000\" flood-opacity=\"0.22\"/>",
        "    </filter>",
        "  </defs>",
        f"  <rect width=\"{SVG_CANVAS_WIDTH}\" height=\"{SVG_CANVAS_HEIGHT}\" fill=\"{SVG_BACKGROUND}\"/>",
        f"  <text x=\"{TITLE_X}\" y=\"{TITLE_Y}\" fill=\"#f5f5f3\" font-size=\"26\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">{layer['name']}</text>",
        f"  <text x=\"{TITLE_X}\" y=\"{SUBTITLE_Y}\" fill=\"#cfcfcb\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">{svg_subtitle(color_config)}</text>",
    ]

    for position in layer["positions"]:
        geometry = visual_geometry(position)
        style = layer_key_style(position, color_config)
        label = visual_label_for_position(position, style["variant"])
        behavior_dots = behavior_indicator_map.get(behavior_lookup_key(position["keycode"])) if position["has_key_behavior"] else None
        combo_badges = combo_badge_map.get(position["keycode"])
        parts.extend(render_svg_key(geometry, label, style, behavior_dots, combo_badges))

    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def svg_subtitle(color_config: dict[str, object]) -> str:
    color = color_config["color"]
    return f"RGB matrix {color_config['mode']} • authored HSV({color['h']}, {color['s']}, {color['v']})"


def render_color_swatch_svg(fill_hex: str) -> str:
    stroke = adjust_hex(fill_hex, -30)
    return "\n".join(
        [
            '<?xml version="1.0" encoding="UTF-8"?>',
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{SWATCH_WIDTH}" height="{SWATCH_HEIGHT}" viewBox="0 0 {SWATCH_WIDTH} {SWATCH_HEIGHT}" role="img" aria-label="Color swatch">',
            f'  <rect x="1" y="1" width="{SWATCH_WIDTH - 2}" height="{SWATCH_HEIGHT - 2}" rx="6" fill="{fill_hex}" stroke="{stroke}" stroke-width="2"/>',
            "</svg>",
            "",
        ]
    )


def visual_geometry(position: dict[str, object]) -> dict[str, float]:
    layout_index = position["layout_index"]
    cluster = position["cluster"]
    layout_row = position["layout_row"]
    layout_col = position["layout_col"]

    if layout_index in THUMB_VISUALS:
        thumb = THUMB_VISUALS[layout_index]
        return {"x": thumb["x"], "y": thumb["y"] + KEYBOARD_Y_OFFSET, "angle": thumb["angle"]}

    if cluster == "left":
        return {
            "x": LEFT_COLUMN_X[layout_col],
            "y": LEFT_COLUMN_TOP_Y[layout_col] + (layout_row * ROW_Y_STEP) + KEYBOARD_Y_OFFSET,
            "angle": 0,
        }

    return {
        "x": RIGHT_COLUMN_X[layout_col],
        "y": RIGHT_COLUMN_TOP_Y[layout_col] + (layout_row * ROW_Y_STEP) + KEYBOARD_Y_OFFSET,
        "angle": 0,
    }


def layer_key_style(position: dict[str, object], color_config: dict[str, object]) -> dict[str, object]:
    keycode = position["keycode"]
    is_transparent = keycode == "_______"
    is_none = keycode == "XXXXXXX"
    has_real_mapping = not is_transparent and not is_none
    preview_color = color_config["preview_color"]
    preview_enabled = preview_color is not None and preview_color["v"] > 0
    tint_all = color_config["mode"] == "ALL_KEYS" and preview_enabled
    tint_mapped = color_config["mode"] == "KEYS_MAPPED_ON_THIS_LAYER_ONLY" and has_real_mapping and preview_enabled
    tint = tint_all or tint_mapped

    if tint:
        fill = preview_color["hex"]
        text = ideal_text_color(fill)
        return {"fill": fill, "text": text, "stroke": adjust_hex(fill, -28), "label_opacity": 1.0, "variant": "mapped"}

    if is_transparent:
        return {"fill": "#777777", "text": "#ffffff", "stroke": "#b8bab5", "label_opacity": 0.72, "variant": "transparent"}

    if is_none:
        return {"fill": "#c9cac5", "text": "#6d716d", "stroke": "#a8aaa5", "label_opacity": 0.45, "variant": "none"}

    return {"fill": "#f4f4f1", "text": "#1f221f", "stroke": "#d2d3ce", "label_opacity": 1.0, "variant": "neutral"}


def visual_label_for_position(position: dict[str, object], variant: str) -> str:
    if variant == "transparent":
        return "TRNS"
    if variant == "none":
        return "NO"
    return position["display"]


def render_svg_key(
    geometry: dict[str, float],
    label: str,
    style: dict[str, object],
    behavior_dots: list[dict[str, object]] | None = None,
    combo_badges: list[str] | None = None,
) -> list[str]:
    x = geometry["x"]
    y = geometry["y"]
    angle = geometry["angle"]
    cx = x + (KEY_WIDTH / 2)
    cy = y + (KEY_HEIGHT / 2)
    parts = [f'  <g transform="rotate({angle} {cx:.1f} {cy:.1f})">']
    parts.append(
        f'    <rect x="{x:.1f}" y="{y:.1f}" width="{KEY_WIDTH}" height="{KEY_HEIGHT}" rx="{KEY_RADIUS}" fill="{style["fill"]}" stroke="{style["stroke"]}" stroke-width="1.5" filter="url(#shadow)"/>'
    )
    if label:
        font_size = font_size_for_key_label(label)
        text_attributes = render_svg_text_fit_attributes(label, font_size)
        text_suffix = f" {text_attributes}" if text_attributes else ""
        parts.append(
            f'    <text x="{cx:.1f}" y="{cy + 4:.1f}" fill="{style["text"]}" fill-opacity="{style["label_opacity"]}" font-size="{font_size}" text-anchor="middle" font-family="Helvetica, Arial, sans-serif" font-weight="600"{text_suffix}>{escape_xml(label)}</text>'
        )
    if behavior_dots:
        dot_count = len(behavior_dots)
        start_x = x + KEY_WIDTH - 10 - ((dot_count - 1) * 12)
        for index, behavior_dot in enumerate(behavior_dots):
            dot_x = start_x + (index * 12)
            dot_y = y + 10
            dot_color = behavior_dot["color"]
            dot_label = str(behavior_dot["count"]) if int(behavior_dot["count"]) > 1 else ""
            parts.append(
                f'    <circle cx="{dot_x:.1f}" cy="{dot_y:.1f}" r="5.5" fill="{dot_color}" stroke="{style["text"]}" stroke-width="1.5"/>'
            )
            if dot_label:
                parts.append(
                    f'    <text x="{dot_x:.1f}" y="{dot_y + 0.5:.1f}" fill="{ideal_text_color(dot_color)}" font-size="7" text-anchor="middle" dominant-baseline="central" font-family="Helvetica, Arial, sans-serif" font-weight="700">{escape_xml(dot_label)}</text>'
                )
    if combo_badges:
        badge_height = 12
        badge_spacing = 3
        badge_widths = [max(15, 7 + (len(badge) * 4)) for badge in combo_badges]
        total_width = sum(badge_widths) + (badge_spacing * (len(badge_widths) - 1))
        badge_x = x + ((KEY_WIDTH - total_width) / 2)
        badge_y = y + KEY_HEIGHT - badge_height - 5
        for badge, badge_width in zip(combo_badges, badge_widths, strict=False):
            parts.append(
                f'    <rect x="{badge_x:.1f}" y="{badge_y:.1f}" width="{badge_width}" height="{badge_height}" rx="5" fill="#141714" fill-opacity="0.94" stroke="#f5f5f3" stroke-width="1"/>'
            )
            parts.append(
                f'    <text x="{badge_x + (badge_width / 2):.1f}" y="{badge_y + 8.4:.1f}" fill="#f5f5f3" font-size="7.5" text-anchor="middle" font-family="Helvetica, Arial, sans-serif" font-weight="700">{escape_xml(badge)}</text>'
            )
            badge_x += badge_width + badge_spacing
    parts.append("  </g>")
    return parts


def font_size_for_key_label(label: str) -> int:
    if len(label) <= 5:
        return 12
    if len(label) <= 8:
        return 11
    if len(label) <= 10:
        return 8
    return 7


def render_svg_text_fit_attributes(label: str, font_size: int) -> str:
    text_length = estimated_svg_text_length(label, font_size)
    max_width = KEY_WIDTH - (2 * KEY_TEXT_HORIZONTAL_PADDING)
    if text_length <= max_width:
        return ""
    return f'textLength="{max_width}" lengthAdjust="spacingAndGlyphs"'


def estimated_svg_text_length(label: str, font_size: int) -> float:
    width = 0.0
    for char in label:
        if char in "MW@#%&":
            width += font_size * 0.9
        elif char in "Il1()[]{}'\"|.,:;!":
            width += font_size * 0.35
        elif char == " ":
            width += font_size * 0.32
        else:
            width += font_size * 0.62
    return width


def escape_xml(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def ideal_text_color(fill_hex: str) -> str:
    red = int(fill_hex[1:3], 16)
    green = int(fill_hex[3:5], 16)
    blue = int(fill_hex[5:7], 16)
    luminance = ((0.299 * red) + (0.587 * green) + (0.114 * blue)) / 255.0
    return "#1c1f1c" if luminance > 0.6 else "#f7f7f4"


def adjust_hex(fill_hex: str, delta: int) -> str:
    red = max(0, min(255, int(fill_hex[1:3], 16) + delta))
    green = max(0, min(255, int(fill_hex[3:5], 16) + delta))
    blue = max(0, min(255, int(fill_hex[5:7], 16) + delta))
    return "#{:02x}{:02x}{:02x}".format(red, green, blue)


def render_key_behavior_section(profile: dict[str, object]) -> str:
    timing_defaults = profile["config"]["timing_defaults"]
    lines = [
        "## Key Behavior Inventory",
        "",
        "| Keycode | Tap Count | Tap | Hold | Long Hold | Timing |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for behavior in profile["key_behaviors"]:
        timing = format_timing(behavior, timing_defaults)
        for step in behavior["steps"]:
            lines.append(
                "| `{keycode}` | `{tap_count}` | `{tap}` | `{hold}` | `{long_hold}` | `{timing}` |".format(
                    keycode=behavior["keycode"],
                    tap_count=TAP_COUNT_NAMES.get(step["tap_count"], str(step["tap_count"])),
                    tap=format_behavior_action(step["tap"]),
                    hold=format_behavior_action(step["hold"]),
                    long_hold=format_behavior_action(step["long_hold"]),
                    timing=timing,
                )
            )
    lines.append("")
    return "\n".join(lines)


def render_key_behavior_feedback_section(profile: dict[str, object]) -> str:
    feedback_colors = profile["rgb"]["key_behavior_feedback_colors"]
    feedback_locality = profile["rgb"]["key_behavior_feedback_locality"]
    feedback_groups = profile["rgb"]["key_behavior_feedback_led_groups"]
    rgb_link = markdown_path_link(RGB_CONFIG_FILE, "rgb_config.c")
    lines = [
        "## Key-Behavior Feedback LEDs",
        "",
    ]

    if not feedback_colors:
        lines.extend(
            [
                f"No authored key-behavior feedback colors were found in {rgb_link}.",
                "",
            ]
        )
        return "\n".join(lines)

    lines.extend(
        [
            f"These colors come from `key_behavior_feedback_colors` in {rgb_link} and render last on top of the current layer, combo feedback, preview, and any pd-mode overlay. Internally the runtime keeps truthful per-key semantics; broadened authored localities intentionally collapse that truth to a half or full-board presentation.",
            "",
        ]
    )

    if feedback_locality is not None:
        lines.extend(
            [
                f"Current authored feedback locality: `{feedback_locality['locality']}`.",
                "",
                "| Available Locality | Meaning |",
                "| --- | --- |",
                f"| `RGB_BOTH_HALVES` | {key_behavior_feedback_locality_description('RGB_BOTH_HALVES')} |",
                f"| `RGB_LEFT_HALF` | {key_behavior_feedback_locality_description('RGB_LEFT_HALF')} |",
                f"| `RGB_RIGHT_HALF` | {key_behavior_feedback_locality_description('RGB_RIGHT_HALF')} |",
                f"| `RGB_KEY_HALF` | {key_behavior_feedback_locality_description('RGB_KEY_HALF')} |",
                f"| `RGB_KEYS_ONLY` | {key_behavior_feedback_locality_description('RGB_KEYS_ONLY')} |",
                "",
            ]
        )

    lines.extend(
        [
            "| State | Meaning | Authored HSV | Preview Color |",
            "| --- | --- | --- | --- |",
        ]
    )

    for row in feedback_colors:
        color = row["color"]
        preview_color = row["preview_color"]
        preview_swatch = markdown_color_swatch(preview_color, f"{row['label']} color")
        lines.append(
            f"| `{row['label']}` | {row['meaning']} | `HSV({color['h']}, {color['s']}, {color['v']})` | {preview_swatch} |"
        )

    if feedback_groups:
        lines.extend(
            [
                "",
                "Authored key-feedback LED groups repaint after the feedback locality render inside this stage.",
                "",
                "| Semantic Group | LEDs | Count | Authored HSV | Preview Color |",
                "| --- | --- | --- | --- | --- |",
            ]
        )
        for row in feedback_groups:
            color = row["color"]
            preview_swatch = markdown_color_swatch(row["preview_color"], f"{row['label']} group color")
            lines.append(
                f"| `{row['semantic']}` | `{row['leds']}` | `{row['count']}` | `HSV({color['h']}, {color['s']}, {color['v']})` | {preview_swatch} |"
            )

    lines.append("")
    return "\n".join(lines)


def render_combo_feedback_section(profile: dict[str, object]) -> str:
    combo_feedback_color = profile["rgb"]["combo_feedback_color"]
    combo_feedback_locality = profile["rgb"]["combo_feedback_locality"]
    combo_feedback_groups = profile["rgb"]["combo_feedback_led_groups"]
    rgb_link = markdown_path_link(RGB_CONFIG_FILE, "rgb_config.c")
    lines = [
        "## Combo Feedback LEDs",
        "",
    ]

    if combo_feedback_color is None:
        lines.extend(
            [
                f"No authored combo feedback color was found in {rgb_link}.",
                "",
            ]
        )
        return "\n".join(lines)

    lines.extend(
        [
            f"This steady combo layer comes from `combo_feedback_colors` in {rgb_link}. It stays visible while a combo chord is active, sits underneath preview and pd-mode indicators when that combo owns those states, and otherwise repaints above preview and pd-mode overlays but below key-behavior feedback.",
            "",
        ]
    )

    if combo_feedback_locality is not None:
        lines.extend(
            [
                f"Current authored combo feedback locality: `{combo_feedback_locality['locality']}`.",
                "",
                "| Available Locality | Meaning |",
                "| --- | --- |",
                f"| `RGB_BOTH_HALVES` | {combo_feedback_locality_description('RGB_BOTH_HALVES')} |",
                f"| `RGB_LEFT_HALF` | {combo_feedback_locality_description('RGB_LEFT_HALF')} |",
                f"| `RGB_RIGHT_HALF` | {combo_feedback_locality_description('RGB_RIGHT_HALF')} |",
                f"| `RGB_KEY_HALF` | {combo_feedback_locality_description('RGB_KEY_HALF')} |",
                f"| `RGB_KEYS_ONLY` | {combo_feedback_locality_description('RGB_KEYS_ONLY')} |",
                "",
            ]
        )

    color = combo_feedback_color["color"]
    preview_swatch = markdown_color_swatch(combo_feedback_color["preview_color"], "Active combo color")
    lines.extend(
        [
            "| State | Meaning | Authored HSV | Preview Color |",
            "| --- | --- | --- | --- |",
            f"| `{combo_feedback_color['label']}` | {combo_feedback_color['meaning']} | `HSV({color['h']}, {color['s']}, {color['v']})` | {preview_swatch} |",
        ]
    )
    if combo_feedback_groups:
        lines.extend(
            [
                "",
                "Authored combo feedback LED groups repaint after the combo locality render inside the current combo underlay or overlay substage.",
                "",
                "| Group | LEDs | Count | Authored HSV | Preview Color |",
                "| --- | --- | --- | --- | --- |",
            ]
        )
        for index, row in enumerate(combo_feedback_groups):
            group_color = row["color"]
            group_swatch = markdown_color_swatch(row["preview_color"], f"Combo feedback group {index + 1} color")
            lines.append(
                f"| `{index + 1}` | `{row['leds']}` | `{row['count']}` | `HSV({group_color['h']}, {group_color['s']}, {group_color['v']})` | {group_swatch} |"
            )

    lines.append("")
    return "\n".join(lines)


def default_tap_hold_value(behavior: dict[str, object], timing_defaults: dict[str, int]) -> int:
    return timing_defaults["tap_hold_lt"] if normalize_expr(behavior["keycode"]).startswith("LT(") else timing_defaults["tap_hold"]


def step_has_higher_tap_index(step: dict[str, object], visible_steps: list[dict[str, object]]) -> bool:
    return any(other_step["tap_count"] > step["tap_count"] for other_step in visible_steps)


def step_needs_tap_hold_display(step: dict[str, object]) -> bool:
    return step["hold"] is not None or step["long_hold"] is not None


def format_timing_for_step(
    behavior: dict[str, object],
    step: dict[str, object],
    visible_steps: list[dict[str, object]],
    timing_defaults: dict[str, int],
) -> str:
    parts = []
    needs_tap_hold = step_needs_tap_hold_display(step)
    needs_long_hold = step["long_hold"] is not None
    needs_multi_tap = step["tap_count"] > 0 or step_has_higher_tap_index(step, visible_steps)

    if needs_tap_hold:
        if behavior["tap_hold_term"] is not None:
            parts.append(f"tap_hold={behavior['tap_hold_term']}")
        else:
            parts.append(f"tap_hold({default_tap_hold_value(behavior, timing_defaults)})")
    if needs_long_hold:
        if behavior["longer_hold_term"] is not None:
            parts.append(f"long_hold={behavior['longer_hold_term']}")
        else:
            parts.append(f"long_hold({timing_defaults['long_hold']})")
    if needs_multi_tap:
        if behavior["multi_tap_term"] is not None:
            parts.append(f"multi_tap={behavior['multi_tap_term']}")
        else:
            parts.append(f"multi_tap({timing_defaults['multi_tap']})")

    if parts:
        return ", ".join(parts)

    if step["tap"] is not None and step["tap_count"] == 0 and not step_has_higher_tap_index(step, visible_steps):
        if behavior["tap_hold_term"] is not None:
            return f"release before tap_hold={behavior['tap_hold_term']}; otherwise normal hold"
        return f"release before tap_hold({default_tap_hold_value(behavior, timing_defaults)}); otherwise normal hold"

    return "no threshold"


def format_timing(behavior: dict[str, object], timing_defaults: dict[str, int]) -> str:
    visible_steps = [
        step
        for step in behavior["steps"]
        if step["tap"] is not None or step["hold"] is not None or step["long_hold"] is not None
    ]
    if not visible_steps:
        return "-"
    rendered = {
        format_timing_for_step(behavior, step, visible_steps, timing_defaults)
        for step in visible_steps
    }
    if len(rendered) == 1:
        return next(iter(rendered))
    return "; ".join(sorted(rendered))


def format_behavior_action(action: dict[str, object] | None) -> str:
    if action is None:
        return "-"
    helper = action["helper"]
    if action["repeat_hz"] is not None:
        return f"{helper}({action['action']}, {action['repeat_hz']}Hz)"
    return f"{helper}({action['action']})"


def render_combo_section(profile: dict[str, object]) -> str:
    lines = [
        "## Combos",
        "",
        "| Inputs | Output |",
        "| --- | --- |",
    ]
    for combo in profile["combos"]:
        inputs = ", ".join(f"`{item}`" for item in combo["inputs"])
        lines.append(f"| {inputs} | `{combo['output']}` |")

    lines.extend(["", "### Combo Graph", "", "```mermaid", "flowchart LR"])
    for index, combo in enumerate(profile["combos"]):
        combo_id = f"combo_{index}"
        lines.append(f'    {combo_id}["Combo {index + 1}"]')
        lines.append(f'    {combo_id}_out["{escape_mermaid_label(combo["output"])}"]')
        lines.append(f"    {combo_id} --> {combo_id}_out")
        for input_index, keycode in enumerate(combo["inputs"]):
            input_id = f"{combo_id}_in_{input_index}"
            lines.append(f'    {input_id}["{escape_mermaid_label(keycode)}"]')
            lines.append(f"    {input_id} --> {combo_id}")
    lines.extend(["```", ""])
    return "\n".join(lines)


def escape_mermaid_label(label: str) -> str:
    return label.replace('"', '\\"')


def render_macro_section(profile: dict[str, object]) -> str:
    via_slots = [slot for slot in profile["via_macros"] if not slot["empty"]]
    hardcoded_slots = [slot for slot in profile["hardcoded_macros"] if not slot["empty"]]
    lines = [
        "## Macro Inventory",
        "",
        "### VIA Macros",
        "",
    ]
    if via_slots:
        lines.extend(
            [
                "| Slot | Payload | Usage |",
                "| --- | --- | --- |",
            ]
        )
        for slot in via_slots:
            lines.append(f"| `{slot['keycode']}` | `{slot['payload']}` | {format_usages(slot['usages'])} |")
    else:
        lines.append("No filled VIA macro slots.")

    lines.extend(["", "### Hardcoded Macros", ""])
    if hardcoded_slots:
        lines.extend(
            [
                "| Slot | Payload | Usage |",
                "| --- | --- | --- |",
            ]
        )
        for slot in hardcoded_slots:
            lines.append(f"| `{slot['keycode']}` | `{slot['payload']}` | {format_usages(slot['usages'])} |")
    else:
        lines.append("No filled hardcoded macro slots.")
    lines.append("")
    return "\n".join(lines)


def format_usages(usages: list[dict[str, object]]) -> str:
    if not usages:
        return "-"
    labels: list[str] = []
    for usage in usages:
        kind = usage["kind"]
        if kind == "layer":
            labels.append(f"`{usage['layer']} @ {usage['display']}`")
        elif kind == "behavior":
            tap_count = TAP_COUNT_NAMES.get(usage["tap_count"], str(usage["tap_count"]))
            labels.append(f"`{usage['owner']} {tap_count} {usage['field']}`")
        elif kind == "combo_output":
            labels.append(f"`combo {usage['combo_index'] + 1} output`")
        elif kind == "combo_input":
            labels.append(f"`combo {usage['combo_index'] + 1} input`")
    return ", ".join(labels)


def format_layout_position(position: dict[str, object]) -> str:
    if position["cluster"] == "thumb":
        return f"thumb[{position['layout_index'] - (LAYOUT_SLOT_COUNT - len(THUMB_VISUALS))}]"
    return f"{position['cluster']}[{position['layout_row']},{position['layout_col']}]"


def render_generated_assets_section() -> str:
    asset_dir_link = markdown_path_link(ASSET_OUTPUT_DIR, "docs/media/profile-introspection/")
    tool_link = markdown_path_link(SCRIPT_DIR / "profile_introspect.py", "tools/profile_introspect.py")
    return "\n".join(
        [
            "## Generated Assets",
            "",
            f"- Generated layer images live under {asset_dir_link} as `profile-layer-*.svg`",
            f"- Generated color swatches live under {asset_dir_link} as `profile-color-swatch-*.svg`",
            f"- Regenerate the report, layer images, and swatches with `python3 tools/profile_introspect.py --write`; script: {tool_link}",
            f"- Verify they are current with `python3 tools/profile_introspect.py --check`; script: {tool_link}",
            "",
        ]
    )


def managed_generated_artifact_paths(assets: dict[Path, str]) -> set[Path]:
    managed_paths = set(assets)
    managed_paths.update(RETIRED_JSON_OUTPUTS)
    managed_paths.update(RETIRED_MARKDOWN_OUTPUTS)

    for retired_output_dir in RETIRED_OUTPUT_DIRS:
        for path in retired_output_dir.glob(f"{LAYER_IMAGE_PREFIX}*.svg"):
            managed_paths.add(path)
        for path in retired_output_dir.glob(f"{COLOR_SWATCH_PREFIX}*.svg"):
            managed_paths.add(path)

    for path in ASSET_OUTPUT_DIR.glob(f"{LAYER_IMAGE_PREFIX}*.svg"):
        managed_paths.add(path)

    for path in ASSET_OUTPUT_DIR.glob(f"{COLOR_SWATCH_PREFIX}*.svg"):
        managed_paths.add(path)

    for retired_asset_dir in RETIRED_ASSET_OUTPUT_DIRS:
        for path in retired_asset_dir.glob(f"{LAYER_IMAGE_PREFIX}*.svg"):
            managed_paths.add(path)
        for path in retired_asset_dir.glob(f"{COLOR_SWATCH_PREFIX}*.svg"):
            managed_paths.add(path)

    return managed_paths


def stale_generated_paths(assets: dict[Path, str]) -> list[Path]:
    expected_paths = set(assets)
    return sorted(path for path in managed_generated_artifact_paths(assets) if path.exists() and path not in expected_paths)


def stale_generated_directories() -> list[Path]:
    stale_dirs: list[Path] = []
    for path in sorted(RETIRED_ASSET_OUTPUT_DIRS + RETIRED_OUTPUT_DIRS, key=lambda directory: len(directory.parts), reverse=True):
        if path.exists() and path.is_dir() and not any(path.iterdir()):
            stale_dirs.append(path)
    return stale_dirs


def write_outputs(assets: dict[Path, str]) -> None:
    for parent in {path.parent for path in assets}:
        parent.mkdir(parents=True, exist_ok=True)

    for path, contents in assets.items():
        path.write_text(contents)

    for stale_path in stale_generated_paths(assets):
        stale_path.unlink()

    while True:
        stale_dirs = stale_generated_directories()
        if not stale_dirs:
            break
        for stale_dir in stale_dirs:
            stale_dir.rmdir()


def check_outputs(assets: dict[Path, str]) -> int:
    failures: list[str] = []

    for path, expected in assets.items():
        if not path.exists():
            failures.append(f"missing generated file: {path}")
            continue
        actual = path.read_text()
        if actual != expected:
            diff = "".join(
                difflib.unified_diff(
                    actual.splitlines(keepends=True),
                    expected.splitlines(keepends=True),
                    fromfile=str(path),
                    tofile=f"{path} (expected)",
                )
            )
            failures.append(diff)

    for stale_path in stale_generated_paths(assets):
        failures.append(f"stale generated file should be removed: {stale_path}")

    for stale_dir in stale_generated_directories():
        failures.append(f"stale generated directory should be removed: {stale_dir}")

    if failures:
        for failure in failures:
            sys.stderr.write(failure)
            if not failure.endswith("\n"):
                sys.stderr.write("\n")
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="write docs/KEYMAP-OVERVIEW.md and docs/media/profile-introspection/*")
    parser.add_argument("--check", action="store_true", help="fail if the generated artifacts are not current")
    parser.add_argument("--print-markdown", action="store_true", help="print the Markdown report to stdout")
    args = parser.parse_args()

    if not any((args.write, args.check, args.print_markdown)):
        args.write = True

    profile = build_profile_model()
    assets = build_generated_assets(profile)
    markdown = assets[MARKDOWN_OUTPUT]

    if args.write:
        write_outputs(assets)

    if args.check:
        check_status = check_outputs(assets)
        if check_status != 0:
            return check_status

    if args.print_markdown:
        sys.stdout.write(markdown)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
