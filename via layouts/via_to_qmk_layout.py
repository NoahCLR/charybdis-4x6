#!/usr/bin/env python3
# Convert a VIA layout JSON export into formatted QMK LAYOUT() blocks and
# keep the authored keymap.c tables in sync.
#
# Usage:
#   python via_to_qmk_layout.py                    Choose a JSON export from this
#                                                  directory and use default MODE
#   python via_to_qmk_layout.py --write            Update keymap.c in-place:
#                                                  rewrite keymaps[][] and make
#                                                  VIA_MACROS(MACRO) match the
#                                                  selected VIA export
#   python via_to_qmk_layout.py --print            Print VIA_MACROS(MACRO) and
#                                                  keymaps[][] previews to stdout
#   python via_to_qmk_layout.py --via-json path/to/export.json
#                                                  Override the default VIA export input
#   python via_to_qmk_layout.py --sync-macros-from-via
#                                                  Import only non-empty VIA
#                                                  macros into VIA_MACROS(MACRO)
#                                                  without touching keymaps[][]
#
# Reads the active VIA export from this directory and rewrites only the
# VIA-owned parts of keymap.c.
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
KEYMAP_FILE = REPO_ROOT / "keyboards" / "bastardkb" / "charybdis" / "4x6" / "keymaps" / "noah" / "keymap.c"

# Default mode: "write" makes the selected VIA export authoritative for
# keymaps[][] and VIA_MACROS(MACRO). If you do not run this script,
# keymap.c compiles exactly as authored.
MODE = "write"

# ─── Configuration ────────────────────────────────────────────────────────────
# These are the only things you should need to edit when your keymap changes.

# Must match the layer enum order in users/noah/noah_keymap.h.
# → Adding or renaming a layer? Update this list to match.
#   Extra VIA layers beyond this list get named LAYER_0, LAYER_1, etc.
LAYER_NAMES = [
    "LAYER_BASE",
    "LAYER_NUM",
    "LAYER_SYM",
    "LAYER_NAV",
    "LAYER_POINTER",
]

MACRO_COUNT = 16
VIA_CUSTOM_BASE = 64

# Upstream Charybdis keyboard keycodes, defined by the keyboard in the
# QK_KB_0..7 range. These are separate from this repo's
# users/noah/noah_keymap.h SAFE_RANGE enum.
CHARYBDIS_UPSTREAM_KEYCODES = [
    "DPI_MOD",
    "DPI_RMOD",
    "S_D_MOD",
    "S_D_RMOD",
    "SNIPING",
    "SNP_TOG",
    "DRGSCRL",
    "DRG_TOG",
]

# Current custom_keycodes enum entries that can appear directly on VIA layers.
PD_MODE_KEYCODES = [
    "VOLUME_MODE",
    "BRIGHTNESS_MODE",
    "ARROW_MODE",
    "ZOOM_MODE",
    "DRAGSCROLL",
    "PINCH_MODE",
]

# Token normalisation: VIA JSON format → QMK keymap style.
# VIA exports keycodes in its own format (CUSTOM(), MACRO(), S(), etc.)
# that don't match what we use in the QMK keymap sources. This dict translates them.
REPLACEMENTS = {
    # Transparent / no key
    "KC_NO": "XXXXXXX",
    "KC_TRNS": "_______",

    # Shifted symbols
    "S(KC_GRV)": "KC_TILD",
    "S(KC_1)": "KC_EXLM",
    "S(KC_2)": "KC_AT",
    "S(KC_3)": "KC_HASH",
    "S(KC_4)": "KC_DLR",
    "S(KC_5)": "KC_PERC",
    "S(KC_6)": "KC_CIRC",
    "S(KC_7)": "KC_AMPR",
    "S(KC_8)": "KC_ASTR",
    "S(KC_9)": "KC_LPRN",
    "S(KC_0)": "KC_RPRN",
    "S(KC_MINS)": "KC_UNDS",

    # Deprecated / renamed keycodes (QMK 0.32+)
    "KC_GESC": "QK_GESC",
    "RESET": "QK_BOOT",
    "QK_CLEAR_EEPROM": "EE_CLR",

    # Deprecated short modifier keycodes → full names (QMK 0.32+)
    "KC_LCTL": "KC_LEFT_CTRL",
    "KC_RCTL": "KC_RIGHT_CTRL",
    "KC_LSFT": "KC_LEFT_SHIFT",
    "KC_RSFT": "KC_RIGHT_SHIFT",
    "KC_LALT": "KC_LEFT_ALT",
    "KC_RALT": "KC_RIGHT_ALT",
    "KC_LGUI": "KC_LEFT_GUI",
    "KC_RGUI": "KC_RIGHT_GUI",

    # Deprecated mouse button keycodes → QMK 0.32+ names
    "KC_MS_BTN1": "MS_BTN1",
    "KC_MS_BTN2": "MS_BTN2",
    "KC_MS_BTN3": "MS_BTN3",
    "KC_MS_BTN4": "MS_BTN4",
    "KC_MS_BTN5": "MS_BTN5",

    # Shifted symbols with named equivalents
    "S(KC_QUOT)": "KC_DQUO",
    "S(KC_RBRC)": "KC_RCBR",
    "S(KC_LBRC)": "KC_LCBR",
}

