# Userspace Architecture Review

## Scope

This review records the legacy cleanup thread opened on 2026-04-23, starting
with the legacy multi-tap engine and continuing with a conservative dead-code
hunt across nearby runtime surfaces.

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
- Macro payload visitor/direct-run path: resolved. Text payload playback now
  compiles to macro IR and plays that IR; the old visitor/direct execution path
  had no callers.
- Deferred-release blocker storage leftovers: resolved. Deferred-release
  blockers are derived from live press tokens, so the old blocker type and
  capacity constant were stale.
- Runtime diagnostic test-backend setters: resolved. Tests seed watchdog reboot
  state through the focused seed helper and read through public accessors; the
  generic setters had no callers.
- Public pending multi-tap reset helper: resolved. Resetting pending multi-tap
  state is release-settlement internals, so `key_runtime_core_reset_pending_multi_tap()`
  is now file-local.
- No-op hold-preference helper: resolved. The shared userspace has no hold
  preference policy, so `noah_get_hold_on_other_key_press()` was removed and
  the weak QMK hook returns `false` directly.

## Current Architecture

`key_runtime_core` is the single runtime authority for multi-tap sequencing.
Pending multi-tap state is represented by `tap_series_t`, and scan/release
planning emits delayed-action effects from that reducer-owned state.

Transparent tap metadata remains covered at the authored-behavior/materialize
boundary. Runtime behavior remains covered by the key-runtime host runners
instead of a separate sequencing engine.

Macro payload playback uses the compiled IR path as the single execution
surface. Deferred-release blocker counts remain token-derived rather than
backed by a separate blocker table.

Hold-preference behavior is not a shared userspace contract. Keymaps that need
it should define QMK's `get_hold_on_other_key_press()` directly.

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
- Removed the unused macro-payload visitor/direct-run path.
- Removed unused runtime diagnostic test-backend setters.
- Removed stale deferred-release blocker storage leftovers from
  `key_runtime_core`.
- Made the pending multi-tap reset helper private to `key_runtime_core`.
- Removed the no-op `noah_get_hold_on_other_key_press()` runtime helper and
  documented direct QMK override as the hold-preference extension point.

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
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_runtime_diag_tests.sh`
- `sh tests/host/run_hook_chaining_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Recommended Follow-Up

1. Keep future multi-tap behavior inside `key_runtime_core`.
2. Add or extend key-runtime host coverage for any new multi-tap sequencing
   behavior.
3. Continue treating low-reference QMK hooks, feature-gated code, and symmetric
   public query APIs as contracts unless a separate design pass intentionally
   removes the contract.
