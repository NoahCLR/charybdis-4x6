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

Plain keys without an authored row keep their normal QMK behavior.

## Timing Model

Default timing lives in the active keymap
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).
That keymap chooses:

- `TAPPING_TERM` for built-in QMK dual-role keys such as `LT()` and `MT()`
- `CUSTOM_TAP_HOLD_TERM`
- `CUSTOM_LONGER_HOLD_TERM`
- `CUSTOM_MULTI_TAP_TERM`

Individual `key_behaviors[]` rows can override those defaults with:

- `.tap_hold_term`
- `.longer_hold_term`
- `.multi_tap_term`

If one of those fields is omitted, C zero-initializes it. A value of `0` means
"use the default timing for this row."

In plain terms:

- a quick release before the tap-hold term is treated as a tap
- crossing the tap-hold term can trigger the hold tier
- crossing the longer-hold term can promote to the longer-hold tier
- repeated taps must stay within the multi-tap term to remain part of the same
  sequence

Foreign-key interruption only cancels the quick tap for true momentary-layer
taps. Other authored hold families, such as press-registering modifier holds
and pd-mode quick-lock taps, keep their own release contract instead of
borrowing the momentary-layer interrupt rule.

Immediate-hold keys still remember that another physical key overlapped them,
but that overlap fact is separate from momentary-layer cancellation. Its job is
narrower: once an immediate-hold key was actually used in an overlap, release
must not reopen the key's quick-release tap or first-tap multi-tap path.

One practical consequence is that a single tap on a multi-tap key is delayed by
one multi-tap window so the firmware can tell whether you meant one tap or
more.

Those pending multi-tap windows are tracked per physical key. Pressing a
different key does not flush an unrelated pending tap series by itself, so
independent keys can keep separate tap counts and timing windows alive at the
same time.

Behavior change note: this per-key pending-series retention became the intended
contract on `2026-04-20`. Older runtime policy flushed an unrelated pending
multi-tap series as soon as a different key was pressed. If independent
simultaneous tap sequences stop working again, treat that as a regression
against this document and re-run
`sh tests/host/run_key_runtime_scenario_tests.sh` plus
`sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`.

One important nuance: inside `key_behaviors[]`, an omitted `.tap_hold_term`
inherits `TAPPING_TERM` for `LT()` rows, but `CUSTOM_TAP_HOLD_TERM` for other
custom rows.

## Normal Tap And Hold Fallbacks

An authored row does not automatically replace everything about a key.

- if `.tap` is omitted for a tap-count branch, that branch keeps the key's
  normal tap behavior
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
- `LT()` rows keep their normal momentary layer hold when only the tap is
  overridden
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

- multi-tap pending can show the currently selected unresolved tap branch,
  including the terminal tap-only branch for the normal pending window
- committed tap branches can pulse once after the tap output resolves; the
  authored RGB config can disable those pulses, limit them to double-tap and
  higher branches, or allow them for every tap branch
- pending momentary-layer holds can preview the target layer's authored color
  and LED groups before that layer actually commits
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
- [KEY_RUNTIME.md](./KEY_RUNTIME.md): maintainer-facing handled-key runtime map
- [KEYMAP.md](./KEYMAP.md): Noah's current concrete profile choices
- [POINTER_MODES.md](./POINTER_MODES.md): raw pointing-device mode behavior
- [RGB_CONFIG.md](./RGB_CONFIG.md): RGB authoring and render order
