# Key Runtime

This document is the maintainer-facing map for the handled-key runtime under
[`users/noah/lib/key/`](../users/noah/lib/key/).

Use this when you are changing runtime behavior. For user-facing semantics, see
[INTERACTION_MODEL.md](./INTERACTION_MODEL.md). For the authored profile, see
[KEYMAP.md](./KEYMAP.md).

## Current Shape

The key runtime is now a `runtime_v2`-owned system.

- Authored behavior resolution still starts in `interaction/`.
- Reducer-owned state now lives in `runtime_v2/`, not in legacy slot/index
  storage.
- `users/noah/lib/key/runtime/` is now thin orchestration around reducer entry
  points, effect transport, and QMK hook integration.
- Long-lived external ownership still lives in the dedicated registries under
  `state/ownership/` and `key/ownership/`.

The legacy slot reducers, slot result transport, slot/index shared state, and
stub-backed mixed-runtime host surfaces were removed during the full cutover.

## Why Two Folders Still Exist

The folder split is architectural, not a sign that two runtimes still share
authority.

- [`users/noah/lib/runtime_v2/`](../users/noah/lib/runtime_v2/) is the
  reducer/state-owner layer. It holds the canonical runtime state, plans
  effects, and exposes the debug/projection surface.
- [`users/noah/lib/key/runtime/`](../users/noah/lib/key/runtime/) is the
  QMK-facing integration layer. It owns process/scan entry flow, preflight,
  effect-plan transport, trace/debug adapters, and effect projection.
- The old slot/index runtime is no longer a live production subsystem. The
  only remaining `slot/` file in the production tree is the stateless release
  resolver helper used by the v2 adapters.

So when you see both folders, read that as "decision layer plus integration
layer," not "old runtime plus new runtime running side by side."

## Design Rules

- Runtime authority is by physical key position, not by the keycode currently
  visible on the active layer.
- `runtime_v2` is the only source of truth for active presses, tap series,
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
| [`key_runtime_interaction.h`](../users/noah/lib/key/runtime/key_runtime_interaction.h) | Shared interaction contract used by the reducer and release resolver. |
| [`runtime_v2.h`](../users/noah/lib/runtime_v2/runtime_v2.h), [`runtime_v2.c`](../users/noah/lib/runtime_v2/runtime_v2.c), [`runtime_v2_projection.h`](../users/noah/lib/runtime_v2/runtime_v2_projection.h) | Single-authority runtime state, reducer entry points, release planning, pending multi-tap state, leases, persistent intents, pending release transport, and projection/debug capture. |
| [`key_runtime_process.c`](../users/noah/lib/key/runtime/key_runtime_process.c) | `process_record_user()` entry flow, preflight ordering, release-keycode recovery, and non-handled release finalization. |
| [`key_runtime_preflight.c`](../users/noah/lib/key/runtime/key_runtime_preflight.c) | Cross-key interruption and foreign pending-multi-tap flush before the current press proceeds. |
| [`key_runtime_press.c`](../users/noah/lib/key/runtime/key_runtime_press.c), [`key_runtime_release.c`](../users/noah/lib/key/runtime/key_runtime_release.c), [`key_runtime_scan.c`](../users/noah/lib/key/runtime/key_runtime_scan.c) | Thin press/release/scan orchestration around reducer-owned effect plans. |
| [`key_runtime_transition.c`](../users/noah/lib/key/runtime/key_runtime_transition.c), [`key_runtime_transition.h`](../users/noah/lib/key/runtime/key_runtime_transition.h) | Effect-plan transport and execution seam. This is the last shared transport layer between reducer decisions and concrete effect projection. |
| [`key_runtime_slot_release_resolver.h`](../users/noah/lib/key/runtime/slot/key_runtime_slot_release_resolver.h) | Shared stateless release decision contract reused by the v2 release adapters. |
| [`key_runtime_effect.h`](../users/noah/lib/key/runtime/effects/key_runtime_effect.h) | Shared runtime effect vocabulary. |
| [`held_action.c`](../users/noah/lib/key/ownership/held_action.c), [`held_repeat.c`](../users/noah/lib/key/ownership/held_repeat.c), [`layer_ownership.c`](../users/noah/lib/state/ownership/layer_ownership.c), [`keyboard_mod_ownership.c`](../users/noah/lib/state/ownership/keyboard_mod_ownership.c) | External ownership registries projected by runtime effects. |
| [`runtime_debug.h`](../users/noah/lib/state/runtime/runtime_debug.h), [`runtime_reset.h`](../users/noah/lib/state/runtime/runtime_reset.h), [`runtime_trace.h`](../users/noah/lib/state/runtime/runtime_trace.h) | Public debug, reset, and tracing seams used by host tests and runtime diagnostics. |

## Reducer-Owned State

`runtime_v2` owns these runtime shapes:

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
- `runtime_v2_shadow_projection_t`: reducer-owned projected view used by
  blocking queries, debug snapshots, and overlap reasoning.

If a future change needs new runtime state, it belongs in `runtime_v2` unless
it is purely an external ownership registry or a stateless authored-behavior
helper.

## End-To-End Flow

### 1. Physical key event entry

[`key_runtime_process.c`](../users/noah/lib/key/runtime/key_runtime_process.c)
observes every physical event into `runtime_v2` first.

That observation step gives the reducer position-stable press/release identity
before any QMK path, macro path, or pd-mode path narrows the event.

### 2. Preflight

[`key_runtime_preflight.c`](../users/noah/lib/key/runtime/key_runtime_preflight.c)
does the cross-key work that must happen before the current press resolves:

- suppress default modifier handling when ownership requires it
- interrupt other active handled keys on foreign press
- flush foreign pending multi-tap chains before a different key continues

### 3. Press routing

Handled presses go through [`key_runtime_press.c`](../users/noah/lib/key/runtime/key_runtime_press.c),
which asks `runtime_v2` for a press effect plan and executes it through
[`key_runtime_transition.c`](../users/noah/lib/key/runtime/key_runtime_transition.c).

The reducer owns:

- press-token creation and replacement
- same-key multi-tap reuse
- foreign active-key interruption
- foreign pending-multi-tap flush
- press-time lease activation

### 4. Release routing

Handled releases go through [`key_runtime_release.c`](../users/noah/lib/key/runtime/key_runtime_release.c).
The reducer resolves release by physical key position, not by the raw release
keycode currently visible to QMK.

The release path now owns:

- active release resolution
- pending-multi-tap release resolution
- release-time lease cleanup
- pending release dispatch queueing
- token retirement and pending-series seeding

Non-handled releases still pass through the shared process flow, but
`key_runtime_process.c` now finalizes any reducer-owned observed state for
those keys too. That keeps raw ownership keys such as `MO()`/modifier/pd-mode
keys from leaving stale v2 leases behind.

### 5. Scan

[`key_runtime_scan.c`](../users/noah/lib/key/runtime/key_runtime_scan.c) asks
`runtime_v2` for the current scan plan and then drains pending release
dispatches.

The reducer scan path owns:

- threshold hold and long-hold promotion
- pending multi-tap expiry and delayed action flush
- scan-time release blocker clearing

## Debugging Expectations

When you inspect runtime state, prefer the v2 debug surface:

- `runtime_v2_press_token_at(...)`
- `runtime_v2_tap_series_at(...)`
- `runtime_v2_projection_snapshot_capture()`
- `runtime_v2_shadow_projection()`

Do not reintroduce slot/index mirrors for debug convenience. If a debug view is
missing, add it to the v2 surface.

## Verification

Use the current runners that match the v2-only runtime:

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
