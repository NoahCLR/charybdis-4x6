# Key Runtime

This document is the maintainer-facing map for the handled-key runtime under
[`users/noah/lib/key/`](../users/noah/lib/key/).

Use this when you are changing runtime behavior, tests, or debug surfaces. For
the wider userspace architecture, start with
[architecture/README.md](./architecture/README.md) and
[architecture/runtime-flow.md](./architecture/runtime-flow.md). For user-facing
semantics, see [INTERACTION_MODEL.md](./INTERACTION_MODEL.md). For the authored
profile, see [KEYMAP.md](./KEYMAP.md).

## How To Use This Doc

Use this as a routing map when a behavior bug crosses press, release, scan,
ownership, split, or RGB feedback boundaries.

Skip this file for ordinary profile edits. If you are changing what one key,
combo, macro, layer, or RGB table does, start with `keymap.c`,
`rgb_config.c`, [INTERACTION_MODEL.md](./INTERACTION_MODEL.md), or
[RGB_CONFIG.md](./RGB_CONFIG.md) instead. Runtime changes should usually land
only after the authored-data path is not enough.

## Current Shape

The key runtime is now a single-authority reducer-owned system.

- Authored behavior resolution starts in `key/behavior/`.
- Reducer-owned state now lives in `key/runtime/reducer/`, with planning,
  projection, queue mechanics, slot helpers, and core trace helpers split into
  adjacent role-based packages.
- `users/noah/lib/key/runtime/` is now thin orchestration around reducer entry
  points, effect transport, and QMK hook integration.
- Long-lived external ownership still lives in the dedicated registries under
  `state/ownership/` and `key/ownership/`.
- The reducer-owned code uses the `key_runtime_core_*` symbol family.

The legacy slot reducers, slot result transport, slot/index shared state, and
stub-backed mixed-runtime host surfaces were removed during the full cutover.

## Reducer Packages vs Integration

The key runtime now lives under one permanent tree, but that tree still has
two architectural layers.

- [`users/noah/lib/key/runtime/reducer/`](../users/noah/lib/key/runtime/reducer/)
  is the reducer/state-owner layer. It holds the canonical runtime state,
  ownership mechanics, and read-only state query surface.
- [`users/noah/lib/key/runtime/planning/`](../users/noah/lib/key/runtime/planning/)
  owns release, scan, tap-series, and effect-plan construction.
- [`users/noah/lib/key/runtime/projection/`](../users/noah/lib/key/runtime/projection/)
  applies planned effects to QMK-facing registries and captures projection
  snapshots.
- [`users/noah/lib/key/runtime/queue/`](../users/noah/lib/key/runtime/queue/)
  owns pending-release queue mechanics.
- [`users/noah/lib/key/runtime/slot/`](../users/noah/lib/key/runtime/slot/)
  owns packed key-position and slot interaction helpers.
- The top-level files in
  [`users/noah/lib/key/runtime/`](../users/noah/lib/key/runtime/) are the
  QMK-facing integration layer. They own process/scan entry flow, preflight,
  effect-plan transport, and trace/debug adapters.
- The old slot/index runtime is no longer a live production subsystem. Release
  semantics now live behind the core release planner instead of a `slot/`
  helper.

So when you see both layers, read that as "decision layer plus integration
layer," not "old runtime plus new runtime running side by side."

## Design Rules

- Runtime authority is by physical key position, not by the keycode currently
  visible on the active layer.
- `key_runtime_core` is the only source of truth for active presses, tap series,
  reducer-owned leases, persistent lock intents, pending release dispatches,
  and shadow projection state.
- The runtime plans effects first and projects them second. Runtime logic does
  not reach into QMK side effects ad hoc.
- Authored behavior remains keymap-owned. Shared runtime policy remains under
  `users/noah/`.

## Main Components

