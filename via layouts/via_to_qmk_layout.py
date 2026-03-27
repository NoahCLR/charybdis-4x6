#!/usr/bin/env python3
# Convert a VIA layout JSON export into formatted QMK LAYOUT() blocks.
#
# Usage:
#   python via_to_qmk_layout.py           Use default MODE (set below)
#   python via_to_qmk_layout.py --write   Update key_config.h in-place
#   python via_to_qmk_layout.py --print   Print keymaps block to stdout
#
# Reads charybdis.layout.json from the same directory.
import json
import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
VIA_JSON = SCRIPT_DIR / "charybdis.layout.json"
KEYMAP_C = SCRIPT_DIR.parent / "keyboards" / "bastardkb" / "charybdis" / "4x6" / "keymaps" / "noah" / "key_config.h"

# Default mode: "write" updates keymap.c in-place, "print" prints to stdout.
# Can be overridden with --write or --print on the command line.
MODE = "write"

# ─── Configuration ────────────────────────────────────────────────────────────
# These are the only things you should need to edit when your keymap changes.

# Must match the layer enum order in key_config.h.
# → Adding or renaming a layer?  Update this list to match.
#   Extra VIA layers beyond this list get named LAYER_0, LAYER_1, etc.
LAYER_NAMES = [
    "LAYER_BASE",
    "LAYER_NUM",
    "LAYER_LOWER",
    "LAYER_RAISE",
    "LAYER_POINTER",
]

# Token normalisation: VIA JSON format → QMK keymap style.
# VIA exports keycodes in its own format (CUSTOM(), MACRO(), S(), etc.)
# that don't match what we use in key_config.h.  This dict translates them.
REPLACEMENTS = {
    # Transparent / no key
    "KC_NO": "XXXXXXX",
    "KC_TRNS": "_______",

    # Shifted symbols
    "S(KC_GRV)":  "KC_TILD",
    "S(KC_1)":    "KC_EXLM",
    "S(KC_2)":    "KC_AT",
    "S(KC_3)":    "KC_HASH",
    "S(KC_4)":    "KC_DLR",
    "S(KC_5)":    "KC_PERC",
    "S(KC_6)":    "KC_CIRC",
    "S(KC_7)":    "KC_AMPR",
    "S(KC_8)":    "KC_ASTR",
    "S(KC_9)":    "KC_LPRN",
    "S(KC_0)":    "KC_RPRN",
    "S(KC_MINS)": "KC_UNDS",

    # Deprecated / renamed keycodes (QMK 0.32+)
    "KC_GESC": "QK_GESC",
    "RESET": "QK_BOOT",
    "QK_CLEAR_EEPROM": "EE_CLR",

    # Deprecated short modifier keycodes → full names (QMK 0.32+)
    "KC_LCTL":  "KC_LEFT_CTRL",
    "KC_RCTL":  "KC_RIGHT_CTRL",
    "KC_LSFT":  "KC_LEFT_SHIFT",
    "KC_RSFT":  "KC_RIGHT_SHIFT",
    "KC_LALT":  "KC_LEFT_ALT",
    "KC_RALT":  "KC_RIGHT_ALT",
    "KC_LGUI":  "KC_LEFT_GUI",
    "KC_RGUI":  "KC_RIGHT_GUI",

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

    # Charybdis firmware keycodes (VIA CUSTOM 0–7)
    "CUSTOM(0)": "DPI_MOD",
    "CUSTOM(1)": "DPI_RMOD",
    "CUSTOM(2)": "S_D_MOD",
    "CUSTOM(3)": "S_D_RMOD",
    "CUSTOM(4)": "SNIPING",
    "CUSTOM(5)": "SNP_TOG",
    "CUSTOM(6)": "DRGSCRL",
    "CUSTOM(7)": "DRG_TOG",

    # Custom keycodes (VIA CUSTOM 80+)
    # VIA assigns CUSTOM(64 + n) where n is the keycode's position in
    # key_config.h's custom_keycodes enum (0-indexed from SAFE_RANGE).
    # MACRO_0–15 are positions 0–15 → CUSTOM(64)–CUSTOM(79) (generated below).
    # The keycodes after MACRO_15 start at position 16 → CUSTOM(80).
    # → Adding a new custom keycode?  Append it to the enum in key_config.h,
    #   then add a CUSTOM(64 + position) → NAME entry here.
    "CUSTOM(80)": "VOLUME_MODE",      # enum position 16
    "CUSTOM(81)": "BRIGHTNESS_MODE",  # enum position 17
    "CUSTOM(82)": "ARROW_MODE",       # enum position 18
    "CUSTOM(83)": "ZOOM_MODE",        # enum position 19
    "CUSTOM(84)": "DRG_TOG_ON_HOLD",  # enum position 20

}

# VIA represents macros as both MACRO(n) and CUSTOM(64+n) depending on
# firmware version.  Generate both mappings → MACRO_n.
for i in range(16):
    REPLACEMENTS[f"MACRO({i})"] = f"MACRO_{i}"
    REPLACEMENTS[f"CUSTOM({64 + i})"] = f"MACRO_{i}"

# ─── Layout structure ─────────────────────────────────────────────────────────
# These control the physical key layout and output formatting.
# You shouldn't need to change these unless the keyboard hardware changes.

# VIA exports keys in a flat array (60 entries) with its own ordering.
# QMK's LAYOUT() macro expects a different 56-key matrix order.
# This table maps: LAYOUT position → VIA flat index.
# The right half indices are reversed per row because VIA numbers them
# right-to-left while LAYOUT expects left-to-right.
LAYOUT_FROM_VIA_INDEX = [
    0,  1,  2,  3,  4,  5,   35, 34, 33, 32, 31, 30,
    6,  7,  8,  9, 10, 11,   41, 40, 39, 38, 37, 36,
   12, 13, 14, 15, 16, 17,   47, 46, 45, 44, 43, 42,
   18, 19, 20, 21, 22, 23,   53, 52, 51, 50, 49, 48,
   27, 28, 25, 55, 57,       29, 26, 59,
]

