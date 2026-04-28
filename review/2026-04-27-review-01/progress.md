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

Reconciliation note: deferred-release deferral and draining moved into `deferred_release.c` in the later 2026-04-28 deferred release transport adapter pass recorded below. Pending-release queue storage remains core-owned state by design.

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

## 2026-04-28 - Deferred Release Transport Adapter

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/key/runtime/deferred_release.h` and `users/noah/lib/key/runtime/deferred_release.c` as the release transport adapter.
- Moved blocked-release dispatch deferral out of `transition.c` and into `deferred_release.c`.
- Moved deferred-release draining out of `release.c` and into `deferred_release.c`.
- Kept pending-release queue storage, ordering, token cleanup, and projection primitives core-owned in `runtime.c`.
- Updated `release.c` and `scan.c` so both call the deferred-release adapter after executing transition plans.
- Removed deferred-release transport from `transition.h` and removed the old drain declaration from `process_internal.h`.
- Wired the new source file into `users/noah/source_manifest.mk`, host source manifests, and manual key-runtime integration runners.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` to describe the final release ownership split.

## Finding Status

- Release semantics and release transport are resolved.
- Resolved in this pass:
  - release semantics are planner-owned
  - blocked-release dispatch deferral has one adapter
  - deferred-release draining has one adapter
  - `release.c` and `transition.c` no longer own deferred-release queue adaptation
- Accepted boundary:
  - `runtime.c` owns pending-release queue storage because it is canonical reducer state tied to press tokens and blocker queries

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
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/deferred_release.c`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/deferred_release.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks returned the expected nonzero diff status for new files and produced no diagnostics.

## Next Steps

1. Continue with the broader `runtime.c` accumulator finding: ownership ledgers, PD bridge behavior, feedback projection, and pending multi-tap scan are still candidates for focused internal modules.
2. Preserve the release matrix and deferred-release debug coverage as guardrails for future runtime splits.
3. Avoid reintroducing release decisions in transition, press/release wrappers, or slot-owned helpers.

## 2026-04-28 - Ownership Authority Map

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added an ownership authority map to `docs/KEY_RUNTIME.md`.
- Labeled reducer-owned press tokens, tap series, leases, persistent intents, pending releases, release planner decisions, feedback pulse state, and shadow projection as the key-runtime authority surface.
- Labeled `held_action.c`, `held_repeat.c`, `layer_ownership.c`, `keyboard_mod_ownership.c`, `pd_mode_state.c`, pointer layer policy, feedback/RGB projection, and split-facing state as projected or externally authoritative surfaces.
- Labeled `compat/qmk_combo_origin.c` and `origin_registry.c` as compatibility-only origin recovery, not key-runtime ownership.
- Updated `userspace-architecture-review.md` so the multiple-owner-ledgers finding is partially resolved instead of fully open.

## Finding Status

- Multiple owner ledgers are partially resolved.
- Resolved in this pass:
  - the intended ownership direction is documented
  - projected registries are named as QMK/action/layer/modifier/PD side-effect sinks, not independent key-runtime truth
  - combo-origin recovery is documented as compatibility-only
- Still open:
  - `layer_ownership_set_lock_state()` still updates external layer lock state and then refreshes core state
  - `pd_mode_set_lock_state_at()` still updates PD runtime state and then refreshes core state
  - core effect projection still coordinates several outward writes directly from `runtime.c`

## Verification

- `git diff --check`

Host tests and `qmk compile` are intentionally skipped for this docs/review-only
pass because no runtime source, authored profile data, build wiring, generated
firmware input, or behavior changed.

## Next Steps

1. Use the ownership authority map to choose one code target, preferably the PD lock bridge or core lease projection split.
2. Preserve PD mode integration, PD runtime, pointer layer policy, split sync, modifier-hold, layer-lock, runtime-debug, full host, and firmware compile coverage for any runtime source changes.
3. Keep future projected registries from adding new key-runtime owner state unless the authority map and tests are updated in the same pass.

## 2026-04-28 - PD Key-Runtime Bridge Extraction

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.h` and `users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c`.
- Moved the direct PD lock write-back out of `pd_mode_state.c` and into the bridge.
- Updated `pd_mode_state.c` so it no longer includes `users/noah/lib/key/runtime/core/runtime.h` directly.
- Wired the new bridge source into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and the manual host runners that compile `pd_mode_state.c`.
- Added feature-gate checks so production PD runtime code outside `pd_mode_key_runtime_bridge.c` cannot include `key/runtime/core/runtime.h` or write directly into key-runtime core.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` to describe the bridge as an explicit transition point.

## Finding Status

- PD mode authority is partially resolved.
- Resolved in this pass:
  - the direct PD runtime dependency on key-runtime core is isolated behind one bridge file
  - compile gates mechanically enforce that `pd_mode_state.c` does not reintroduce the direct write-back
  - the active review and key-runtime docs name the remaining bridge
- Still open:
  - PD runtime still owns actual PD mode state while key runtime owns PD intent
  - lock state still writes back into core after PD runtime changes local state
  - core effect projection still calls PD lock/toggle behavior directly

## Verification

Pre-change baseline:

- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`

