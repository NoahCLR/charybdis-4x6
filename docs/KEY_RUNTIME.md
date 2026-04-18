# Key Runtime

This document is the maintainer-facing map for the handled-key runtime under
[`users/noah/lib/key/`](../users/noah/lib/key/).

Use this document when you are changing runtime behavior, not when you are
authoring a keymap profile. For user-facing interaction semantics, see
[INTERACTION_MODEL.md](./INTERACTION_MODEL.md). For Noah's current authored
profile, see [KEYMAP.md](./KEYMAP.md).

`users/noah/lib/key/` is organized by ownership:

- `interaction/` for authored behavior schema, lookup, handled-key
  interpretation, multi-tap semantics, and keymap validation
- `ownership/` for held-action and held-repeat ownership
- `runtime/` for delayed action replay and top-level execution flow
- `runtime/effects/` for shared runtime effect vocabulary
- `runtime/slot/` for slot reducers, slot results, and slot-local helpers

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
| [`handled_key.h`](../users/noah/lib/key/interaction/handled_key.h), [`handled_key_lookup.c`](../users/noah/lib/key/interaction/handled_key_lookup.c), [`handled_key_materialize.c`](../users/noah/lib/key/interaction/handled_key_materialize.c), [`handled_key_defaults.c`](../users/noah/lib/key/interaction/handled_key_defaults.c), and [`handled_key_resolution_accessors.c`](../users/noah/lib/key/interaction/handled_key_resolution_accessors.c) | Resolve authored behavior into a `handled_key_resolution_t`, then materialize it into slot-owned interaction metadata, transparency defaults, and release-policy helpers |
| [`key_runtime_interaction.h`](../users/noah/lib/key/runtime/key_runtime_interaction.h) | Materialize authored handled-key resolution into the slot-owned `key_runtime_slot_interaction_t` contract: cached binding, hold strategy, release semantics, and preview/feedback policy |
| [`key_runtime_api.h`](../users/noah/lib/key/runtime/key_runtime_api.h), [`runtime_reset.h`](../users/noah/lib/state/runtime/runtime_reset.h), [`runtime_debug.h`](../users/noah/lib/state/runtime/runtime_debug.h), and the pd-mode read APIs in [`pd_mode_flags.h`](../users/noah/lib/pointing/defs/pd_mode_flags.h) / [`pd_modes.h`](../users/noah/lib/pointing/defs/pd_modes.h) | Public cross-module key-runtime entry seam, public runtime reset seam, public key-runtime observation helpers, and semantic pd-mode read state |
| [`key_runtime_internal.h`](../users/noah/lib/key/runtime/key_runtime_internal.h), [`key_runtime_process_internal.h`](../users/noah/lib/key/runtime/key_runtime_process_internal.h), [`key_runtime_index_internal.h`](../users/noah/lib/key/runtime/key_runtime_index_internal.h), and [`key_runtime_shared_state.h`](../users/noah/lib/key/runtime/key_runtime_shared_state.h) | Key-runtime-owned slot/process/index/storage internals for runtime modules and allowlisted white-box host suites |
| [`key_runtime_process.c`](../users/noah/lib/key/runtime/key_runtime_process.c) | `process_record_user` entry flow and top-level branching |
| [`key_runtime_preflight.c`](../users/noah/lib/key/runtime/key_runtime_preflight.c) | Physical-event preflight, modifier suppression, active-slot interrupts, and pending-multi-tap flushing |
| [`key_runtime_press.c`](../users/noah/lib/key/runtime/key_runtime_press.c), [`key_runtime_release.c`](../users/noah/lib/key/runtime/key_runtime_release.c), and [`key_runtime_scan.c`](../users/noah/lib/key/runtime/key_runtime_scan.c) | Outer orchestration for press, release, and scan passes |
| [`key_runtime_slot_step.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_step.c), [`key_runtime_slot_press_reduce.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c), [`key_runtime_slot_release_reduce.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_reduce.c), [`key_runtime_slot_release_active.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c), and [`key_runtime_slot_release_resolver.h`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_resolver.h) | Slot-event router plus the narrower press and release reducers it delegates to, including the shared release-decision helper consumed by active-slot and pending-multi-tap paths |
| [`key_runtime_slot_policy.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c) | Threshold and hold-policy helpers that mutate slot state and request effects |
| [`key_runtime_slot_scan_reduce.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.c) | Scan-time threshold logic |
| [`key_runtime_slot_pending_multi_tap.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c) | Deferred multi-tap ownership after release |
| [`key_runtime_effect.h`](../users/noah/lib/key/runtime/effects/key_runtime_effect.h), [`key_runtime_effect_queue.h`](../users/noah/lib/key/runtime/effects/key_runtime_effect_queue.h), [`key_runtime_slot_result.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_result.c), and [`key_runtime_transition.c`](../users/noah/lib/key/runtime/key_runtime_transition.c) | Shared runtime effect vocabulary, shared queue field layout, request expansion, plan batching, and effect execution |
| [`action_dispatch.h`](../users/noah/lib/action/action_dispatch.h), [`action_dispatch.c`](../users/noah/lib/action/action_dispatch.c), [`action_lifecycle.c`](../users/noah/lib/action/action_lifecycle.c), [`owned_keycode.c`](../users/noah/lib/action/owned_keycode.c), and [`keyboard_mod_state.c`](../users/noah/lib/state/runtime/keyboard_mod_state.c) | Action classification, lifecycle dispatch, overlap-safe literal key ownership, and explicit output-intent helpers for authored taps, synthetic QMK taps, temporary modifier suspension, and fallback-hold settling before emitted actions |
| [`held_action.c`](../users/noah/lib/key/ownership/held_action.c), [`held_repeat.c`](../users/noah/lib/key/ownership/held_repeat.c), [`layer_ownership.c`](../users/noah/lib/state/ownership/layer_ownership.c), and [`keyboard_mod_ownership.c`](../users/noah/lib/state/ownership/keyboard_mod_ownership.c) | Long-lived ownership registries touched by runtime effects |
| [`runtime_debug.h`](../users/noah/lib/state/runtime/runtime_debug.h), [`runtime_reset.h`](../users/noah/lib/state/runtime/runtime_reset.h), [`runtime_trace.h`](../users/noah/lib/state/runtime/runtime_trace.h), and [`runtime_trace.c`](../users/noah/lib/state/runtime/runtime_trace.c) | Public key-runtime observation surface, public runtime reset seam, and the optional shared trace ring buffer used for cross-subsystem debugging |

