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

1. Decide whether pending multi-tap release resolution should move behind the release planner too, or stay in `runtime.c` as state-gathering reducer logic.
2. Write an ownership authority map for core leases and subsystem ledgers.
3. Split more of `runtime.c` into smaller internal modules after tests prove parity.
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

Reconciliation note: active-release and pending multi-tap effect planning was moved out of `runtime.c` in the later 2026-04-28 release effect planner extraction recorded below. Pending-release queue ownership remains open.

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

1. Decide whether pending multi-tap release resolution should move behind the release planner too, or stay in `runtime.c` as state-gathering reducer logic.
2. Add or preserve host coverage that proves pending-release queue ownership and deferred dispatch ordering before changing the pending-release queue.
3. Keep the release finding open until full host tests and firmware compile pass after the remaining release resolution or pending-release queue work.

## 2026-04-28 - Release Effect Planner Extraction

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/key/runtime/core/release_planner.c` for active-release and pending multi-tap release effect planning.
- Removed the corresponding release effect-plan push helpers and release effect planning functions from `users/noah/lib/key/runtime/core/runtime.c`.
- Exposed narrow internal helpers through `users/noah/lib/key/runtime/core/release_internal.h` so the release planner can validate key positions, inspect tap series, start branch-confirm windows, reset pending multi-tap state, and apply tap-commit feedback policy without duplicating that logic.
- Wired the new source file into `users/noah/source_manifest.mk`.
- Updated host source manifests and manual key-runtime integration runners so host compile gates include `release_planner.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` to describe the new split.

## Finding Status

- Release semantics remain partially resolved.
- Resolved in this pass:
  - active-release effect planning now lives in `release_planner.c`
  - pending multi-tap release effect planning now lives in `release_planner.c`
  - source manifests and host runners mechanically include the new planner source
- Still open:
  - pending multi-tap release resolution still lives in `runtime.c`
  - pending-release queue ownership remains in `runtime.c`

Reconciliation note: active-release and pending multi-tap release resolution moved out of `runtime.c` in the later 2026-04-28 release resolver consolidation recorded below. Pending-release queue ownership remains open.

## Verification

Pre-change baseline:

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`

Post-change targeted checks:

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`

Final checks:

- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/release_planner.c` produced no whitespace diagnostics for the new source file

All listed pass/fail commands passed. The `--no-index` whitespace check returned the expected nonzero diff status for a new file and produced no diagnostics.

## Next Steps

1. Decide whether pending multi-tap release resolution should move behind the release planner too.
2. Preserve pending-release queue ownership coverage before moving that queue out of `runtime.c`.
3. Keep `runtime.c` as the orchestration point while extracting remaining release resolution only if the planner can own the contract without duplicating core state facts.

## 2026-04-28 - Release Resolver Consolidation

Starting worktree status:

- This pass continued the in-flight release consolidation diff from the release effect planner extraction.

## Completed

- Moved active-release resolution from `users/noah/lib/key/runtime/core/runtime.c` into `users/noah/lib/key/runtime/core/release_planner.c`.
- Moved pending multi-tap release resolution from `runtime.c` into `release_planner.c`.
- Left `runtime.c` as the canonical state owner for press tokens, tap series, pending multi-tap scan progression, and pending-release queue transport.
- Exposed `key_runtime_core_owner_has_lease_kind()` through `release_internal.h` so the planner can resolve active release decisions without duplicating lease traversal.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so they describe release decisions as planner-owned.

## Finding Status

- Release semantics are now consolidated behind the core release planner.
- Resolved in this pass:
  - active-release resolution now lives in `release_planner.c`
  - pending multi-tap release resolution now lives in `release_planner.c`
  - `runtime.c` no longer calls the release semantic reducer directly
- Still open:
  - pending-release queue storage and draining remain split across `runtime.c`, `transition.c`, and `release.c`
  - the review finding stays partially open until that transport boundary is either accepted as core-owned or moved behind a smaller release transport API

## Verification

Pre-change baseline:

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`

Post-change targeted checks:

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

All listed pass/fail commands passed.

## Next Steps

1. Decide whether the pending-release queue/drain path should remain core-owned transport or move behind a smaller release transport API.
2. Keep `release.c` thin unless the transition/process layers can absorb the release adapter without making event flow harder to read.
3. Use the current release matrix, modifier-hold, PD-mode, scenario, layer-lock, and full host suite as the guardrail for any pending-release transport cleanup.