Post-change targeted checks:

- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c`
- `git diff --check --no-index /dev/null users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks
returned the expected nonzero diff status for new files and produced no
diagnostics.

## Next Steps

1. Decide whether the remaining PD lock write-back should become a core-owned command completion event or a PD-owned snapshot observation.
2. Split core PD effect projection only after the bridge semantics are covered by PD mode integration, PD runtime, pointer policy, split sync, runtime debug, full host, and firmware compile checks.
3. Keep `pd_mode_state.c` free of direct key-runtime core includes and direct core lock-shadow writes.

## 2026-04-28 - PD Lock Observation Contract

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Renamed the key-runtime receiver from `key_runtime_core_pd_mode_lock_set()` to `key_runtime_core_observe_pd_mode_lock_state()`.
- Renamed the bridge entrypoint to `pd_mode_key_runtime_bridge_observe_local_lock_state()`.
- Centralized local PD lock/unlock handling through `pd_mode_apply_local_lock_state_at()` so public set/toggle and internal lock/unlock helpers observe changed local lock state through the same bridge.
- Updated feature-gate checks so PD runtime code outside the bridge cannot call `key_runtime_core_observe_pd_mode_lock_state()` directly.
- Updated scenario/stub tests, key-runtime docs, and the active architecture review to describe the Option 1 contract: PD runtime owns actual local PD state; key runtime observes changed local lock state for projection.

## Finding Status

- PD mode authority remains partially resolved, but the intended direction is now explicit in code names and enforcement.
- Resolved in this pass:
  - the core API is named as an observation, not an ownership setter
  - all local PD lock entrypoints share one observation path
  - feature gates enforce the bridge as the only PD-runtime caller of the key-runtime observation API
- Still open:
  - key runtime still directly projects PD lock/toggle effects from `runtime.c`
  - PD held-action preemption still lives in `runtime.c`
  - the next split should move core-side PD projection/preemption behind a small internal module without changing PD runtime ownership

## Verification

Pre-change baseline:

- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Post-change targeted checks:

- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

All listed pass/fail commands passed.

## Next Steps

1. Split the core-side PD projection/preemption code out of `runtime.c` behind an internal key-runtime PD projection module.
2. Keep PD runtime as the owner of actual local/display/remote/split mode state.
3. Preserve PD mode integration, PD runtime, pointer policy, split sync, runtime debug, scenario, full host, and firmware compile coverage for that split.

## 2026-04-28 - Core PD Projection Split

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/key/runtime/core/pd_projection.h` and `users/noah/lib/key/runtime/core/pd_projection.c`.
- Moved core-side PD held-action preemption and PD lock-tap projection out of `runtime.c`.
- Kept `runtime.c` as the effect projection orchestrator while delegating PD-specific effect projection to `pd_projection.c`.
- Wired the new source into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and the manual key-runtime/PD integration runners that compile `runtime.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so the active review now treats the PD authority boundary as explicit and resolved.

## Finding Status

- PD mode authority is resolved.
- Resolved in this pass:
  - PD runtime remains the owner of actual local/display/remote/split PD state
  - key runtime owns key-driven PD intent/projection through `pd_projection.c`
  - changed local PD lock state is still observed into key runtime through the PD/key-runtime bridge
  - source manifests and host runners mechanically include the new module
