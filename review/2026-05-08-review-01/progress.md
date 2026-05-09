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
- Gated key-feedback split packet building so idle clean scans skip the heavy
  semantic/branch map derivation until forced, dirty, active, or heartbeat-due.
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

Re-run after restoring immediate combo feedback:

- `sh tests/host/run_split_runtime_sync_tests.sh`
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
- Gated `noah_key_runtime_scan()` so idle scans with no active press tokens and
  no pending multi-tap series skip transition-plan allocation, timer-driven
  core refresh, scan tracing, and empty plan execution.
- Kept pending release dispatches draining when key-runtime core work is idle
  by adding a pending-dispatch query to the deferred-release adapter.
- Avoided the deferred-release blocker scan when there are no pending releases,
  and only checked blockers when active press tokens could block the queue.
- Optimized normal pointer reports so they read the local active PD mode
  directly and skip full PD snapshots and registry lookups unless a local mode
  is active.
- Removed the temporary delta array from idle-noise absolute-motion
  calculation.
- Added runtime-debug coverage for idle key-runtime scan gating and pending
  release drains without active core work.
- Restored per-tick combo feedback packet building so idle-to-active combo RGB
  feedback is immediate again while unchanged combo packets still avoid
  transport sends until heartbeat.
- Added split-sync coverage for idle-to-active combo feedback without relying
  on a dirty notification.
- Removed inert runtime diagnostic scope calls from matrix scan, housekeeping,
  keyboard post-init, pointer task, layer-state handling, and RGB render paths.
  The watchdog heartbeat remains in housekeeping.
- Added held-repeat active binding counting so idle housekeeping returns before
  timer reads or board-sized repeat-table scans.
- Changed active held-repeat ticks to read the timer once per pass.
- Restored phase-preserving held-repeat scheduling so 100 Hz click-spam does
  not lose cadence to normal scan-loop timing drift.
- Bounded held-repeat catch-up to four taps per housekeeping tick so long
  stalls do not replay an unbounded backlog.
- Updated held-action host coverage for 100 Hz drift catch-up, scan-gap
  catch-up, excessive-backlog bounding, and runtime reset isolation through
  the public reset hook.
- Reordered the pointing idle-noise filter so normal motion above the noise
  threshold does not query long-idle timers on every report.
- Removed the QMK 1 ms pointing-device throttle gate by setting
  `POINTING_DEVICE_TASK_THROTTLE_MS` to `0`; the current firmware loop is
  slower than 1 kHz, so the throttle was only adding hot-path timer work.
- Regenerated the profile overview after the userspace config change.
- Added a combined combo feedback bitmap API and changed split sync to build
  combo underlay and overlay packets with one combo-origin pass.
- Added a weak combined-combo fallback in split sync so isolated host runners
  that link `runtime_sync.c` without `feedback.c` keep the same build contract
  as the older underlay/overlay hooks.
- Skipped PD owner bitmap snapshots on split-sync ticks with no local owner
  side.
- Changed local/display PD owner-side snapshot helpers to read direct state
  instead of building a full PD mode snapshot.
- Added held-repeat idle coverage and split-sync coverage for skipped idle PD
  owner bitmap snapshots.
- Shifted the current investigation away from split-sync special casing and
  back to fixed master main-loop costs.
- Lowered `MATRIX_IO_DELAY` from QMK's 30 us default to 10 us for the local
  ROW2COL 4x6 scan.
- Changed runtime diagnostics so the RP2040 watchdog still refreshes from
  housekeeping, but only every 8 housekeeping passes after the immediate first
  heartbeat refresh.
- Added runtime diagnostic coverage for skipped per-loop watchdog writes.
- Regenerated the profile overview after adding the matrix scan timing knob.

### In Flight

- No implementation work is currently in flight.

### Verification

Passed for the watchdog / HSV pass:

- `sh tests/host/run_runtime_diag_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Passed for the optimization pass:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Passed for the follow-up inefficiency pass:

- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Passed for the held-repeat cadence fix:

- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Passed for the pointer-task optimization pass so far:

- `sh tests/host/run_pd_runtime_tests.sh`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Passed for the master-loop fixed-overhead pass:

- `sh tests/host/run_runtime_diag_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

### Next Steps

1. Measure click-spam and firmware loop/report rates on hardware.
2. If report rate is still low, profile the next hottest scan-loop surfaces.