## End-To-End Flow

### 1. `noah_process_record_user()`

[`key_runtime_process.c`](../users/noah/lib/key/runtime/key_runtime_process.c) is the
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

[`key_runtime_preflight.c`](../users/noah/lib/key/runtime/key_runtime_preflight.c)
does the cross-cutting work that must happen before the current key is reduced:

- track physical modifier ownership
- suppress the raw QMK path for managed modifier transitions when appropriate
- interrupt other active handled keys on a new press
- flush unrelated pending multi-tap chains before a press on another physical
  key proceeds

That pending-chain flush is intentionally narrower than "any slot that still has
multi-tap state". A slot can keep a same-key multi-tap chain internally while
it is actively processing the next press in that chain; preflight must not
treat that live active slot as a foreign pending chain and flush it on another
key's press.

This is why a new physical press can affect another key's slot before the new
key itself resolves.

### 3. Handled Press

[`key_runtime_press.c`](../users/noah/lib/key/runtime/key_runtime_press.c) selects the
slot for the pressed physical key position and computes whether any currently
owned held action should survive a slot reclaim.

It then feeds one `KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS` event into
[`key_runtime_transition.c`](../users/noah/lib/key/runtime/key_runtime_transition.c),
which passes it through the slot-event router in
[`key_runtime_slot_step.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_step.c),
records an ordered effect plan, and only then executes those effects. The
router delegates handled-press work into
[`key_runtime_slot_press_reduce.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c).