| File | Responsibility |
| --- | --- |
| [`handled_key.h`](../users/noah/lib/key/behavior/handled_key.h), [`handled_key_lookup.c`](../users/noah/lib/key/behavior/handled_key_lookup.c), [`handled_key_materialize.c`](../users/noah/lib/key/behavior/handled_key_materialize.c) | Resolve authored behavior into `handled_key_resolution_t` and materialize it into runtime interaction contracts. |
| [`slot/slot_interaction.h`](../users/noah/lib/key/runtime/slot/slot_interaction.h) | Shared interaction contract cached by the reducer after authored behavior materialization. |
| [`reducer/runtime.h`](../users/noah/lib/key/runtime/reducer/runtime.h), [`reducer/runtime.c`](../users/noah/lib/key/runtime/reducer/runtime.c) | Single-authority runtime state type, reducer entry points, press/tap orchestration, and scan orchestration. |
| [`planning/effect_plan.h`](../users/noah/lib/key/runtime/planning/effect_plan.h), [`planning/effect_plan.c`](../users/noah/lib/key/runtime/planning/effect_plan.c) | Internal effect-plan initialization, sink buffering, append helpers, tap-commit feedback filtering, and release-plan transfer shared by planning modules. |
| [`projection/feedback_projection.h`](../users/noah/lib/key/runtime/projection/feedback_projection.h), [`projection/feedback_projection.c`](../users/noah/lib/key/runtime/projection/feedback_projection.c) | Feedback pulse projection and pulse queueing for key-runtime effects. |
| [`reducer/ownership_state.h`](../users/noah/lib/key/runtime/reducer/ownership_state.h), [`reducer/ownership_state.c`](../users/noah/lib/key/runtime/reducer/ownership_state.c) | Reducer-owned lease and persistent-intent mechanics, shadow projection recomputation, held/repeat feedback visibility queries, and lock observation updates over `key_runtime_core_state_t` storage. |
| [`projection/pd_projection.h`](../users/noah/lib/key/runtime/projection/pd_projection.h), [`projection/pd_projection.c`](../users/noah/lib/key/runtime/projection/pd_projection.c) | Key-runtime PD projection for held-action preemption, PD lock-tap effects, and explicit lock-state requests; actual PD mode state remains PD-runtime-owned. |
| [`queue/pending_release_queue.h`](../users/noah/lib/key/runtime/queue/pending_release_queue.h), [`queue/pending_release_queue.c`](../users/noah/lib/key/runtime/queue/pending_release_queue.c) | Pending-release queue mechanics: allocation, ordering, drain snapshots, and released-token pending-emission markers over `key_runtime_core_state_t` storage. |
| [`projection/projection.h`](../users/noah/lib/key/runtime/projection/projection.h), [`projection/projection.c`](../users/noah/lib/key/runtime/projection/projection.c) | Runtime effect execution into QMK-facing registries, pending-release dispatch projection, projection snapshot capture/comparison, and trace projection checkpoints. |
| [`planning/release_planner.h`](../users/noah/lib/key/runtime/planning/release_planner.h), [`planning/release_planner.c`](../users/noah/lib/key/runtime/planning/release_planner.c) | Shared release decision contract, active-release resolution, pending multi-tap release resolution, and release effect planning. |
| [`planning/scan_planner.h`](../users/noah/lib/key/runtime/planning/scan_planner.h), [`planning/scan_planner.c`](../users/noah/lib/key/runtime/planning/scan_planner.c) | Scan-time active hold promotion, release-hold-pending marking, fallback hold settlement, and pending multi-tap scan planning over core state. |
| [`reducer/state_query.h`](../users/noah/lib/key/runtime/reducer/state_query.h), [`reducer/state_query.c`](../users/noah/lib/key/runtime/reducer/state_query.c) | Core state inspection and blocker-query surface for debug, feedback, projection snapshots, preflight, and deferred-release transport. |
| [`planning/tap_series.h`](../users/noah/lib/key/runtime/planning/tap_series.h), [`planning/tap_series_flush.c`](../users/noah/lib/key/runtime/planning/tap_series_flush.c) | Internal tap-series helpers for pending multi-tap flush resolution, delayed action completion, and foreign/global multi-tap flush planning. |
| [`deferred_release.h`](../users/noah/lib/key/runtime/deferred_release.h), [`deferred_release.c`](../users/noah/lib/key/runtime/deferred_release.c) | Adapter that defers blocked release dispatch effects into the core pending-release queue and drains queued dispatches after release/scan execution. |
| [`keyboard_mod_policy.h`](../users/noah/lib/state/modifiers/keyboard_mod_policy.h), [`keyboard_mod_policy.c`](../users/noah/lib/state/modifiers/keyboard_mod_policy.c) | Shared modifier snapshot, filtering, preservation-window, masked-emit, action-replay, and real-mod masking policy used by action dispatch, key-runtime processing, tap-series capture, deferred release, and PD mode modifier masking. |
| [`process.c`](../users/noah/lib/key/runtime/process.c) | `process_record_user()` entry flow, preflight ordering, release-keycode recovery, and non-handled release finalization. |
| [`preflight.c`](../users/noah/lib/key/runtime/preflight.c) | Cross-key interruption and default-suppression work before the current press proceeds, while unrelated pending multi-tap chains stay position-owned until timeout or same-key reuse. |
| [`press.c`](../users/noah/lib/key/runtime/press.c), [`release.c`](../users/noah/lib/key/runtime/release.c), [`scan.c`](../users/noah/lib/key/runtime/scan.c) | Thin press/release/scan orchestration around reducer-owned effect plans. |
| [`transition.c`](../users/noah/lib/key/runtime/transition.c), [`transition.h`](../users/noah/lib/key/runtime/transition.h) | Effect-plan transport between reducer decisions and concrete effect projection through `projection/projection.c`. |
| [`planning/effect.h`](../users/noah/lib/key/runtime/planning/effect.h), [`planning/effect_queue.h`](../users/noah/lib/key/runtime/planning/effect_queue.h) | Shared runtime effect vocabulary and queue helper. |
| [`held_action.c`](../users/noah/lib/key/ownership/held_action.c), [`held_repeat.c`](../users/noah/lib/key/ownership/held_repeat.c), [`layer_ownership.c`](../users/noah/lib/state/ownership/layer_ownership.c), [`keyboard_mod_ownership.c`](../users/noah/lib/state/ownership/keyboard_mod_ownership.c) | External ownership registries projected by runtime effects. |
| [`pd_mode_key_runtime_bridge.h`](../users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.h), [`pd_mode_key_runtime_bridge.c`](../users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c) | Narrow PD-to-key-runtime observer bridge for changed local PD lock state. |
| [`runtime_debug.h`](../users/noah/lib/state/diagnostics/runtime_debug.h), [`runtime_reset.h`](../users/noah/lib/state/shared/runtime_reset.h), [`runtime_trace.h`](../users/noah/lib/state/diagnostics/runtime_trace.h) | Public debug, reset, and tracing seams used by host tests and runtime diagnostics. |

