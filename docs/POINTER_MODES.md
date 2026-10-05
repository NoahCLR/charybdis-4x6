# Pointer Modes

This doc explains what the pointing-device modes do after a mode is active,
regardless of how that mode was entered.

It is about shared mode behavior, not the current keymap's physical placement,
tap / hold gestures, or profile-specific double-tap actions. For the current
authored choices, see [KEYMAP.md](./KEYMAP.md). For the shared tap / hold /
multi-tap model, see [INTERACTION_MODEL.md](./INTERACTION_MODEL.md) and the
[firmware guide](./GUIDE.md#authoring-the-profile-in-c).

The [PD-mode domain contract](architecture/pd-mode-domain-v1.md) specifies the
eight configurable slots and the remaining hardware acceptance checks. Side-specific schema-2 firmware exposes eight slots in
Charybdis Ark → Pointing modes. The six defaults below are records in those
slots, followed by Undo / Redo in slot 6 and an empty slot 7; their names do not
select special code. Undo / Redo uses horizontal motion at 100 DPI and a
threshold of 40 to send Cmd+Z or Shift+Cmd+Z. Right Alt double-tap hold activates it.
The `PD_SLOT_n` keycodes select slots, while the named behavior in this
document describes the shipped factory profile. Editing a slot can change
that behavior without changing its keycode.

Use an empty slot to create directional key/shortcut actions or a scrolling
mode, or duplicate an existing slot. The normal path is name, movement type,
a DPI preset (including normal pointer speed), then either an axis choice and
the relevant direction actions or scroll direction/modifiers. Direction
actions and mouse-button shortcuts use the same keycode picker as Layout, so a
plain key or modified key such as `G(KC_Z)` can be selected instead of typed.
Choose **Keep mode**, assign the mode's hold or toggle action in Layout, set
its color under RGB → Pointing modes, then review and
Apply the draft. The layout key picker reads populated slots from the current
draft, so a newly named mode appears immediately with its canonical hold and
toggle actions.

**Advanced** holds pointer-layer policy, active axes and thresholds (with the
ball movement each threshold is at the mode's DPI), what each direction's
shortcut does with held modifiers (inherit them, leave chosen ones out, or send
exactly the shortcut), scroll gesture ratios/timing and mouse-button overrides.
A mouse-button shortcut keeps the modifier handling it was stored with.
Volume, brightness, zoom, arrow navigation, history shortcuts and
modifier-assisted scrolling use the same facilities. Mouse buttons 1–3 can pass
through, be consumed, tap a shortcut or hold modifiers. Choose what a button
does first; its shortcut field or modifier switches then appear, and the
override is staged once it has a shortcut or at least one modifier. A slot
with existing bindings must be unbound before clearing it.

Macro programs, recursive mode/layer actions and arbitrary scripts are not
motion outputs. Ordinary pointer movement and auto-sniping remain outside the
slot bank. Dragscroll and Pinch use this repository's scroll implementation.
All current firmware builds use the eight-slot engine. The generic build runs
the compiled factory slots without a live profile owner; the side-specific pair
supports editing and saving them from Charybdis Ark.

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

In the factory profile, `PD_SLOT_4`, `PD_SLOT_3`, `PD_SLOT_1`, and `PD_SLOT_2` convert motion
to synthetic key taps. They share one overload policy:

- one successful pointing poll emits at most four taps
- at most 32 whole steps plus the exact sub-step residual are retained
- zero-motion polls continue draining retained steps, so no separate scheduler
  competes with the pointing task
- motion beyond the retained bound is intentionally discarded and counted in
  per-mode diagnostics instead of blocking matrix, split, or RGB work
- reversing direction or leaving the mode clears obsolete retained work
- a directional mode can instead send **once per movement**: one shortcut
  however far the ball moves, then nothing until a pause of 150 ms or the mode
  ending starts the next movement, or the ball moves back against the
  direction that sent (more than 90° from it), which sends once more; turning
  into another direction within a movement sends nothing, and a single stray
  report does not count as moving back. It is the record's output setting
  (`.direction_output = NOAH_PD_DIRECTION_OUTPUT_ONCE` in `pd_config.c`); the
  factory slots send once per step

At the configured limits, a saturated backlog drains in no more than eight
successful polls. `PD_SLOT_0` and `PD_SLOT_5` produce wheel reports through a
different gesture handler and do not use this tap backlog.

## Pointer-Layer Policy

The shared pointer-layer policy is separate from the raw mode handlers, but it
changes how the modes feel in practice.

- modes configured to keep auto mouse anchored can keep the configured
  auto-mouse layer anchored while active or locked
- when no active mode is configured to prefer the typing layer, the auto-mouse
  layer is allowed to overlap
  other active keyboard layers instead of being forced off underneath them,
  except for the configured auto-sniping layer, which keeps precedence over a
  separate auto-mouse layer
- a mode configured to prefer the typing layer, such as the factory Arrow
  record in `PD_SLOT_4`, stays on the current typing or navigation surface
  instead of forcing the pointer layer back underneath it
- this policy follows pd-mode state itself, so it behaves the same whether the
  mode was entered by a plain mode key, an authored `key_behaviors[]` row, or
  a lock action

The same policy answers QMK's mouse-record question, which decides whether a
key press keeps the pointer layer up or resets auto mouse. Mouse keycodes and
pointer-mode keys are classified automatically. A key that drives the mouse
without being either — the held window-drag button of `CUSTOM_KEY_3` (Drag
Window), the repeated clicks of `CUSTOM_KEY_2` (Click Spam) — claims the same
anchor from its authored row with
`.keeps_auto_mouse_anchored = true`. That is not cosmetic: auto mouse tears the
pointer layer down on the press otherwise, and a `TAP_SENDS(KC_TRNS)` tier on
such a key then has no layer left to fall through from, so its tap sends
nothing at all.

A key that is not anchored still closes the auto-mouse layer, but only once
its own press has been processed. QMK resolves a record's keycode, then
`process_auto_mouse` turns its layer off, then the default handler looks the
action up again; a key taken from the auto-mouse layer would run the key
beneath it while every hook before that saw the auto-mouse layer's key. With
`KC_J` on the Pointing layer over `LT(NAV, KC_F)`, that handler held Navigation
while the runtime saw `KC_J`, and nothing released it. So a press resolved
from the auto-mouse layer counts as a mouse key for its own record, and
`pointer_layer_policy_settle_record()` applies QMK's reset right after, unless
something else — a held mouse key, a lock — now holds the layer. Positions the
layer leaves transparent resolve from a lower layer and close it before their
key, as QMK does. Layer keys on the auto-mouse layer follow QMK's own rules
and never reach this question.

A lock on the auto-mouse layer is the runtime's own lock, like any other
layer's (`TG()`, `LOCK_LAYER()`, `TO()`, `TT()`'s locking tap, from a key, a
behaviour or a combo). While it lasts it holds QMK's auto-mouse on the way a
held mouse key does, so neither the timeout nor the next ordinary key turns
the layer off, and `TO(0)` or unlocking it lets it go. A runtime hold of the
layer (a behaviour's `MO()` hold, which QMK sees only as a layer change and
never as an `MO()` key) holds auto-mouse the same way until it is released;
without it QMK's next pointing task turned the layer straight back off. QMK's auto-mouse also
flips a toggle of its own on the release of `TG()`/`TO()` of its layer and on
`TT()`'s locking tap; nothing in the runtime released that second lock, so
`TO(pointer)` then `TO(0)` used to leave the pointer layer on. The runtime
takes each of those flips back as the record arrives
(`pointer_layer_policy_take_back_qmk_toggle`), and
`tests/host/run_qmk_contract_checks.sh` runs QMK's own `process_auto_mouse`
against the contract that names them, so the two cannot drift. Pointing-mode
locks keep using that toggle.

## Mode Reference

| Factory slot | Raw behavior | Notable side effects |
| --- | --- | --- |
| `PD_SLOT_0` | trackball motion becomes scrolling instead of cursor movement | uses the local dragscroll handler while active |
| `PD_SLOT_5` | same scroll path as `PD_SLOT_0`, but with an owned real left `Cmd` hold | uses the local dragscroll handler and holds left `Cmd` while active |
| `PD_SLOT_3` | vertical trackball motion sends `Cmd+=` / `Cmd+-` taps | no dragscroll; explicit keyboard zoom |
| `PD_SLOT_4` | dominant trackball motion emits arrow key taps instead of moving the cursor | repurposes mouse buttons for selection/copy/paste |
| `PD_SLOT_1` | vertical trackball motion changes system volume in steps | no extra side effects |
| `PD_SLOT_2` | vertical trackball motion changes display brightness in steps | no extra side effects |

## Dragscroll (`PD_SLOT_0`)

`PD_SLOT_0` is the basic scroll mode.

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
- a scrolling mode can be limited to **horizontal only** or **vertical only**
  (the record's scroll axes, `.axis = NOAH_PD_SCROLL_VERTICAL` in
  `pd_config.c`); a gesture on the other axis then does nothing, and its
  drift does not turn into scrolling on the allowed axis. The factory slots
  scroll both axes

This is the base mode that scroll-like modes build on.

## Pinch (`PD_SLOT_5`)

`PD_SLOT_5` is the scroll-with-modifier mode.

While active:

- the cursor stays frozen
- the same local dragscroll handler as `PD_SLOT_0` is active
- left `Cmd` is held through the same owned real-mod path as other runtime modifiers
- the ball is effectively producing command-scroll input

On macOS, [BetterMouse](https://better-mouse.com/) can turn that
command-scroll path into pinch-style zoom. That BetterMouse dependency applies
to `PD_SLOT_5`, not to `PD_SLOT_3`.

Without BetterMouse, `PD_SLOT_5` is still just command-modified scrolling.

## Zoom (`PD_SLOT_3`)

`PD_SLOT_3` is the explicit keyboard-zoom mode.

While active:

- the cursor stays frozen
- one vertical direction sends `Cmd+=`
- the other sends `Cmd+-`
- large motion follows the shared bounded discrete-output policy

This mode does not depend on BetterMouse. It is direct key-based zoom, not
scroll-based pinch emulation.

## Arrow (`PD_SLOT_4`)

`PD_SLOT_4` turns the trackball into directional navigation.

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

This makes `PD_SLOT_4` more than a motion remap; it becomes a small editing
tool with supporting button behavior.

## Volume (`PD_SLOT_1`)

`PD_SLOT_1` turns vertical motion into audio volume changes.

While active:

- the cursor stays frozen
- one vertical direction raises volume
- the other lowers volume
- motion is accumulated and emitted in discrete steps
- large motion follows the shared bounded discrete-output policy

## Brightness (`PD_SLOT_2`)

`PD_SLOT_2` turns vertical motion into display brightness changes.

While active:

- the cursor stays frozen
- one vertical direction brightens
- the other dims
- motion is accumulated and emitted in discrete steps
- large motion follows the shared bounded discrete-output policy
