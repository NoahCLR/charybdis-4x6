# Noah's Charybdis 4x6 Keymap - QMK 0.32.5 - 03-2026

> **Branch notice:** This is the `qmk-latest` branch, which targets a newer version of QMK (0.32.5) than the `main` branch. It builds against the [`qmk-latest`](https://github.com/NoahCLR/bastardkb-qmk/tree/qmk-latest) branch of the firmware fork (instead of `bkb-master` on `main`). The two branches are **not interchangeable** — keycodes and APIs differ between QMK versions.

A QMK keymap for the [Bastard Keyboards Charybdis 4x6](https://bastardkb.com/charybdis/), a split ergonomic keyboard with a built-in trackball on the right half.

## Features

- **5 layers** — Base (QWERTY), Numpad, Lower (symbols), Raise (navigation/media), and Pointer (auto-mouse)
- **Trackball modes** — Hold a key to turn the trackball into a volume knob, arrow-key emitter, scroll wheel, or zoom control
- **Auto-mouse layer with countdown gradient** — The pointer layer activates automatically when you move the trackball. LEDs fade from white to red over 1.2 seconds to show remaining time before the layer deactivates, giving you a visual countdown. The gradient is synced to the slave half over RPC so both sides animate together.
- **Custom tap/hold system** — Number row and punctuation keys do different things based on hold duration: tap for the plain key, hold for the shifted symbol, longer hold for a third action
- **Per-layer RGB indicators** — Each layer has a distinct color; trackball modes overlay a color on the right half. Colors are defined as HSV values in `keymap.c` — see [hsv colors.jpg](hsv%20colors.jpg) for a quick reference of hue values. Split-safe RGB helpers in `rgb_helpers.h` handle LED chunk boundaries so you can target individual LEDs, specific halves, or both without worrying about the split addressing.
- **Hi-res scroll** — 120x scroll multiplier for smooth, precise scrolling
- **Split state sync** — Auto-mouse countdown and mode flags are synced from master to slave over RPC so both halves show correct LEDs

## Layers

| Layer | Activation | Color | Purpose |
|-------|-----------|-------|---------|
| Base | Default | RGB effect | QWERTY typing |
| Num | Hold `B`| Green | Numpad on the right half |
| Lower | `MO(2)` / Hold `J` | Blue | Symbols, DPI controls, brackets |
| Raise | `MO(3)` / Hold `F` / Hold `/` | Purple | Navigation, media, mouse buttons, macOS shortcuts. Sniping (lower DPI) auto-enables; auto-mouse is disabled to avoid conflicts. |
| Pointer | Auto (trackball movement) | White-to-red gradient | Mouse buttons, scroll, trackball mode toggles. Automatically stripped when another layer is explicitly active to prevent flickering. |

The Caps Lock position is a dual-purpose key: hold for Shift, tap for Caps Lock.

## Trackball Modes

Mode keys are dual-purpose: tap sends the base-layer key at that position, hold activates the trackball mode. The right-half LEDs change color to show the active mode.

| Mode | Key | Color | Behavior |
|------|-----|-------|----------|
| Drag Scroll | `DRGSCRL` | Orange | Trackball motion becomes scrolling |
| Volume | `VOLUME_MODE` | Yellow | Trackball Y-axis controls system volume |
| Brightness | `BRIGHTNESS_MODE` | Magenta | Trackball Y-axis controls screen brightness |
| Zoom | `ZOOM_MODE` | Chartreuse | Trackball Y-axis zooms in (GUI+Plus) / out (GUI+Minus) |
| Arrow | `ARROW_MODE` | Cyan | Trackball motion sends arrow keys (dominant axis wins) |

`DRG_TOG_ON_HOLD` is a dual-purpose key on the Pointer layer: tap sends the base-layer key at that position, hold toggles drag-scroll lock on/off.

If drag-scroll is already toggled on (locked), pressing and releasing the momentary `DRGSCRL` key will unlock it — so you don't have to reach for the toggle key to turn it off.

Sniping (lower DPI) works during trackball modes — it slows down the trackball input, so arrow keys emit less frequently and volume, brightness, and zoom adjustments become finer-grained.

## Macros (macOS)

| Macro | Shortcut | Purpose |
|-------|----------|---------|
| `MACRO_0` | GUI + Space | Spotlight search |
| `MACRO_1` | Alt + Space | Claude |
| `MACRO_2` | Alt + GUI + Space | Terminal |
| `MACRO_3` | Ctrl + Alt + GUI + C | OCR text copy |
| `MACRO_4` | Ctrl + Alt + GUI + X | Screenshot |
| `MACRO_5` | Ctrl + GUI + Space | Emoji picker |

## Tap / Hold / Longer Hold

The number row, punctuation, arrows, and Enter use a custom three-tier system (not QMK's built-in mod-tap):

| Duration | Action | Example (`1` key) |
|----------|--------|-------------------|
| < 150ms | Tap — plain key | `1` |
| 150–400ms | Hold — shifted variant | `!` |
| > 400ms | Longer hold — third action | (falls back to hold for most keys) |

Arrow keys have all three tiers: tap = arrow, hold = Alt+Arrow (word jump), longer hold = GUI+Arrow (line jump).

## VIA Layout Conversion

The `via layouts/` directory contains VIA layout JSON files and a conversion script:

- `charybdis.layout.json` — the current keymap exported from VIA
- `via_to_qmk_layout.py` — converts the VIA JSON into the `keymaps` array for `keymap.c`

VIA uses a flat key index and its own token format (`CUSTOM(80)`, `KC_NO`, etc.) that doesn't match the QMK `LAYOUT()` macro's matrix order or keycode names. The script handles the index remapping and token translation so you can freely edit in VIA's GUI, export, and update the keymap without manually reformatting 5 layers of 56 keys each.

```bash
python via_to_qmk_layout.py          # default mode (set via MODE variable in script)
python via_to_qmk_layout.py --write  # update keymap.c in-place
python via_to_qmk_layout.py --print  # print to stdout
```

The top of the script has a configuration section with the things you'd need to update when the keymap changes:

- `VIA_JSON` — path to the VIA layout JSON (default: `charybdis.layout.json` next to the script)
- `KEYMAP_C` — path to `keymap.c` (default: `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`)
- `MODE` — default output mode (`"write"` or `"print"`)
- `LAYER_NAMES` — layer names in enum order, must match `keymap.c`
- `REPLACEMENTS` — VIA token → QMK keycode translations (add new custom keycodes here)

## File Structure

```
keyboards/bastardkb/charybdis/4x6/keymaps/noah/
  keymap.c                  Main keymap: layers, key processing, RGB indicators
  config.h                  QMK/Charybdis configuration overrides
  pointing_device_modes.h   Trackball mode system (volume, brightness, zoom, arrow, dragscroll)
  split_sync.h              Master → slave state sync via RPC (mode flags + elapsed time)
  rgb_automouse.h           Auto-mouse countdown gradient (white → red)
  rgb_helpers.h             Split-safe RGB matrix helper functions
  rules.mk                  Build flags (VIA, hi-res scroll)

via layouts/
  charybdis.layout.json     VIA layout export for this keymap
  via_to_qmk_layout.py      VIA JSON → QMK LAYOUT() converter
```

## Documentation

| File | Contents |
|------|----------|
| [README.md](README.md) | This file — feature overview, layers, modes, macros |
| [INTERNALS.md](INTERNALS.md) | Technical deep-dive: custom firmware fork, split serial timeout fix, split sync protocol, how to add a new trackball mode |
| [DIAGRAMS.md](DIAGRAMS.md) | Mermaid diagrams: architecture overview, auto-mouse lifecycle, key processing pipeline, trackball motion flow, RGB rendering, split sync sequence |

## Building & Flashing

> **Important:** This keymap is built against [NoahCLR/bastardkb-qmk](https://github.com/NoahCLR/bastardkb-qmk) (branch `qmk-latest`), a fork of the Bastard Keyboards QMK firmware — not stock QMK. See [INTERNALS.md](INTERNALS.md) for details on the firmware changes and other technical notes.
