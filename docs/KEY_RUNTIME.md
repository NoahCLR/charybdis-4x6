# Key Runtime

This document is the maintainer-facing map for the handled-key runtime under
[`users/noah/lib/key/`](../users/noah/lib/key/).

Use this document when you are changing runtime behavior, not when you are
authoring a keymap profile. For user-facing interaction semantics, see
[INTERACTION_MODEL.md](./INTERACTION_MODEL.md). For Noah's current authored
profile, see [KEYMAP.md](./KEYMAP.md).

## Design Goals

The key runtime is built around a few fixed rules:

- authored behavior lives in `keymap.c` and `key_behaviors[]`
- the runtime owns gesture resolution by physical key position, not by the
  current layer's remapped keycode
- slot reducers mutate runtime state and emit ordered effects instead of
  calling QMK side effects directly
- long-lived ownership state such as layers, held actions, repeats, and
  modifiers lives in named modules instead of ad hoc globals

That shape is deliberate. This board has custom tap/hold, pd-mode, layer, RGB,
and repeat behavior that would be hard to reason about if each path called into
QMK directly at arbitrary points.

## Main Components

These files are the core map of the runtime:

| File | Responsibility |
| --- | --- |
| [`handled_key.h`](../users/noah/lib/key/handled_key.h) and [`key_runtime.c`](../users/noah/lib/key/key_runtime.c) | Resolve a keycode into a fully interpreted `handled_key_view_t` with tap, hold, long-hold, timing, layer, pd-mode, and flags |
| [`runtime_shared_state.h`](../users/noah/lib/state/runtime_shared_state.h) | Own the central slot storage and pd-mode runtime flags |
| [`key_runtime_process.c`](../users/noah/lib/key/key_runtime_process.c) | `process_record_user` entry flow and top-level branching |
| [`key_runtime_preflight.c`](../users/noah/lib/key/key_runtime_preflight.c) | Physical-event preflight, modifier suppression, active-slot interrupts, and pending-multi-tap flushing |
| [`key_runtime_press.c`](../users/noah/lib/key/key_runtime_press.c), [`key_runtime_release.c`](../users/noah/lib/key/key_runtime_release.c), and [`key_runtime_scan.c`](../users/noah/lib/key/key_runtime_scan.c) | Outer orchestration for press, release, and scan passes |
| [`key_runtime_slot_step.c`](../users/noah/lib/key/key_runtime_slot_step.c) | Single reducer seam for slot events |
| [`key_runtime_slot_policy.c`](../users/noah/lib/key/key_runtime_slot_policy.c) | Threshold and hold-policy helpers that mutate slot state and request effects |
| [`key_runtime_slot_release_reduce.c`](../users/noah/lib/key/key_runtime_slot_release_reduce.c) | Release-resolution logic |
| [`key_runtime_slot_scan_reduce.c`](../users/noah/lib/key/key_runtime_slot_scan_reduce.c) | Scan-time threshold logic |
| [`key_runtime_slot_pending_multi_tap.c`](../users/noah/lib/key/key_runtime_slot_pending_multi_tap.c) | Deferred multi-tap ownership after release |
| [`key_runtime_effect.h`](../users/noah/lib/key/key_runtime_effect.h), [`key_runtime_slot_result.c`](../users/noah/lib/key/key_runtime_slot_result.c), and [`key_runtime_transition.c`](../users/noah/lib/key/key_runtime_transition.c) | Shared runtime effect vocabulary, request expansion, plan batching, and effect execution |
| [`action_dispatch.c`](../users/noah/lib/action/action_dispatch.c) and [`keyboard_mod_state.c`](../users/noah/lib/state/keyboard_mod_state.c) | Explicit output-intent helpers for authored action taps, synthetic QMK taps, and literal taps that may need fallback-hold settlement or temporary modifier suspension |
| [`held_action.c`](../users/noah/lib/key/held_action.c), [`held_repeat.c`](../users/noah/lib/key/held_repeat.c), [`layer_ownership.c`](../users/noah/lib/state/layer_ownership.c), and [`keyboard_mod_ownership.c`](../users/noah/lib/state/keyboard_mod_ownership.c) | Long-lived ownership registries touched by runtime effects |
| [`runtime_debug.c`](../users/noah/lib/state/runtime_debug.c) | Aggregate runtime snapshot and test reset surface |

## End-To-End Flow

### 1. `noah_process_record_user()`

[`key_runtime_process.c`](../users/noah/lib/key/key_runtime_process.c) is the
only top-level runtime entry point for physical key events.

Its order matters:

1. synthetic records bypass the physical-key runtime so keymap-local custom
   actions can re-enter `process_record_user()` safely
