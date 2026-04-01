# Noah's Charybdis Userspace

This is the shared userspace for my `noah` Charybdis 4x6 keymaps.

It is intentionally Charybdis-specific. The trackball behavior, split sync,
auto-mouse layer, and RGB assumptions are built around this board and this
layout.

This is still a personal configuration, but it is not meant to be a pile of
one-off hacks. The point is to keep the behavior I use every day centralized
and editable, so someone changing `keymap.c` or `config.h` can adjust the board
without having to rework the runtime.

> **Firmware note:** This userspace is updated for QMK `0.32.5` and builds
> against my `qmk-latest` firmware branch rather than the older
> `bkb-master`-based setup.

This repo also sits on top of [BastardKB](https://bastardkb.com/), designed by
Quentin. His work, and the community built around this board, have given me
hundreds of hours of fun optimizing both the hardware and the firmware, and I
am very thankful for that. If you are considering buying the hardware, buy it
from him rather than from a knockoff seller.

## What This Userspace Is For

This userspace tries to make the board feel consistent in a few areas:

- keys can use the same tap / hold / longer-hold / multi-tap language
- pointer-mode keys follow the same timing model as the rest of the board
- the pointer layer appears automatically when the trackball is in use
- RGB reflects layers, pointer modes, and auto-mouse timeout

The value is not that it adds a lot of unrelated features. The value is that
common actions stay grouped together and behave predictably.

## Where To Change Things

If you want to adapt this layout, these are the main files to touch first:

| File | What You Change There |
| --- | --- |
| `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c` | physical layout, combos, macros, and the authored `key_behaviors[]` table |
| `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.h` | layer colors, pointer-mode colors, LED groups, and the auto-mouse gradient |
| `keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h` | tap/hold timing, multi-tap timing, auto-mouse target layer and timeout, auto-sniping, dragscroll feel, and other keymap-facing behavior |
| `users/noah/config.h` | split transport settings, RGB geometry, pointing-device polling, sensor/report settings, and low-level QMK overrides |

In other words:

- if you want to change what a key does, start in `keymap.c`
- if you want to change how the board looks, start in `rgb_config.h`
- if you want to change how the keyboard feels, start in the keymap `config.h`
- if you want to change board plumbing, start in `users/noah/config.h`

## Layers And VIA

The current keymap uses five layers, in this order:

- `0 = LAYER_BASE`: default typing layer
- `1 = LAYER_NUM`: numpad layer
- `2 = LAYER_SYM`: symbols, brackets, and pointer/DPI-adjacent shortcuts
- `3 = LAYER_NAV`: navigation, media, macros, mouse buttons, and the current auto-sniping layer
- `4 = LAYER_POINTER`: dedicated pointer layout and the default auto-mouse target layer

That layer order matters in a few places:

- VIA is configured for 5 dynamic layers
- `AUTO_MOUSE_DEFAULT_LAYER` in the keymap `config.h` uses the numeric layer index QMK expects
- the authored layer names in `keymap.c` and the VIA conversion script need to stay in sync

If you change the layout in VIA and want to bring it back into source, use
[`via_to_qmk_layout.py`](./via%20layouts/via_to_qmk_layout.py) in
[`via layouts`](./via%20layouts). It reads
[`charybdis.layout.json`](./via%20layouts/charybdis.layout.json), maps VIA's
layer indices and custom keycodes back to this keymap, and can either print
formatted `LAYOUT()` blocks or write them back into `keymap.c`.

## Combos

Combos are authored directly in `keymap.c`. They are separate from the custom
key-behavior engine: a combo is just a simultaneous chord that emits one
keycode or action.

The current combo timing is configured in the keymap `config.h` through
`COMBO_TERM`. If a combo emits a keycode that also appears in
`key_behaviors[]`, the emitted key can still reuse the same custom behavior
handling after the combo resolves.

So the current split is:

- use combos for simultaneous chords
- use `key_behaviors[]` for tap / hold / longer-hold / multi-tap behavior
  attached to an authored keycode

## Key Behavior

The richer custom tap / hold / multi-tap behavior is authored in
`key_behaviors[]`. Plain keys without a row keep their normal QMK behavior,
while momentary layer keys still keep their normal layer handling.

Keys with authored behavior rows can distinguish between:

- tap
- hold
- longer hold
- multi-tap sequences

This is what makes the current layout possible:

- number-row symbols on hold
- thumb keys that combine layer access, locks, and media
- nav arrows that cover character, word, and line movement
- pointer keys that can do more than one thing without inventing their own
  timing rules

Default timing lives in the keymap `config.h`
(`keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`):

- `TAPPING_TERM = 200`
- `CUSTOM_TAP_HOLD_TERM = 150`
- `CUSTOM_LONGER_HOLD_TERM = 400`
- `CUSTOM_MULTI_TAP_TERM = 150`

There is one important nuance: built-in QMK dual-role keys like `LT()` and
`MT()` still use `TAPPING_TERM`. Inside `key_behaviors[]`, an omitted
`.tap_hold_term` also falls back to `TAPPING_TERM` for `LT()` rows, but to
`CUSTOM_TAP_HOLD_TERM` for other custom rows.
That split is intentional for typing feel: I want the built-in dual-role keys
to stay comfortable during fast typing, while the fully custom rows can still
use a shorter threshold where that helps.

Timing can also be customized per key. A `key_behaviors[]` row may set:

- `.tap_hold_term`
- `.longer_hold_term`
- `.multi_tap_term`

If one of those fields is omitted, C zero-initializes it. A value of `0` means
"use the default timing for this row."

### Hold Tiers

The custom keys support three different hold styles:

- hold-and-keep-held
- fire-on-threshold
- fire-on-release-after-hold

That is what lets one key behave differently in different contexts. For
example, the nav arrows can cover character movement on tap, word movement on a
medium hold, and line movement on a longer hold.

### Multi-Tap

Multi-tap is part of the same model, not a separate feature. A key can define
different behavior for the first tap, second tap, third tap, and so on.

That is why the thumb keys and some pointer keys can keep their normal role on
hold while still exposing locks, media, or alternate actions on additional
presses.

For the full interaction model, see
[`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md).

## Pointing-Device Modes

The trackball modes use the same timing language as the rest of the board.

Current modes:

- `DRAGSCROLL`: converts ball movement into scrolling
- `PINCH_MODE`: dragscroll with `Cmd` held for
  [BetterMouse](https://better-mouse.com/)-backed pinch emulation on macOS
- `ZOOM_MODE`: converts vertical movement into `Cmd+-` and `Cmd+=`
- `ARROW_MODE`: converts ball movement into arrow keys and repurposes mouse
  buttons for editing
- `VOLUME_MODE`: converts vertical movement into volume control
- `BRIGHTNESS_MODE`: converts vertical movement into display brightness control

The current keymap also enables Charybdis sniping automatically while
`LAYER_NAV` is active. That is configured in the keymap `config.h` through
`CHARYBDIS_AUTO_SNIPING_ENABLE` and `CHARYBDIS_AUTO_SNIPING_LAYER`, so the
sniping layer is easy to change. The same file also exposes both pointer DPI
ladders: `CHARYBDIS_MINIMUM_DEFAULT_DPI` and
`CHARYBDIS_DEFAULT_DPI_CONFIG_STEP` for normal cursor movement, plus
`CHARYBDIS_MINIMUM_SNIPING_DPI` and `CHARYBDIS_SNIPING_DPI_CONFIG_STEP` for
sniping mode.

These modes are not all entered the same way from the physical keys. That is
because the physical mode keys are authored through `key_behaviors[]` in
`keymap.c`, not hard-wired to one fixed gesture. These entry patterns are fully
customizable.

Unless a `[0]` tap override is authored, a quick single tap still falls through
to the base-layer key at that physical position. The patterns below describe
the extra hold and second-press behavior.

- `ARROW_MODE` and `DRAGSCROLL` use a simple pattern: hold for momentary mode,
  quick second tap to lock
- `VOLUME_MODE` uses the quick second tap for mute
- `BRIGHTNESS_MODE` currently has no authored second-press behavior
- the physical `PINCH_MODE` key is the custom exception: first hold enters
  `PINCH_MODE`, a second quick tap triggers Accessibility Zoom, and a second
  hold enters `ZOOM_MODE`

For the user-facing pointer behavior, see
[`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md).

## Auto-Mouse And RGB

`LAYER_POINTER` is the default auto-mouse layer in this keymap. Moving the
trackball brings the configured auto-mouse layer up automatically, and it
clears after `AUTO_MOUSE_TIME` unless pointer activity or an active mode keeps
it alive. That behavior is configured in the keymap `config.h` through
`POINTING_DEVICE_AUTO_MOUSE_ENABLE`, `AUTO_MOUSE_DEFAULT_LAYER`, and
`AUTO_MOUSE_TIME`.

RGB is used as feedback, not decoration:

- active layers can render layer colors
- active pointer modes can render a mode color
- the configured auto-mouse layer uses a white-to-red timeout gradient instead
  of a fixed solid color

Those colors and LED groups live in the keymap `rgb_config.h`
(`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.h`).
For the RGB authoring model and render order, see
[`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md).

## If You Want To Go Deeper

The main implementation lives under `users/noah/`, but most customization does
not need low-level changes.

These docs are the next place to look:

- [`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md): detailed tap,
  hold, and multi-tap behavior
- [`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md): pointer-layer and
  trackball mode behavior
- [`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md): RGB colors, LED groups, and
  auto-mouse gradient configuration
- [`docs/ADDING_PD_MODE.md`](./docs/ADDING_PD_MODE.md): how to add a new
  pointing-device mode safely

<p align="center" width="100%">
<video src="https://github.com/NoahCLR/charybdis-4x6/raw/refs/heads/refactor/userspace-modularity/docs/media/charybdis.mp4" width="80%" controls></video>
</p>
