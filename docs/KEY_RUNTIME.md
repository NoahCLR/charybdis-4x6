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
| [`core/runtime.h`](../users/noah/lib/key/runtime/core/runtime.h), [`core/runtime.c`](../users/noah/lib/key/runtime/core/runtime.c), [`core/projection.h`](../users/noah/lib/key/runtime/core/projection.h) | Single-authority runtime state, reducer entry points, release resolution inputs, pending multi-tap state, leases, persistent intents, pending release transport, and projection/debug capture. |
| [`core/release_planner.h`](../users/noah/lib/key/runtime/core/release_planner.h), [`core/release_planner.c`](../users/noah/lib/key/runtime/core/release_planner.c) | Shared release decision contract and effect planning for active release and pending multi-tap release. |
| [`process.c`](../users/noah/lib/key/runtime/process.c) | `process_record_user()` entry flow, preflight ordering, release-keycode recovery, and non-handled release finalization. |
| [`preflight.c`](../users/noah/lib/key/runtime/preflight.c) | Cross-key interruption and default-suppression work before the current press proceeds, while unrelated pending multi-tap chains stay position-owned until timeout or same-key reuse. |
| [`press.c`](../users/noah/lib/key/runtime/press.c), [`release.c`](../users/noah/lib/key/runtime/release.c), [`scan.c`](../users/noah/lib/key/runtime/scan.c) | Thin press/release/scan orchestration around reducer-owned effect plans. |
| [`transition.c`](../users/noah/lib/key/runtime/transition.c), [`transition.h`](../users/noah/lib/key/runtime/transition.h) | Effect-plan transport and execution seam, including blocked-release dispatch deferral. This is the last shared transport layer between reducer decisions and concrete effect projection. |
| [`effects/effect.h`](../users/noah/lib/key/runtime/effects/effect.h) | Shared runtime effect vocabulary. |
| [`held_action.c`](../users/noah/lib/key/ownership/held_action.c), [`held_repeat.c`](../users/noah/lib/key/ownership/held_repeat.c), [`layer_ownership.c`](../users/noah/lib/state/ownership/layer_ownership.c), [`keyboard_mod_ownership.c`](../users/noah/lib/state/ownership/keyboard_mod_ownership.c) | External ownership registries projected by runtime effects. |
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
  order after blockers clear.
- `key_runtime_core_shadow_projection_t`: reducer-owned projected view used by
  blocking queries, debug snapshots, and overlap reasoning.

If a future change needs new runtime state, it belongs in `key_runtime_core` unless
it is purely an external ownership registry or a stateless authored-behavior
helper.

Pending multi-tap behavior is owned by `tap_series_t` inside `key_runtime_core`.
Do not add a second state machine for multi-tap sequencing; new behavior should
extend the core reducer and its release/scan planning tests.

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

- threshold hold and long-hold promotion
- pending multi-tap expiry and delayed action flush
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
