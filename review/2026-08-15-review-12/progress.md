# Pointing Tap-Backlog Progress

## Why This Review Exists

Review 10 remains open only for Finding 05's physical hardware matrix, while
Review 11 is closed and immutable. Finding 10 is the next roadmap item and a
materially distinct bounded-work topic, so it uses Review 12.

## 2026-08-15 — Baseline

Passed on clean commit `f528fc36` before Finding 10 changes:

- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_owned_keycode_tests.sh`

## Audit Findings

- One maximum report can dispatch roughly 820 arrow, 547 volume/brightness,
  or 410 zoom taps synchronously.
- Accumulation is signed and unbounded; sustained legal reports can eventually
  overflow `int32_t`.
- Direction reversal and mode reset are already the correct invalidation
  seams and should be preserved.
- The pinned QMK pointing task invokes the active handler on successful
  zero-motion polls, so retained debt can drain through the existing path.
- Arrow dominance negates `int16_t` directly and cannot represent the magnitude
  of `INT16_MIN`.
- Existing tests cover ordinary thresholds, direction changes, modifier
  handling, and reset-adjacent behavior, but not extreme reports, bounded
  draining, overload policy, or extreme signed magnitudes.

## In Progress

1. Add failing maximum-report and `INT16_MIN` regressions.
2. Implement a four-tap per-call budget with a 32-tap retained-debt cap.
3. Preserve exact residual motion and expose saturation/backlog diagnostics.
4. Add compile-time configuration guards and full boundary coverage.

## Verification Still Required

- all Finding 10 targeted pointing and ownership runners;
- compile-fail configuration probes and feature-gate compile tests;
- full host suite;
- ordinary Charybdis firmware build;
- fresh reviewed-path target stack gate;
- `git diff --check`.

## Next Steps

Capture the on-device worst-case duration and subjective 4/32 backlog behavior.
All software and linked-target work below is complete.

## 2026-08-15 — Software and Target-Build Implementation Complete

### Landed behavior

- Added a four-tap budget shared by each active discrete-mode invocation.
- Retained at most 32 whole taps plus the exact sub-threshold residual.
- Added deterministic whole-tap dropping with saturating per-activation
  diagnostics for saturation count, dropped taps, maximum backlog, and maximum
  taps emitted.
- Preserved reversal clearing, zero-report draining, complete mode reset,
  frozen reports, thresholds, keycodes, DPI, and ownership-safe dispatch.
- Widened arrow magnitude before negation so `INT16_MIN` is exact while strict
  dominance and tie retention stay unchanged.
- Made the shared helper self-contained for standalone compile variants and
  mirrored QMK's extended report widths in the host fixture.
- Added explicit target stack paths for generic and vertical-arrow fallback
  settlement.

### Red/green and failure evidence

- The first extreme-report run initially failed to compile because the host
  fixture incorrectly modeled extended reports as 8-bit. After fixing that
  contract, the unchanged emitter failed by overflowing the eight-entry call
  log on one maximum report, proving the unbounded burst.
- The first full-suite run then found standalone pointing compile variants
  without profile config; guarded shared defaults fixed that wiring defect.
- The next full-suite run correctly found stale generated profile docs after
  the two authored config constants were added; introspection output was
  regenerated and checked.
- A target force-inline experiment was measured and rejected: text grew by
  312 B, arrow code expanded, and software division remained. The smaller
  shared helper was restored.

### Verification passed

- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_owned_keycode_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
- `git diff --check`

### Target evidence

- Ordinary target: 150,908 B text, 0 B data, 245,584 B BSS.
- Delta from Finding 11: +160 B text, unchanged BSS.
- Generic bounded fallback path: 1,240/1,920 B.
- Vertical-arrow bounded fallback path: 1,280/1,920 B.
- Overall reviewed maxima: 1,904/1,920 B main and 336/768 B split.

## Remaining Closure Gate

Review 12 remains open. Flash the build, record worst-case discrete-handler
duration, and validate the 4/32 policy's feel across all four discrete modes.
No software, host, ordinary target-build, or target-stack failure remains.