- Still open:
  - the broader key-runtime accumulator finding remains partially resolved
  - feedback projection, tap-series flushing, projection snapshot comparison, and some owner-ledger boundaries remain in or near `runtime.c`

## Verification

Pre-change baseline:

- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Post-change targeted checks:

- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/pd_projection.c`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/pd_projection.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks
returned the expected nonzero diff status for new files and produced no
diagnostics.

## Next Steps

1. Continue reducing the broader `runtime.c` accumulator by choosing one remaining owner: feedback projection, tap-series flush, projection snapshot comparison, or lease projection.
2. Keep the PD authority boundary fixed: PD runtime owns actual PD state, key runtime owns key-driven PD projection, and the bridge observes local PD lock changes.
3. Preserve full host and firmware compile checks for further runtime splits.

## 2026-04-28 - Core Feedback Projection Split

Starting worktree status:

- `git status --short` showed the two new `feedback_projection` files already present as untracked working-tree entries from the interrupted pass and no unrelated entries.

## Completed

- Added `users/noah/lib/key/runtime/core/feedback_projection.h` and `users/noah/lib/key/runtime/core/feedback_projection.c`.
- Moved feedback pulse queueing and feedback pulse observation out of `runtime.c`.
- Kept `runtime.c` as the effect projection orchestrator while delegating `KEY_RUNTIME_EFFECT_FEEDBACK_PULSE` to `feedback_projection.c`.
- Wired the new source into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and the manual key-runtime/PD integration runners that compile `runtime.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so feedback pulse projection is documented as an explicit core projection module.

## Finding Status

- The broader key-runtime accumulator finding remains partially resolved.
- Resolved in this pass:
  - feedback pulse projection now has a named internal core module
  - feedback pulse queueing and observation are no longer local helpers inside `runtime.c`
  - source manifests and host runners mechanically include the new module
- Still open:
  - pending multi-tap scan/flush helpers remain in `runtime.c`
  - pending-release queue storage remains in `runtime.c`
  - projection snapshot capture/comparison remains in `runtime.c`
  - broad runtime state/debug surfaces remain in `runtime.h`

## Verification

Post-change targeted checks:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/feedback_projection.c`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/feedback_projection.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks
returned the expected nonzero diff status for new files and produced no
diagnostics.

## Next Steps

1. Split one remaining `runtime.c` accumulator concern next: pending multi-tap scan/flush, pending-release queue storage, or projection snapshot comparison.
2. Keep `feedback_projection.c` limited to feedback pulse projection and queueing; do not let it become a general feedback/RGB renderer.
3. Preserve runtime debug, trace, scenario, release matrix, full host, compile-gate, and firmware compile coverage for the next runtime split.

## 2026-04-28 - Tap-Series Flush Split

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/key/runtime/core/tap_series.h` and `users/noah/lib/key/runtime/core/tap_series_flush.c`.
- Moved pending multi-tap flush resolution, tap-series flush extraction, delayed branch-confirm completion, and foreign/global multi-tap flush loops out of `runtime.c`.
- Added `users/noah/lib/key/runtime/core/effect_plan.h` so internal core modules append delayed-action and feedback effects through the existing core effect-plan helpers instead of hand-editing plan storage.
- Kept `tap_series_t` storage in `key_runtime_core_state_t`; the new module operates on core state and does not introduce a second tap-series truth.
- Wired `tap_series_flush.c` into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and the manual key-runtime/PD integration runners that compile `runtime.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so pending multi-tap flush planning is documented as an explicit core module.

## Finding Status

- The broader key-runtime accumulator finding remains partially resolved.
- Resolved in this pass:
  - foreign and global pending multi-tap flush loops now live in `tap_series_flush.c`
  - flush resolution and tap-series flush extraction are no longer local helpers inside `runtime.c`
  - delayed branch-confirm action completion is shared through the tap-series helper instead of being duplicated by flush paths
  - source manifests and host runners mechanically include the new module
- Still open:
  - pending multi-tap scan threshold planning remains in `runtime.c`
  - pending-release queue storage remains in `runtime.c`
  - projection snapshot capture/comparison remains in `runtime.c`
  - broad runtime state/debug surfaces remain in `runtime.h`

## Verification

Post-change targeted checks:

- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/effect_plan.h`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/tap_series.h`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/tap_series_flush.c`