# Row slicing in the 56-key LAYOUT order
ROW_SLICES = [
    (0, 12),   # row 0: main
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
COL_WIDTH     = 17       # "ideal" width per key
MIN_PAD       = 1        # minimum spaces in front of a token
INDENT_MAIN   = " " * 8
INDENT_THUMB1 = " " * 65
INDENT_THUMB2 = " " * 84

# ─── Implementation ───────────────────────────────────────────────────────────

# Patterns that should always have a REPLACEMENTS entry.
# If one slips through, it's likely a new keycode that was added in VIA
# but not mapped here yet.
_WARN_PATTERNS = re.compile(r"^(CUSTOM|MACRO)\(\d+\)$")

def apply_replacements(token: str) -> str:
    if token in REPLACEMENTS:
        return REPLACEMENTS[token]
    if _WARN_PATTERNS.match(token):
        print(f"WARNING: unmapped token '{token}' — add it to REPLACEMENTS", file=sys.stderr)
    return token

def load_via_layers(path: Path):
    if not path.exists():
        print(f"ERROR: VIA JSON not found: {path}", file=sys.stderr)
        sys.exit(1)
    with path.open() as f:
        data = json.load(f)
    return data["layers"]

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

    # Initial padding to right-align in COL_WIDTH, but at least MIN_PAD
    pads = [max(COL_WIDTH - L, MIN_PAD) for L in lengths]

    # Target width for this group (without any external indentation)
    target = cols * COL_WIDTH + 2 * (cols - 1)

    # Current width: sum(padding + token) + ", " separators
    current = sum(pads[i] + lengths[i] for i in range(cols)) + 2 * (cols - 1)
    overflow = current - target

    # If overflow > 0, try to steal padding space (left-to-right) until
    # we hit the target width or we can't shrink further.
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
    left  = row_tokens[:6]
    right = row_tokens[6:]
    left_str  = format_group(left, 6)
    right_str = format_group(right, 6)
    return f"{INDENT_MAIN}{left_str},    {right_str},"

def format_thumb_row1(row_tokens):
    left  = row_tokens[:3]
    right = row_tokens[3:]
    left_str  = format_group(left, 3)
    right_str = format_group(right, 2)
    return f"{INDENT_THUMB1}{left_str},    {right_str},"

def format_thumb_row2(row_tokens):
    left  = row_tokens[:2]
    right = row_tokens[2:]
    left_str  = format_group(left, 2)
    right_str = format_group(right, 1)
    return f"{INDENT_THUMB2}{left_str},    {right_str}"

def render_layer(layer_name, tokens, indent="  "):
    out = [f"{indent}[{layer_name}] = LAYOUT("]

    # 4 main rows
    for i in range(4):
        start, end = ROW_SLICES[i]
        out.append(ROW_COMMENTS[i])
        out.append(format_split_row(tokens[start:end]))

    # Thumb rows
    start, end = ROW_SLICES[4]
    out.append(ROW_COMMENTS[4])
    out.append(format_thumb_row1(tokens[start:end]))
    start, end = ROW_SLICES[5]
    out.append(format_thumb_row2(tokens[start:end]))
    out.append(ROW_COMMENTS[5])

    out.append(f"{indent}),")
    return "\n".join(out)

def render_all_layers(indent="  "):
    """Convert all VIA layers into a single string of LAYOUT() blocks."""
    via_layers = load_via_layers(VIA_JSON)
    all_tokens = [via_layer_to_layout_tokens(v) for v in via_layers]

    blocks = []
    for idx, tokens in enumerate(all_tokens):
        name = LAYER_NAMES[idx] if idx < len(LAYER_NAMES) else f"LAYER_{idx}"
        blocks.append(render_layer(name, tokens, indent))
    return "\n\n".join(blocks)

# The keymaps declaration line used to find and replace the entire block.
_KEYMAPS_DECL = "const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {\n"

def render_keymaps_block():
    """Render the complete keymaps array including declaration and clang-format tags."""
    layouts = render_all_layers(indent="    ")
    return (
        "const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {\n"
        "    // clang-format off\n"
        f"{layouts}\n"
        "    // clang-format on\n"
        "};"
    )

def write_to_keymap():
    """Replace the entire keymaps array in key_config.h."""
    if not KEYMAP_C.exists():
        print(f"ERROR: key_config.h not found: {KEYMAP_C}", file=sys.stderr)
        sys.exit(1)
    text = KEYMAP_C.read_text()

    start = text.find(_KEYMAPS_DECL)
    if start == -1:
        print("ERROR: could not find keymaps declaration in key_config.h", file=sys.stderr)
        sys.exit(1)

    # Find the closing "};" after the opening declaration.
    end = text.find("};\n", start + len(_KEYMAPS_DECL))
    if end == -1:
        print("ERROR: could not find closing '};' for keymaps array", file=sys.stderr)
        sys.exit(1)
    end += len("};\n")

    new_text = text[:start] + render_keymaps_block() + "\n" + text[end:]

    KEYMAP_C.write_text(new_text)
    print(f"Updated {KEYMAP_C}")

def main():
    mode = MODE
    if "--write" in sys.argv:
        mode = "write"
    elif "--print" in sys.argv:
        mode = "print"

    if mode == "write":
        write_to_keymap()
    else:
        print(render_keymaps_block())

if __name__ == "__main__":
    main()
