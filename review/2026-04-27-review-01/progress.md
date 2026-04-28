# Progress

## 2026-04-27 - Review Opened

Used `prompts/initial-architecture-review.md`.

This folder was opened as a new review thread because the newest active review folder, `review/2026-04-23-review-04/`, is scoped to RGB runtime architecture. This audit is materially different: it reviews every `.c` and `.h` file under `users/noah` for ad-hoc patches, duplicated responsibilities, and overlapping runtime authority.

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Read the newest review folder before starting this review.
- Enumerated all `users/noah` `.c` and `.h` files.
- Inspected 161 files by subsystem:
  - root userspace headers and hooks
  - action dispatch and lifecycle
  - QMK/VIA/pointing compatibility
  - handled-key interaction and validation
  - held ownership registries
  - key runtime core, process, release, trace, and API wrappers
  - macro payload, dispatch, and VIA providers
  - pointing-device modes and PD runtime
  - RGB runtime, validation, automouse, and stages
  - runtime shared state, ownership, diagnostics, trace, and split sync
- Created `userspace-architecture-review.md` with findings, current architecture assessment, recommended refactor sequence, and a per-file inventory.

## Findings Summary

- No immediate must-fix runtime bug was proven by the static pass.
- The highest-risk clutter is concentrated in key runtime authority:
  - `users/noah/lib/key/runtime/core/runtime.c`
  - `users/noah/lib/key/runtime/core/runtime.h`
  - release planning and slot release helpers
  - owner ledgers for held actions, repeats, layers, keyboard modifiers, and PD modes
- The biggest compatibility patch is `users/noah/lib/compat/qmk_combo_origin.c`.
- Modifier mask/replay behavior is split across action dispatch, key process, delayed action, and PD mode modules.
- RGB and macro code are comparatively cohesive, with mostly low-risk duplicate helper shapes.

## In Flight

- None.

## Verification

- `git diff --check` passed for the tracked diff.
- `git diff --check --no-index /dev/null review/2026-04-27-review-01/userspace-architecture-review.md` produced no whitespace diagnostics for the new review file.
- `git diff --check --no-index /dev/null review/2026-04-27-review-01/progress.md` produced no whitespace diagnostics for the new progress file.
- Host tests and `qmk compile` are intentionally skipped for this review-note-only pass because no runtime source, authored profile data, build wiring, generated firmware input, or behavior changed.

## Next Steps

1. Continue release consolidation by moving active-release and pending multi-tap effect planning out of `runtime.c`.
2. Write an ownership authority map for core leases and subsystem ledgers.
3. Split `runtime.c` into smaller internal modules after tests prove parity.
4. Tighten the `qmk_combo_origin` compatibility contract and coverage.
5. Centralize keyboard modifier mask/replay policy.
6. Clean up low-risk duplicate helpers after the authority and release work is stable.

## 2026-04-28 - Release Planner Boundary Pass

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Removed the derived release contract and helper surface from `users/noah/lib/key/runtime/interaction.h`.
- Replaced `users/noah/lib/key/runtime/slot/release_resolver.h` with `users/noah/lib/key/runtime/core/release_planner.h`.
- Renamed release decision types and enum values so active and pending release code now call the core release planner API instead of a slot release resolver.
- Moved foreign tap-release deferred-dispatch adaptation out of `release.c` and into `transition.c`.
- Kept `release.c` as a thin release-event adapter plus deferred-release drain.
- Updated `docs/KEY_RUNTIME.md` to point at `core/release_planner.h` instead of the removed slot release resolver.
- Updated `review/2026-04-27-review-01/userspace-architecture-review.md` so the release finding and file inventory match the current tree.

## Finding Status

- Release semantics are partially resolved.
- Resolved in this pass:
  - quick-release, fallback suppression, buffered base tap, nonquick release, and hold-action selection now live behind `users/noah/lib/key/runtime/core/release_planner.h`
  - `interaction.h` no longer exposes derived release contract structs
  - `release.c` no longer owns the post-plan dispatch deferral scan
- Still open:
  - active-release and pending multi-tap effect planning still live in `runtime.c`
  - pending-release queue ownership remains in `runtime.c`

## Verification

Pre-change baseline:

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`

Post-change targeted checks:

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/release_planner.h` produced no whitespace diagnostics for the new header

All listed pass/fail commands passed. The `--no-index` whitespace check returned the expected nonzero diff status for a new file and produced no diagnostics.

## Next Steps

1. Extract active-release and pending multi-tap effect planning from `runtime.c` into the release planner/internal release module.
2. Add or preserve host coverage that proves pending-release queue ownership and deferred dispatch ordering during that extraction.
3. Keep the release finding open until full host tests and firmware compile pass after the effect-planning extraction.
