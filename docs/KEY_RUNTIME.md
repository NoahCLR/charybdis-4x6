# Key Runtime

This document is the maintainer-facing map for the handled-key runtime under
[`users/noah/lib/key/`](../users/noah/lib/key/).

Use this when you are changing runtime behavior, tests, or debug surfaces. For
user-facing semantics, see [INTERACTION_MODEL.md](./INTERACTION_MODEL.md). For
the authored profile, see [KEYMAP.md](./KEYMAP.md).

## Current Shape

The key runtime is now a single-authority reducer-owned system.

- Authored behavior resolution still starts in `interaction/`.
- Reducer-owned state now lives in `key/runtime/core/`, not in legacy slot/index
  storage.
- `users/noah/lib/key/runtime/` is now thin orchestration around reducer entry
  points, effect transport, and QMK hook integration.
- Long-lived external ownership still lives in the dedicated registries under
  `state/ownership/` and `key/ownership/`.
- The reducer-owned code now uses the `key_runtime_core_*` symbol family.
  Historical review notes may still mention `runtime_v2` because that was the
  cutover thread name.

The legacy slot reducers, slot result transport, slot/index shared state, and
stub-backed mixed-runtime host surfaces were removed during the full cutover.

## Core vs Integration

The key runtime now lives under one permanent tree, but that tree still has
two architectural layers.

- [`users/noah/lib/key/runtime/core/`](../users/noah/lib/key/runtime/core/) is
  the reducer/state-owner layer. It holds the canonical runtime state, plans
  effects, and exposes the debug/projection surface.
- The top-level files in
  [`users/noah/lib/key/runtime/`](../users/noah/lib/key/runtime/) are the
  QMK-facing integration layer. They own process/scan entry flow, preflight,
  effect-plan transport, trace/debug adapters, and effect projection.
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
| [`handled_key.h`](../users/noah/lib/key/interaction/handled_key.h), [`handled_key_lookup.c`](../users/noah/lib/key/interaction/handled_key_lookup.c), [`handled_key_materialize.c`](../users/noah/lib/key/interaction/handled_key_materialize.c) | Resolve authored behavior into `handled_key_resolution_t` and materialize it into runtime interaction contracts. |
| [`interaction.h`](../users/noah/lib/key/runtime/interaction.h) | Shared interaction contract cached by the reducer after authored behavior materialization. |
| [`core/runtime.h`](../users/noah/lib/key/runtime/core/runtime.h), [`core/runtime.c`](../users/noah/lib/key/runtime/core/runtime.c) | Single-authority runtime state, reducer entry points, leases, persistent intents, scan orchestration, and effect projection orchestration. |
| [`core/effect_plan.h`](../users/noah/lib/key/runtime/core/effect_plan.h) | Internal effect-plan append helpers shared by core planning modules. |
| [`core/feedback_projection.h`](../users/noah/lib/key/runtime/core/feedback_projection.h), [`core/feedback_projection.c`](../users/noah/lib/key/runtime/core/feedback_projection.c) | Core-side feedback pulse projection and pulse queueing for key-runtime effects. |
| [`core/pd_projection.h`](../users/noah/lib/key/runtime/core/pd_projection.h), [`core/pd_projection.c`](../users/noah/lib/key/runtime/core/pd_projection.c) | Core-side key-runtime PD projection for held-action preemption and PD lock-tap effects; actual PD mode state remains PD-runtime-owned. |
| [`core/pending_release_queue.h`](../users/noah/lib/key/runtime/core/pending_release_queue.h), [`core/pending_release_queue.c`](../users/noah/lib/key/runtime/core/pending_release_queue.c) | Core pending-release queue mechanics: allocation, ordering, drain snapshots, and released-token pending-emission markers over `key_runtime_core_state_t` storage. |
| [`core/projection.h`](../users/noah/lib/key/runtime/core/projection.h), [`core/projection.c`](../users/noah/lib/key/runtime/core/projection.c) | Projection API declarations, pending-release dispatch projection, projection snapshot capture/comparison, and trace projection checkpoints. |
| [`core/release_planner.h`](../users/noah/lib/key/runtime/core/release_planner.h), [`core/release_planner.c`](../users/noah/lib/key/runtime/core/release_planner.c) | Shared release decision contract, active-release resolution, pending multi-tap release resolution, and release effect planning. |
| [`core/scan_planner.h`](../users/noah/lib/key/runtime/core/scan_planner.h), [`core/scan_planner.c`](../users/noah/lib/key/runtime/core/scan_planner.c) | Scan-time active hold promotion, release-hold-pending marking, fallback hold settlement, and pending multi-tap scan planning over core state. |
| [`core/tap_series.h`](../users/noah/lib/key/runtime/core/tap_series.h), [`core/tap_series_flush.c`](../users/noah/lib/key/runtime/core/tap_series_flush.c) | Internal tap-series helpers for pending multi-tap flush resolution, branch-confirm delayed action windows, delayed action completion, and foreign/global multi-tap flush planning. |
| [`deferred_release.h`](../users/noah/lib/key/runtime/deferred_release.h), [`deferred_release.c`](../users/noah/lib/key/runtime/deferred_release.c) | Adapter that defers blocked release dispatch effects into the core pending-release queue and drains queued dispatches after release/scan execution. |
| [`process.c`](../users/noah/lib/key/runtime/process.c) | `process_record_user()` entry flow, preflight ordering, release-keycode recovery, and non-handled release finalization. |
| [`preflight.c`](../users/noah/lib/key/runtime/preflight.c) | Cross-key interruption and default-suppression work before the current press proceeds, while unrelated pending multi-tap chains stay position-owned until timeout or same-key reuse. |
| [`press.c`](../users/noah/lib/key/runtime/press.c), [`release.c`](../users/noah/lib/key/runtime/release.c), [`scan.c`](../users/noah/lib/key/runtime/scan.c) | Thin press/release/scan orchestration around reducer-owned effect plans. |
| [`transition.c`](../users/noah/lib/key/runtime/transition.c), [`transition.h`](../users/noah/lib/key/runtime/transition.h) | Effect-plan transport and execution seam between reducer decisions and concrete effect projection. |
| [`effects/effect.h`](../users/noah/lib/key/runtime/effects/effect.h) | Shared runtime effect vocabulary. |
| [`held_action.c`](../users/noah/lib/key/ownership/held_action.c), [`held_repeat.c`](../users/noah/lib/key/ownership/held_repeat.c), [`layer_ownership.c`](../users/noah/lib/state/ownership/layer_ownership.c), [`keyboard_mod_ownership.c`](../users/noah/lib/state/ownership/keyboard_mod_ownership.c) | External ownership registries projected by runtime effects. |
| [`pd_mode_key_runtime_bridge.h`](../users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.h), [`pd_mode_key_runtime_bridge.c`](../users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c) | Narrow PD-to-key-runtime observer bridge for changed local PD lock state. |
| [`runtime_debug.h`](../users/noah/lib/state/runtime/runtime_debug.h), [`runtime_reset.h`](../users/noah/lib/state/runtime/runtime_reset.h), [`runtime_trace.h`](../users/noah/lib/state/runtime/runtime_trace.h) | Public debug, reset, and tracing seams used by host tests and runtime diagnostics. |