All listed pass/fail commands passed. The `--no-index` whitespace checks
returned the expected nonzero diff status for new files and produced no
diagnostics.

## Next Steps

1. Continue with one remaining accumulator concern: pending multi-tap scan threshold planning, pending-release queue storage, or projection snapshot comparison.
2. Keep `tap_series_flush.c` limited to tap-series flush/branch-confirm delayed-action completion; threshold scan policy should move separately if it is split.
3. Preserve scenario, release matrix, integration harness, runtime debug/trace, full host, compile-gate, and firmware compile coverage for the next split.

## 2026-04-28 - Projection Snapshot Split

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/key/runtime/core/projection.c`.
- Moved projection snapshot capture, snapshot comparison, and trace projection checkpoint capture out of `runtime.c`.
- Kept core shadow projection state in `key_runtime_core_state_t`; the new module observes the core state and external debug snapshots without introducing a second projection truth.
- Updated `users/noah/lib/key/runtime/core/projection.h` so projection snapshot capture/comparison are declared with the projection API.
- Wired `projection.c` into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and the manual key-runtime/PD integration runners that compile `runtime.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so projection snapshot capture/comparison is documented as an explicit core projection module.

## Finding Status

- The broader key-runtime accumulator finding remains partially resolved.
- Resolved in this pass:
  - projection snapshot capture and comparison now live in `projection.c`
  - trace projection checkpoint capture now lives with snapshot capture
  - `runtime.c` no longer includes the debug snapshot and refcount-mask plumbing used only for projection capture
  - source manifests and host runners mechanically include the new module
- Still open:
  - pending multi-tap scan threshold planning remains in `runtime.c`
  - pending-release queue storage remains in `runtime.c`
  - broad runtime state/debug surfaces remain in `runtime.h`

## Verification

Post-change targeted checks:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/projection.c`

All listed pass/fail commands passed. The `--no-index` whitespace check
returned the expected nonzero diff status for the new file and produced no
diagnostics.

## Next Steps

1. Continue with one remaining accumulator concern: pending multi-tap scan threshold planning, pending-release queue storage, or broad runtime state/debug surfaces.
2. Keep `projection.c` limited to projection snapshots, comparison, and trace projection checkpoints; effect projection orchestration remains a separate concern.
3. Preserve runtime debug, runtime trace, integration harness, PD/key-runtime, scenario, release matrix, full host, compile-gate, and firmware compile coverage for the next split.

## 2026-04-28 - Pending Release Queue Split

Starting worktree status:

- `git status --short` showed an in-flight pending-release queue split: `runtime.c` modified and the new `pending_release_queue` source/header untracked.

## Completed

- Added `users/noah/lib/key/runtime/core/pending_release_queue.h` and `users/noah/lib/key/runtime/core/pending_release_queue.c`.
- Moved pending-release slot helpers, allocation, ordering, snapshots, drain, per-key counting, deferred/drained observations, and released-token pending-emission cleanup out of `runtime.c`.
- Kept `pending_release_slot_t` storage in `key_runtime_core_state_t`; the new module owns queue mechanics over the existing single core state truth.
- Kept release semantics in `release_planner.c` and deferred-release adaptation in `deferred_release.c`.
- Wired `pending_release_queue.c` into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and the manual key-runtime/PD integration runners that compile `runtime.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so pending-release queue mechanics are documented as an explicit core module.

## Finding Status

- The broader key-runtime accumulator finding remains partially resolved.
- Resolved in this pass:
  - pending-release queue mechanics now have a named internal core module
  - queue allocation, ordering, drain snapshots, and pending-emission token cleanup are no longer local helpers inside `runtime.c`
  - source manifests and host runners mechanically include the new module
- Still open:
  - pending multi-tap scan threshold planning remains in `runtime.c`
  - broad runtime state/debug surfaces remain in `runtime.h`
  - effect projection orchestration and some owner-ledger boundaries remain in `runtime.c`

