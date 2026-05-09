# Runtime Loop Performance Progress

This review was opened because `review/2026-05-05-review-01/` is closed and
covered the Profile Studio new-keymap workflow, not runtime loop performance.
The current work is a post-closure runtime thread.

## 2026-05-08

### Completed

- Removed userspace watchdog enable/update behavior from runtime diagnostics.
- Removed per-scope watchdog scratch writes from the runtime diagnostic hot
  path.
- Changed runtime diagnostics to start a boot indicator on every runtime init.
- Changed RGB diagnostic rendering to show a full-board white boot indicator
  instead of per-stage reboot colors.
- Added split runtime sync dirty state in
  `users/noah/lib/split/runtime_sync_dirty.c`.
- Gated combo and key-feedback split packet building so idle clean scans skip
  the heavy bitmap/map derivation until forced, dirty, active, or heartbeat-due.
- Added dirty notifications from combo origin observation, key-runtime scan
  state, transition plans, and feedback pulse paths.
- Wired the new dirty-state source into the userspace source manifest and the
  host runners that compile isolated key-runtime/split-sync source lists.
- Updated runtime architecture docs that previously described watchdog
  servicing or diagnostic stage-color rendering.
- Renamed stale host-test wording that referred to a watchdog heartbeat where
  the test only verifies runtime heartbeats during delayed macro playback.

### In Flight

- No implementation work is currently in flight.

### Verification

Passed so far:

- `sh tests/host/run_runtime_diag_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

### Next Steps

1. Measure firmware loop/report rates on hardware.
2. If report rate is still low, profile the next hottest scan-loop surfaces.

## 2026-05-09

### Completed

- Restored the minimal RP2040 watchdog restart path after clarifying that hard
  freezes should still reboot the device.
- Kept stage-specific reboot diagnostics removed: no scratch writes, no latched
  stage reporting, and no stage-color palette.
- Kept the boot indicator on every runtime init so a watchdog restart still
  shows visible white feedback.
- Lowered the boot-indicator white from full RGB white to the keymap's authored
  `HSV(0,0,150)` white brightness.
- Switched the boot-indicator implementation and RGB render test to express
  that white as HSV and convert through `hsv_to_rgb()`.
- Updated runtime diagnostic and RGB render tests for the restored watchdog
  restart contract and capped white indicator.

### In Flight

- No implementation work is currently in flight.

### Verification

Passed:

- `sh tests/host/run_runtime_diag_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

### Next Steps

1. Measure firmware loop/report rates on hardware.
2. If report rate is still low, profile the next hottest scan-loop surfaces.