# VIA encodes upstream Charybdis keyboard-level keycodes as CUSTOM(0)..CUSTOM(7).
for i, keycode in enumerate(CHARYBDIS_UPSTREAM_KEYCODES):
    REPLACEMENTS[f"CUSTOM({i})"] = keycode

# VIA exports its dynamic macro keycodes as MACRO(n).
# Those map only to the repo-local VIA_MACRO_n aliases.
for i in range(MACRO_COUNT):
    REPLACEMENTS[f"MACRO({i})"] = f"VIA_MACRO_{i}"

# VIA assigns CUSTOM(64 + n) where n is the keycode's position in
# users/noah/noah_keymap.h's custom_keycodes enum (0-indexed from SAFE_RANGE).
# MACRO_0–15 are the first custom keycodes → CUSTOM(64)–CUSTOM(79).
# These remain the hardcoded custom macros handled by macro_dispatch(), and
# must stay distinct from VIA's dynamic MACRO(n) keycodes in the JSON export.
for i in range(MACRO_COUNT):
    REPLACEMENTS[f"CUSTOM({VIA_CUSTOM_BASE + i})"] = f"MACRO_{i}"

# The pointing-device mode keys follow at positions 16–21 → CUSTOM(80)–CUSTOM(85).
# PD-mode and layer-lock action keycodes live above that range in
# noah_keymap.h, but this script does not map them because VIA exports the
# direct layout keycodes that appear on layers, not the derived lock actions
# authored through key_behaviors[].
for i, keycode in enumerate(PD_MODE_KEYCODES, start=MACRO_COUNT):
    REPLACEMENTS[f"CUSTOM({VIA_CUSTOM_BASE + i})"] = keycode

# ─── Layout structure ─────────────────────────────────────────────────────────
# These control the physical key layout and output formatting.
# You shouldn't need to change these unless the keyboard hardware changes.

# VIA exports keys in a flat array (60 entries) with its own ordering.
# QMK's LAYOUT() macro expects a different 56-key matrix order.
# This table maps: LAYOUT position → VIA flat index.
# The right half indices are reversed per row because VIA numbers them
# right-to-left while LAYOUT expects left-to-right.
LAYOUT_FROM_VIA_INDEX = [
    0,
    1,
    2,
    3,
    4,
    5,
    35,
    34,
    33,
    32,
    31,
    30,
    6,
    7,
    8,
    9,
    10,
    11,
    41,
    40,
    39,
    38,
    37,
    36,
    12,
    13,
    14,
    15,
    16,
    17,
    47,
    46,
    45,
    44,
    43,
    42,
    18,
    19,
    20,
    21,
    22,
    23,
    53,
    52,
    51,
    50,
    49,
    48,
    27,
    28,
    25,
    55,
    57,
    29,
    26,
    59,
]

# Row slicing in the 56-key LAYOUT order
ROW_SLICES = [
    (0, 12),  # row 0: main
    (12, 24),  # row 1: main
    (24, 36),  # row 2: main
    (36, 48),  # row 3: main
    (48, 53),  # thumb row 1 (5 keys)
    (53, 56),  # thumb row 2 (3 keys)
]

# ASCII art row separators
ROW_COMMENTS = [
    "  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮",
    "  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤",
    "  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤",
    "  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤",
    "  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯",
    "  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯",
]