## Verification

Post-change targeted checks:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/pending_release_queue.c`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/pending_release_queue.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks returned the expected nonzero diff status for new files and produced no diagnostics.

## Next Steps

1. Continue with pending multi-tap scan threshold planning if reducing `runtime.c` remains the priority.
2. Keep `pending_release_queue.c` limited to queue mechanics; release decisions stay in `release_planner.c` and blocked-release adaptation stays in `deferred_release.c`.
3. Preserve release matrix, modifier-hold, PD/key-runtime, scenario, layer-lock, runtime-debug, full host, compile-gate, and firmware compile coverage for the next runtime split.

## 2026-04-28 - Scan Planner Split

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/key/runtime/core/scan_planner.h` and `users/noah/lib/key/runtime/core/scan_planner.c`.
- Moved pending multi-tap scan resolution, active scan hold promotion, release-hold-pending marking, branch-confirm completion, threshold hold effect planning, and pending fallback hold settlement out of `runtime.c`.
- Kept press-token, tap-series, and lease storage in `key_runtime_core_state_t`; the new module plans over core state and does not introduce a second scan state truth.
- Moved branch-confirm window setup and same-key branch-confirm interruption into `tap_series_flush.c`, alongside existing tap-series flush mechanics.
- Moved the pending multi-tap scan resolution contract from `release_internal.h` to `scan_planner.h`.
- Expanded `effect_plan.h` so internal core modules append dispatch, held-action, repeat, release-owned-state, and deferred delayed-action effects through shared helpers.
- Wired `scan_planner.c` into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and the manual key-runtime/PD integration runners that compile `runtime.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so scan-time hold and pending multi-tap planning are documented as an explicit core module.

## Finding Status

- The broader key-runtime accumulator finding remains partially resolved.
- Resolved in this pass:
  - scan-time active hold promotion now lives in `scan_planner.c`
  - pending multi-tap scan outcomes now live in `scan_planner.c`
  - fallback hold settlement now lives in `scan_planner.c`
  - branch-confirm window setup/completion is split between tap-series helpers and scan planning instead of local `runtime.c` helpers
  - source manifests and host runners mechanically include the new module
- Still open:
  - broad runtime state/debug surfaces remain in `runtime.h`
  - effect projection and some owner-ledger boundaries remain in `runtime.c`

## Verification

Post-change targeted checks:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/scan_planner.c`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/scan_planner.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks returned the expected nonzero diff status for new files and produced no diagnostics.

## Next Steps

1. Continue with broad runtime state/debug surface cleanup if reducing `runtime.h` remains the priority.
2. Keep `scan_planner.c` limited to scan-time hold, pending multi-tap scan, and fallback-hold settlement mechanics.
3. Preserve runtime debug, runtime trace, integration harness, PD/key-runtime, scenario, release matrix, full host, compile-gate, and firmware compile coverage for further runtime splits.

## 2026-04-28 - Effect Projection Ownership Split

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Moved runtime effect execution out of `users/noah/lib/key/runtime/core/runtime.c` and into `users/noah/lib/key/runtime/core/projection.c`.
- Kept reducer-owned press-token, lease, persistent-intent, pending-release, and shadow-projection storage in `key_runtime_core_state_t`; projection only applies planned effects outward.
- Removed projection execution and snapshot API declarations from `core/runtime.h`; callers now include `core/projection.h` for projection entry points.
- Updated transition and deferred-release transport code so effect execution and pending-release dispatch projection flow through `core/projection.h`.
- Updated PD/key-runtime and real-profile integration tests to include the projection API explicitly.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so the effect projection boundary matches the current tree.

## Finding Status

- The broader key-runtime accumulator finding remains partially resolved.
- Resolved in this pass:
  - concrete `KEY_RUNTIME_EFFECT_*` execution now lives in `projection.c`
  - pending-release dispatch projection now lives in `projection.c`
  - `runtime.h` no longer exposes projection execution or snapshot APIs directly
  - transition/deferred-release transport no longer depends on projection declarations leaking from `runtime.h`
