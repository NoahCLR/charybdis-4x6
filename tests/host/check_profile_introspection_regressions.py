#!/usr/bin/env python3

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
REPORT = ROOT / "docs" / "generated" / "profile-introspection.md"


EXPECTED_SNIPPETS = {
    "timing legend": "Timing legend for the layer-local behavior tables:",
    "pd manifest note": "PD mode identities and mode-key mapping are resolved in the background from the shared manifest",
    "esc long-hold timing": "| `ESC` | `ESC` (`KC_ESC`) | `single` | `-` | `-` | `TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))` | `tap_hold(150), long_hold(400), multi_tap(150)` |",
    "minus hold timing": "| `-` | `-` (`KC_MINS`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_UNDS)` | `-` | `tap_hold(150)` |",
    "left shift fallback timing": "| `LSFT` | `LSFT` (`KC_LEFT_SHIFT`) | `single` | `TAP_SENDS(KC_CAPS)` | `-` | `-` | `tap < tap_hold(150); else normal hold` |",
    "right alt fallback timing": "| `RALT` | `RALT` (`KC_RIGHT_ALT`) | `single` | `TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))` | `-` | `-` | `tap < tap_hold(150); else normal hold` |",
    "lt nav timing override": "| `LT[NAV]/SLSH` | `LT[NAV]/SLSH` (`LT(LAYER_NAV,KC_SLSH)`) | `double` | `-` | `TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NAV))` | `-` | `tap_hold=100, multi_tap(150)` |",
}


def main() -> int:
    text = REPORT.read_text()
    missing = [name for name, snippet in EXPECTED_SNIPPETS.items() if snippet not in text]

    if not missing:
        return 0

    for name in missing:
        print(f"profile introspection regression missing: {name}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