## Reducer-Owned State

`key_runtime_core` owns these runtime shapes:

- `press_token_t`: immutable press identity plus live phase, authored
  interaction contract, and release-time facts for one physical key.
- `tap_series_t`: pending multi-tap chain state separated from the active press
  lifetime.
- `lease_t`: reducer-owned temporary ownership for layers, modifiers, held
  actions, repeats, pd modes, and pointer anchors. Slots are stored in
  `key_runtime_core_state_t` and managed by `reducer/ownership_state.c`.
- `persistent_intent_t`: lock-like state that survives a single press lifetime,
  such as layer locks, pd-mode locks, and pointer toggles. Slots are stored in
  `key_runtime_core_state_t` and managed by `reducer/ownership_state.c`.
- `pending_release_t`: deferred release dispatches that must drain in authored
  order after blockers clear. Compact slots are linked by bounded array index,
  with explicit head and tail indices in `key_runtime_core_state_t`; no
  wrapping clock participates in queue order. The mechanics live in
  `queue/pending_release_queue.c`.
- `key_runtime_core_shadow_projection_t`: reducer-owned projected view used by
  blocking queries, debug snapshots, and overlap reasoning.

Press-token ID zero is reserved as “no owner.” Allocation wraps explicitly
from `0xFFFF` to `1` and rejects any candidate still referenced by an active
press, a retained deferred-release token, an active lease, or an active
pending-release slot. Allocation happens before the press mutates token,
series, interruption, count, or lease state. The theoretically exhausted path
increments `core_token_allocation_failure_count`; a handled press is consumed
without effects, while non-handled QMK processing remains unowned. Any new
owner-bearing reducer store must join this liveness scan and its host test.

If a future change needs new runtime state, it belongs in `key_runtime_core` unless
it is purely an external ownership registry or a stateless authored-behavior
helper.

Pending multi-tap state is owned by `tap_series_t` inside `key_runtime_core`,
while pending multi-tap release decisions are owned by
`planning/release_planner.c`.
Do not add a second state machine for multi-tap sequencing; new behavior should
extend the core reducer and its release/scan planning tests.

Preview-owner queries use the reducer's active press count and bitmap, visiting
active slots in ascending matrix order so the lowest eligible position still
wins. An empty press set visits no slots, even when a released multi-tap series
remains. Resolve coordinates only for eligible active candidates, directly from
the slot index. This query runs for both base and combo split feedback on every
tick. Keep the display-preview bridge and immediate combo polling outside this
shortcut: their state can change without an active preview owner or a dirty
notification. Real-profile integration tests cover sparse traversal and owner
order; runtime-debug and split-sync tests cover bridge timing and idle combo
activation.

