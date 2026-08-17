# Pointer Modes

This doc explains what the pointing-device modes do after a mode is active,
regardless of how that mode was entered.

It is about shared mode behavior, not the current keymap's physical placement,
tap / hold gestures, or profile-specific double-tap actions. For the current
authored choices, see [KEYMAP.md](./KEYMAP.md). For the shared tap / hold /
multi-tap model, see [INTERACTION_MODEL.md](./INTERACTION_MODEL.md) and the
top-level [README](../README.md).

## Shared Rules

Across the current pd-mode runtime:

- an active mode can transform trackball motion
- some modes also intercept key events while active
- unlocked modes are exclusive while held: the newest active mode wins
- locked modes are exclusive: activating or locking a different mode clears the
  previous lock
- pressing the same runtime-handled mode key while that mode is locked clears
  the lock immediately, then behaves as a normal momentary hold until release
- active modes can render a mode-specific RGB overlay

One important non-rule:

- auto-sniping is not a pd mode; it is layer state whose CPI change is applied
  by the shared scan-time DPI policy

### Bounded discrete output

`ARROW_MODE`, `ZOOM_MODE`, `VOLUME_MODE`, and `BRIGHTNESS_MODE` convert motion
to synthetic key taps. They share one overload policy:

- one successful pointing poll emits at most four taps
- at most 32 whole steps plus the exact sub-step residual are retained
- zero-motion polls continue draining retained steps, so no separate scheduler
  competes with the pointing task
- motion beyond the retained bound is intentionally discarded and counted in
  per-mode diagnostics instead of blocking matrix, split, or RGB work
- reversing direction or leaving the mode clears obsolete retained work

At the configured limits, a saturated backlog drains in no more than eight
successful polls. `DRAGSCROLL` and `PINCH_MODE` produce wheel reports through a
different gesture handler and do not use this tap backlog.

## Pointer-Layer Policy

The shared pointer-layer policy is separate from the raw mode handlers, but it
changes how the modes feel in practice.

- non-arrow modes can keep the configured auto-mouse layer anchored while
  active or locked
- when `ARROW_MODE` is not active, the auto-mouse layer is allowed to overlap
  other active keyboard layers instead of being forced off underneath them,
  except for the configured auto-sniping layer, which keeps precedence over a
  separate auto-mouse layer
- `ARROW_MODE` prefers staying on the current typing or navigation surface
  instead of forcing the pointer layer back underneath it
- this policy follows pd-mode state itself, so it behaves the same whether the
  mode was entered by a plain mode key, an authored `key_behaviors[]` row, or
  a lock action

The same policy answers QMK's mouse-record question, which decides whether a
key press keeps the pointer layer up or resets auto mouse. Mouse keycodes and
pointer-mode keys are classified automatically. A key that drives the mouse
without being either — `DRAG_WINDOW`'s modified drag, `CLICK_SPAM`'s repeated
clicks — claims the same anchor from its authored row with
`.keeps_auto_mouse_anchored = true`. That is not cosmetic: auto mouse tears the
pointer layer down on the press otherwise, and a `TAP_SENDS(KC_TRNS)` tier on
such a key then has no layer left to fall through from, so its tap sends
nothing at all.

## Mode Reference

| Mode | Raw behavior | Notable side effects |
| --- | --- | --- |
| `DRAGSCROLL` | trackball motion becomes scrolling instead of cursor movement | uses the local dragscroll handler while active |
| `PINCH_MODE` | same scroll path as `DRAGSCROLL`, but with an owned real left `Cmd` hold | uses the local dragscroll handler and holds left `Cmd` while active |
| `ZOOM_MODE` | vertical trackball motion sends `Cmd+=` / `Cmd+-` taps | no dragscroll; explicit keyboard zoom |
| `ARROW_MODE` | dominant trackball motion emits arrow key taps instead of moving the cursor | repurposes mouse buttons for selection/copy/paste |
| `VOLUME_MODE` | vertical trackball motion changes system volume in steps | no extra side effects |
| `BRIGHTNESS_MODE` | vertical trackball motion changes display brightness in steps | no extra side effects |

## DRAGSCROLL

`DRAGSCROLL` is the basic scroll mode.

While active:

- the cursor stays frozen
- trackball motion is routed through the shared local dragscroll handler
- forward / backward motion becomes vertical scrolling
- the handler uses a sticky single-axis gesture model, so near-diagonal motion
  waits for one axis to win instead of emitting both axes together
- a pause longer than 55 ms releases the prior axis lock before the next
  report is classified; a pause longer than 80 ms also discards residual
  motion, so a new gesture cannot inherit stale scroll state
- horizontal motion can still contribute to horizontal scroll when the host
  surface accepts horizontal wheel input

This is the base mode that scroll-like modes build on.

## PINCH_MODE

`PINCH_MODE` is the scroll-with-modifier mode.

While active:

- the cursor stays frozen
- the same local dragscroll handler as `DRAGSCROLL` is active
- left `Cmd` is held through the same owned real-mod path as other runtime modifiers
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
- large motion follows the shared bounded discrete-output policy

This mode does not depend on BetterMouse. It is direct key-based zoom, not
scroll-based pinch emulation.

## ARROW_MODE

`ARROW_MODE` turns the trackball into directional navigation.

While active:

- the cursor stays frozen
- dominant horizontal motion emits left / right arrow taps
- dominant vertical motion emits up / down arrow taps
- dominant-axis magnitude handles the complete signed 16-bit report range,
  including `-32768`
- large motion follows the shared bounded discrete-output policy
- horizontal arrow taps keep held modifiers such as `Alt` intact
- vertical arrow taps temporarily mask held `Alt` modifiers so up / down stay
  plain

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
- large motion follows the shared bounded discrete-output policy

## BRIGHTNESS_MODE

`BRIGHTNESS_MODE` turns vertical motion into display brightness changes.

While active:

- the cursor stays frozen
- one vertical direction brightens
- the other dims
- motion is accumulated and emitted in discrete steps
- large motion follows the shared bounded discrete-output policy