# Formatting constants
COL_WIDTH = 17  # "ideal" width per key
MIN_PAD = 1  # minimum spaces in front of a token
INDENT_MAIN = " " * 8
INDENT_THUMB1 = " " * 65
INDENT_THUMB2 = " " * 84

# ─── Implementation ───────────────────────────────────────────────────────────

_KEYMAPS_DECL = "const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {\n"
_VIA_MACROS_DEFINE = "#define VIA_MACROS(MACRO)"
_VIA_MACROS_DEFINE_PATTERN = re.compile(r"^#define\s+VIA_MACROS\([^)]+\)", re.MULTILINE)
_WARN_PATTERNS = re.compile(r"^(CUSTOM|MACRO)\(\d+\)$")
_ONE_ARG_LAYER_PATTERNS = re.compile(r"^(MO|TO|TG|TT|OSL|DF)\((\d+)\)$")
_TWO_ARG_LAYER_PATTERNS = re.compile(r"^(LT|LM)\((\d+),(.*)\)$")
_VIA_MACRO_SLOT_PATTERN = re.compile(r"^VIA_MACRO_(\d+)$")
_VIA_MACRO_DEFAULT_LINE_PATTERN = re.compile(
    r'^\s*[A-Z_][A-Z0-9_]*\(\s*(?:VIA_MACRO_(\d+)|(\d+))\s*,\s*("(?:\\.|[^"\\])*")\s*\)\s*\\?\s*$'
)
_VIA_MACRO_DEFAULT_LINE_PATTERN_WITH_NAME = re.compile(
    r'^\s*[A-Z_][A-Z0-9_]*\(\s*(?:VIA_MACRO_(\d+)|(\d+))\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\)\s*\\?\s*$'
)
_VIA_MACRO_DEFAULT_LINE_PATTERN_LEGACY = re.compile(
    r'^\s*[A-Z_][A-Z0-9_]*\(\s*(?:VIA_MACRO_(\d+)|(\d+))\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\)\s*\\?\s*$'
)
_SIMPLE_DEFINE_PATTERN = re.compile(r"^#define\s+(X_[A-Z0-9_]+)\s+([A-Za-z0-9_]+)\s*$")