## Reducer-Owned State

`key_runtime_core` owns these runtime shapes:

- `press_token_t`: immutable press identity plus live phase, authored
  interaction contract, and release-time facts for one physical key.
- `tap_series_t`: pending multi-tap chain state separated from the active press
  lifetime.
- `lease_t`: reducer-owned temporary ownership for layers, modifiers, held
  actions, repeats, pd modes, and pointer anchors.
- `persistent_intent_t`: lock-like state that survives a single press lifetime,
  such as layer locks, pd-mode locks, and pointer toggles.
- `pending_release_t`: deferred release dispatches that must drain in authored
  order after blockers clear. Slots are stored in `key_runtime_core_state_t`
  and managed by `core/pending_release_queue.c`.
- `key_runtime_core_shadow_projection_t`: reducer-owned projected view used by
  blocking queries, debug snapshots, and overlap reasoning.

If a future change needs new runtime state, it belongs in `key_runtime_core` unless
it is purely an external ownership registry or a stateless authored-behavior
helper.

Pending multi-tap state is owned by `tap_series_t` inside `key_runtime_core`,
while pending multi-tap release decisions are owned by `core/release_planner.c`.
Do not add a second state machine for multi-tap sequencing; new behavior should
extend the core reducer and its release/scan planning tests.

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
| Physical press identity and active key phase | `press_token_t` in `key_runtime_core` | debug and trace snapshots | Physical events are observed into core first; other registries must not create or mutate press tokens. Covered by key runtime scenario, release matrix, and integration harness tests. |
| Pending multi-tap chain state | `tap_series_t` in `key_runtime_core` | `core/tap_series_flush.c` plans explicit flushes; `core/scan_planner.c` owns scan-time thresholds; release planner reads the state | Core stores the chain; `core/tap_series_flush.c` flushes expired or foreign chains, `core/scan_planner.c` resolves scan-time hold/flush outcomes, and `core/release_planner.c` resolves release decisions over it. Covered by release matrix, scenario, runtime debug, and integration harness tests. |
| Release semantics | `core/release_planner.c` | `deferred_release.c` adapts blocked dispatches into the core pending-release queue | Planner owns quick release, fallback suppression, buffered base tap, active releases, and pending multi-tap releases; adapters must not re-decide those semantics. |
| Pending release dispatch queue | `pending_release_t` slots in `key_runtime_core`, with mechanics in `core/pending_release_queue.c` | `deferred_release.c`, `release.c`, and `scan.c` drain through the adapter | Queue storage stays core-owned because blockers are press-token facts; `core/pending_release_queue.c` owns allocation, ordering, drain snapshots, and pending-emission token cleanup. Covered by release matrix and runtime debug tests. |
| Temporary held ownership intent for held actions, repeats, momentary layers, managed modifiers, pd holds, and pointer anchors | `lease_t` in `key_runtime_core` | `held_action.c`, `held_repeat.c`, `layer_ownership.c`, `keyboard_mod_ownership.c`, `pd_mode_state.c`, and pointer layer policy | Core effect projection writes outward; applied registries perform QMK, action, repeat, layer, modifier, or pd-mode side effects. Those registries must not mint independent key-runtime leases. |
| Lock-like runtime intent | `persistent_intent_t` in `key_runtime_core` | `layer_ownership.c` and `pd_mode_state.c` apply the actual layer or pd-mode lock | Current accepted bridge points are `layer_ownership_set_lock_state()` and `pd_mode_key_runtime_bridge_observe_local_lock_state()`, which update external state and then refresh core shadow state. Treat new two-way lock writes as architecture work, not local fixes. |
| Physical keyboard modifier observation | QMK live modifier state plus `keyboard_mod_ownership.c` physical refcounts | core shadow projection stores physical and managed masks for overlap reasoning | `process.c` observes physical modifier events and preflight may suppress default release; core may reason over the shadow but QMK remains the live report sink. Covered by keyboard mod ownership and modifier-hold integration tests. |
| PD runtime local, display, remote, and split state | `pd_mode_state.c` and split sync runtime | key-runtime leases and persistent intents request local pd behavior | Key runtime may request PD transitions through projected effects; PD runtime owns actual mode state and snapshots. Changed local PD lock state is observed into core through `pd_mode_key_runtime_bridge.c`. |
| Feedback pulse lifecycle | feedback pulse fields in `key_runtime_core` | `core/feedback_projection.c`, `key_feedback_pulse_observe()`, and RGB/split feedback snapshots | Core state remains authoritative; `core/feedback_projection.c` queues key-runtime pulse effects and feedback/RGB surfaces render the projection. Covered by runtime debug, split sync, and RGB render tests. |
| Combo origin recovery | `compat/qmk_combo_origin.c` plus `origin_registry.c` | key runtime, PD mode, RGB, and split feedback consume normalized origins | Compatibility-only. It repairs QMK combo records and origin bitmaps; it must not become an owner of key-runtime press, lease, or release state. Covered by combo origin, PD mode, RGB render, and real profile integration tests. |

