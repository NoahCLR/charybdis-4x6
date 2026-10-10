# Interaction Model

This document explains the shared interaction semantics supported by the
`noah` userspace.

It is about the interaction model, not the current physical layout, exact
bindings, or profile-specific timing tweaks. For the visual snapshot, see
[KEYMAP-OVERVIEW.md](./KEYMAP-OVERVIEW.md). For the current authored profile,
see [KEYMAP.md](./KEYMAP.md). For the maintainer-facing runtime map behind
these semantics, see [KEY_RUNTIME.md](./KEY_RUNTIME.md).

## What The Engine Adds

For keys with authored `key_behaviors[]` rows, the firmware can distinguish
between:

- tap
- hold
- longer hold
- single tap through quintuple tap
- hold styles that fire at different times

Each tap-count branch can define its own:

- tap action
- hold tier
- longer-hold tier
- hold style

That makes patterns like these possible:

- a navigation key that covers character, word, and line movement
- a thumb key that combines momentary layer access, persistent layer changes,
  and media actions
- a number or punctuation key that exposes shifted symbols on hold
- a pointer-mode key that stays simple by default or grows richer tap /
  double-tap behavior

Plain keys without an authored row keep their normal QMK behavior, except the
layer keys the runtime owns: `MO(layer)` holds its layer, `TT(layer)` holds it
too and locks it on its `TAPPING_TOGGLE`-th tap (QMK's default, 5),
`OSL(layer)` holds it too and on a tap turns it on for the next key press,
`LM(layer, mods)` holds it together with its modifiers,
`TG(layer)` is `LOCK_LAYER(layer)`, and `TO(layer)` locks that layer alone. A
plain `LT(layer, kc)` keeps QMK's own tap/hold decision and `TAPPING_TERM`;
only the layer its hold turns on is owned, so releasing it leaves a locked
layer on.

A one-shot layer follows QMK's rule for what uses it up: any key press except a
modifier, a one-shot modifier, a mod-tap still held, or `OSL()` itself. The
layer turns off once that press has been processed, so the press itself still
resolves on the one-shot layer. A runtime-owned `MT()` uses its owner's resolved
tap on release instead of QMK's tap count: a qualifying tap consumes the layer
then, even if its output waits for the multi-tap window. Its already selected
action (including a transparent tap) keeps the layer it resolved on. Each
repeated tap follows its selected branch; holds, empty taps, modifier-only taps,
`OSM()` and `OSL()` tap output leave the one-shot armed. Delayed or synthetic
output does not consume it again, so it cannot use up a newly armed one-shot.
As with QMK's `OSL()`, a long press on its own
still arms it, a second tap within `TAPPING_TERM` cancels it, a slower second
tap keeps it on, and a hold that another key used is only a hold. `TO()`
releases it with the locks. `ONESHOT_TIMEOUT` and `ONESHOT_TAP_TOGGLE` apply to
QMK's `OSM()` only, and a build that sets them says so; with one-shot keys
turned off (Magic / VIA), `OSL()` is only a hold, as QMK's is.

`TT(layer)`'s last tap locks the layer on its release. A tap that keeps the
key's own layer on (that lock, or `OSL()` arming) skips the multi-tap
feedback window other last taps get, because the layer itself shows the
result, and the layer never drops between the release and the lock.

## Timing Model

Default timing lives in the active keymap
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).
That keymap chooses:

- `TAPPING_TERM` for QMK's dual-role keys: `LT()`, `MT()`, `TT()`, `OSL()`
  and `OSM()`. Except for `LT()` keys with an authored behaviour row, QMK's
  tapping engine resolves their tap or hold before the runtime sees the press. With a live profile the dual-role setting replaces
  it through `get_tapping_term()` and `get_quick_tap_term()`, so the edited
  term reaches QMK itself, not only the runtime (D-L38)
