#!/usr/bin/env python3
import json
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
VIA_JSON = SCRIPT_DIR / "charybdis.layout.json"

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

# ASCII art
ROW_COMMENTS = [
    "  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮",
    "  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤",
    "  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤",
    "  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤",
    "  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯",
    "  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯",
]

# Token normalisation: JSON → your keymap style
REPLACEMENTS = {
    # Transparent / no key
    "KC_NO": "XXXXXXX",
    "KC_TRNS": "_______",

    # Shifted symbols on LOWER
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
    "KC_GESC":    "QK_GESC",
    
    "RESET": "QK_BOOT",
    "QK_CLEAR_EEPROM": "EE_CLR",
    "CUSTOM(0)": "DPI_MOD",
    "CUSTOM(1)": "DPI_RMOD",
    "CUSTOM(2)": "S_D_MOD",
    "CUSTOM(3)": "S_D_RMOD",
    "CUSTOM(4)": "SNIPING",
    "CUSTOM(5)": "SNP_TOG",
    "CUSTOM(6)": "DRGSCRL",
    "CUSTOM(7)": "DRG_TOG",
    
    "MACRO(0)": "MACRO_0",
    "MACRO(1)": "MACRO_1",
    "MACRO(2)": "MACRO_2",
    "MACRO(3)": "MACRO_3",
    "MACRO(4)": "MACRO_4",
    "MACRO(5)": "MACRO_5",
    "MACRO(6)": "MACRO_6",
    "MACRO(7)": "MACRO_7",
    "MACRO(8)": "MACRO_8",
    "MACRO(9)": "MACRO_9",
    "MACRO(10)": "MACRO_10",
    "MACRO(11)": "MACRO_11",
    "MACRO(12)": "MACRO_12",
    "MACRO(13)": "MACRO_13",
    "MACRO(14)": "MACRO_14",
    "MACRO(15)": "MACRO_15",
    
    "CUSTOM(64)": "MACRO_0",
    "CUSTOM(65)": "MACRO_1",
    "CUSTOM(66)": "MACRO_2",
    "CUSTOM(67)": "MACRO_3",
    "CUSTOM(68)": "MACRO_4",
    "CUSTOM(69)": "MACRO_5",
    "CUSTOM(70)": "MACRO_6",
    "CUSTOM(71)": "MACRO_7",
    "CUSTOM(72)": "MACRO_8",
    "CUSTOM(73)": "MACRO_9",
    "CUSTOM(74)": "MACRO_10",
    "CUSTOM(75)": "MACRO_11",
    "CUSTOM(76)": "MACRO_12",
    "CUSTOM(77)": "MACRO_13",
    "CUSTOM(78)": "MACRO_14",
    "CUSTOM(79)": "MACRO_15",
    
    "CUSTOM(80)": "VOLMODE",
    "CUSTOM(81)": "CARET_MODE",
    "CUSTOM(82)": "DRG_TOG_ON_HOLD",
}

def apply_replacements(token: str) -> str:
    return REPLACEMENTS.get(token, token)

def load_via_layers(path: Path):
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

# Formatting helpers – mostly fixed, with local dynamic adjustment
COL_WIDTH     = 17       # "ideal" width per key
MIN_PAD       = 1        # minimum spaces in front of a token
INDENT_MAIN   = "        "  # 8 spaces
INDENT_THUMB1 = "                                                                 "
INDENT_THUMB2 = "                                                                                    "

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
    inner = format_group(row_tokens, 5)
    return f"{INDENT_THUMB1}{inner},"

def format_thumb_row2(row_tokens):
    inner = format_group(row_tokens, 3)
    return f"{INDENT_THUMB2}{inner}"

def render_layer(layer_name, tokens):
    out = [f"  [{layer_name}] = LAYOUT("]

    # 4 main rows
    for i in range(4):
        start, end = ROW_SLICES[i]
        out.append(ROW_COMMENTS[i])
        out.append(format_split_row(tokens[start:end]))

    # Thumb rows
    out.append(ROW_COMMENTS[4])
    out.append(format_thumb_row1(tokens[48:53]))
    out.append(format_thumb_row2(tokens[53:56]))
    out.append(ROW_COMMENTS[5])

    out.append("  ),")
    return "\n".join(out)

def main():
    via_layers = load_via_layers(VIA_JSON)
    all_tokens = [via_layer_to_layout_tokens(v) for v in via_layers]

    layer_names = [
        "LAYER_BASE",
        "LAYER_LOWER",
        "LAYER_RAISE",
        "LAYER_POINTER",
    ]

    for idx, tokens in enumerate(all_tokens):
        name = layer_names[idx] if idx < len(layer_names) else f"LAYER_{idx}"
        print(render_layer(name, tokens))
        print()  # blank line between layers

if __name__ == "__main__":
    main()