## Ownership Authority Map

Runtime ownership is intentionally split between reducer-owned intent and
QMK-facing applied registries. Use these labels when changing runtime behavior:

- Authoritative: the source of truth for key-runtime decisions.
- Projected: an applied registry or hardware-facing state sink updated from
  reducer effects.
- Compatibility-only: a bridge that repairs or normalizes upstream QMK/fork
  behavior without becoming key-runtime ownership truth.

| Runtime fact | Authoritative owner | Projected or compatibility surface | Write direction and guardrail |
| --- | --- | --- | --- |
| Physical press identity and active key phase | `press_token_t` in `key_runtime_core` | debug and trace snapshots | Physical events are observed into core first; other registries must not create or mutate press tokens. Nonzero IDs are allocated against every live owner store before press mutation, with exhaustion exposed in the projection snapshot. Covered by runtime debug's production and tiny-domain builds, key runtime scenario, release matrix, and integration harness tests. |
| Pending multi-tap chain state | `tap_series_t` in `key_runtime_core` | `planning/tap_series_flush.c` plans explicit flushes; `planning/scan_planner.c` owns scan-time thresholds; release planner reads the state | Core stores the chain; `planning/tap_series_flush.c` flushes expired or foreign chains, `planning/scan_planner.c` resolves scan-time hold/flush outcomes, and `planning/release_planner.c` resolves release decisions over it. Covered by release matrix, scenario, runtime debug, and integration harness tests. |
| Release semantics | `planning/release_planner.c` | `deferred_release.c` adapts blocked dispatches into the core pending-release queue | Planner owns quick release, fallback suppression, buffered base tap, active releases, and pending multi-tap releases; adapters must not re-decide those semantics. |
| Pending release dispatch queue | Index-linked `pending_release_t` slots in `key_runtime_core`, with mechanics in `queue/pending_release_queue.c` | `deferred_release.c`, `release.c`, and `scan.c` drain through the adapter | Queue storage stays core-owned because blockers are press-token facts. Head/tail linkage defines FIFO order independently of uptime; one unlink path owns reconnection, count changes, and pending-emission token cleanup. Covered directly by the pending-release queue runner and by release matrix/runtime debug integration tests. |
| Runtime inspection and blocker queries | `key_runtime_core_state_t` read through `reducer/state_query.c` | `debug.c`, `feedback.c`, projection snapshots, preflight, and deferred release transport | Query code reads reducer-owned state and reports derived facts; it must not mutate press, tap, lease, or pending-release storage. Covered by runtime debug, release matrix, scenario, trace, and integration harness tests. |
| Temporary held ownership intent for held actions, repeats, momentary layers, managed modifiers, pd holds, and pointer anchors | `lease_t` in `key_runtime_core`, with mechanics in `reducer/ownership_state.c` | `held_action.c`, `held_repeat.c`, `layer_ownership.c`, `keyboard_mod_ownership.c`, `pd_mode_state.c`, and pointer layer policy | `projection/projection.c` writes outward from planned effects; applied registries perform QMK, action, repeat, layer, modifier, or pd-mode side effects. Those registries must not mint independent key-runtime leases. |
| Physical and managed literal report ownership | `owned_keycode.c` physical/managed usage counts plus `keyboard_mod_ownership.c` modifier counts | `process.c` settles default-handler report ownership in the finalize hook, once the event result says whether QMK will register the usage at all; `preflight.c` suppresses a default transition while a managed owner keeps the same report component live | A press userspace consumes reaches no default handler and therefore owns nothing in the report, so it neither blocks a managed owner of the same usage from registering it nor blocks the last managed modifier release from clearing the report bit. `keyboard_mod_ownership.c` keeps its physical refcounts for the separate "this modifier key is down" question that masking policy asks, and gates report teardown on its report refcounts. Managed basic, system, consumer, and mouse usages use owner-scoped `owned_keycode_lease_t` values. QMK register/unregister calls occur only on aggregate zero-to-one and one-to-zero transitions. Modded actions preflight every component before mutation and delegate modifier counts to `keyboard_mod_ownership.c`. Persistent held actions, macro holds/chords, and PD arrow selection retain their exact leases. `run_owned_keycode_tests.sh` also rejects new raw or unscoped mutation callers outside the action-dispatch compatibility boundary. |
| Lock-like runtime intent | `persistent_intent_t` in `key_runtime_core`, with mechanics in `reducer/ownership_state.c` | `layer_ownership.c` and `pd_mode_state.c` apply the actual layer or pd-mode lock | Current accepted bridge points are `layer_ownership_set_lock_state()`, the one-shot layer's `layer_ownership_oneshot_*()` (a `PERSISTENT_INTENT_KIND_LAYER_ONESHOT` through `key_runtime_core_layer_oneshot_set()`), and `pd_mode_key_runtime_bridge_observe_local_lock_state()`, which update external state and then refresh core shadow state. `run_feature_gate_compile_tests.sh` enforces those bridge directions. Treat new two-way lock writes as architecture work, not local fixes. |
| Physical keyboard modifier observation and replay filtering | QMK live modifier state plus `keyboard_mod_ownership.c` physical refcounts; `keyboard_mod_policy.c` owns shared snapshot/filter/replay helpers and preservation windows | core shadow projection stores physical and managed masks for overlap reasoning | `process.c` observes physical modifier events in pre-process so masking policy can tell a user-held modifier from a mode-owned one, and preflight may suppress default release; action dispatch, delayed action replay, tap-series capture, deferred release, and PD mode modifier masking use `keyboard_mod_policy.h` instead of local snapshot/filter/preserve logic. QMK remains the live report sink. Covered by keyboard mod ownership, action dispatch, delayed action, modifier-hold, and PD-mode integration tests. |
| PD runtime local, display, remote, and split state | `pd_mode_state.c` and split sync runtime | key-runtime leases and persistent intents request local pd behavior | Key runtime may request PD transitions through projected effects; PD runtime owns actual mode state and snapshots. Changed local PD lock state is observed into core through `pd_mode_key_runtime_bridge.c`. |
| Feedback pulse lifecycle | feedback pulse fields in `key_runtime_core` | `projection/feedback_projection.c`, `key_feedback_pulse_observe()`, and RGB/split feedback snapshots | Core state remains authoritative; `projection/feedback_projection.c` queues key-runtime pulse effects and feedback/RGB surfaces render the projection. Covered by runtime debug, split sync, and RGB render tests. |
| Combo origin recovery | `compat/qmk_combo_origin.c` plus `origin_registry.c` | key runtime, PD mode, RGB, and split feedback consume normalized origins | Compatibility-only. It repairs QMK combo records and owns a bounded mirror of pending/active QMK combo origins, keyed by combo index and physical completion generation. Suppressed candidates retire immediately; inactive candidates get one final `combo_task()` opportunity after their legal deadline before expiry. It must not become an owner of key-runtime press, lease, or release state. Covered by combo origin, init order, QMK contract, PD mode, RGB render, split sync, and real profile integration tests. |