- `CUSTOM_TAP_HOLD_TERM`
- `CUSTOM_LONGER_HOLD_TERM`
- `CUSTOM_MULTI_TAP_TERM`

Individual `key_behaviors[]` rows can override those defaults with:

- `.tap_hold_term`
- `.longer_hold_term`
- `.multi_tap_term`

For the scalar timing fields, omission means C zero-initializes the field and a
value of `0` means "use the default timing for this row."

Rows carry one policy flag that is not about timing at all:

- `.keeps_auto_mouse_anchored` marks the row as a mouse gesture, so pressing the
  key keeps the pointer layer up instead of letting auto mouse reset on it. It
  is only needed for keys that drive the mouse through authored actions rather
  than being mouse keycodes or pointer-mode keys, and it is what lets a
  `TAP_SENDS(KC_TRNS)` tier on such a key still find a layer to fall through to.
  See [POINTER_MODES.md](./POINTER_MODES.md).

In plain terms:

- a quick release before the tap-hold term is treated as a tap
- crossing the tap-hold term can trigger the hold tier
- crossing the longer-hold term can promote to the longer-hold tier
- repeated taps must stay within the multi-tap term to remain part of the same
  sequence

Foreign-key interruption only cancels the quick tap for true momentary-layer
taps (`MO()`, `TT()`, `OSL()`, `LM()`), whose layer is on from the press. An
`LT()` row is not one: its layer is a hold, so an interrupted tap is still sent. Other authored hold families, such as press-registering modifier holds
and pd-mode lock gestures, keep their own release contract instead of borrowing
the momentary-layer interrupt rule.

For runtime-handled pd-mode keys, pressing the same mode that is currently
locked consumes that lock on press. The physical key still registers its normal
momentary hold, so the mode stays active while the key remains down and
deactivates on release. That unlock press does not later reopen the key's tap
fallback or re-toggle the lock on release. Explicit `*_LOCK` actions remain
normal tap actions and keep their toggle semantics.

Immediate-hold keys still remember that another physical key overlapped them,
but that overlap fact is separate from momentary-layer cancellation. Its job is
narrower: once an immediate-hold key was actually used in an overlap, release
must not reopen the key's quick-release tap or first-tap multi-tap path.

One practical consequence is that a tap on a multi-tap key is delayed by one
multi-tap window so the firmware can tell whether you meant one tap or more. That
applies at every authored depth, the deepest included: a terminal branch preserves
the chain on release rather than firing immediately, so every depth waits the same
window and shows its branch color for the same length of time. Inherited
normal-tap repeats from branches that omit `.tap` still show the branch color, but
they skip tap-commit feedback.

Tap actions are release-settled. Reaching a tap-count branch on press selects the
candidate branch, but `TAP_SENDS(...)` is selected by release and emitted when the
pending tap series flushes. For terminal tap-only multi-tap branches, this means
the final press can identify the branch before the action has actually been sent.

Holding is the one thing that resolves early. Crossing a hold threshold enters the
branch there and then, so the hold tier claims the key without waiting out the
multi-tap window.

The tap index cycles through the authored branches rather than ending the gesture
at the deepest one. A row with four branches answers a fifth tap with branch one
again and a sixth with branch two, so a run of taps of any length resolves to
exactly one action. The consequence is that tapping cannot repeat an action inside
one multi-tap window: to send the same branch twice you have to let the window
close between them.

Those pending multi-tap windows are tracked per physical key. Other handled
keys can keep independent series alive. An unhandled key can settle pending
taps to preserve typing order, but cannot discard an on-time continuation
already buffered in QMK. A tap/hold key's series (an authored `LT()`, `MT()` or
`OSM()` row) is settled by any other key's press, so its tap is typed first.
A pending tap that changes layers, such as a thumb key's tap that locks a
layer, is settled by the press of a key without a behaviour before that key is
resolved: tap the thumb, press `J` at once, and the lock comes first and `J`
types on the locked layer, without waiting for the multi-tap window to close.