- Still open:
  - broad runtime state/debug surfaces remain in `runtime.h`
  - lease and persistent-intent mechanics remain in `runtime.c`
  - layer-lock and PD lock observation bridge points remain accepted but still worth watching

## Verification

Pre-change baseline:

- `sh tests/host/run_runtime_debug_tests.sh`

Post-change targeted checks:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

All listed pass/fail commands passed.

## Next Steps

1. Continue with broad runtime state/debug surface cleanup if reducing `runtime.h` remains the priority.
2. Consider whether lease and persistent-intent mechanics should get an internal module next, because they are the largest remaining state-management block in `runtime.c`.
3. Preserve runtime debug, scenario, release matrix, PD/key-runtime, layer-lock, modifier-hold, full host, compile-gate, and firmware compile coverage for further runtime splits.

## 2026-04-28 - Ownership State Split

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/key/runtime/core/ownership_state.h` and `users/noah/lib/key/runtime/core/ownership_state.c`.
- Moved reducer-owned lease helpers, persistent-intent updates, shadow projection recomputation, press/hold lease attachment, held/repeat observation, release-owned-state cleanup, non-handled release finalization, held/repeat feedback visibility queries, layer-lock updates, and PD lock observation out of `users/noah/lib/key/runtime/core/runtime.c`.
- Kept press-token, lease, persistent-intent, and shadow-projection storage in `key_runtime_core_state_t`; the new module manages the existing core state instead of introducing another ownership truth.
- Wired `ownership_state.c` into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and the manual key-runtime/PD integration runners that compile `runtime.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so the ownership authority map and active review match the current tree.

## Finding Status

- The broader key-runtime accumulator finding remains partially resolved.
- Resolved in this pass:
  - reducer-owned lease mechanics now have a named internal core module
  - persistent-intent mechanics now have a named internal core module
  - shadow projection recomputation and lock observation updates are no longer embedded in the main reducer file
  - held/repeat feedback visibility queries now live with the lease mechanics they inspect
  - source manifests and host runners mechanically include the new module
- Still open:
  - broad runtime state/debug surfaces remain in `runtime.h`
  - `runtime.c` still owns press/tap orchestration, scan orchestration, and effect-plan construction helpers
  - modifier mask/replay and combo-origin compatibility remain separate review findings

## Verification

First targeted check:

- `sh tests/host/run_runtime_debug_tests.sh` initially failed because `ownership_state.c` used `key_runtime_core_keypos_valid()` before that internal helper was declared outside `runtime.c`.
- Fixed by declaring `key_runtime_core_keypos_valid()` through `ownership_state.h` and including that header from `release_internal.h`.
- `sh tests/host/run_runtime_debug_tests.sh` passed after the declaration fix.

Post-change targeted checks:

- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/ownership_state.c`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/ownership_state.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks returned the expected nonzero diff status for new files and produced no diagnostics.

## Next Steps

1. Continue with broad runtime state/debug surface cleanup if reducing `runtime.h` remains the priority.
2. Keep `ownership_state.c` limited to reducer-owned lease, persistent-intent, shadow projection, and observation mechanics over `key_runtime_core_state_t`.
3. Preserve runtime debug, scenario, release matrix, PD/key-runtime, layer-lock, modifier-hold, full host, compile-gate, and firmware compile coverage for further runtime splits.

## 2026-04-28 - State Query Surface Split

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/key/runtime/core/state_query.h` and `users/noah/lib/key/runtime/core/state_query.c`.
- Moved blocker queries, press-token/tap-series inspection, active/pending key listing, preview-owner lookup, pending-fallback lookup, and per-key deferred-release blocker inspection out of `users/noah/lib/key/runtime/core/runtime.c`.
- Removed debug/query, ownership-observation, and pending-release queue declarations from `users/noah/lib/key/runtime/core/runtime.h`.
- Moved ownership-observation declarations to `users/noah/lib/key/runtime/core/ownership_state.h`.
- Moved pending-release queue declarations to `users/noah/lib/key/runtime/core/pending_release_queue.h`.
- Updated runtime, PD bridge, layer ownership, debug, feedback, projection, release planner, tap-series flush, and test includes to depend on the narrower owner headers.
- Wired `state_query.c` into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and the manual key-runtime/PD integration runners that compile `runtime.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so the active architecture notes match the new query boundary.

## Finding Status

- The broader key-runtime accumulator finding remains partially resolved.
- Resolved in this pass:
  - core debug/state inspection queries now have a named internal module
  - deferred-release blocker query logic now lives with state inspection instead of the main reducer file
  - `runtime.h` no longer exposes debug/query, ownership-observation, or pending-release queue APIs directly
  - source manifests and host runners mechanically include the new module
- Still open:
  - `runtime.h` still owns the broad `key_runtime_core_state_t` storage shape
  - `runtime.c` still owns press/tap orchestration, scan orchestration, and effect-plan construction helpers
  - modifier mask/replay and combo-origin compatibility remain separate review findings

## Verification

Post-change targeted checks:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/state_query.c`
- `git diff --check --no-index /dev/null users/noah/lib/key/runtime/core/state_query.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks returned the expected nonzero diff status for new files and produced no diagnostics.

## Next Steps

1. Continue with broad `key_runtime_core_state_t` storage cleanup only if a clear smaller owner emerges.
2. Consider the separate modifier mask/replay finding next because it remains one behavior domain split across action dispatch, delayed actions, key process, and PD modes.
3. Keep `state_query.c` read-only over reducer-owned state; do not let it become a second owner of press, tap, lease, or pending-release storage.

## 2026-04-28 - Keyboard Modifier Policy Split

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Added `users/noah/lib/state/runtime/keyboard_mod_policy.h` and `users/noah/lib/state/runtime/keyboard_mod_policy.c`.
- Moved shared QMK modifier current-state capture, all-bucket filtering, real-mod-only filtering/restoration, and delayed action one-shot replay restoration into the new policy surface.
- Updated action dispatch, delayed actions, process-record modifier masking, deferred release, transition release snapshots, and tap-series saved-mod capture to call `keyboard_mod_policy.h` instead of open-coding QMK modifier snapshots or filters.
- Updated PD pinch managed-only GUI masking to call `keyboard_mod_policy_managed_only_mask()` while keeping PD pinch's actual GUI hold lifecycle in `keyboard_mod_ownership.c`.
- Wired `keyboard_mod_policy.c` into `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`, and manual host runners that compile the affected runtime/action sources directly.
- Added direct host coverage for `keyboard_mod_policy_current_state()`, modifier filtering helpers, and one-shot replay restoration in `tests/host/keyboard_mod_ownership_test.c`.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` so the active architecture notes match the new modifier policy surface.

## Finding Status

- The modifier masking/replay finding is partially resolved.
- Resolved in this pass:
  - raw current-mod snapshots are no longer repeated in action dispatch, delayed action, deferred release, transition release handling, tap-series saved-mod capture, or key-runtime process masking
  - masked synthetic QMK taps use `keyboard_mod_policy_without_mods()`
  - key-runtime PD keyboard-event masking uses `keyboard_mod_policy_without_real_mods()`, `keyboard_mod_policy_with_real_mods()`, and `keyboard_mod_policy_managed_only_mask()`
  - delayed replay one-shot preservation has one shared helper instead of a delayed-action-local special case
  - source manifests and host runners mechanically include the new policy module where needed
- Still open:
  - `noah_emit_policy_t` remains an action-dispatch public policy surface for fallback settlement and full modifier preservation
  - PD arrow still owns mode-specific Shift lifecycle, though its masked vertical taps route through action dispatch and the shared policy below it
  - this is not yet a single high-level modifier planner; it is a shared policy layer over the existing behavior

## Verification

Pre-change baseline checks:

- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_delayed_action_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`

Post-change targeted checks:

- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_delayed_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`

All listed post-change checks passed after runner wiring fixes.

Final checks:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/state/runtime/keyboard_mod_policy.c`
- `git diff --check --no-index /dev/null users/noah/lib/state/runtime/keyboard_mod_policy.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks returned the expected nonzero diff status for new files and produced no diagnostics.

## Next Steps