2. preflight runs before handled-key resolution
3. the active pd-mode handler gets a chance to consume the event
4. handled keys route into the custom runtime
5. direct action keys such as layer-lock and pd-mode-lock taps bypass slot
   ownership and dispatch immediately
6. macro dispatch runs last on press only

### 2. Preflight

[`key_runtime_preflight.c`](../users/noah/lib/key/key_runtime_preflight.c)
does the cross-cutting work that must happen before the current key is reduced:

- track physical modifier ownership
- suppress the raw QMK path for managed modifier transitions when appropriate
- interrupt other active handled keys on a new press
- flush unrelated pending multi-tap chains before a non-handled press proceeds

This is why a new physical press can affect another key's slot before the new
key itself resolves.

### 3. Handled Press

[`key_runtime_press.c`](../users/noah/lib/key/key_runtime_press.c) selects the
slot for the pressed physical key position and computes whether any currently
owned held action should survive a slot reclaim.

It then feeds one `KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS` event into
[`key_runtime_transition.c`](../users/noah/lib/key/key_runtime_transition.c),
which passes it through the slot reducer, records an ordered effect plan, and
only then executes those effects.

The press path can:

- begin a new slot
- reuse a pending multi-tap chain for the same physical key
- reclaim an already-active slot at that position
- start immediate held ownership for press-registering holds
- start momentary layer ownership

### 4. Handled Release

[`key_runtime_release.c`](../users/noah/lib/key/key_runtime_release.c) routes
release by physical key position.

That is an important invariant: if the layer changed while the key was held,
release still resolves against the slot that owns that physical switch. If the
slot owner's keycode differs from the raw release keycode, the release path
re-resolves the owner's handled behavior before reducing the release.

The release reducer can turn one physical release into:

- a normal tap
- a pending multi-tap chain
- a release-time hold action
- a pd-mode lock tap
- pure ownership cleanup with no dispatch

### 5. Scan

[`key_runtime_scan.c`](../users/noah/lib/key/key_runtime_scan.c) runs two slot
event passes every matrix scan:

- `KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN`
- `KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN`

Those scan reducers handle threshold firing, long-hold promotion, and
pending-multi-tap expiry through the same transition-plan mechanism as press
and release.

After the transition plan executes, [`held_repeat.c`](../users/noah/lib/key/held_repeat.c)
advances time-based repeat bindings and dispatches any repeat taps that are due.

## Slot Model

The central runtime storage is
[`active_key_state_t`](../users/noah/lib/state/runtime_shared_state.h) inside
[`runtime_shared_state.h`](../users/noah/lib/state/runtime_shared_state.h).
Each slot owns one physical position's active press state plus any deferred
multi-tap chain that still belongs to that position.

The main fields are:

- `owner`: original keycode and physical key position
- `lifecycle`: slot phase, current held-action ownership, repeat ownership,
  hold strategy, pd-mode lock state on press, and whether a layer hold was
  interrupted by another key
- `binding`: resolved tap, hold, and long-hold actions for the active press
- `timing`: tap-hold, longer-hold, and multi-tap terms
- `semantic`: cached metadata for feedback and debugging such as layer,
  preview-layer hint, pd-mode, and multi-tap capability
- `pending_multi_tap`: deferred tap-chain state that remains after release

The phase enum is small but important:

- `IDLE`: no active press
- `TAP_WINDOW`: the key may still become a tap or a threshold-fired hold
- `PRESS_HELD_WINDOW`: an immediate hold is already active, but tap release is
  still allowed until the tap-hold term closes
- `RELEASE_HOLD_PENDING`: the hold will send on release instead of at the
  threshold
- `HOLD_TIER_ACTIVE`: the hold tier is committed, but long-hold promotion can
  still happen
- `HOLD_COMPLETE`: no more threshold work remains for this slot

## Effect Model

The handled-key path now has one shared executable effect vocabulary:
[`key_runtime_effect_t`](../users/noah/lib/key/key_runtime_effect.h).

There are still two layers inside the reducer implementation, on purpose:

- `key_runtime_slot_effect_request_t` in
  [`key_runtime_slot_effect.h`](../users/noah/lib/key/key_runtime_slot_effect.h)
  is a narrow local helper used by slot-policy code
- `key_runtime_effect_t` in
  [`key_runtime_effect.h`](../users/noah/lib/key/key_runtime_effect.h) is the
  real runtime effect surface that leaves the reducer and reaches transition
  execution

The pipeline is:

1. slot-policy helpers mutate slot state and return effect requests
2. [`key_runtime_slot_result.c`](../users/noah/lib/key/key_runtime_slot_result.c)
   expands those requests into ordered runtime effects
3. [`key_runtime_transition.c`](../users/noah/lib/key/key_runtime_transition.c)
   batches effects into a plan
4. the transition executor applies the plan in order

