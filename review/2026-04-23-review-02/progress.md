# Progress

## 2026-04-23

### Review Opened

- Opened `review/2026-04-23-review-02/` for the legacy multi-tap engine cleanup.
- Did not continue `review/2026-04-23-review-01/` because that review thread was
  already closed. Closed review folders must remain immutable history.

### Completed

- Scoped `multi_tap_engine` after dead-code hunting found it was still compiled
  but not used by production key-runtime flow.
- Confirmed active firmware multi-tap behavior is owned by `tap_series_t` in
  `key_runtime_core`.
- Removed `users/noah/lib/key/interaction/multi_tap_engine.c` and
  `users/noah/lib/key/interaction/multi_tap_engine.h`.
- Removed the old `multi_tap_t` delayed-action adapter surface and made delayed
  action dispatch depend directly on captured `keyboard_mod_state_t`.
- Cleaned the canonical userspace source manifest, host source lists, and the
  PD buffered-tap internal feature-gate allowlist.
- Kept transparent multi-tap lookup coverage by asserting materialized
  press-vs-release metadata directly in `tests/host/key_behavior_lookup_test.c`.
- Updated `docs/KEY_RUNTIME.md` to document `tap_series_t` as the only runtime
  multi-tap sequencing authority.

### Verification

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

### Next Steps

- No follow-up is currently required for the legacy multi-tap engine cleanup.
- Future multi-tap behavior changes should extend `key_runtime_core` and keep
  coverage in the key-runtime host runners, not in a separate sequencing engine.
