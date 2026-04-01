# Noah Charybdis Userspace

> **Firmware note:** This userspace is updated for QMK `0.32.5` and builds
> against my `qmk-latest` firmware branch rather than the older
> `bkb-master`-based setup.

This is the shared userspace for my `noah` Charybdis 4x6 keymaps.

It is intentionally Charybdis-specific. The trackball behavior, split sync,
auto-mouse layer, and RGB assumptions are built around this board and this
layout.

This is still a personal configuration, but it is not meant to be a pile of
one-off hacks. The point is to keep the behavior I use every day centralized
and editable, so someone changing `keymap.c` or `config.h` can adjust the board
without having to rework the runtime.

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
| `keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h` | tap/hold timing, multi-tap timing, auto-mouse timeout, dragscroll feel, and other keymap-facing behavior |
| `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.h` | layer colors, pointer-mode colors, LED groups, and the auto-mouse gradient |
| `users/noah/config.h` | split transport settings, pointing-device polling, sensor/report settings, and low-level QMK overrides |

In other words:

- if you want to change what a key does, start in `keymap.c`
- if you want to change how the keyboard feels, start in the keymap `config.h`
- if you want to change board plumbing, start in `users/noah/config.h`

## Key Behavior

Only keys with rows in `key_behaviors[]` use the custom key-behavior engine.
Those keys can distinguish between:

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

Default timing lives in `keymaps/noah/config.h`:

- `TAPPING_TERM = 200`
- `CUSTOM_TAP_HOLD_TERM = 150`
- `CUSTOM_LONGER_HOLD_TERM = 400`
- `CUSTOM_MULTI_TAP_TERM = 150`

There is one important nuance: built-in QMK dual-role keys like `LT()` and
`MT()` still use `TAPPING_TERM`. Inside `key_behaviors[]`, an omitted
`.tap_hold_term` also falls back to `TAPPING_TERM` for `LT()` rows, but to
`CUSTOM_TAP_HOLD_TERM` for other custom rows.

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

These modes are not all entered the same way from the physical keys. That is
because the physical mode keys are authored through `key_behaviors[]` in
`keymap.c`, not hard-wired to one fixed gesture. These entry patterns are fully
customizable.

- `ARROW_MODE` and `DRAGSCROLL` use a simple pattern: hold for momentary mode,
  second press to lock
- `VOLUME_MODE` uses the second press for mute
- `BRIGHTNESS_MODE` is just a straight mode key
- the physical `PINCH_MODE` key is the custom exception: first hold enters
  `PINCH_MODE`, a second quick press triggers Accessibility Zoom, and a second
  hold enters `ZOOM_MODE`

For the user-facing pointer behavior, see
[`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md).

## Auto-Mouse And RGB

`LAYER_POINTER` is an auto-mouse layer. Moving the trackball brings it up
automatically, and it clears after `AUTO_MOUSE_TIME` unless pointer activity or
an active mode keeps it alive.

RGB is used as feedback, not decoration:

- active layers can render layer colors
- active pointer modes can render a mode color
- the pointer layer uses a white-to-red timeout gradient instead of a fixed
  solid color

Those colors and LED groups live in `keymaps/noah/rgb_config.h`.

## If You Want To Go Deeper

The main implementation lives under `users/noah/`, but most customization does
not need low-level changes.

These docs are the next place to look:

- [`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md): detailed tap,
  hold, and multi-tap behavior
- [`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md): pointer-layer and
  trackball mode behavior
- [`docs/ADDING_PD_MODE.md`](./docs/ADDING_PD_MODE.md): how to add a new
  pointing-device mode safely