The press path can:

- begin a new slot
- reuse a pending multi-tap chain for the same physical key
- settle foreign pending multi-tap chains before a different physical key starts
- reclaim an already-active slot at that position
- start immediate held ownership for press-registering holds
- start momentary layer ownership

### 4. Handled Release

[`key_runtime_release.c`](../users/noah/lib/key/runtime/key_runtime_release.c) routes
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

That release path is intentionally split now:

- [`key_runtime_slot_release_reduce.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_reduce.c)
  handles outer release routing, pending-multi-tap handoff, and orphaned
  cleanup
- [`key_runtime_slot_release_active.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c)
  resolves the active-slot tap vs hold vs pd-mode-lock outcome

Shared tap-vs-hold-vs-pd-lock semantics now live in
[`key_runtime_slot_release_resolver.h`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_resolver.h).
The active-slot and pending-multi-tap release paths are adapters around that
one decision surface rather than parallel release engines.

### 5. Scan

[`key_runtime_scan.c`](../users/noah/lib/key/runtime/key_runtime_scan.c) runs two slot
event passes every matrix scan:

- `KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN`
- `KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN`

Those scan reducers handle threshold firing, long-hold promotion, and
pending-multi-tap expiry through the same transition-plan mechanism as press
and release.

The pending-multi-tap index only tracks inactive deferred chains. If an active
slot is still carrying the same-key multi-tap chain internally, the scan pass
services that chain explicitly from the active-slot scan loop instead of
relying on the global pending index.

Fallback-hold settlement follows a similar overlap rule: action emission must
drain every currently pending fallback-hold candidate before emitting a tap or
synthetic keycode, not just the first indexed slot. Fast alternating handled
keys can leave more than one active fallback-hold candidate live at the same
time, and settling only one of them reintroduces overlap-sensitive ownership
ordering bugs.

Handled release dispatch now follows the same overlap discipline. If a handled
release resolves to a tap/action while another handled slot is still
tap-release-eligible, the release tap is deferred into key-runtime-owned state
and drained from scan only after no such sibling remains live. That keeps
release cleanup local while avoiding re-entrant emitted-action timing during
two-key overlap.

Time-based repeat bindings now advance from the userspace housekeeping hook in
[`runtime_init.c`](../users/noah/runtime_init.c), after QMK has already
processed matrix changes for the loop. [`held_repeat.c`](../users/noah/lib/key/ownership/held_repeat.c)
therefore schedules repeats from a post-event phase instead of the matrix-scan
phase.

## Slot Model

The central slot storage lives in
[`key_runtime_shared_state.h`](../users/noah/lib/key/runtime/key_runtime_shared_state.h)
and is composed into the internal runtime owner layer from
[`runtime_shared_state_internal.h`](../users/noah/lib/state/runtime/runtime_shared_state_internal.h).
Each slot owns one physical position's active
press state plus any deferred multi-tap chain that still belongs to that
position.

The main fields are:

- `timer`: press timestamp used for threshold and release-time evaluation
- `owner`: original keycode and physical key position
- `lifecycle`: slot phase, current held-action ownership, repeat ownership,
  hold strategy, pd-mode lock state on press, and whether a layer hold was
  interrupted by another key
- `interaction`: cached `key_runtime_slot_interaction_t`, which owns the
  active press's authored branch selection plus slot-owned binding,
  cached hold policy, and cached release semantics including typed tap
  materialization and shared release-hold selection used by feedback, scan,
  and release reducers
- `pending_multi_tap`: deferred tap-chain state that remains after release
- `deferred_release_dispatch`: queued release-time authored taps/actions whose
  emission was postponed until overlapping tap-release siblings retired

Release-time overlap safety now follows a narrow rule:

- if a handled release resolves to an authored tap/action while a foreign
  tap-release-eligible handled sibling is still live, only that authored
  dispatch defers into `deferred_release_dispatch`
