# Userspace Architecture Review

## Scope

This review records the legacy multi-tap engine cleanup opened on 2026-04-23.

It does not continue `review/2026-04-23-review-01/` because that thread was
explicitly closed before this dead-code investigation. Closed review folders are
treated as immutable history; follow-up architecture work after closure belongs
in a new sortable review folder.

## Finding Status

- Legacy standalone `multi_tap_engine`: resolved. The production key runtime
  uses `tap_series_t` inside `key_runtime_core` for pending multi-tap sequencing.
  The standalone engine was still compiled, but its public state-machine
  functions had no production callers and were only directly exercised by a
  lookup host test.

## Current Architecture

`key_runtime_core` is the single runtime authority for multi-tap sequencing.
Pending multi-tap state is represented by `tap_series_t`, and scan/release
planning emits delayed-action effects from that reducer-owned state.

Transparent tap metadata remains covered at the authored-behavior/materialize
boundary. Runtime behavior remains covered by the key-runtime host runners
instead of a separate sequencing engine.

## Changes

- Removed `users/noah/lib/key/interaction/multi_tap_engine.c`.
- Removed `users/noah/lib/key/interaction/multi_tap_engine.h`.
- Removed the old `multi_tap_t` delayed-action adapter surface.
- Removed the stale source-manifest and host-runner entries for the standalone
  engine.
- Removed the feature-gate allowlist exception that let the standalone engine
  include the private PD buffered-tap header.
- Updated `docs/KEY_RUNTIME.md` to document `tap_series_t` as the only runtime
  multi-tap sequencing authority.

## Verification

Passed:

- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_delayed_action_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Recommended Follow-Up

1. Keep future multi-tap behavior inside `key_runtime_core`.
2. Add or extend key-runtime host coverage for any new multi-tap sequencing
   behavior.
