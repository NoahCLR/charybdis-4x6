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

## 2026-07-13

### Completed

- Reproduced the target stack-risk finding against a 2,048-byte process stack
  and recorded the complete QMK-to-userspace handled-release chain rather than
  treating individual function frames as independent budgets.
- Replaced the pending-release-capacity automatic drain array with a fixed
  four-record transport batch. Queue capacity remains 120 records.
- Defined bounded drain semantics: one drain snapshots and removes only its
  entry-time batch, preserves FIFO order, rejects synchronous re-entry, and
  leaves records enqueued during projection for the next release or scan
  boundary.
- Moved release-triggered draining out of the release planner/executor frame so
  release planning and projection storage unwind before deferred projection.
- Split mutually exclusive release and handled-key materialization phases into
  no-inline helpers, and replaced avoidable by-value resolution copies with
  borrowed or output-parameter seams.
- Replaced unmatched-release full materialization with a narrow HOLD-source
  momentary-layer query while preserving the reducer's shadow-inclusive lookup
  context and transition recovery's live/base-only context as separate
  policies.
- Documented the release planner's borrowed-interaction lifetime: the pointer
  is valid only during synchronous planning before settlement or reducer state
  mutation.
- Removed a redundant 512-byte macro-IR compile from VIA default seeding. Each
  non-empty default is compiled once during seed; post-init validation remains
  a separate sequential pass.
- Added host coverage for full-capacity drain batching, overflow, FIFO order,
  enqueue-during-projection, re-entry suppression, exact modifier snapshots,
  feedback adjacency, multi-owner cleanup, repeated production scans,
  momentary fallback context, active-token authority, materialization parity,
  and the VIA seed compile-call budget.
- Added opt-in final-linked stack artifacts, a reviewed-path target budget
  checker, fixture tests, and separate host-only and target runners. The
  checker parses direct calls/tails, requires documented indirect edges,
  separates main and split-thread contexts, and states explicitly that it is
  not a proof of the global call-graph maximum.

### In Flight

- Reconcile the source-reviewed target path manifest against a fresh
  post-refactor linked image. The gate intentionally fails closed on symbol or
  edge drift after LTO.
- Run the stack budget report for both the main process stack and vendor split
  callback thread.

### Verification

Passed on the final host/documentation tree for this pass:

- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` (15 tests)
- `env PATH=/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `git diff --check`

Intermediate exact `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
builds passed during the frame-isolation work, but they predate the final
source state and are not closure evidence. The final target rebuild is blocked
because QMK must write into `../bastardkb-qmk/.build` and the sandbox approval
service rejected that external write due an account usage-limit condition. No
sibling source files were edited.

Reconciliation note: this paragraph records the 2026-07-13 checkpoint only.
The 2026-08-15 continuation below resolves the external blocker and supplies
the final target evidence.

### Next Steps

1. When sibling build writes are available, run a fresh instrumented firmware
   compile and `sh tests/host/run_firmware_stack_budget_checks.sh`.
2. Reconcile any fail-closed LTO symbol/edge drift, then close the finding only
   when the final ELF satisfies the configured reserve for each context.

## 2026-08-15

### Completed

- Produced a fresh post-LTO target image and reconciled the reviewed-path
  manifest to its actual linked symbols, direct calls/tails, and documented
  register-indirect stage/provider/action-kind callbacks.
- The initial linked report found a real handled-press projection overrun, not
  only LTO name drift. Split press planning from projection; the linked press
  wrapper dropped from 488 bytes to 176 bytes and planning owns an independent
  336-byte frame.
- The reconciled report proved five reviewed paths exceeded the 1,536-byte
  budget under the platform-default 2,048-byte process stack. The worst path,
  including QMK callers and nested fallback settlement, measured 1,808 bytes.
- Configured a 2,560-byte process stack after measuring linked RAM. The target
  gate now reserves 640 bytes, leaving a 1,920-byte reviewed-path budget. The
  worst main path passes at 1,808 bytes; the independent split callback context
  passes at 328 bytes in a 768-byte budget.
- Final linked sections are `.text` 101,712 B, `.rodata` 15,460 B, `.data`
  23,760 B, `.bss` 65,204 B, `.ram4` 288 B, and heap 173,176 B.
- No sibling source was edited. The target commands only refreshed artifacts
  under `../bastardkb-qmk/.build`.

### Verification

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_firmware_stack_budget_checks.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

All commands passed. The stack checker remains a reviewed-path regression gate,
not a claim of global call-graph maximum proof.

### Next Steps

1. Preserve the target stack gate for later runtime changes.
2. Continue the Sol remediation roadmap with Finding 03.
3. Measure firmware loop/report rates on hardware when available.