### Keys pressed while a tap/hold key is undecided

While a runtime-owned tap/hold key is down and has not reached its hold, keys
pressed after it wait, as QMK's tapping engine holds keys behind a tapping key.
That is an authored `LT()`, `MT()` or `OSM()` row, or an authored key whose
hold is a layer held until release (`PRESS_AND_HOLD_UNTIL_RELEASE(MO(n))`,
previewed or not), at the tap count it is on:

- released before the tap-hold term, it is a tap: its tap is typed, then the
  waiting keys, so rolling `/` into `,` types `/,`;
- held past the term, its hold starts (layer or modifiers), then the waiting
  keys run on it, so `/` held with an arrow tapped meanwhile sends the Nav arrow.
  A waiting key types the key the hold's layer gives it and follows that
  layer's behaviour settings, as if it had been pressed after the hold began.

Waiting keys replay in order with their physical timestamps. The release of a
key pressed before the tap/hold key is not held back, as in QMK. The wait is at
most the tap-hold term; a fast roll gains no delay beyond the tap/hold key's
release. Plain `MO()`, `TT()`, `OSL()`, `LM()` keys and plain keys do not hold
keys back; they act on their press.

The waiting queue is bounded (eight records by default). If a record that must
wait arrives at capacity, the oldest waiting record runs immediately and the
new record takes its place at the tail. Only one record replays for that
arrival. Under this overload, that oldest key may run before the deciding
key's tap or hold, using the layer and modifiers active at delivery. The rest
still wait; records retain their timestamps and no release overtakes its own
press. Releases of keys already delivered, including the deciding key, continue
to pass through. Normal rolls and hold timing below capacity keep the rules above.

### Physical gestures and buffered delivery

For a handled physical key, hold time starts at its physical press. Repeated
taps measures the released gap: release, then press again within the term
(including equality). Double hold means press–release–press-and-hold, not two
completed taps followed by a third press. The first press must qualify as a tap.
A combo can postpone output while it decides which action owns the input; it
does not shorten the allowed physical gap or restart a hold's clock.

An on-time press waiting in QMK keeps its tap series alive. If it wins a combo,
the constituent does not run; if delivered as a key, it continues the series.
A queued physical release prevents scans from inventing a longer hold while
delivery is delayed. Release settlement uses the actual press/release interval
and the existing balanced action lifecycle. Synthetic combo outputs still start
their hold clock on delivery and use the existing combo-origin continuation
protection; there is no single constituent press that defines their hold.

A runtime-owned key is classified once, by userspace. This includes authored
`LT()`, `MT()` and `OSM()` rows, and `TT()`/`OSL()` which already have intrinsic
userspace layer ownership. An unhandled `LT()`, `MT()` or `OSM()` still uses native
QMK tapping. A row that does not author a first hold keeps the key's own, which
starts only once the key is held past the tap-hold term: `LT()` holds its layer
as `MO()`, `MT()` and `OSM()` hold their modifiers. A tap never turns that layer
or those modifiers on, and a hold sends no tap or one-shot on release. The first
tap stays the key's own tap, so adding a repeated-tap branch cannot remove the
hold. A transparent hold over a lower `LT()` inherits the same threshold hold.
The broader bypass is advertised by feature bit 18; bit 17 alone only promises
the authored LT bypass. Pointer-mode interception remains earlier than authored
mouse behaviour: Arrow's button remapping can intentionally consume Button 3.
These rules are advertised by Profile Wire feature bit 17. Older builds can
lose a physically on-time repeat during buffering; increasing timing values
can mask that defect while also delaying normal actions.

One important nuance: inside `key_behaviors[]`, an omitted `.tap_hold_term`
inherits `TAPPING_TERM` for `LT()` rows, but `CUSTOM_TAP_HOLD_TERM` for other
custom rows.

## Normal Tap And Hold Fallbacks

An authored row does not automatically replace everything about a key.