When a future change needs to touch both core state and one of the projected
registries, update the core plan first and project outward through an explicit
effect or bridge. If the projected registry has to write back into core, document
the bridge here and cover it with projection or ownership tests in the same pass.

## Combo Origin Compatibility Contract

`compat/qmk_combo_origin.c` is a QMK/fork adapter, not a runtime authority. It
exists because QMK combo records can arrive as `COMBO_EVENT` at `(0,0)`, while
userspace needs the physical owner key and full combo footprint for runtime
feedback, PD owner bitmaps, RGB combo feedback, and split sync.

This adapter is allowed to mirror only the QMK combo facts needed to repair that
origin:

- live physical member key state observed before combo normalization
- `key_combos[]` and `noah_combo_count`
- QMK active/disabled combo state, including the `EXTRA_SHORT_COMBOS` state-bit
  representation
- combo keycode lookup through `COMBO_ONLY_FROM_LAYER` or `combo_ref_from_layer`
- active and pending combo-output caches needed when QMK emits the combo record
  after member release; entries are identified by combo index and physical
  completion generation rather than output keycode alone
- scan-boundary reconciliation of QMK active/disabled state, with immediate
  suppression retirement and a two-observation deadline rule that leaves
  `combo_task()` one final legal emission opportunity. QMK marks a fired combo
  both active and disabled, so only a disabled completion that was never seen
  active counts as suppressed
- conservative capacity refusal and snapshot diagnostics; a full cache never
  overwrites a still-awaiting origin
- fallback owner recovery from the latest or last physical combo member, using a
  full-keyboard bitmap when no exact footprint can be proven

The adapter targets the BK revision pinned in `qmk-pin.json`. That QMK activates
a combo before emitting its press; pending candidates are selected through
those active indices. It does not support forks that emit before exposing
active state. QMK's tapping queue can deliver a fired combo's press after its
release has deactivated it (the chord was tapped while a native LT/MT was
undecided). A completion seen firing therefore keeps its exact footprint until
its press arrives: a press with no active candidate pairs with the oldest fired
completion for that keycode, since queued outputs leave QMK in firing order.
A fired completion that never arrives retires one tapping term after it was
seen firing, the longest a tap-hold decision queued ahead of it can take.
Conservative owner recovery remains only for a completion never seen firing.

