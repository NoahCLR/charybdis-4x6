# Userspace Architecture Review

## Scope

This review tracks the `2026-04-29` PD-mode runtime-semantics change: pressing
the same currently locked runtime-handled PD-mode key unlocks that mode on
press, while the physical key remains a normal momentary hold until release.

This is a new review thread because `review/2026-04-28-review-01/` is about
folder/package layout and documentation shape, while this pass changes runtime
behavior and effect vocabulary. Closed review folders remain immutable history.

## Intended Semantics

- Plain runtime-handled PD-mode keys keep their default momentary-hold path.
- If the pressed PD-mode key matches the currently locked mode, key-runtime
  planning emits an explicit unlock effect on press.
- The held-action registration still happens after that unlock, so the mode is
  active for the duration of the physical hold.
- Release after the consumed unlock only clears the momentary hold. It must not
  send the key's tap fallback and must not re-toggle the lock.
- Explicit generated `*_LOCK` keycodes and authored `TAP_SENDS(<MODE>_LOCK)`
  actions keep their existing toggle behavior through action dispatch.

## Ownership Decision

Key runtime owns the interaction decision: "this press consumes the matching
locked mode." PD runtime remains authoritative for actual local active and
locked PD state.

The boundary is an explicit projected effect:

- `KEY_RUNTIME_EFFECT_PD_MODE_LOCK_STATE` requests a concrete lock state.
- Projection calls `pd_mode_set_lock_state_at(mode, false, key_pos)` for the
  unlock-on-press case.
- Existing `KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP` remains the explicit lock
  action/toggle path.

This keeps the decision and the side effect separate: release planning can
reason from the press token, while PD mode state continues to live in
`pd_mode_state.c`.

## Touched Contracts

- Press planning in `key/runtime/reducer/runtime.c` records that a same-mode
  lock was consumed before registering the held action.
- Release planning in `key/runtime/planning/release_planner.*` suppresses the
  old release-time quick-lock toggle and tap fallback for consumed locks.
- Projection in `key/runtime/projection/` applies the new explicit lock-state
  effect through the PD runtime API.
- Runtime trace and host scenario capture include the new effect vocabulary.
- User and maintainer docs describe the new visible rule.

## Verification Plan

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