When a future change needs to touch both core state and one of the projected
registries, update the core plan first and project outward through an explicit
effect or bridge. If the projected registry has to write back into core, document
the bridge here and cover it with projection or ownership tests in the same pass.

## End-To-End Flow

### 1. Physical key event entry

[`process.c`](../users/noah/lib/key/runtime/process.c)
observes every physical event into `key_runtime_core` first.

That observation step gives the reducer position-stable press/release identity
before any QMK path, macro path, or pd-mode path narrows the event.

### 2. Preflight

[`preflight.c`](../users/noah/lib/key/runtime/preflight.c)
does the cross-key work that must happen before the current press resolves:

- suppress default modifier handling when ownership requires it
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
[`transition.c`](../users/noah/lib/key/runtime/transition.c).

The reducer owns:

- press-token creation and replacement
- same-key multi-tap reuse
- foreign active-key interruption
- independent pending-multi-tap retention across foreign presses
- press-time lease activation

### 4. Release routing

Handled releases go through [`release.c`](../users/noah/lib/key/runtime/release.c).
The reducer resolves release by physical key position, not by the raw release
keycode currently visible to QMK.

The release path now owns:

- active release resolution
- pending-multi-tap release resolution
- release-time lease cleanup
- pending release dispatch queueing
- token retirement and pending-series seeding

Non-handled releases still pass through the shared process flow, but
`process.c` now finalizes any reducer-owned observed state for
those keys too. That keeps raw ownership keys such as `MO()`/modifier/pd-mode
keys from leaving stale core leases behind.

### 5. Scan

[`scan.c`](../users/noah/lib/key/runtime/scan.c) asks
`key_runtime_core` for the current scan plan and then drains pending release
dispatches.

The reducer scan path owns:

- threshold hold and long-hold promotion through `core/scan_planner.c`
- pending multi-tap expiry and delayed action flush through `core/scan_planner.c`
- scan-time release blocker clearing

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
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Non-Goals

This runtime no longer preserves the old slot/index internal shapes as API.
The maintained contracts are user-visible behavior, overlap correctness, and
the reducer-owned debug/projection surfaces described above.
