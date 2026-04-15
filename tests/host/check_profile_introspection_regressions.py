#!/usr/bin/env python3

from __future__ import annotations

import sys
import importlib.util
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
REPORT = ROOT / "docs" / "profile-introspection.md"
SCRIPT = ROOT / "tools" / "profile_introspect.py"
AUTHORED_FILES = [
    ROOT / "keyboards" / "bastardkb" / "charybdis" / "4x6" / "keymaps" / "noah" / "keymap.c",
    ROOT / "keyboards" / "bastardkb" / "charybdis" / "4x6" / "keymaps" / "noah" / "rgb_config.c",
    ROOT / "keyboards" / "bastardkb" / "charybdis" / "4x6" / "keymaps" / "noah" / "config.h",
]


EXPECTED_SNIPPETS = {
    "timing legend": "Timing legend for the layer-local behavior tables:",
    "pd manifest note": "PD mode names and bindings in this report stay in sync with the shared definitions in",
    "esc long-hold timing": "| `ESC` | `ESC` (`KC_ESC`) | `single` | `-` | `-` | `TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))` | `tap_hold(150), long_hold(400), multi_tap(150)` |",
    "minus hold timing": "| `-` | `-` (`KC_MINS`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_UNDS)` | `-` | `tap_hold(150)` |",
    "left shift fallback timing": "| `LSFT` | `LSFT` (`KC_LEFT_SHIFT`) | `single` | `TAP_SENDS(KC_CAPS)` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |",
    "right alt fallback timing": "| `RALT` | `RALT` (`KC_RIGHT_ALT`) | `single` | `TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |",
    "lt nav timing override": "| `LT[NAV]/SLSH` | `LT[NAV]/SLSH` (`LT(LAYER_NAV,KC_SLSH)`) | `double` | `-` | `TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NAV))` | `-` | `tap_hold=100, multi_tap(150)` |",
}

EXPECTED_DISPLAY_TOKENS = {
    "KC_LEFT_SHIFT": "LSFT",
    "KC_RIGHT_ALT": "RALT",
    "KC_EXLM": "!",
    "KC_LPRN": "(",
    "KC_TRNS": "TRNS",
    "KC_P3": "P3",
    "KC_CAPS": "CAPS",
    "KC_MS_UP": "MS UP",
    "KC_PRINT_SCREEN": "PSCR",
    "KC_HOME": "HOME",
    "KC_MSTP": "STOP",
}


def load_profile_introspect_module():
    spec = importlib.util.spec_from_file_location("profile_introspect", SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load module from {SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def current_authored_kc_tokens() -> set[str]:
    tokens: set[str] = set()
    for path in AUTHORED_FILES:
        tokens.update(re.findall(r"\bKC_[A-Z0-9_]+\b", path.read_text()))
    return tokens


def main() -> int:
    text = REPORT.read_text()
    missing = [name for name, snippet in EXPECTED_SNIPPETS.items() if snippet not in text]
    module = load_profile_introspect_module()

    mismatched_display_tokens = [
        f"{token} -> expected {expected!r}, got {module.display_token(token)!r}"
        for token, expected in EXPECTED_DISPLAY_TOKENS.items()
        if module.display_token(token) != expected
    ]
    unresolved_current_tokens = [
        f"{token} -> {module.display_token(token)!r}"
        for token in sorted(current_authored_kc_tokens())
        if "_" in module.display_token(token) and module.display_token(token) != "_"
    ]

    if not missing and not mismatched_display_tokens and not unresolved_current_tokens:
        return 0

    for name in missing:
        print(f"profile introspection regression missing: {name}", file=sys.stderr)
    for message in mismatched_display_tokens:
        print(f"profile introspection display regression: {message}", file=sys.stderr)
    for message in unresolved_current_tokens:
        print(f"profile introspection unresolved authored KC token: {message}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
