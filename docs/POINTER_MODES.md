# Pointer Modes

This file explains the raw behavior of the pointing-device modes after a mode
is active.

It does not describe the current keymap's physical placement, tap / hold
gestures, or double-tap actions. Those are authored separately in `keymap.c`
and described at a higher level in [INTERACTION_MODEL.md](./INTERACTION_MODEL.md)
and the top-level [README](../README.md).

## Shared Rules

Across the current pd-mode runtime:

- an active mode can transform trackball motion
- some modes also intercept key events while active
- unlocked modes are exclusive while held: the newest active mode wins
- locked modes are exclusive: locking one mode clears the others
- the first active mode in `pd_modes[]` provides the active motion handler
- active modes can render a mode-specific RGB overlay

One important non-rule:

- auto-sniping is not a pd mode

The current keymap enables sniping from `LAYER_NAV`, but that is a separate
layer-driven rule rather than part of any mode definition here.

## Mode Reference

| Mode | Raw behavior | Notable side effects |
| --- | --- | --- |
| `DRAGSCROLL` | trackball motion becomes scrolling instead of cursor movement | enables Charybdis dragscroll while active |
| `PINCH_MODE` | same scroll path as `DRAGSCROLL`, but with `Cmd` held as a weak modifier | enables dragscroll and holds left `Cmd` while active |
| `ZOOM_MODE` | vertical trackball motion sends `Cmd+=` / `Cmd+-` taps | no dragscroll; explicit keyboard zoom |
| `ARROW_MODE` | dominant trackball motion emits arrow key taps instead of moving the cursor | repurposes mouse buttons for selection/copy/paste |
| `VOLUME_MODE` | vertical trackball motion changes system volume in steps | no extra side effects |
| `BRIGHTNESS_MODE` | vertical trackball motion changes display brightness in steps | no extra side effects |

## DRAGSCROLL

`DRAGSCROLL` is the basic scroll mode.

While active:

- the cursor stays frozen
- trackball motion is routed through Charybdis dragscroll
- forward / backward motion becomes vertical scrolling
- horizontal motion can still contribute to horizontal scroll if the firmware
  dragscroll path allows it

This is the base mode that scroll-like modes build on.

## PINCH_MODE

`PINCH_MODE` is the scroll-with-modifier mode.

While active:

- the cursor stays frozen
- Charybdis dragscroll is enabled
- left `Cmd` is held as a weak modifier
- the ball is effectively producing command-scroll input

On macOS, [BetterMouse](https://better-mouse.com/) can turn that
command-scroll path into pinch-style zoom. That BetterMouse dependency applies
to `PINCH_MODE`, not to `ZOOM_MODE`.

Without BetterMouse, `PINCH_MODE` is still just command-modified scrolling.

## ZOOM_MODE

`ZOOM_MODE` is the explicit keyboard-zoom mode.

While active:

- the cursor stays frozen
- one vertical direction sends `Cmd+=`
- the other sends `Cmd+-`

This mode does not depend on BetterMouse. It is direct key-based zoom, not
scroll-based pinch emulation.

## ARROW_MODE

`ARROW_MODE` turns the trackball into directional navigation.

While active:

- the cursor stays frozen
- dominant horizontal motion emits left / right arrow taps
- dominant vertical motion emits up / down arrow taps

It also remaps mouse buttons while active:

- `MS_BTN1`: hold `Shift` for selection while moving with arrows
- `MS_BTN2`: copy
- `MS_BTN3`: paste

This makes `ARROW_MODE` more than a motion remap; it becomes a small editing
tool with supporting button behavior.

## VOLUME_MODE

`VOLUME_MODE` turns vertical motion into audio volume changes.

While active:

- the cursor stays frozen
- one vertical direction raises volume
- the other lowers volume
- motion is accumulated and emitted in discrete steps

## BRIGHTNESS_MODE

`BRIGHTNESS_MODE` turns vertical motion into display brightness changes.

While active:

- the cursor stays frozen
- one vertical direction brightens
- the other dims
- motion is accumulated and emitted in discrete steps

## What This File Does Not Cover

This file intentionally does not document:

- which physical key enters a mode
- whether a mode key locks, mutes, or branches on a second press
- which layer currently exposes a mode
- the current auto-mouse layer configuration

Those are keymap and policy choices, not raw mode semantics.