- ownership cleanup (`RELEASE_OWNED_STATE_BY_KEY`) still executes immediately
- pending multi-tap synthetic held lifecycle (`HELD_ACTION_REGISTER` /
  `HELD_ACTION_UNREGISTER`) still executes immediately
- momentary layer release still executes immediately
- pd-mode lock-tap effects still execute immediately
- when scan later drains the deferred queue, it does so after executing the
  normal scan plan, so any sibling delayed-action replay for that loop runs
  before the deferred release dispatch

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
[`key_runtime_effect_t`](../users/noah/lib/key/runtime/effects/key_runtime_effect.h).

Slot results and transition plans now also share one queue field vocabulary
through [`key_runtime_effect_queue.h`](../users/noah/lib/key/runtime/effects/key_runtime_effect_queue.h):
`items`, `count`, and `overflowed`.

There are still two layers inside the reducer implementation, on purpose:

- `key_runtime_effect_builder_t` in
  [`key_runtime_slot_effect.h`](../users/noah/lib/key/runtime/slot/key_runtime_slot_effect.h)
  is a narrow reducer-local builder used by slot-policy code
- `key_runtime_effect_t` in
  [`key_runtime_effect.h`](../users/noah/lib/key/runtime/effects/key_runtime_effect.h) is the
  real runtime effect surface carried directly by slot results and transition
  plans

The pipeline is:

1. slot-policy helpers mutate slot state and return effect builders
2. [`key_runtime_slot_result.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_result.c)
   expands those builders into ordered runtime effects
3. [`key_runtime_transition.c`](../users/noah/lib/key/runtime/key_runtime_transition.c)
   batches those same effects into a plan
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

Both surfaces expose `overflowed` rather than reallocating, and the host test
surface now fails fast if either queue overflows.

## Important Invariants

These are the easiest runtime rules to break by accident:

- Release is routed by physical key position, not by the current layer's live
  keycode mapping.
- A slot may temporarily carry both an active press owner and an in-slot
  same-key multi-tap chain, but the global `pending_multi_tap` index must only
  contain inactive deferred chains. Foreign-press flushing is allowed to target
  only those inactive chains.
- `handled_key_resolution_t` is the authored handled-key resolution contract,
  while `key_runtime_slot_interaction_t` is the slot-owned cached interaction
  contract. Keep that boundary explicit and prefer
  [`handled_key_materialize(...)`](../users/noah/lib/key/interaction/handled_key.h)
  plus
  [`key_runtime_slot_interaction_from_materialized(...)`](../users/noah/lib/key/runtime/key_runtime_interaction.h)
  when you need a synthetic slot interaction instead of re-deriving release or
  hold policy by hand.
- `key_runtime_slot_interaction(...)` and
  `key_runtime_slot_cached_interaction(...)` both expose the slot-owned
  interaction contract. Test/debug seams that need a slot-shaped value should
  prefer the materialization helpers above instead of open-coding interaction
  fields.
- Feedback and debug readers should prefer cached slot semantic metadata over
  re-running handled-key resolution against mutable slot state.
- New emitters should prefer the explicit helpers in
  [`action_dispatch.h`](../users/noah/lib/action/action_dispatch.h) so they
  state whether they settle pending fallback holds or preserve keyboard mod
  state. If a helper needs to ignore specific ambient modifiers for one
  synthetic QMK tap, prefer
  `noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(...)` over manual
  mod suspend/apply sequences in the caller.
- `action_dispatch()` now exists as the compatibility wrapper for the
  runtime-default authored tap path. New code should not treat it as the only
  output seam.
- Slot reset should flow through `key_runtime_slot_reset()` for individual slots
  or [`noah_runtime_reset_for_test()`](../users/noah/lib/state/runtime/runtime_reset.h)
  for whole-runtime host resets so default timing and semantic sentinels stay
  valid.
- Authored momentary layers are owned through `layer_ownership.c`, not raw
  `layer_on()` and `layer_off()` calls in the key runtime.
- Unusual pd-mode side effects belong in the pd-mode lifecycle seam owned by
  the `pd_mode_def_t` row, not in ad hoc key-runtime branches or registry
  mode-selection switches.

## Where To Change What

If you are changing one of these categories, start here:

- resolved handled-key semantics:
  [`handled_key.h`](../users/noah/lib/key/interaction/handled_key.h),
  [`handled_key_lookup.c`](../users/noah/lib/key/interaction/handled_key_lookup.c),
  [`handled_key_materialize.c`](../users/noah/lib/key/interaction/handled_key_materialize.c),
  [`handled_key_defaults.c`](../users/noah/lib/key/interaction/handled_key_defaults.c),
  and [`handled_key_resolution_accessors.c`](../users/noah/lib/key/interaction/handled_key_resolution_accessors.c)
- slot interaction materialization and cached release semantics:
  [`key_runtime_interaction.h`](../users/noah/lib/key/runtime/key_runtime_interaction.h)
- outer event orchestration:
  [`key_runtime_process.c`](../users/noah/lib/key/runtime/key_runtime_process.c),
  [`key_runtime_preflight.c`](../users/noah/lib/key/runtime/key_runtime_preflight.c),
  [`key_runtime_press.c`](../users/noah/lib/key/runtime/key_runtime_press.c),
  [`key_runtime_release.c`](../users/noah/lib/key/runtime/key_runtime_release.c), and
  [`key_runtime_scan.c`](../users/noah/lib/key/runtime/key_runtime_scan.c)
- public cross-module key-runtime entry points:
  [`key_runtime_api.h`](../users/noah/lib/key/runtime/key_runtime_api.h)
- slot press/release/scan behavior:
  [`key_runtime_slot_step.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_step.c),
  [`key_runtime_slot_press_reduce.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c),
  [`key_runtime_slot_policy.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c),
  [`key_runtime_slot_release_reduce.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_reduce.c),
  [`key_runtime_slot_release_active.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c),
  [`key_runtime_slot_release_resolver.h`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_resolver.h),
  [`key_runtime_slot_scan_reduce.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.c),
  and
  [`key_runtime_slot_pending_multi_tap.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c)
