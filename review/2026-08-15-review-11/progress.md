# Dragscroll Stall-Recovery Progress

## Why This Review Exists

Review 10 remains open only for Finding 05's manual hardware matrix. Finding
11 is a materially different Phase 4 gesture-lifecycle topic, so it uses the
next sortable review folder. The overarching roadmap requires Finding 11 before
Finding 10; stale next-action text that named Finding 10 first is reconciled in
this pass.

## 2026-08-15 — Baseline

Passed on the clean `sol` branch before implementation:

- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

## Audit Findings

- Current motion is accumulated and refreshes `last_motion_time` before stale
  lock or residual state is evaluated.
- A 56–80 ms gap can preserve the old lock into a new gesture; an 81+ ms gap
  can preserve both old lock and residual motion.
- One handler invocation uses one direct timer read plus up to three elapsed
  helper calls, so boundary decisions need not share one timestamp.
- Existing tests cover ordinary lock switching, jitter, decay, divisors, and
  reset, but not first-report-after-gap, exact deadlines, wrap, no-motion
  expiry, pinch reuse, or timer-call budgets.
- The private state/reset boundary and shared DRAGSCROLL/PINCH registry wiring
  are suitable and should remain unchanged.

## In Progress

1. Add deterministic failing regressions and timer-call instrumentation.
2. Expire prior lock/buffers before accumulating current motion.
3. Replace elapsed-helper reads with unsigned ages from one sampled `now`.

## Verification Still Required

- all Finding 11 targeted pointing runners;
- feature-gate compile tests;
- full host suite;
- ordinary Charybdis firmware build;
- fresh reviewed-path target stack gate;
- `git diff --check`.

## Next Steps

Land the red/green lifecycle tests, record exact boundaries and timer-call
reduction, run every closure gate, then reconcile Finding 11 and the roadmap
before committing.

## 2026-08-15 — Implementation Complete

### Landed behavior

- Added prior-state expiry before current motion accumulation.
- Preserved inclusive 55 ms lock and 80 ms residual boundaries; expiry begins
  at 56 ms and 81 ms respectively.
- Replaced three possible elapsed-helper reads with ages derived from one
  `timer_read32()` sample.
- Preserved immediate first-report classification/emission, the 8 ms inclusive
  rate boundary, unsigned timer wrap, complete reset, and shared
  DRAGSCROLL/PINCH handler ownership.
- Kept all thresholds, ratios, divisors, direction flags, DPI, and rate values
  unchanged.

### Red/green evidence

`sh tests/host/run_pd_mode_handlers_tests.sh` failed first at the 56 ms
post-stall assertion because the old implementation emitted the retained
horizontal step. It passed after expiry moved before accumulation. The final
fixture measures one direct timer read and zero elapsed-helper calls per
handler invocation, down from one direct read plus up to three elapsed reads.

### Verification passed

- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` (15 tests)
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
- `git diff --check`

### Target evidence

- Ordinary target: 150,748 B text, 0 B data, 245,584 B BSS (unchanged from
  Finding 05).
- Explicit post-LTO dragscroll path: 360/1,920 B.
- Overall reviewed maxima: 1,904/1,920 B main and 336/768 B split.
- The manifest documents the pd-mode registry callback adjacency from
  `pointing_device_task` to `handle_dragscroll_mode`.

## Closure Verdict

Finding 11 is **resolved and closed**. Code, enforcement, documentation, full
host, ordinary target, and fresh reviewed-path stack evidence agree. This
folder is immutable closure history; Finding 10 belongs in the next sortable
review folder.
