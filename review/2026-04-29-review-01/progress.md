# Progress

## 2026-04-29 - Review Opened

This folder was opened as a new review thread because the active
`review/2026-04-28-review-01/` thread covers folder/package layout, while this
pass changes PD-mode runtime semantics and the key-runtime effect vocabulary.
The closed `review/2026-04-27-review-01/` folder remains immutable history.

Starting worktree status:

- `git status --short` returned no entries at the start of this implementation
  pass.

## Completed

- Added an explicit `KEY_RUNTIME_EFFECT_PD_MODE_LOCK_STATE` effect and
  projection path.
- Planned same-locked-PD-mode unlocks on press before held-action registration.
- Marked consumed press tokens so release planning suppresses both the old
  quick-lock retoggle and normal tap fallback.
- Updated scenario, release-matrix, PD-mode integration, runtime-debug, and
  real-profile dragscroll coverage for the new rule.
- Updated user-facing and architecture docs for the visible behavior and new
  effect vocabulary.

## In Flight

- None.

## Verification

Passed:

- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Failure handled:

- The first `run_all_host_tests.sh` pass exposed a host-link gap in
  `run_key_runtime_layer_lock_integration_tests.sh`: the test binary linked
  `pd_projection.c` without a `pd_mode_set_lock_state_at()` stub.
- Added narrow stubs to the layer-lock and modifier-hold integration tests,
  reran both directly, then reran the full suite successfully.

## Next Steps

1. Use the keyboard to confirm the subjective dragscroll feel: locked
   dragscroll plus same-key press should unlock immediately, scroll while held,
   and stop on release.
2. Keep any future changes to lock gestures covered by scenario, release-matrix,
   PD-mode integration, and real-profile tests.