Current executable effects cover:

- dispatching one action
- registering or unregistering held ownership
- releasing owned state by physical key
- starting repeat scheduling
- layer ownership press and release
- feedback pulses
- pd-mode lock taps
- delayed action replay for flushed multi-taps

`KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY` is intentionally broader than
"unregister held action": it releases per-key held action ownership and any
repeat binding owned by that physical key position.

Two size limits are worth knowing when debugging overflow:

- slot results hold up to `8` effects
- transition plans hold up to `16` effects

Both surfaces mark overflow rather than reallocating.

## Important Invariants

These are the easiest runtime rules to break by accident:

- Release is routed by physical key position, not by the current layer's live
  keycode mapping.
- `handled_key_view_t` is the resolved handled-key contract. If downstream code
  needs extra semantics later, add them there first instead of re-deriving
  policy from raw authored data.
- Feedback and debug readers should prefer cached slot semantic metadata over
  re-running handled-key resolution against mutable slot state.
- New emitters should prefer the explicit helpers in
  [`action_dispatch.h`](../users/noah/lib/action/action_dispatch.h) so they
  state whether they settle pending fallback holds or preserve keyboard mod
  state.
- `action_dispatch()` now exists as the compatibility wrapper for the
  runtime-default authored tap path. New code should not treat it as the only
  output seam.
- Slot reset should flow through `key_runtime_slot_reset()` or
  `runtime_shared_state_reset()` so default timing and semantic sentinels stay
  valid.
- Authored momentary layers are owned through `layer_ownership.c`, not raw
  `layer_on()` and `layer_off()` calls in the key runtime.
- Unusual pd-mode side effects belong in the pd-mode lifecycle seam owned by
  the `pd_mode_def_t` row, not in ad hoc key-runtime branches or registry
  mode-selection switches.

## Where To Change What

If you are changing one of these categories, start here:

- resolved handled-key semantics:
  [`handled_key.h`](../users/noah/lib/key/handled_key.h) and
  [`key_runtime.c`](../users/noah/lib/key/key_runtime.c)
- outer event orchestration:
  [`key_runtime_process.c`](../users/noah/lib/key/key_runtime_process.c),
  [`key_runtime_preflight.c`](../users/noah/lib/key/key_runtime_preflight.c),
  [`key_runtime_press.c`](../users/noah/lib/key/key_runtime_press.c),
  [`key_runtime_release.c`](../users/noah/lib/key/key_runtime_release.c), and
  [`key_runtime_scan.c`](../users/noah/lib/key/key_runtime_scan.c)
- slot threshold and release behavior:
  [`key_runtime_slot_policy.c`](../users/noah/lib/key/key_runtime_slot_policy.c),
  [`key_runtime_slot_release_reduce.c`](../users/noah/lib/key/key_runtime_slot_release_reduce.c),
  [`key_runtime_slot_scan_reduce.c`](../users/noah/lib/key/key_runtime_slot_scan_reduce.c),
  and
  [`key_runtime_slot_pending_multi_tap.c`](../users/noah/lib/key/key_runtime_slot_pending_multi_tap.c)
- new side-effect kind:
  [`key_runtime_effect.h`](../users/noah/lib/key/key_runtime_effect.h),
  [`key_runtime_slot_result.c`](../users/noah/lib/key/key_runtime_slot_result.c),
  and
  [`key_runtime_transition.c`](../users/noah/lib/key/key_runtime_transition.c)
- output-emission policy:
  [`action_dispatch.h`](../users/noah/lib/action/action_dispatch.h),
  [`action_dispatch.c`](../users/noah/lib/action/action_dispatch.c), and
  [`keyboard_mod_state.c`](../users/noah/lib/state/keyboard_mod_state.c)
- new hidden runtime state:
  either [`runtime_shared_state.h`](../users/noah/lib/state/runtime_shared_state.h)
  or one explicit ownership module plus
  [`runtime_debug.c`](../users/noah/lib/state/runtime_debug.c)

If a change crosses those boundaries, it is probably architectural enough to
deserve updates to this doc and to the active review log in `review/`.

## Debugging And Tests

For higher-level debugging, use
[`runtime_debug.c`](../users/noah/lib/state/runtime_debug.c) instead of
rebuilding partial resets inside individual tests.

The most relevant host checks for the runtime are:

- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

When runtime wiring or behavior changes, finish with:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Related Docs

- [README.md](../README.md): top-level userspace overview
- [INTERACTION_MODEL.md](./INTERACTION_MODEL.md): user-facing tap/hold/multi-tap semantics
- [KEYMAP.md](./KEYMAP.md): current authored profile
- [ADDING_PD_MODE.md](./ADDING_PD_MODE.md): pd-mode extension surface