It must not create or mutate key-runtime press tokens, tap series, release
decisions, leases, layer locks, modifier ownership, or PD mode ownership. Its
only handoff into the runtime ownership model is the normalized event key and
origin bitmap stored in `origin_registry.c`.

For lighting, `feedback.c` snapshots current momentary bindings from
`layer_ownership.c` and passes an owner bitmap to the adapter's active-footprint
partition. Entries with those owners contribute to neither combo substage.
This is a visual filter only: their origins still match releases and their
layer ownership remains intact. Pending behavior outputs keep combo feedback
until their resolved hold establishes a binding; locks and armed one-shots
alone do not qualify. Filtering precedes footprint union so overlapping active
combos keep their own lighting. See [the feedback model](rgbflow.md#combo-identity-during-a-layer-hold).

Coverage for this contract lives in `run_qmk_combo_origin_tests.sh`: reference
layer lookup, stable combo owner selection, active and pending combo bitmaps,
pending output after member release, suppressed overlap, deadline boundaries
and timer wrap, capacity refusal/recovery, exact same-output generations,
cached release footprints, fired outputs delivered after deactivation (in
firing order, retired after one tapping term), cross-half and three-key combos,
reset diagnostics,
and preview/PD owner partitioning and quiet-owner exclusion for RGB
underlay/overlay feedback. The runner
executes normal and `EXTRA_SHORT_COMBOS` layouts and compile-checks the timerless
branch. `run_qmk_contract_checks.sh` compares both combo layouts with the pinned
fork and enforces the upstream pre-hook, matrix-scan, and `combo_task()` order.
`run_qmk_gesture_pipeline_tests.sh` drives the real QMK combo and tapping
engines through a chord tapped behind an undecided native LT and requires the
exact two-key footprint.

## End-To-End Flow

### 1. Physical key event entry

[`process.c`](../users/noah/lib/key/runtime/process.c)
observes every physical event into `key_runtime_core` first.

That observation step gives the reducer position-stable press/release identity
before any QMK path, macro path, or pd-mode path narrows the event.
The pre-process hook also updates physical literal-key and modifier ownership
before QMK can mutate its report, which lets preflight suppress a duplicate
press or premature release when a managed lease already owns that component.

### 2. Preflight

[`preflight.c`](../users/noah/lib/key/runtime/preflight.c)
does the cross-key work that must happen before the current press resolves:

- suppress default literal-key or modifier handling when aggregate ownership
  requires it
- interrupt other active handled keys on foreign press
- leave unrelated pending multi-tap chains live until their own timeout or
  same-key continuation resolves them

Behavior change note: the preflight path changed on `2026-04-20`. Before that
change, a foreign press explicitly flushed unrelated pending multi-tap chains.
The current runtime intentionally keeps those chains position-owned until they
resolve themselves. If that behavior changes later, treat it as a regression
unless the tests and docs are updated together; the locking checks are
`sh tests/host/run_key_runtime_scenario_tests.sh` and
`sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`.

### 3. Press routing

Handled presses go through [`press.c`](../users/noah/lib/key/runtime/press.c),
which asks `key_runtime_core` for a press effect plan and executes it through
[`transition.c`](../users/noah/lib/key/runtime/transition.c) and
[`projection/projection.c`](../users/noah/lib/key/runtime/projection/projection.c).

The reducer owns:

- press-token creation and replacement
- same-key multi-tap reuse
- foreign active-key interruption
- independent pending-multi-tap retention across foreign presses
- same-locked-pd-mode unlock requests before momentary held-action registration
- press-time lease activation

### 4. Release routing

Handled releases go through [`release.c`](../users/noah/lib/key/runtime/release.c).
The reducer resolves release by physical key position, not by the raw release
keycode currently visible to QMK.

The release path now owns:

- active release resolution
- pending-multi-tap release resolution
- release-time lease cleanup
- suppression of release-time fallback or lock retoggle after a same-mode pd
  lock was consumed on press
- pending release dispatch queueing
- token retirement and pending-series seeding

Press and release planning return before tracing/projection begins, so their
planner-local storage is not live beneath action emission. Release planning,
tracing/projection, and deferred draining are deliberately sequential stack
phases. `process.c` starts the deferred drain only after the handled-release
helper has returned, so the release plan is no longer live under deferred
projection.

Non-handled releases still pass through the shared process flow, but
`process.c` now finalizes any reducer-owned observed state for
those keys too. That keeps raw ownership keys such as `MO()`/modifier/pd-mode
keys from leaving stale core leases behind.

For runtime-owned MT keys, the release planner's resolved TAP outcome also
plans `KEY_RUNTIME_EFFECT_LAYER_ONESHOT_CONSUME` when its selected tap action
qualifies. Projection applies that effect through `layer_ownership_oneshot_consume()`
and the existing `key_runtime_core_layer_oneshot_set()` bridge. This occurs on
release, including a release preserving a pending multi-tap chain, before any
tap dispatch. The press-layer action is already materialized and stays fixed.
Hold outcomes never plan consumption; scan, deferred and synthetic emission
never repeat it. `process.c` settles native MT presses from QMK's tap count and
leaves runtime-owned MTs to their planner. The compatibility helper in
`compat/qmk_oneshot_contract.h` qualifies selected output, not gesture timing.
The layer-lock integration regression and real QMK gesture pipeline cover
consumption, transparency, repeats, modifier-only output and new one-shots
surviving later emission.

### 5. Scan

[`scan.c`](../users/noah/lib/key/runtime/scan.c) asks
`key_runtime_core` for the current scan plan and then drains pending release
dispatches.

Each explicit drain transports at most four records from the queue's entry-time
snapshot. Records remain FIFO ordered by an explicit index-linked list. A
release whose owner is still active stays linked in place while the drain may
take the first eligible later record; when that owner becomes eligible, its
original relative order is unchanged. Matching completion removes the oldest
matching record, and owner pending state clears only after that owner's final
record is unlinked. Synchronous drain re-entry is ignored, and records enqueued
during projection remain pending for a later release or scan boundary. The
queue itself keeps its full configured capacity; only the automatic transport
batch is bounded.

The reducer scan path owns:

- threshold hold and long-hold promotion through `planning/scan_planner.c`
- pending multi-tap expiry and delayed action flush through `planning/scan_planner.c`
- scan-time release blocker clearing

## Mod-tap modifier representation

Modifier masks in reducer leases and shadow projections are eight-bit keyboard
report masks. QMK's MT modifier field is a five-bit encoding: the low nibble
names Ctrl/Shift/Alt/GUI and bit 4 selects the right-hand bank. Thus
`MT(MOD_RCTL, KC_Q)` encodes `0x11` but holds report bit `0x10`.
The existing `compat/qmk_mod_contract.h` owns that conversion through
`noah_qmk_mods_to_report_mask()`, shared by the shadow lease path and applied
mod-tap physical tracking. It interprets encoding only; QMK still owns Magic
remapping and native tapping policy.

`run_key_runtime_physical_ownership_integration_tests.sh` compares shadow
report/managed masks with the real applied modifier ledger for every nonempty
left/right modifier subset, native taps/holds and authored holds, through
release and quiescence. Its synchronous delivery timestamps require scan-time
hold promotion. `run_qmk_gesture_pipeline_tests.sh` covers native delayed
QMK tapping delivery and sparse authored rows that retain their intrinsic MT
hold. That gesture harness stubs the applied ledger; it proves delivery and
shadow state, while the physical-ownership runner proves ledger agreement.
`run_qmk_contract_checks.sh` compares all 32 MT fields with the pinned QMK
action representation, including empty fields. These checks establish shadow
consistency, not a prior host modifier output fault.

## Debugging Expectations

When you inspect runtime state, prefer the core debug surface:

- `key_runtime_core_press_token_at(...)`
- `key_runtime_core_tap_series_at(...)`
- `key_runtime_core_projection_snapshot_capture()`
- `key_runtime_core_shadow_projection()`

Do not reintroduce slot/index mirrors for debug convenience. If a debug view is
missing, add it to the core surface.

## Verification

Use the current runners that match the current core-owned runtime:

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_firmware_stack_budget_tool_tests.sh`
- `sh tests/host/run_firmware_memory_budget_tool_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_firmware_memory_budget_checks.sh`
- `sh tests/host/run_firmware_stack_budget_checks.sh`

The final two commands are target-only gates. The memory gate consumes the
fresh ordinary ELF; the stack gate consumes artifacts from its fresh
instrumented build. Host checker fixtures prove neither target RAM nor target
stack safety by themselves.

## Non-Goals

This runtime no longer preserves the old slot/index internal shapes as API.
The maintained contracts are user-visible behavior, overlap correctness, and
the reducer-owned debug/projection surfaces described above.

## Physical-event timing at the QMK boundary

The timing contract is in [INTERACTION_MODEL.md](INTERACTION_MODEL.md#physical-gestures-and-buffered-delivery).
`lib/compat/qmk_gesture_timing.h` reads physical `KEY_EVENT` timestamps and
queries the selected QMK fork's existing combo and tapping queues. The queues
remain QMK-owned; userspace stores no shadow input queue. Queries match physical
position, event direction and the unsigned 16-bit interval. Consumed combo
members cease to reserve a key's series. Combo outputs keep their origin-based
protection and delivery timestamps.

The reducer's global clock stays at delivery/scan time. Tokens retain physical
press/release times, and released series retain the release timestamp even after
the token is settled. Expiry and foreign-key flushing consult pending physical
continuations. Hold progress has one gate,
`key_runtime_core_press_token_hold_eligible()`: a token whose physical release is
already queued cannot advance toward a hold. Phase refresh and every scan-time
hold planner go through it; `run_qmk_gesture_pipeline_tests.sh` rejects any other
queued-release query. QMK queues such a release only for a modifier or layer
key released during another key's tapping term. This separates gesture eligibility from permission to emit
output without rewinding shared time.

`lib/compat/qmk_record_admission.c` holds records back behind an undecided
tap/hold key ([the contract](INTERACTION_MODEL.md#keys-pressed-while-a-taphold-key-is-undecided)).
The fork's `process_record_admit_user()` offers every record at the top of
`process_record()`, after combos and native tapping and before every QMK feature,
so a held record is processed once, on replay. `noah_matrix_scan_user` runs the
replay after the key runtime scan, so a hold reached that scan applies first;
`key_runtime_core_undecided_dual_role_key_pos()` is the only decision it reads.
It counts LT/MT/OSM rows and keys whose hold is a layer held until release
(the token's hold preview layer). Before a press is resolved, physically or
on replay, `noah_key_runtime_settle_layer_taps_before_press()` settles other
keys' pending layer-changing taps when the press is of a key without a
behaviour; it costs no lookup unless such a tap is pending.
At capacity (eight records by default), capture removes and replays exactly the
oldest held record through `process_record()` before appending the incoming
record. This bounded overload path may deliver that oldest record before an
undecided key resolves; it never drops a record or lets a release bypass its
buffered press. A replay guard prevents recapture, and the record's physical
timestamp, keycode, event type and tap metadata survive unchanged. The remaining
queue follows normal scan replay, including pausing for a newly undecided key.
The real QMK pipeline covers saturation, smaller capacities, nested decisions,
combo outputs, repeated taps, timer wrap and final runtime quiescence.

The fork's `is_tap_keycode_user` hook exempts all runtime-handled keys from native
tapping. Lookup includes authored MT/OSM rows and intrinsic TT/OSL ownership.
Unhandled keys retain QMK policy; the pipeline tests each family and native
controls. QMK then gives `is_tap_record_user` the last word per record, with
its own answer and the keycode hook's; its default keeps the keycode hook's.
Userspace answers from the press's participation decision, so a placement that bypasses its behaviour row is
native QMK tap-hold again
([participation policy](architecture/participation-policy.md)). Feature bit 18 distinguishes this broader ownership rule from bit 17. Queue queries and
the hook are fork contracts pinned by `run_qmk_contract_checks.sh`;
`run_qmk_gesture_pipeline_tests.sh` runs the actual selected QMK combo/tapping
engines with userspace, including delayed records and timer wrap. The ordinary
synchronous host harness intentionally keeps delivery-time records; it does not
prove this boundary. Neither harness substitutes for physical acceptance.

Participation is captured per physical record before either QMK queue. Its
opaque context holds the source layer and behavior/combo decisions; the
keycode is not frozen, so QMK resolves a waiting press again on delivery, on
the layer a hold turned on. Delivery settles the press once: a press whose
source layer changed while it waited is re-decided against the new one, and
its release takes that decision. Admission retains the complete record and
re-opens settlement for a press it holds back, so a repeated press at the same
position cannot change an earlier queued owner's decision.
Generated combo outputs capture their permission against the retained origin
layer before admission, and normalize their origin only when dispatched.

Origin normalization keeps a separate compiled call boundary: its reconstruction
workspace retires before process stages can recursively project held actions.
The reviewed-path stack manifests follow the pinned compiler's linked callers
for inlined helpers and cover this boundary without raising stack policies.
