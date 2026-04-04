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
#
# Reads the active VIA export from this directory and rewrites only the
# VIA-owned parts of keymap.c.
import argparse
import json
import re
import sys
from pathlib import Path
from typing import NoReturn

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
NOAH_USERSPACE_CUSTOM_KEYCODE_COUNT = MACRO_COUNT + (2 * len(PD_MODE_KEYCODES)) + len(LAYER_NAMES)

# Token normalization: VIA JSON format → QMK keymap style.
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

_KEYMAP_CUSTOM_KEYCODES_PATTERN = re.compile(
    r"enum\s+keymap_custom_keycodes\s*\{(?P<body>.*?)\};",
    re.DOTALL,
)
_LINE_COMMENT_PATTERN = re.compile(r"//.*?$", re.MULTILINE)
_BLOCK_COMMENT_PATTERN = re.compile(r"/\*.*?\*/", re.DOTALL)
_ENUM_ENTRY_PATTERN = re.compile(r"^([A-Z_][A-Z0-9_]*)(?:\s*=\s*(.+))?$")

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


def load_keymap_local_custom_keycodes() -> list[str]:
    if not KEYMAP_FILE.exists():
        die(f"keymap.c not found: {KEYMAP_FILE}")

    keymap_text = KEYMAP_FILE.read_text()
    match = _KEYMAP_CUSTOM_KEYCODES_PATTERN.search(keymap_text)
    if not match:
        return []

    body = _BLOCK_COMMENT_PATTERN.sub("", match.group("body"))
    body = _LINE_COMMENT_PATTERN.sub("", body)

    keycodes: list[str] = []

    for raw_entry in body.split(","):
        entry = raw_entry.strip()
        if not entry:
            continue

        match = _ENUM_ENTRY_PATTERN.fullmatch(entry)
        if not match:
            die(f"could not parse enum keymap_custom_keycodes entry: {entry!r}")

        name, value = match.groups()
        if name == "KEYMAP_CUSTOM_KEYCODE_SENTINEL":
            if value is None or value.strip() != "NOAH_KEYMAP_SAFE_RANGE - 1":
                die("KEYMAP_CUSTOM_KEYCODE_SENTINEL must be assigned to NOAH_KEYMAP_SAFE_RANGE - 1")
            continue

        if value is not None:
            normalized_value = value.strip()
            if keycodes or normalized_value != "NOAH_KEYMAP_SAFE_RANGE":
                die(
                    "only the first real keymap-local custom keycode may have an explicit assignment, "
                    "and it must be NOAH_KEYMAP_SAFE_RANGE"
                )

        keycodes.append(name)

    return keycodes


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
_VIA_MACRO_DEFAULT_LINE_PATTERN = re.compile(
    r'^\s*[A-Z_][A-Z0-9_]*\(\s*(?:VIA_MACRO_(\d+)|(\d+))\s*,\s*("(?:\\.|[^"\\])*")\s*\)\s*\\?\s*$'
)
_VIA_MACRO_DEFAULT_LINE_PATTERN_WITH_NAME = re.compile(
    r'^\s*[A-Z_][A-Z0-9_]*\(\s*(?:VIA_MACRO_(\d+)|(\d+))\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\)\s*\\?\s*$'
)
_VIA_MACRO_DEFAULT_LINE_PATTERN_LEGACY = re.compile(
    r'^\s*[A-Z_][A-Z0-9_]*\(\s*(?:VIA_MACRO_(\d+)|(\d+))\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\)\s*\\?\s*$'
)


def die(message: str) -> NoReturn:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


for i, keycode in enumerate(load_keymap_local_custom_keycodes(), start=NOAH_USERSPACE_CUSTOM_KEYCODE_COUNT):
    REPLACEMENTS[f"CUSTOM({VIA_CUSTOM_BASE + i})"] = keycode


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
        print(
            f"WARNING: unmapped token '{token}' — add it to REPLACEMENTS or keymap_custom_keycodes",
            file=sys.stderr,
        )
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
        if active_match is None:
            die(f"could not parse VIA_MACROS line: {raw_line.strip()}")
        slot_text = active_match.group(1) or active_match.group(2)
        slot = int(slot_text)
        if slot < 0 or slot >= MACRO_COUNT:
            die(f"invalid VIA macro slot index: {slot}")
        if slot in slot_map:
            die(f"duplicate VIA macro slot {slot} in keymap.c")

        if match is not None:
            value = parse_c_string_literal(match.group(3))
        elif legacy_with_name is not None:
            value = parse_c_string_literal(legacy_with_name.group(4))
        elif legacy_match is not None:
            value = parse_c_string_literal(legacy_match.group(5))
        else:
            die(f"could not resolve VIA_MACROS line payload: {raw_line.strip()}")

        slot_map[slot] = {
            "slot": slot,
            "via": {"kind": "raw", "value": value},
        }

    return slot_map


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
    rewrite_macros = prompt_confirm("[1/2] Update VIA_MACROS(MACRO) in keymap.c from VIA export macros[]?")
    rewrite_keymaps = prompt_confirm("[2/2] Update keymaps[][] in keymap.c from VIA export layers?")
    return rewrite_macros, rewrite_keymaps


def write_generated_outputs(via_json_path: Path, rewrite_macros: bool, rewrite_keymaps: bool) -> None:
    if not KEYMAP_FILE.exists():
        die(f"keymap.c not found: {KEYMAP_FILE}")
    if not rewrite_macros and not rewrite_keymaps:
        print("Cancelled; keymap.c unchanged.", file=sys.stderr)
        return

    keymap_text = KEYMAP_FILE.read_text()
    via_data = load_via_data(via_json_path)
    current_via_slot_map = load_via_macros_from_keymap_text(keymap_text)
    replacement_via_slot_map = load_all_exported_via_macros(via_data, via_json_path) if rewrite_macros else current_via_slot_map

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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--write",
        action="store_true",
        help="update keymap.c in-place: rewrite keymaps[][] and VIA_MACROS(MACRO) from the selected VIA export",
    )
    group.add_argument("--print", action="store_true", help="print rendered VIA_MACROS(MACRO) and keymaps[][] previews to stdout")
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

    via_json_path = args.via_json.expanduser().resolve() if args.via_json else prompt_via_json_choice().resolve()

    if mode == "write":
        rewrite_macros, rewrite_keymaps = confirm_write_targets(via_json_path)
        if not rewrite_macros and not rewrite_keymaps:
            print("Cancelled; keymap.c unchanged.", file=sys.stderr)
            sys.exit(1)
        write_generated_outputs(via_json_path, rewrite_macros, rewrite_keymaps)
    else:
        print(render_write_preview(via_json_path))


if __name__ == "__main__":
    main()