def die(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def layer_name_for_index(idx: int) -> str:
    return LAYER_NAMES[idx] if idx < len(LAYER_NAMES) else f"LAYER_{idx}"


def rewrite_layer_token(token: str) -> str:
    match = _ONE_ARG_LAYER_PATTERNS.match(token)
    if match:
        wrapper, layer_idx = match.groups()
        return f"{wrapper}({layer_name_for_index(int(layer_idx))})"

    match = _TWO_ARG_LAYER_PATTERNS.match(token)
    if match:
        wrapper, layer_idx, rest = match.groups()
        return f"{wrapper}({layer_name_for_index(int(layer_idx))},{rest})"

    return token


def apply_replacements(token: str) -> str:
    token = rewrite_layer_token(token)
    if token in REPLACEMENTS:
        return REPLACEMENTS[token]
    if _WARN_PATTERNS.match(token):
        print(f"WARNING: unmapped token '{token}' — add it to REPLACEMENTS", file=sys.stderr)
    return token


def load_via_data(path: Path) -> dict:
    if not path.exists():
        die(f"VIA JSON not found: {path}")
    with path.open() as f:
        return json.load(f)


def load_via_layers(path: Path):
    return load_via_data(path)["layers"]


def list_via_json_candidates() -> list[Path]:
    candidates = [path for path in SCRIPT_DIR.glob("*.json") if path.is_file()]
    candidates.sort(key=lambda path: path.name.lower())
    return candidates


def prompt_via_json_choice() -> Path:
    candidates = list_via_json_candidates()

    if not candidates:
        die(f"no VIA JSON files found in {SCRIPT_DIR}")

    if len(candidates) == 1:
        return candidates[0]

    sys.stderr.write("Select a VIA JSON export:\n")
    for idx, candidate in enumerate(candidates, start=1):
        sys.stderr.write(f"  {idx}. {candidate.name}\n")

    while True:
        sys.stderr.write(f"Choose 1-{len(candidates)} (or enter a file name): ")
        sys.stderr.flush()
        response = sys.stdin.readline()

        if response == "":
            die("VIA JSON selection required; no input received")

        response = response.strip()

        if response.isdigit():
            index = int(response)
            if 1 <= index <= len(candidates):
                return candidates[index - 1]

        for candidate in candidates:
            if response == candidate.name:
                return candidate

        sys.stderr.write("Invalid selection.\n")


def load_all_exported_via_macros(via_data: dict, via_json_path: Path) -> dict[int, dict]:
    macros = via_data.get("macros")
    if macros is None:
        return {}
    if not isinstance(macros, list):
        die(f"{via_json_path} is missing a top-level 'macros' list")
    if len(macros) != MACRO_COUNT:
        die(f"expected {MACRO_COUNT} macro slots in {via_json_path}, found {len(macros)}")

    slot_map: dict[int, dict] = {}

    for slot, raw_value in enumerate(macros):
        if not isinstance(raw_value, str):
            die(f"VIA macro slot {slot} is not a string")

        slot_map[slot] = {
            "slot": slot,
            "via": {"kind": "raw", "value": raw_value},
        }

    return slot_map


def merge_non_empty_exported_via_macros(slot_map: dict[int, dict], via_data: dict, via_json_path: Path) -> int:
    macros = via_data.get("macros")
    if macros is None:
        return 0
    if not isinstance(macros, list):
        die(f"{via_json_path} is missing a top-level 'macros' list")
    if len(macros) != MACRO_COUNT:
        die(f"expected {MACRO_COUNT} macro slots in {via_json_path}, found {len(macros)}")

    imported_count = 0

    for slot, raw_value in enumerate(macros):
        if not isinstance(raw_value, str):
            die(f"VIA macro slot {slot} is not a string")
        if not raw_value:
            continue

        imported_count += 1

        if slot not in slot_map:
            slot_map[slot] = {
                "slot": slot,
                "via": {"kind": "raw", "value": raw_value},
            }
            continue

        entry = slot_map[slot]
        entry.setdefault("via", {"kind": "raw", "value": raw_value})

        if entry["via"].get("kind") != "raw":
            die(f"slot {slot} has unsupported VIA payload kind during import")

        entry["via"]["value"] = raw_value

    return imported_count


def via_layer_to_layout_tokens(via_layer):
    if len(via_layer) != 60:
        raise ValueError(f"Expected 60 entries per layer, got {len(via_layer)}")
    tokens = [apply_replacements(via_layer[i]) for i in LAYOUT_FROM_VIA_INDEX]
    if len(tokens) != 56:
        raise ValueError("Mapping should produce 56 tokens per layer")
    return tokens


def format_group(tokens, cols):
    """
    Format a group of `cols` tokens so that:
    - normally each field is COL_WIDTH wide, separated by ", "
    - if some token is longer than COL_WIDTH, we shrink left padding
      (down to MIN_PAD) so that the final comma still lines up,
      if possible.
    """
    assert len(tokens) == cols
    lengths = [len(t) for t in tokens]

    pads = [max(COL_WIDTH - length, MIN_PAD) for length in lengths]
    target = cols * COL_WIDTH + 2 * (cols - 1)
    current = sum(pads[i] + lengths[i] for i in range(cols)) + 2 * (cols - 1)
    overflow = current - target

    while overflow > 0 and any(p > MIN_PAD for p in pads):
        for i in range(cols):
            if pads[i] > MIN_PAD:
                pads[i] -= 1
                overflow -= 1
                if overflow <= 0:
                    break

    fields = [" " * pads[i] + tokens[i] for i in range(cols)]
    return ", ".join(fields)


def format_split_row(row_tokens):
    left = row_tokens[:6]
    right = row_tokens[6:]
    left_str = format_group(left, 6)
    right_str = format_group(right, 6)
    return f"{INDENT_MAIN}{left_str},    {right_str},"


def format_thumb_row1(row_tokens):
    left = row_tokens[:3]
    right = row_tokens[3:]
    left_str = format_group(left, 3)
    right_str = format_group(right, 2)
    return f"{INDENT_THUMB1}{left_str},    {right_str},"


def format_thumb_row2(row_tokens):
    left = row_tokens[:2]
    right = row_tokens[2:]
    left_str = format_group(left, 2)
    right_str = format_group(right, 1)
    return f"{INDENT_THUMB2}{left_str},    {right_str}"


def render_layer(layer_name, tokens, indent="  "):
    out = [f"{indent}[{layer_name}] = LAYOUT("]

    for i in range(4):
        start, end = ROW_SLICES[i]
        out.append(ROW_COMMENTS[i])
        out.append(format_split_row(tokens[start:end]))

    start, end = ROW_SLICES[4]
    out.append(ROW_COMMENTS[4])
    out.append(format_thumb_row1(tokens[start:end]))
    start, end = ROW_SLICES[5]
    out.append(format_thumb_row2(tokens[start:end]))
    out.append(ROW_COMMENTS[5])

    out.append(f"{indent}),")
    return "\n".join(out)


def render_all_layers(via_json_path: Path, indent="  "):
    via_layers = load_via_layers(via_json_path)
    all_tokens = [via_layer_to_layout_tokens(layer) for layer in via_layers]

    blocks = []
    for idx, tokens in enumerate(all_tokens):
        name = layer_name_for_index(idx)
        blocks.append(render_layer(name, tokens, indent))
    return "\n\n".join(blocks)


def render_keymaps_block(via_json_path: Path):
    layouts = render_all_layers(via_json_path, indent="    ")
    return (
        "const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {\n"
        "    // clang-format off\n"
        f"{layouts}\n"
        "    // clang-format on\n"
        "};"
    )


def render_write_preview(via_json_path: Path) -> str:
    via_data = load_via_data(via_json_path)
    via_macros_block = render_via_macros_block(load_all_exported_via_macros(via_data, via_json_path))
    keymaps_block = render_keymaps_block(via_json_path)
    return f"{via_macros_block}\n\n{keymaps_block}"


def find_qmk_home() -> Path:
    candidates = []

    env_qmk_home = os.environ.get("QMK_HOME")
    if env_qmk_home:
        candidates.append(Path(env_qmk_home).expanduser())

    qmk_cli = shutil.which("qmk")
    if qmk_cli:
        result = subprocess.run([qmk_cli, "config", "user.qmk_home"], capture_output=True, text=True, check=False)
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                if line.startswith("user.qmk_home="):
                    candidates.append(Path(line.split("=", 1)[1]).expanduser())

    candidates.extend(
        [
            Path("~/qmk_firmware").expanduser(),
            Path("~/dev/charybdis/bastardkb-qmk").expanduser(),
        ]
    )

    for candidate in candidates:
        header = candidate / "quantum" / "send_string" / "send_string_keycodes.h"
        if header.exists():
            return candidate

    die("could not locate QMK home with quantum/send_string/send_string_keycodes.h")


def load_send_string_keycodes() -> dict[str, int]:
    header_path = find_qmk_home() / "quantum" / "send_string" / "send_string_keycodes.h"
    direct: dict[str, int] = {}
    aliases: dict[str, str] = {}

    for line in header_path.read_text().splitlines():
        match = _SIMPLE_DEFINE_PATTERN.match(line.strip())
        if not match:
            continue
        name, rhs = match.groups()
        if re.fullmatch(r"[0-9A-Fa-f]+", rhs):
            direct[name] = int(rhs, 16)
        elif rhs.startswith("X_"):
            aliases[name] = rhs

    resolved: dict[str, int] = {}

    def resolve(name: str) -> int:
        if name in resolved:
            return resolved[name]
        if name in direct:
            resolved[name] = direct[name]
            return resolved[name]
        if name in aliases:
            resolved[name] = resolve(aliases[name])
            return resolved[name]
        raise KeyError(name)

    for name in set(direct) | set(aliases):
        resolve(name)

    return resolved


def x_token_for_keycode(keycode: str) -> str:
    if not keycode.startswith("KC_"):
        die(f"unsupported VIA macro keycode '{keycode}' — expected KC_*")
    return f"X_{keycode[3:]}"


def send_string_value_for_keycode(keycode: str, send_string_keycodes: dict[str, int]) -> int:
    token = x_token_for_keycode(keycode)
    if token not in send_string_keycodes:
        die(f"'{keycode}' is not available in QMK send_string keycodes")
    return send_string_keycodes[token]


def parse_via_macro_ops(raw_value: str):
    ops = []
    text = []
    idx = 0

    def flush_text():
        if text:
            ops.append(("text", "".join(text)))
            text.clear()

    while idx < len(raw_value):
        char = raw_value[idx]
        if char == "{":
            flush_text()
            end = raw_value.find("}", idx + 1)
            if end == -1:
                die(f"unterminated '{{' in VIA macro payload: {raw_value!r}")
            content = raw_value[idx + 1 : end].strip()
            if not content:
                die(f"empty '{{}}' command in VIA macro payload: {raw_value!r}")

            if content.isdigit():
                ops.append(("delay", int(content)))
            elif content.startswith("+") or content.startswith("-"):
                action = "down" if content[0] == "+" else "up"
                keycode = content[1:].strip()
                if "," in keycode:
                    die(f"unsupported multi-key {action} command in VIA macro payload: {raw_value!r}")
                ops.append((action, keycode))
            else:
                keycodes = [part.strip() for part in content.split(",")]
                if any(not keycode for keycode in keycodes):
                    die(f"malformed key list in VIA macro payload: {raw_value!r}")
                ops.append(("tap", keycodes))
            idx = end + 1
            continue

        if char == "}":
            die(f"unmatched '}}' in VIA macro payload: {raw_value!r}")

        text.append(char)
        idx += 1

    flush_text()
    return ops


def via_ops_to_send_string_bytes(ops, send_string_keycodes: dict[str, int]) -> bytes:
    out = bytearray()

    for op in ops:
        kind = op[0]
        if kind == "text":
            for char in op[1]:
                codepoint = ord(char)
                if codepoint == 0:
                    die("NUL bytes are not supported in VIA macro text")
                if codepoint > 0x7F:
                    die(f"non-ASCII character {char!r} is not supported in VIA macro text")
                out.append(codepoint)
        elif kind == "delay":
            out.extend((1, 4))
            out.extend(str(op[1]).encode("ascii"))
            out.append(ord("|"))
        elif kind == "down":
            out.extend((1, 2, send_string_value_for_keycode(op[1], send_string_keycodes)))
        elif kind == "up":
            out.extend((1, 3, send_string_value_for_keycode(op[1], send_string_keycodes)))
        elif kind == "tap":
            keycodes = op[1]
            if len(keycodes) == 1:
                out.extend((1, 1, send_string_value_for_keycode(keycodes[0], send_string_keycodes)))
            else:
                for keycode in keycodes[:-1]:
                    out.extend((1, 2, send_string_value_for_keycode(keycode, send_string_keycodes)))
                out.extend((1, 1, send_string_value_for_keycode(keycodes[-1], send_string_keycodes)))
                for keycode in reversed(keycodes[:-1]):
                    out.extend((1, 3, send_string_value_for_keycode(keycode, send_string_keycodes)))
        else:
            die(f"unsupported macro op kind '{kind}'")

    return bytes(out)


def parse_c_string_literal(literal: str) -> str:
    return json.loads(literal)


def find_define_block_range(text: str, define_pattern: re.Pattern[str], label: str) -> tuple[int, int]:
    match = define_pattern.search(text)
    if not match:
        die(f"could not find {label} define in keymap.c")
    start = match.start()

    line_end = text.find("\n", start)
    if line_end == -1:
        return start, len(text)

    end = line_end + 1
    line = text[start:line_end]

    while line.rstrip().endswith("\\"):
        next_line_end = text.find("\n", end)
        if next_line_end == -1:
            return start, len(text)
        line = text[end:next_line_end]
        end = next_line_end + 1

    return start, end


def load_via_macros_from_keymap_text(text: str) -> dict[int, dict]:
    start, end = find_define_block_range(text, _VIA_MACROS_DEFINE_PATTERN, "VIA_MACROS(MACRO)")
    block_text = text[start:end]
    slot_map: dict[int, dict] = {}

    for raw_line in block_text.splitlines():
        line = raw_line.strip()
        if not line or _VIA_MACROS_DEFINE_PATTERN.match(line):
            continue

        match = _VIA_MACRO_DEFAULT_LINE_PATTERN.match(line)
        legacy_with_name = _VIA_MACRO_DEFAULT_LINE_PATTERN_WITH_NAME.match(line) if not match else None
        legacy_match = _VIA_MACRO_DEFAULT_LINE_PATTERN_LEGACY.match(line) if not match and not legacy_with_name else None
        if not match and not legacy_with_name and not legacy_match:
            die(f"could not parse VIA_MACROS line: {raw_line.strip()}")

        active_match = match or legacy_with_name or legacy_match
        slot_text = active_match.group(1) or active_match.group(2)
        slot = int(slot_text)
        if slot < 0 or slot >= MACRO_COUNT:
            die(f"invalid VIA macro slot index: {slot}")
        if slot in slot_map:
            die(f"duplicate VIA macro slot {slot} in keymap.c")

        if match:
            value = parse_c_string_literal(match.group(3))
        elif legacy_with_name:
            value = parse_c_string_literal(legacy_with_name.group(4))
        else:
            value = parse_c_string_literal(legacy_match.group(5))

        slot_map[slot] = {
            "slot": slot,
            "via": {"kind": "raw", "value": value},
        }

    return slot_map


def validate_via_macros(slot_map: dict[int, dict], send_string_keycodes: dict[str, int]) -> None:
    for slot, entry in sorted(slot_map.items()):
        via = entry.get("via")

        if not isinstance(via, dict) or via.get("kind") != "raw":
            die(f"slot {slot} must have via.kind == 'raw'")

        via_value = via.get("value")
        if not isinstance(via_value, str):
            die(f"slot {slot} must have a string via.value")

        if via_value:
            via_ops_to_send_string_bytes(parse_via_macro_ops(via_value), send_string_keycodes)


def render_via_macros_block(slot_map: dict[int, dict]) -> str:
    entries = []
    for slot in range(MACRO_COUNT):
        entry = slot_map.get(slot, {"via": {"value": ""}})
        value = json.dumps(entry["via"]["value"])
        entries.append(f"    MACRO(VIA_MACRO_{slot}, {value})")

    lines = [_VIA_MACROS_DEFINE]
    if not entries:
        return "\n".join(lines)
    else:
        lines[0] += " \\"
        for idx, entry_line in enumerate(entries):
            suffix = " \\" if idx < len(entries) - 1 else ""
            lines.append(f"{entry_line}{suffix}")
    return "\n".join(lines)


def collect_used_macro_slots(via_layers) -> set[int]:
    used_slots: set[int] = set()

    for layer in via_layers:
        for token in layer:
            rewritten = apply_replacements(token)
            match = _VIA_MACRO_SLOT_PATTERN.match(rewritten)
            if match:
                used_slots.add(int(match.group(1)))

    return used_slots


def validate_used_macro_slots(used_slots: set[int], slot_map: dict[int, dict]) -> None:
    missing = sorted(slot for slot in used_slots if slot not in slot_map)
    if missing:
        formatted = ", ".join(f"VIA_MACRO_{slot}" for slot in missing)
        die(f"layout references VIA macro slots missing from VIA_MACROS(MACRO): {formatted}")


def replace_keymaps_block(text: str, new_block: str) -> str:
    start = text.find(_KEYMAPS_DECL)
    if start == -1:
        die("could not find keymaps declaration in keymap.c")

    end = text.find("};\n", start + len(_KEYMAPS_DECL))
    if end == -1:
        die("could not find closing '};' for keymaps array")
    end += len("};\n")

    return text[:start] + new_block + "\n" + text[end:]


def replace_via_macros_block(text: str, new_block: str) -> str:
    start, end = find_define_block_range(text, _VIA_MACROS_DEFINE_PATTERN, "VIA_MACROS(MACRO)")
    return text[:start] + new_block + "\n" + text[end:]


def display_path(path: Path) -> str:
    try:
        return str(path.relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def prompt_confirm(prompt: str) -> bool:
    sys.stderr.write(f"{prompt} [y/N]: ")
    sys.stderr.flush()
    response = sys.stdin.readline()
    if response == "":
        die("confirmation required; no input received")
    response = response.strip().lower()
    return response in {"y", "yes"}


def confirm_write_targets(via_json_path: Path) -> tuple[bool, bool]:
    sys.stderr.write(
        f"Sync {display_path(via_json_path)} -> {display_path(KEYMAP_FILE)}\n"
    )
    rewrite_macros = prompt_confirm("[1/2] Update VIA_MACROS(MACRO) from export macros[]?")
    rewrite_keymaps = prompt_confirm("[2/2] Update keymaps[][] from export layers?")
    return rewrite_macros, rewrite_keymaps


def confirm_macro_sync(via_json_path: Path) -> None:
    if not prompt_confirm(
        f"Update VIA_MACROS(MACRO) in {display_path(KEYMAP_FILE)} "
        f"from macros[] in {display_path(via_json_path)}?"
    ):
        print("Cancelled; keymap.c unchanged.", file=sys.stderr)
        sys.exit(1)


def write_generated_outputs(via_json_path: Path, rewrite_macros: bool, rewrite_keymaps: bool) -> None:
    if not KEYMAP_FILE.exists():
        die(f"keymap.c not found: {KEYMAP_FILE}")
    if not rewrite_macros and not rewrite_keymaps:
        print("Cancelled; keymap.c unchanged.", file=sys.stderr)
        return

    keymap_text = KEYMAP_FILE.read_text()
    via_data = load_via_data(via_json_path)
    send_string_keycodes = load_send_string_keycodes()
    current_via_slot_map = load_via_macros_from_keymap_text(keymap_text)
    replacement_via_slot_map = load_all_exported_via_macros(via_data, via_json_path) if rewrite_macros else current_via_slot_map
    effective_via_slot_map = replacement_via_slot_map if rewrite_macros else current_via_slot_map

    validate_via_macros(effective_via_slot_map, send_string_keycodes)
    if rewrite_keymaps:
        validate_used_macro_slots(collect_used_macro_slots(via_data["layers"]), effective_via_slot_map)

    if rewrite_macros:
        keymap_text = replace_via_macros_block(keymap_text, render_via_macros_block(replacement_via_slot_map))
    if rewrite_keymaps:
        keymap_text = replace_keymaps_block(keymap_text, render_keymaps_block(via_json_path))
    KEYMAP_FILE.write_text(keymap_text)

    updated_sections = []
    if rewrite_macros:
        updated_sections.append("VIA_MACROS(MACRO)")
    if rewrite_keymaps:
        updated_sections.append("keymaps[][]")
    print(f"Updated {display_path(KEYMAP_FILE)} ({', '.join(updated_sections)})")


def sync_macros_from_via(via_json_path: Path) -> None:
    if not KEYMAP_FILE.exists():
        die(f"keymap.c not found: {KEYMAP_FILE}")

    keymap_text = KEYMAP_FILE.read_text()
    via_data = load_via_data(via_json_path)
    slot_map = load_via_macros_from_keymap_text(keymap_text)
    imported_count = merge_non_empty_exported_via_macros(slot_map, via_data, via_json_path)

    if imported_count == 0:
        print(f"No non-empty VIA macros found in {via_json_path}; keymap.c unchanged")
        return

    validate_via_macros(slot_map, load_send_string_keycodes())
    keymap_text = replace_via_macros_block(keymap_text, render_via_macros_block(slot_map))
    KEYMAP_FILE.write_text(keymap_text)
    print(f"Updated {display_path(KEYMAP_FILE)} (VIA_MACROS(MACRO))")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--write",
        action="store_true",
        help="update keymap.c in-place: rewrite keymaps[][] and VIA_MACROS(MACRO) from the selected VIA export",
    )
    group.add_argument("--print", action="store_true", help="print rendered VIA_MACROS(MACRO) and keymaps[][] previews to stdout")
    group.add_argument(
        "--sync-macros-from-via",
        action="store_true",
        help="import only non-empty VIA macros into VIA_MACROS(MACRO) without touching keymaps[][]",
    )
    parser.add_argument(
        "--via-json",
        type=Path,
        help="path to the VIA export JSON to read; if omitted, choose one from this directory interactively",
    )
    args = parser.parse_args()

    mode = MODE
    if args.write:
        mode = "write"
    elif args.print:
        mode = "print"
    elif args.sync_macros_from_via:
        mode = "sync-macros-from-via"

    via_json_path = args.via_json.expanduser().resolve() if args.via_json else prompt_via_json_choice().resolve()

    if mode == "write":
        rewrite_macros, rewrite_keymaps = confirm_write_targets(via_json_path)
        if not rewrite_macros and not rewrite_keymaps:
            print("Cancelled; keymap.c unchanged.", file=sys.stderr)
            sys.exit(1)
        write_generated_outputs(via_json_path, rewrite_macros, rewrite_keymaps)
    elif mode == "sync-macros-from-via":
        confirm_macro_sync(via_json_path)
        sync_macros_from_via(via_json_path)
    else:
        print(render_write_preview(via_json_path))


if __name__ == "__main__":
    main()
