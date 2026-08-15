# RGB Render-Work Progress

## Why This Review Exists

Review 13 is closed and remains immutable. Finding 15 is a distinct RGB
performance and frame-coherence topic, so it uses Review 14.

## 2026-08-15 — Audit Baseline

- Finding 13 is committed at `6ff13f46` and supplies the preview color-parity
  oracle.
- QMK's advanced callback range is produced by
  `rgb_matrix_get_limits(params->iter - 1)`.
- Comparing the incoming range with `rgb_matrix_get_limits(0)` gives an exact
  frame-start signal for the pinned integration, including the right half's
  initially inverted split-clamped range.
- The current runtime has no top-level physical-half fast exit.
- Combo projections are rebuilt by separate underlay, overlay, and key
  suppression consumers for every chunk.
- Key feedback rebuilds its semantic source while deriving flash visibility.

## Implementation Sequence

1. [x] Add a 58-LED five-chunk parity and work-count fixture.
2. [x] Add exact frame-start invalidation and physical-half normalization.
3. [x] Add one lazy runtime-owned source snapshot per frame.
4. [x] Pass immutable snapshot fields into combo and key-feedback painters.
5. [x] Run targeted, full-host, target, resource, and stack verification.
6. [x] Reconcile Finding 15 documentation and record the closure verdict.

## Red Work-Budget Evidence

Before production changes, `sh tests/host/run_rgb_layer_render_tests.sh`
failed in the new 58-LED workload at the one-semantic-build assertion. The
audited five-callback baseline performed 10 semantic builds and 20 combo
projection invocations, and entered the stage pipeline for all five global
chunks on each half.

## Implementation

- Exact equality with `rgb_matrix_get_limits(0)` invalidates the source
  snapshot at every QMK RGB frame boundary.
- The boundary check runs before physical normalization, so the right half's
  initial `(29, 12)` range still starts a new frame and then exits.
- One lazy snapshot stores combined combo partitions plus semantic,
  tap-branch, flash-visibility, and broad-owner truth.
- Flash visibility consumes the stored semantic map. Combo suppression consumes
  the stored underlay and overlay without another combined temporary.
- Stage painters consume immutable arrays and no longer allocate or fetch
  their source projections per chunk.
- Mid-frame local and remote source mutations remain invisible until the next
  exact frame boundary, preventing mixed-generation output.

## Verification Passed

- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -c -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
- `git diff --check`

## Target Evidence

- Five chunks now produce one semantic, one tap-branch, one flash, one
  broad-owner, and one combined combo projection build.
- Each half enters three local stage pipelines and rejects two nonlocal chunks.
- The linked source snapshot is 80 B and has a 96 B compile-time ceiling.
- Ordinary image: 151,000 B text, 0 B data, 245,592 B BSS.
- Delta from Finding 13: +104 B text, unchanged linked BSS.
- Fresh reviewed maxima: 1,904/1,920 B main and 336/768 B split.
- No sibling QMK source was edited.

## Closure Verdict

**CLOSED — Finding 15 resolved.** Code, exact QMK frame integration, mechanical
work and color enforcement, documentation, the complete host suite, ordinary
target build, resource measurement, and fresh reviewed-path stack gate agree.
This folder is immutable closure history after the Finding 15 commit.