- new side-effect kind:
  [`key_runtime_effect.h`](../users/noah/lib/key/runtime/effects/key_runtime_effect.h),
  [`key_runtime_effect_queue.h`](../users/noah/lib/key/runtime/effects/key_runtime_effect_queue.h),
  [`key_runtime_slot_result.c`](../users/noah/lib/key/runtime/slot/key_runtime_slot_result.c),
  and
  [`key_runtime_transition.c`](../users/noah/lib/key/runtime/key_runtime_transition.c)
- output-emission policy:
  [`action_dispatch.h`](../users/noah/lib/action/action_dispatch.h),
  [`action_dispatch.c`](../users/noah/lib/action/action_dispatch.c),
  [`action_lifecycle.c`](../users/noah/lib/action/action_lifecycle.c),
  [`owned_keycode.c`](../users/noah/lib/action/owned_keycode.c), and
  [`keyboard_mod_state.c`](../users/noah/lib/state/runtime/keyboard_mod_state.c)
- new hidden runtime state:
  one explicit ownership module plus the public observation/reset seams in
  [`runtime_debug.h`](../users/noah/lib/state/runtime/runtime_debug.h) and
  [`runtime_reset.h`](../users/noah/lib/state/runtime/runtime_reset.h).
  Internal aggregate composition lives in
  [`runtime_shared_state_internal.h`](../users/noah/lib/state/runtime/runtime_shared_state_internal.h)
  and the pd-mode storage slice in
  [`pd_mode_runtime_shared_state_internal.h`](../users/noah/lib/pointing/runtime/pd_mode_runtime_shared_state_internal.h).
- new runtime translation units or build wiring:
  [`users/noah/source_manifest.mk`](../users/noah/source_manifest.mk) plus
  `sh tests/host/run_feature_gate_compile_tests.sh`
