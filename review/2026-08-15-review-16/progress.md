# Runtime Lookup Hot-Path Progress

## Why This Review Exists

Review 15 is closed and remains immutable. Finding 16 is a distinct runtime
lookup and active-state optimization topic, so it uses Review 16.

## 2026-08-15 — Audit Baseline

- Work began from a clean `sol` branch after commit `1877184c`.
- The repository's `prompts/initial-architecture-review.md` template guided
  the architecture pass.
- One `handled_key_lookup_tap_count_into()` call currently performs two
  authored searches for the first tap and three for later taps.
- Reducer observation performs that resolution before the process pipeline,
  after which preflight and handled-stage routing can resolve the same key
  again.
- Matched release routing can resolve authored data despite the token already
  owning the materialized interaction.
- `tap_series_t.has_more_taps` is cached but acceptance re-queries the authored
  table.
- Matrix-capacity loops and scan-driven feedback dirtiness remain candidates;
  they will not be rewritten without operation-count evidence.

## Implementation Sequence

1. [x] Add host-only authored-search and row-comparison counters.
2. [x] Enforce one search for a direct handled-key resolution.
3. [x] Propagate observed token state through normal press and matched release.
4. [x] Enforce one primary search per normal press and zero per matched release.
5. [x] Measure active-slot visits and implement the justified active bitmap.
6. [x] Measure feedback dirty notifications and implement deadline-safe invalidation.
7. [x] Run targeted, full-host, target, resource, and stack verification.
8. [x] Reconcile Finding 16 documentation and record a closure verdict.

## Red Evidence

- The direct handled-resolution budget initially failed because first taps
  searched twice and later taps searched three times.
- The real-profile event budget initially failed because observation,
  preflight, and handled-stage routing each resolved the same press; matched
  release also resolved again.
- Reusing the old materialized-source `has_more_taps` field for authored
  series acceptance broke the PINCH transparent double-tap path. The landed
  design keeps those meanings separate with `authored_has_more_taps`.
- The first dirty-notification budget failed because an unchanged active scan
  still called `split_runtime_sync_mark_key_feedback_dirty()` once.
- A fresh instrumented target build initially rejected five stale manifest
  adjacencies. The linked image showed that four release paths now pass through
  `key_runtime_transition_handled_key_release()` and non-handled cleanup now
  passes through `key_runtime_process_resolution()`; the manifest was updated
  to describe those real linked edges and then passed cleanly.

## Implemented

- Added test-only authored-search and row-comparison counters with no target
  storage or runtime cost.
- Added config-pointer view helpers so one primary lookup derives the selected
  step and remaining-tap state.
- Made observed press tokens authoritative for normal preflight and handled
  routing.
- Made matched releases settle from the cached interaction before any
  unmatched-release fallback lookup.
- Preserved token-allocation failure and unmatched-release fallback behavior.
- Cached authored-series continuation separately from transparent materialized
  tap-source continuation.
- Added two compact 60-slot active bitmaps while keeping position-indexed
  arrays authoritative.
- Centralized press-token and tap-series bitmap lifecycle updates and added a
  host verifier that recomputes arrays, counts, and bitmaps.
- Reduced a one-active-press scan from 60 refresh + 60 press + 60 series visits
  to one refresh + one press visit. A pending tap series similarly costs one
  refresh + one series visit; idle remains zero.
- Added a host-only split-feedback mark counter and enforced zero dirty marks
  for an unchanged active scan.
- Replaced scan-wide invalidation with a feedback-sequence comparison for
  no-effect-plan state transitions. A release-hold deadline crossing now marks
  dirty exactly once; the following unchanged scan marks zero times.
- Kept plan-driven invalidation in transition projection. Split sync's existing
  `last_sent_active` rule continues rebuilding an active semantic/branch packet
  until it becomes inactive, preserving flash visibility and expiry updates
  without scan-wide dirty marks.
- Reconciled the reviewed-path stack manifest against the fresh post-LTO call
  graph rather than adding indirect assertions or suppressing adjacency checks.

## Verification Passed

- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_memory_budget_checks.sh`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
- `git diff --check`

## Final Target Evidence

- Ordinary target size: 152,232 B text, 0 B data, 245,336 B ELF BSS.
- Static BSS span remains 25,524 B.
- Linker heap is 212,352 B, above the 200 KiB gate.
- The active bitmaps add exactly 16 bytes to `key_runtime_core_state_t`:
  21,780 B to 21,796 B. Cached authored-branch booleans fit existing padding.
- Compared with Finding 15's 151,000 B text checkpoint, Finding 16 adds 1,232 B
  of code. Static BSS remains within the existing 25,524 B span despite the
  16-byte state addition.
- The fresh reviewed-path main-process maximum is 1,912/1,920 B (`matrix scan
  bounded deferred fallback settlement`). The split-worker maximum is
  336/768 B (`VIA pushed macro fragment write`). This is explicit reviewed-path
  regression evidence, not a proof of the global maximum stack use.
- No sibling QMK source was edited.

## Closure Verdict

Finding 16 is **resolved and verified**. The code matches the selected
position-owned lookup, active-bitmap, and deadline-safe invalidation design;
host enforcement, full-host verification, the ordinary firmware build, memory
gate, fresh post-LTO reviewed-path stack gate, and documentation reconciliation
all pass.

## Next Steps

- Keep this closed review immutable.
- Complete Finding 05's physical split persistence matrix and Finding 10's
  flashed pointing timing/feel check when hardware is available.
