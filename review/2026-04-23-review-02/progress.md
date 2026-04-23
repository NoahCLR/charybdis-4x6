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
- Ran a follow-up legacy-code hunt across explicit stale-code markers, source
  manifest coverage, host runner coverage, low-reference public functions, and
  low-reference public types.
- Removed the unused macro-payload visitor/direct-run path. Macro playback now
  goes through the IR compile/playback path only.
- Removed unused runtime diagnostic test-backend setters that had no callers.
- Removed the stale deferred-release blocker type and capacity constant left
  after blockers became derived from press-token state.
- Narrowed `key_runtime_core_reset_pending_multi_tap()` from public core API to
  a file-local helper because it only serves release settlement inside
  `key_runtime_core`.
- Removed the no-op `noah_get_hold_on_other_key_press()` helper. The weak QMK
  hook now returns `false` directly, and custom hold preference belongs in a
  keymap-owned `get_hold_on_other_key_press()` override.
- Kept config-gated and QMK-owned surfaces that looked low-reference but are
  intentional contracts: `is_keyboard_master_impl()`, weak hook helpers, VIA
  hooks, action-kind matcher callbacks, and the symmetric PD display query API.

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

### Next Steps

- No additional high-confidence legacy cleanup is currently required.
- Future multi-tap behavior changes should extend `key_runtime_core` and keep
  coverage in the key-runtime host runners, not in a separate sequencing engine.
- Future legacy hunts should keep the same standard used here: exact caller
  tracing before deleting anything, and keep config-gated/QMK contract surfaces
  unless the contract itself is intentionally redesigned.

### Closure Verification

- Closure checked with `prompts/closure-verification-review.md` on 2026-04-23.
- Closure verdict: close thread.
- No `must-fix`, `should-fix`, or optional cleanup findings remain for this
  legacy cleanup thread.
- Closure verification passed:
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `git diff --check`
