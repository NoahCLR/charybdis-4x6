# Pointer Modes

This file explains how the trackball and its modes behave from a user point of view.

If you have not read it yet, start with [INTERACTION_MODEL.md](./INTERACTION_MODEL.md). The pointer keys use the same timing language as the rest of the board, and this file assumes that model.

## Pointer Layer

`LAYER_POINTER` is not a normal layer you camp on. It is an auto-mouse layer.

- Move the trackball and it appears automatically.
- Stop moving for about `AUTO_MOUSE_TIME` (`1200 ms`) and it clears.
- If a pointing-device mode is active or locked, the pointer behavior stays alive.
- While the layer is active, `Space` can still be held for `NUM`.

The pointer layer uses a white-to-red timeout gradient instead of a fixed color.

## Where The Keys Live

The most important pointer keys are grouped on the right half:

| Area | Keys |
| --- | --- |
| pointer home row | `BRIGHTNESS_MODE`, `PINCH_MODE`, `MS_BTN3`, `ARROW_MODE` |
| pointer bottom row | `VOLUME_MODE`, `MS_BTN1`, `MS_BTN2`, `DRAGSCROLL` |
| nav layer fallback | `MS_BTN1`, `MS_BTN2`, `DRAGSCROLL`, `ARROW_MODE` |

That means the full pointer workflow is available either from the auto pointer layer or, in a more limited form, directly from `NAV`.

## Common Mode Rules

Across the current layout:

- a first press-and-hold on a mode key enters that mode momentarily
- the newest held mode wins over earlier unlocked modes
- some mode keys have double-tap actions
- locked modes are exclusive: locking one mode clears other locked modes
- `ZOOM_MODE` exists as a mode but does not currently have its own physical key

The important exception is `PINCH_MODE`: its second press does not behave like a normal "double tap locks the mode" key.

That contrast is part of the value of this userspace:

- `ARROW_MODE` and `DRAGSCROLL` show the standard pointer-key pattern
- `PINCH_MODE` shows that the same behavior engine can support a more custom second-press path

## Mode Reference