- if `.tap` is omitted for a tap-count branch, that branch keeps the key's
  normal tap behavior; a quick tap on that branch can still show its branch color
  if the branch authors a hold or long-hold tier, but it does not show
  tap-commit feedback
- `KC_TRNS` inside any authored action helper is transparent for that field:
  tap fields use the lower active layer's tap output, while hold and long-hold
  fields use the lower active layer's same-tier behavior and mode-owned
  metadata
- if `.tap` is present but `.hold` and `.long_hold` are both omitted, keys
  that already have a default held path keep using it for that branch
- stacked pd-mode rows are the exception: when a first-tap override and a
  later tap-count hold can enter a different pd mode, the first pd mode waits
  until the hold threshold instead of activating while the tap count is still
  unresolved
- once `.hold` or `.long_hold` is authored for that branch, the normal held
  fallback is no longer used for that branch

In practice, the common families look like this:

- ordinary keys such as `KC_A` can keep their normal held-key behavior when
  only the tap is overridden
- `KC_TRNS` keeps the current row's timing and multi-tap branching, but the
  lower key still owns the actual transparent field behavior; for hold and
  long-hold fields that includes helper mode and mode-owned metadata such as
  lower momentary-layer or pd-mode ownership
- `LT()` rows keep their layer hold, as `MO()` once held past the tap-hold term,
  when only the tap is overridden
- plain pd-mode keycodes keep their default momentary mode hold when only the
  tap is overridden
- pd-mode keys whose tap path can branch into another pd mode defer the lower
  mode until hold threshold so the two mode lifecycles cannot overlap during
  tap disambiguation
- keycodes without a default held path, such as most custom keycodes, do not
  invent one just because a tap override exists

## The Four Hold Modes

Not every hold behaves the same way.

### `PRESS_AND_HOLD_UNTIL_RELEASE`

The alternate action becomes active at the hold threshold and stays active
until you let go.

Use this when the action should feel immediate and remain active while the key
is held, such as:

- a shifted symbol that should repeat naturally
- a media or navigation action that should stay registered while held
- a momentary layer hold
- a momentary pointing-device mode hold

### `REPEAT_WHILE_HELD`

The alternate action fires once at the hold threshold, then keeps firing at the
authored repeat rate until you let go.

Use this when the action should behave like repeated taps rather than one held
registration, such as:

- repeated left click or other mouse-button spam
- repeated navigation taps
- repeated media or macro triggers that should stop immediately on release

Authored repeat rates are currently limited to `1..100 Hz`, with a maximum of
`100` repeats per second.

### `TAP_AT_HOLD_THRESHOLD`

The alternate action fires once as soon as the threshold is crossed.

Use this when the action should happen as soon as the user has committed to the
hold, such as:

- a persistent layer or mode change
- a one-shot system shortcut
- a key that should escalate cleanly from hold to longer hold

### `TAP_ON_RELEASE_AFTER_HOLD`

Nothing fires at the hold threshold. The action is sent on release instead.
If a longer-hold tier takes over before release, this release-based hold does
not fire.

Use this when the middle tier should stay tentative until the user commits to
releasing, such as:

- a navigation key where the medium hold should not jump early
- a key that distinguishes between a medium hold and a longer hold on the same
  physical switch

## Multi-Tap Behavior

Multi-tap is part of the same model. It is not a separate feature.

A key can define behavior for:

- first tap
- second tap
- third tap
- fourth tap
- fifth tap

Each of those tap counts can also define its own tap, hold, and longer-hold
behavior.

That lets one key combine patterns such as:

- momentary layer access on hold
- persistent layer change on a tap or higher-tap hold
- media or alternate actions on later taps
- branching into a different action family on a later press

Combo outputs follow the same rules when their output keycode has an authored
behavior row. A combo that emits `KC_LEFT_GUI`, for example, participates in
the `KC_LEFT_GUI` tap series like the physical key, but tap actions still settle
from the combo output release rather than from the combo output press.

