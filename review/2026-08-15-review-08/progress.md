# Split Runtime Shared-Clock Progress

## Why This Review Exists

Review 07 is closed and immutable. Finding 17 starts Phase 3 and establishes
the single sampled timestamp that Finding 12 retry/backoff will reuse.

## 2026-08-15 — Baseline and Contract Audit

### Baseline Verification

Passed before implementation:

- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

### Audit Findings

- Heartbeat helpers sample elapsed time independently for up to four domains.
- Each successful domain samples `timer_read32()` again when recording
  `last_send`, so one logical tick can store mixed timestamps.
- Initialization already fans one read out to all domain timestamps, but its
  immediate forced send resamples inside every successful broadcast.
- Runtime fetches auto-mouse elapsed whenever auto mouse is compiled, even when
  the gradient field is absent, auto mouse is inactive, or PD lock forces zero.
- The QMK fork exposes only a no-argument elapsed API that samples its own
  16-bit timer; strict active one-sample behavior needs explicit sibling scope.

### Chosen Contract

- One 32-bit `now` per initialized master tick or force call.
- Pure wrap-safe `now - last_send` heartbeat arithmetic.
- The same `now` stored by every successful domain.
- No timer reads in eligibility/broadcast helpers.
- No auto-mouse elapsed lookup unless its field is compiled and active.
- No sibling source edits without explicit user authorization. Authorization
  was granted for the narrow auto-mouse elapsed-at compatibility extension.

## Checkpoint Status

Finding 17 is **closed and resolved**. The completed history and evidence are
recorded below.

## 2026-08-15 — Red Timer Budget

- Added explicit 16/32-bit timer and auto-mouse elapsed counters to the split
  host fixture.
- Added one-read assertions for normal and forced ticks, a four-domain
  timestamp-coherence snapshot, active-heartbeat wrap coverage, and inactive
  auto-mouse behavior.
- Added a no-gradient object check that rejects a linked reference to
  `auto_mouse_get_time_elapsed()` when the packet field is absent.
- `sh tests/host/run_split_runtime_sync_tests.sh` failed as expected at the
  normal tick's `timer_read32_count == 1u` assertion.

## 2026-08-15 — Local Shared Clock

### Completed

- Threaded one sampled `uint32_t now` through heartbeat, should-build, and all
  four broadcast helpers.
- Replaced `timer_elapsed32()` with pure unsigned `now - last_send` arithmetic.
- Successful sends store the shared `now`; a host-only snapshot enforces equal
  timestamps across base, combo, semantic, and branch domains.
- Initialization reuses its one sample for the immediate forced send.
- Tick, forced-sync, and explicit-elapsed entry points reject uninitialized or
  slave execution before reading the clock.
- Auto-mouse elapsed is fetched only when the gradient field is compiled, no
  local PD lock suppresses it, and auto mouse is active.

### Verification

Passed:

- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`

### Authorized Sibling Compatibility Extension

- Added `auto_mouse_get_time_elapsed_at(uint16_t now)` to the sibling QMK
  auto-mouse C/header pair after explicit user authorization.
- Preserved the existing no-argument function by sampling once and delegating.
- Added the local compatibility wrapper and active-progress 16-bit-wrap test.
- Added a contract check for the sibling declaration, unsigned subtraction,
  and delegating wrapper.

## 2026-08-15 — Closure Verification

Passed on the final tree:

- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
- `git diff --check`

Target measurements:

- shared-clock sample path: 280 B;
- outbound base-broadcast path: 464 B;
- worst reviewed main path: 1,816/1,920 B;
- worst reviewed split-slave path: 328/768 B;
- linked image: 145,060 B text and 245,592 B BSS.

## Closure Verdict

Finding 17 is **resolved**. One sampled timestamp drives the complete outbound
split tick, including active auto-mouse progress, and all required behavioral,
compatibility, full-suite, firmware, and fresh target gates pass. Review 08 is
closed and immutable.

## Next Steps

Open the next sortable review folder for Finding 12. Reuse the sampled `now`
for a shared, wrap-safe RPC outage/backoff gate while preserving dirty state.