- cross-subsystem runtime tracing:
  [`runtime_trace.h`](../users/noah/lib/state/runtime/runtime_trace.h),
  [`runtime_trace.c`](../users/noah/lib/state/runtime/runtime_trace.c), and
  [`key_runtime_trace.c`](../users/noah/lib/key/runtime/key_runtime_trace.c)

If a change crosses those boundaries, it is probably architectural enough to
deserve updates to this doc and to the active review log in `review/`.

## Validation And Compile Gates

Two post-init validation layers protect authored runtime data before normal
use:

- [`key_behavior_validate_all()`](../users/noah/lib/key/interaction/key_behavior_lookup.c)
  checks duplicate `key_behaviors[]` rows, unsupported handled-key keycodes,
  unsupported authored tap/hold actions, and invalid
  `REPEAT_WHILE_HELD(...)` rates
- [`noah_keymap_validate()`](../users/noah/lib/key/interaction/keymap_validation.c)
  checks unsupported raw layer actions in `keymaps[][]`, unsupported raw
  layer-action combo outputs, and unreachable `key_behaviors[]` rows that are
  not referenced by either `keymaps[][]` or combo outputs

Those validations run during `noah_keyboard_post_init_user()`, after
`macro_dispatch_validate_all()` and before VIA default post-init work, RGB
runtime init, and split-sync init.

The most direct host checks for that surface are:

- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

## Debugging And Tests

For higher-level debugging, use the live query helpers in
[`runtime_debug.h`](../users/noah/lib/state/runtime/runtime_debug.h) for
key-runtime state inspection and
[`runtime_reset.h`](../users/noah/lib/state/runtime/runtime_reset.h) for
whole-runtime test resets instead of rebuilding partial reset logic inside
individual tests.

If you need to chase ordering across the key runtime, pd modes, layer
ownership, and split sync, enable `NOAH_RUNTIME_TRACE_ENABLE`. The shared
trace ring buffer in
[`runtime_trace.h`](../users/noah/lib/state/runtime/runtime_trace.h) is included in
its own trace snapshot API and reset with the rest of the runtime.
That buffer now records not only transition plans and executed effects, but
also typed key-runtime decision events for release resolution, hold-policy
selection, and pending multi-tap reuse/flush decisions.

If you also want the verbose console-side key-runtime trace strings, enable
`NOAH_KEY_RUNTIME_TRACE_ENABLE` with `CONSOLE_ENABLE`. That surface lives in
[`key_runtime_trace.c`](../users/noah/lib/key/runtime/key_runtime_trace.c) and
complements the shared ring buffer instead of replacing it.

The scenario harness in
[`tests/host/key_runtime_scenario_harness.h`](../tests/host/key_runtime_scenario_harness.h)
and
[`tests/host/key_runtime_scenario_harness.c`](../tests/host/key_runtime_scenario_harness.c)
now records the shared [`key_runtime_effect_t`](../users/noah/lib/key/runtime/effects/key_runtime_effect.h)
payloads directly and resets runtime state through
[`noah_runtime_reset_for_test()`](../users/noah/lib/state/runtime/runtime_reset.h).
Scenario and integration helpers that need slot state should call the live
`runtime_debug.h` query helpers directly instead of rebuilding aggregate
snapshots inside test harness code.
Keep new scripted scenarios on that shared surface instead of adding a
test-only effect or reset dialect.

The most relevant host checks for the runtime are:

- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

If you changed shared trace/debug plumbing, also run:

- `sh tests/host/run_runtime_trace_tests.sh`

If you changed output-emission policy or direct-action lifecycle wiring, also run:

- `sh tests/host/run_owned_keycode_tests.sh`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`

When runtime wiring or behavior changes, finish with:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Related Docs

- [README.md](../README.md): top-level userspace overview
- [INTERACTION_MODEL.md](./INTERACTION_MODEL.md): user-facing tap/hold/multi-tap semantics
- [KEYMAP.md](./KEYMAP.md): current authored profile
- [ADDING_PD_MODE.md](./ADDING_PD_MODE.md): pd-mode extension surface