| Mode | What It Does | How You Reach It | RGB |
| --- | --- | --- | --- |
| `DRAGSCROLL` | converts ball movement to scrolling | hold `DRAGSCROLL`; quick double tap to lock | orange |
| `PINCH_MODE` | dragscroll with `Cmd` held for [BetterMouse](https://better-mouse.com/)-backed pinch emulation on macOS | first press hold = `PINCH_MODE`; double tap release = Accessibility Zoom toggle; double-tap hold = `ZOOM_MODE` | lime |
| `ZOOM_MODE` | vertical ball movement sends `Cmd+-` / `Cmd+=` | reached through `PINCH_MODE` double-tap hold | light green |
| `ARROW_MODE` | ball movement emits arrow keys instead of moving the cursor | hold `ARROW_MODE`; quick double tap to lock | cyan |
| `VOLUME_MODE` | vertical ball movement changes volume | hold `VOLUME_MODE`; quick double tap mutes | yellow |
| `BRIGHTNESS_MODE` | vertical ball movement changes display brightness | hold `BRIGHTNESS_MODE` | magenta |

## DRAGSCROLL

`DRAGSCROLL` is the basic scroll mode.

- cursor motion stops
- ball motion becomes scrolling
- pushing forward scrolls up
- quick double tap locks the mode

This is the mode for long pages, terminals, and editors.

## PINCH_MODE

`PINCH_MODE` is the most custom pointer key in the current keymap.

It lives on the pointer-layer home row, second key on the right half, in the physical spot where the base layer has the `J`/`SYM` key.

The actual authored behavior in `keymap.c` is:

- first press hold -> `PINCH_MODE`
- second press quick release -> `MACRO_6`
- second press hold past the normal hold threshold -> `ZOOM_MODE`

That means this key does not currently use the usual "double tap to lock the mode" behavior that `DRAGSCROLL` and `ARROW_MODE` use.

While held:

- dragscroll is enabled
- `Cmd` is held as a weak modifier
- the cursor stays frozen

In practice this relies on [BetterMouse](https://better-mouse.com/) on macOS
to turn that command-scroll path into the intended pinch-style zoom behavior.

## Real Pinch-Key Examples

These are the real press sequences the current firmware implements.

### Example 1: App Zoom Via Command-Scroll

1. Move the trackball so `LAYER_POINTER` comes up.
2. Press and hold `PINCH_MODE`.
3. Roll the trackball.

What happens:

- dragscroll is enabled
- left `Cmd` is held
- the ball is effectively sending command-scroll input while the key is held

This is the path to use with [BetterMouse](https://better-mouse.com/) on macOS
when command-scroll is being used as the pinch/zoom gesture.

### Example 2: macOS Accessibility Zoom Toggle

1. Tap `PINCH_MODE`.
2. Tap `PINCH_MODE` again and release quickly.

What happens:

- the key does not lock `PINCH_MODE`
- `MACRO_6` runs
- `MACRO_6` sends `Alt+Cmd+8`

On macOS, that toggles Accessibility Zoom.

### Example 3: Explicit Keyboard Zoom Mode

1. Tap `PINCH_MODE`.
2. Press `PINCH_MODE` again and keep the second press held past the normal hold threshold.
3. Roll the trackball up or down.

What happens:

- the second press does not fire `MACRO_6`
- the runtime switches into `ZOOM_MODE`
- upward movement sends `Cmd+=`
- downward movement sends `Cmd+-`
- releasing the key exits `ZOOM_MODE`

This is the path to use when
[BetterMouse](https://better-mouse.com/)-backed command-scroll is not the right
fit for the current app, or when you want deterministic zoom steps instead of
scroll-based zoom.

`MACRO_6` currently sends macOS `Alt+Cmd+8`, which toggles Accessibility Zoom.

## ZOOM_MODE

`ZOOM_MODE` is the explicit keyboard-zoom mode.

- upward movement sends `Cmd+=`
- downward movement sends `Cmd+-`
- the cursor stays frozen

Use this when command-scroll is not the right interaction for the current app, or when you want deterministic key-based zoom steps.

## ARROW_MODE

`ARROW_MODE` turns the ball into arrow keys.

Behavior:

- dominant horizontal motion emits left/right
- dominant vertical motion emits up/down
- the cursor stays frozen

Mouse buttons are repurposed while arrow mode is active:

- `MS_BTN1`: hold `Shift` for selection while moving with arrows
- `MS_BTN2`: copy
- `MS_BTN3`: paste

This mode is good for text selection and caret movement without leaving the home position.

It is also a good example of why the userspace feels polished:

- hold gives you the mode momentarily
- quick double tap locks it
- the mouse buttons are remapped in a way that matches the job of the mode

That makes `ARROW_MODE` feel like a complete editing tool, not just a novelty mapping.

## VOLUME_MODE

`VOLUME_MODE` uses vertical trackball motion for audio volume.

- one direction raises volume
- the other lowers it
- quick double tap mutes
- the cursor stays frozen

## BRIGHTNESS_MODE

`BRIGHTNESS_MODE` uses vertical trackball motion for display brightness.

- one direction brightens
- the other dims
- the cursor stays frozen

## Overlap Rules Worth Remembering

- Holding a different unlocked mode cancels the earlier unlocked mode.
- Locking one mode clears other locked modes.
- `ARROW_MODE` is the most specialized mode because it also repurposes mouse buttons.
- `PINCH_MODE` and `DRAGSCROLL` both rely on scroll behavior, but only `PINCH_MODE` adds `Cmd`.
- `PINCH_MODE` currently has no direct lock gesture on its physical key because the second tap is already used by `MACRO_6` and `ZOOM_MODE`.