1. Keep future modifier behavior changes behind `keyboard_mod_policy.h` unless they intentionally change the architecture.
2. Decide separately whether action emit policy flags should remain action-owned or move behind a higher-level runtime replay planner.
3. Continue with the remaining open architecture findings, likely combo-origin compatibility contract cleanup or broad `key_runtime_core_state_t` storage shape cleanup.

## 2026-04-28 - Partial Finding Audit

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Used `prompts/follow-up-architecture-audit.md` for a focused audit of the partially resolved findings.
- Audited the runtime accumulator, owner-ledger, and modifier masking/replay findings against the current tree.
- Updated `userspace-architecture-review.md` with findings-first audit notes, a `Prior Finding Status` table, current conclusion, and remaining open findings.
- Updated the recommended next refactor sequence so it no longer treats already-landed release consolidation as future work and so combo-origin work remains deferred per user preference.

## Finding Status

- Key runtime accumulator remains partially resolved. The extracted modules are real improvements, but `runtime.h` still stores broad reducer state and `runtime.c` still owns press/tap orchestration.
- Owner ledgers remain partially resolved. Projection direction is documented and PD lock observation is gated, but layer-lock write-back lacks an equivalent compile-gate boundary.
- Audit-time status: modifier masking/replay remained partially resolved because `noah_emit_policy_t` still owned a separate preservation path. This was superseded by the following `Modifier Policy Closure` pass.
- Audit-time status: no partial finding was marked resolved in this audit. This was superseded for modifier masking/replay by the following `Modifier Policy Closure` pass.

## Verification

- `git diff --check`

Host tests and firmware compile were intentionally skipped because this pass only changed review notes. Runtime and build behavior were not changed.

## Next Steps

1. Decide whether to add a compile-gate guard for the layer-lock bridge.
2. Decide whether `noah_emit_policy_t` stays action-owned or moves behind a higher-level modifier replay planner.
3. Avoid generic `runtime.c` splitting unless a clear next owner boundary emerges.

## 2026-04-28 - Modifier Policy Closure

Starting worktree status:

- `git status --short` showed only the review-note audit edits from the prior pass.

## Completed

- Added modifier policy helpers for preserve-all windows, masked emit windows, delayed action replay windows, and process-record real-mod mask windows.
- Updated `users/noah/lib/action/action_dispatch.c` so `noah_emit_policy_t` still declares action-dispatch intent, but modifier preservation mechanics now call `keyboard_mod_policy_begin_preserve_all()` and `keyboard_mod_policy_end_preserve_all()`.
- Updated masked synthetic QMK taps to use `keyboard_mod_policy_begin_masked_emit()` and `keyboard_mod_policy_end_masked_emit()`.
- Updated `users/noah/lib/key/runtime/delayed_action.c` to use `keyboard_mod_policy_begin_action_replay()` and `keyboard_mod_policy_end_action_replay()`.
- Updated `users/noah/lib/key/runtime/process.c` to use `keyboard_mod_policy_begin_real_mod_mask()` and `keyboard_mod_policy_end_real_mod_mask()`.
- Extended `tests/host/keyboard_mod_ownership_test.c` with direct coverage for preserve-all windows, masked emit windows, action replay windows, and real-mod mask windows.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md`; the modifier masking/replay finding is now marked resolved.

## Finding Status

- Modifier masking/replay is resolved.
- `noah_emit_policy_t` remains action-owned intent, but no longer owns modifier preservation mechanics.
- Remaining partial findings are key runtime accumulator and owner-ledger bridge enforcement.

## Verification

Post-change targeted checks:

- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_delayed_action_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

All listed post-change targeted checks passed.

Final checks:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check --no-index /dev/null users/noah/lib/state/runtime/keyboard_mod_policy.c`
- `git diff --check --no-index /dev/null users/noah/lib/state/runtime/keyboard_mod_policy.h`

All listed pass/fail commands passed. The `--no-index` whitespace checks returned the expected nonzero diff status for new files and produced no diagnostics.

## Next Steps

1. Continue with owner-ledger bridge enforcement or runtime accumulator cleanup.
2. Keep `keyboard_mod_policy.h` as the modifier replay/preserve owner for future action dispatch, delayed action, process, and PD-mode changes.