## Pointer-Mode Keys

Pointing-device mode keys do not use a separate timing system.

- a plain pd-mode keycode placed directly in the keymap works as a default
  momentary mode key
- an authored `key_behaviors[]` row can add explicit tap, hold, longer-hold,
  and multi-tap behavior on top of that default
- if a `[0].tap` override is omitted, a quick single tap sends nothing and the
  default momentary hold remains
- if `[0].tap` is authored and a later hold enters a different pd mode, the
  runtime treats the first mode like a normal threshold hold rather than an
  immediate implicit hold

That means a mode key can stay simple, or it can grow patterns such as:

- double-tap lock
- tap-to-character plus hold-for-mode
- higher-tap mute or alternate action
- a second-press branch into another pointing-device mode

For the raw mode behavior and pointer-layer policy, see
[POINTER_MODES.md](./POINTER_MODES.md). For the current authored patterns on
this keymap, see [KEYMAP.md](./KEYMAP.md).

## RGB Feedback

If `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` is enabled, the key-behavior engine can
also project its state into the RGB overlay.

Shared semantics:

- every tap past the base one shows the color of the branch it reaches, for as
  long as nothing more specific applies. Any action state replaces it: a pending
  hold tier, the tap a release would send, an active hold. A branch that authors a
  `.tap` and no `.hold` therefore shows its tap color once its multi-tap window has
  closed, whether or not it also authors a `.long_hold`; a branch with no `.tap` at
  all keeps the branch color until its hold threshold arrives. Another tap simply
  renames the branch. Branch 0 paints nothing: it is the non-tapping surface, so
  whatever the layer or effect underneath is showing simply stays, and one tap
  does not show intent to enter a tap branch
- committed authored tap-count branches can pulse once after the tap output
  resolves; the authored RGB config can disable those pulses or limit them to
  double-tap and higher branches
- pending momentary-layer holds can preview the target layer's authored color
  and LED groups before that layer actually commits, when every outcome of the
  press lands on that layer (a thumb key whose tap locks the layer its hold
  turns on); an authored `LT()` row, whose tap types a key, previews nothing
- unresolved hold windows can show the hold color while the action is still
  pending
- threshold-fired hold or longer-hold actions can pulse once when they fire
- held non-layer `PRESS_AND_HOLD_UNTIL_RELEASE(...)` actions can stay visibly
  active while the action remains registered
- held layer-switch actions use preview and active layer color instead of a
  pulse or hold overlay
- layer and pointing-device state taps stay on their layer/PD overlays instead
  of also emitting tap-commit feedback

For the full RGB authoring model, render order, and configuration surface, see
[RGB_CONFIG.md](./RGB_CONFIG.md).

## Related Docs

- [README.md](../README.md): top-level overview of the shared userspace
- [GUIDE.md](./GUIDE.md): the full `key_behaviors[]` vocabulary and authoring walkthrough
- [KEY_RUNTIME.md](./KEY_RUNTIME.md): maintainer-facing handled-key runtime map
- [KEYMAP.md](./KEYMAP.md): Noah's current concrete profile choices
- [POINTER_MODES.md](./POINTER_MODES.md): raw pointing-device mode behavior
- [RGB_CONFIG.md](./RGB_CONFIG.md): RGB authoring and render order

### Release intervals and timing advice

When Hold and Long hold both send on release, Hold can be selected only on a
release at or after Tap / hold and before Long hold. If Long hold is at or before
Tap / hold, that interval is empty. The long release action takes precedence on
a qualifying hold. This rule is covered at threshold minus one, equality and
plus one by the release matrix. Other hold modes can emit before release and
must not be inferred unreachable from this rule. Scan cadence and buffered
delivery can affect which threshold is observed first.

Ark warns about the empty interval and excludes that branch from layer reachability;
it also advises on overlapping and narrow windows. Transparent inherited actions
need their resolved context and are not pruned by this simple rule.
