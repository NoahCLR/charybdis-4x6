# Userspace Ad-Hoc Patch and Overlap Review

Prompt used: `prompts/initial-architecture-review.md`.

## Scope

This is a static architecture review of every `.c` and `.h` file under `users/noah` as of 2026-04-27. The pass inspected 161 files and focused on ad-hoc compatibility patches, duplicated responsibilities, runtime policies that can work against each other, and functions or modules that appear to solve the same problem from different places.

No firmware source changes were made in this pass. The active review folder before this audit was `review/2026-04-23-review-04/`, which covers RGB runtime architecture. This review is a new thread because its scope is materially broader: the whole `users/noah` userspace surface.

## Findings

### Must-Fix

No immediate behavior bug was proven from this static pass. The main risk is architectural overlap: several subsystems maintain their own view of runtime ownership, release planning, modifier masking, or QMK compatibility state. These are the areas most likely to produce future regressions or hard-to-debug behavior conflicts.

### Partially Resolved: Key Runtime Core Is an Ad-Hoc Policy Accumulator

`users/noah/lib/key/runtime/core/runtime.c` and `runtime.h` are the highest-risk clutter zone in userspace. The header claims a "single-authority handled-key runtime state and reducer surface", but the current state and reducer also own or coordinate press tokens, tap series, leases, pending releases, persistent intents, shadow projection, feedback pulse bridging, preview display bridging, and keyboard event modifier masking.

Status as of 2026-04-28: partially resolved. Release semantics and deferred-release transport were extracted earlier in this thread. Core-side PD held-action preemption and PD lock-tap projection now live in `users/noah/lib/key/runtime/core/pd_projection.c`, core-side feedback pulse projection now lives in `users/noah/lib/key/runtime/core/feedback_projection.c`, pending multi-tap flush planning now lives in `users/noah/lib/key/runtime/core/tap_series_flush.c`, effect-plan construction now lives in `users/noah/lib/key/runtime/core/effect_plan.c`, effect execution plus projection snapshot capture/comparison now live in `users/noah/lib/key/runtime/core/projection.c`, pending-release queue mechanics now live in `users/noah/lib/key/runtime/core/pending_release_queue.c`, scan-time hold/pending multi-tap planning now lives in `users/noah/lib/key/runtime/core/scan_planner.c`, reducer-owned lease/persistent-intent mechanics now live in `users/noah/lib/key/runtime/core/ownership_state.c`, and core state/debug/blocker queries now live in `users/noah/lib/key/runtime/core/state_query.c`. `runtime.c` remains the reducer state owner, press/tap orchestration point, and scan orchestration point. The remaining accumulator concerns are broad reducer state storage in `runtime.h` and press/tap orchestration inside `runtime.c`.

Evidence:

- `users/noah/lib/key/runtime/core/runtime.h:5` describes the module as the single runtime authority.
- `users/noah/lib/key/runtime/core/runtime.h:298-333` stores press tokens, tap series, leases, pending releases, persistent intents, shadow state, feedback state, and keyboard event modifier state together.
- `users/noah/lib/key/runtime/core/runtime.h:356-367` now exposes the reducer entry surface without the debug/query, ownership-observation, pending-release queue, or effect-plan initialization APIs.
- `users/noah/lib/key/runtime/core/effect_plan.c:11-219` now owns effect-plan initialization, sink buffering, typed effect appends, tap-commit feedback filtering, and release-plan transfer.
- `users/noah/lib/key/runtime/core/pd_projection.c:19-89` now owns core-side PD held-action preemption and PD lock-tap projection.
- `users/noah/lib/key/runtime/core/feedback_projection.c:9-43` now owns feedback pulse queueing and feedback observation.
- `users/noah/lib/key/runtime/core/tap_series_flush.c:56-190` now owns pending multi-tap flush resolution, branch-confirm window setup, delayed action completion, and same-key/foreign/global multi-tap flush planning.
- `users/noah/lib/key/runtime/core/projection.c:19-104` now owns runtime effect execution into QMK-facing registries and pending-release dispatch projection.
- `users/noah/lib/key/runtime/core/projection.c:122-205` now owns projection snapshot capture/comparison and trace projection checkpoints.
- `users/noah/lib/key/runtime/core/pending_release_queue.c:51-331` now owns pending-release queue allocation, ordering, drain snapshots, and pending-emission token cleanup while using core state storage.
- `users/noah/lib/key/runtime/core/scan_planner.c:99-604` now owns scan-time hold phase commits, release-hold-pending marking, fallback hold settlement, active scan planning, and pending multi-tap scan outcomes.
- `users/noah/lib/key/runtime/core/ownership_state.c:650-1250` now owns lease release, lease queries, shadow projection recomputation, held/repeat observation, release-owned-state cleanup, non-handled release finalization, held/repeat feedback visibility, layer-lock updates, and PD lock observation over core state storage.
- `users/noah/lib/key/runtime/core/state_query.c:285-534` now owns blocker queries, press/tap inspection, preview/fallback owner lookup, and per-key deferred-release blocker inspection over core state storage.
- `users/noah/lib/key/runtime/core/runtime.c:528-578` now owns event observation into core state while delegating the extracted planning and projection details.
- `users/noah/lib/key/runtime/core/release_planner.c:189-509` resolves active and pending multi-tap releases.

Why it matters:

The runtime core is not just a reducer anymore; it is also a compatibility bridge, release planner, ownership reconciler, and scan policy coordinator. The code may be correct today, but new behavior changes will tend to land as another local branch inside this file instead of a clear subsystem contract.

Recommended direction:

- Keep `runtime.c` as the reducer state and orchestration entry point, but continue splitting broad state helpers into explicit internal modules.
- Define which state is authoritative in core and which state is only projected into QMK-facing registries.
- Preserve behavior with targeted host tests before moving code.

### Resolved: Release Semantics Are Consolidated

Release behavior was the strongest "same thing in several places" candidate. The 2026-04-28 boundary passes removed the separate slot release resolver, moved derived quick-release, fallback-suppression, buffered-base-tap, hold-action decision semantics, active-release resolution, pending multi-tap release resolution, and release effect planning into `users/noah/lib/key/runtime/core/release_planner.h` and `users/noah/lib/key/runtime/core/release_planner.c`. Blocked-release dispatch deferral and deferred-release draining now live in `users/noah/lib/key/runtime/deferred_release.c`, with queue storage intentionally remaining core-owned state and queue mechanics isolated in `users/noah/lib/key/runtime/core/pending_release_queue.c`.

Evidence:

- `users/noah/lib/key/runtime/core/release_planner.h:30-63` now defines the release contract and semantics surface for buffered base tap, quick release, fallback suppression, and nonquick release.
- `users/noah/lib/key/runtime/core/release_planner.h:147-317` owns the semantic release decision reducer used by active and pending release paths.
- `users/noah/lib/key/runtime/core/release_planner.c:189-509` now resolves active releases, resolves pending multi-tap releases, and maps those decisions into effect plans.
- `users/noah/lib/key/runtime/deferred_release.c:25-61` owns blocked-release dispatch deferral and deferred-release draining.
- `users/noah/lib/key/runtime/core/pending_release_queue.c:51-331` owns pending-release queue allocation, ordering, drain snapshots, and token cleanup over core state storage.
- `users/noah/lib/key/runtime/release.c:11-26` is now a thin release-event adapter.

Why it matters:

This was a real migration smell: the code previously supported both old slot-owned release decisions and newer core-owned release planning. Release semantics now have one owner, and deferred release transport has one adapter, so future release changes have a smaller and more explicit surface to update.

Recommended direction:

- Keep `release_planner.h` and `release_planner.c` as the sole semantic owner of quick-release, fallback suppression, buffered base tap, active-release decisions, and pending multi-tap release decisions.
- Keep `deferred_release.c` as the single adapter between transition plans and the core pending-release queue.
- Treat pending-release queue storage as core-owned state; keep queue mechanics behind `pending_release_queue.c`.
- Keep the existing release matrix, modifier-hold, PD-mode, scenario, layer-lock, runtime-debug, full host, and firmware compile checks as the safety net.

### Resolved: Multiple Owner Ledgers Track Runtime Facts Through Explicit Bridges

Several modules track "who owns this held effect" independently. Some are necessary QMK-facing registries, but the current design makes authority easy to blur.

Status as of 2026-04-28: resolved. `docs/KEY_RUNTIME.md` now contains an ownership authority map that labels reducer-owned state, release planner state, pending-release transport, projected ownership registries, PD runtime ownership, feedback projection, and combo-origin compatibility state. Runtime effect execution now flows through `users/noah/lib/key/runtime/core/projection.c` instead of a local switch in `runtime.c`, and reducer-owned lease/persistent-intent mechanics now live in `users/noah/lib/key/runtime/core/ownership_state.c` instead of being embedded in the main reducer file. The remaining two-way bridge points are intentional: layer lock write-back through `layer_ownership_set_lock_state()` and PD lock observation through `pd_mode_key_runtime_bridge_observe_local_lock_state()`. `tests/host/run_feature_gate_compile_tests.sh` now mechanically guards both bridge directions.

Evidence:

- Core effect leases live in `users/noah/lib/key/runtime/core/runtime.h:177-209`.
- Core effect lease and persistent-intent mechanics live in `users/noah/lib/key/runtime/core/ownership_state.c:650-1250`.
- Held action ownership is tracked in `users/noah/lib/key/ownership/held_action.c:183-256`.
- Held repeat ownership is tracked in `users/noah/lib/key/ownership/held_repeat.c:89-141`.
- Layer ownership is tracked in `users/noah/lib/state/ownership/layer_ownership.c:66-194`.
- Keyboard modifier ownership is tracked in `users/noah/lib/state/ownership/keyboard_mod_ownership.c:86-218`.
- PD local owners are tracked in `users/noah/lib/pointing/runtime/pd_mode_state.c:117-284`.
- `users/noah/lib/state/runtime/runtime_context_internal.h:49-56` aggregates these ledgers into one runtime context.
- `docs/KEY_RUNTIME.md` now states the intended write direction: core reducer state plans effects, `core/projection.c` applies QMK/action/layer/modifier/PD side effects, and compatibility bridges must not become independent key-runtime ownership truth.
- Current two-way bridge points are `users/noah/lib/state/ownership/layer_ownership.c:116-140` and `users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c:1-7`.
- `tests/host/run_feature_gate_compile_tests.sh:129-167` prevents PD runtime modules other than the bridge from calling the key-runtime PD lock observer and prevents repo-owned production code other than `layer_ownership.c` from calling `key_runtime_core_layer_lock_set()`.

Why it matters:

The system has both intended ownership in core leases and applied ownership in subsystem registries. If core and a registry disagree, behavior depends on cleanup order and projection checks. The current snapshot comparison helps, but it does not make the authority model obvious to the next change.

Recommended direction:

- Keep the ownership authority map as the contract for future bridge changes.
- Keep compile gates and host tests that enforce the declared direction of writes.
- Treat any new direct cross-ledger mutation as architecture work requiring review-note and compile-gate updates.

### Resolved: PD Mode Authority Boundary Is Explicit

PD mode behavior is enforced from both key runtime and PD runtime. This appears intentional, but the current two-way coupling is high-risk.

Status as of 2026-04-28: resolved. PD runtime owns actual local/display/remote/split PD mode state. Key runtime owns key-driven PD intent, held-action preemption, and projection through `users/noah/lib/key/runtime/core/pd_projection.c`. Changed local PD lock state is observed into key-runtime shadow projection through `users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c`. Feature gates enforce the bridge direction.

Evidence:

- Key runtime preempts PD-held behavior and projects PD lock-tap effects in `users/noah/lib/key/runtime/core/pd_projection.c:19-89`.
- `users/noah/lib/key/runtime/core/projection.c:29` and `users/noah/lib/key/runtime/core/projection.c:55` route held-action preemption and PD lock-tap projection through `pd_projection.c`.
- PD runtime stores local owner slots and exclusive ownership in `users/noah/lib/pointing/runtime/pd_mode_state.c:287-351`.
- PD runtime clears owner state and commands in `users/noah/lib/pointing/runtime/pd_mode_state.c:537-627`.
- Local PD lock/unlock state changes route through `pd_mode_apply_local_lock_state_at()` in `users/noah/lib/pointing/runtime/pd_mode_state.c:671-695`.
- `pd_mode_apply_local_lock_state_at()` observes changed local lock state through `pd_mode_key_runtime_bridge_observe_local_lock_state()` in `users/noah/lib/pointing/runtime/pd_mode_state.c:679-680`.
- PD lock state is observed into core shadow state through `users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c:1-7`.
- Key-runtime core names the receiving side as observation in `users/noah/lib/key/runtime/core/ownership_state.c:1224-1250`.
- `tests/host/run_feature_gate_compile_tests.sh:129-152` prevents PD runtime modules other than the bridge from including `key/runtime/core/runtime.h` or calling `key_runtime_core_observe_pd_mode_lock_state()`.
- `users/noah/source_manifest.mk` and `tests/host/noah_source_manifest.sh` include `pd_projection.c`, and manual key-runtime/PD integration runners compile it explicitly.

Why it matters:

The authority split is now intentional rather than implicit: PD runtime applies actual mode state; key runtime projects key-driven PD effects and observes changed local lock state for key-runtime shadow reasoning.

Recommended direction:

- Keep PD runtime as the owner of actual local/display/remote/split PD state.
- Keep key-runtime PD effect projection in `pd_projection.c`.
- Keep the feature gate and PD mode integration, PD runtime, pointer layer policy, split sync, runtime debug, scenario, full host, and firmware compile checks as the boundary guardrail.

### Should-Fix: Combo Origin Is a Large Shadow Compatibility Patch

`users/noah/lib/compat/qmk_combo_origin.c` is deliberately centralized, but it is still the largest compatibility patch in the tree. It shadows physical key state because QMK emits combo records with `(0,0)` origin positions.

Evidence:

- `users/noah/lib/compat/qmk_combo_origin.h:5-8` documents the QMK `(0,0)` origin problem.
- `users/noah/lib/compat/qmk_combo_origin.c:15-47` stores physical state and active/pending combo caches.
- `users/noah/lib/compat/qmk_combo_origin.c:88-109` inspects QMK combo state and depends on fork internals such as `EXTRA_SHORT_COMBOS`.
- `users/noah/lib/compat/qmk_combo_origin.c:112-190` builds combo bitmaps from pressed keys.
- `users/noah/lib/compat/qmk_combo_origin.c:193-315` maintains active and pending combo caches.
- `users/noah/lib/compat/qmk_combo_origin.c:370-386` and `576-580` use fallback owner key positions.
- `users/noah/lib/compat/qmk_combo_origin.c:555-594` normalizes combo records and sets origin bitmaps.
- `users/noah/lib/compat/qmk_combo_origin.c:626-650` matches pending combo outputs.
- `users/noah/lib/compat/qmk_combo_origin.c:653-682` partitions combo bitmaps for RGB feedback.

Why it matters:

This module is a necessary bridge today, but it reproduces QMK combo state locally. If QMK/fork combo behavior changes, this module is the likely place where behavior silently diverges.

Recommended direction:

- Keep this code centralized in `lib/compat`.
- Add a short contract document or review subsection that states exactly which QMK internals it mirrors.
- Prefer host tests that model same-key combos, fallback owners, pending outputs, and RGB partitioning.

### Resolved: Modifier Masking and Replay Are Centralized Behind Policy

Keyboard modifier replay is intentionally careful, but the policy is scattered across action dispatch, key processing, delayed actions, and PD modes.

Status as of 2026-04-28: resolved. Shared snapshot, filtering, preservation windows, masked emit windows, action replay restoration, managed-only masking, and real-mod masking now route through `users/noah/lib/state/runtime/keyboard_mod_policy.h` and `keyboard_mod_policy.c`. Action dispatch still exposes `noah_emit_policy_t` to decide whether an emitted action needs fallback settlement or modifier preservation, but the modifier preservation mechanics are now owned by `keyboard_mod_policy`. Delayed actions, key-runtime process masking, deferred release, transition release snapshots, tap-series saved-mod capture, and PD pinch managed-only masking also call that policy surface instead of open-coding local snapshot/filter/preserve helpers. PD arrow still owns mode-specific Shift lifecycle, which is mode state rather than modifier replay policy, and its masked vertical taps route through action dispatch and the shared policy below it.

Evidence:

- `users/noah/lib/state/runtime/keyboard_mod_state.c:7-30` suspends and reapplies keyboard modifiers.
- `users/noah/lib/state/runtime/keyboard_mod_policy.h:17-39` and `keyboard_mod_policy.c:7-93` provide the shared current-state, filtering, preservation-window, masked-emit, action-replay, real-mod masking, managed-only masking, and replay-restoration policy surface.
- `users/noah/lib/action/action_dispatch.c:16-72` wraps emitted actions with modifier preservation and settling, and uses `keyboard_mod_policy_begin_preserve_all()`, `keyboard_mod_policy_end_preserve_all()`, `keyboard_mod_policy_begin_masked_emit()`, and `keyboard_mod_policy_end_masked_emit()` for modifier behavior.
- `users/noah/lib/action/action_dispatch.h:29-34` and `186-190` expose action-level modifier policies.
- `users/noah/lib/key/runtime/process.c:48-75` masks active PD real modifiers and restores managed-only modifiers through `keyboard_mod_policy_begin_real_mod_mask()` and `keyboard_mod_policy_end_real_mod_mask()`.
- `users/noah/lib/key/runtime/delayed_action.c:15-20` runs delayed replay through `keyboard_mod_policy_begin_action_replay()` and `keyboard_mod_policy_end_action_replay()`.
- `users/noah/lib/key/runtime/core/runtime.c:601-603` captures buffered tap-series modifier state through `keyboard_mod_policy.h`.
- `users/noah/lib/pointing/modes/pd_mode_pinch.c:15-43` masks GUI during pinch behavior through `keyboard_mod_policy_managed_only_mask()`.
- `users/noah/lib/pointing/modes/pd_mode_arrow.c:29-47` and `83-120` mask Alt and own Shift during arrow behavior.
- `tests/host/keyboard_mod_ownership_test.c:164-368` covers current-state capture, filtering, replay restoration, preserve-all windows, masked emit windows, action replay windows, and real-mod mask windows.

Why it matters:

This is one behavior domain with several local policies. That makes it hard to reason about whether a new action, delayed action, or PD mode should preserve, mask, or restore modifiers.

Recommended direction:

- Keep `keyboard_mod_policy.h` as the shared modifier mask/replay/preserve API and avoid adding new raw QMK snapshot/filter/preserve helpers in action dispatch, delayed actions, process flow, or PD modes.
- Keep `noah_emit_policy_t` limited to action-dispatch intent such as "settle fallback holds" and "request modifier preservation"; do not let it own modifier mechanics.
- Keep modifier-hold, PD-mode integration, action dispatch, delayed action, and keyboard mod ownership tests around future changes.

### Should-Fix: Compatibility Fallback Macros Keep Old Charybdis Names Alive

The drag-scroll configuration still supports old Charybdis macro names as fallbacks.

Evidence:

- `users/noah/config.h:122-145` says repo-owned `NOAH_DRAGSCROLL_*` macros control gesture feel, while old `CHARYBDIS_*` names remain compatibility fallbacks.
- `users/noah/lib/pointing/modes/pd_mode_dragscroll.c:14-62` maps old macros to new repo-owned names.

Why it matters:

This is not a current behavior risk, but it is configuration clutter. New readers must understand both old and new naming systems.

Recommended direction:

- Keep the fallbacks until all authored config has migrated.
- Then remove the old-name compatibility layer in one small cleanup with docs and tests.

### Optional Cleanup: Repeated Small Helpers

The following are low-risk duplication, not architecture blockers:

- `keypos_equal` variants appear in held action, held repeat, layer ownership, PD mode state, runtime core, and release helpers.
- LED range/group intersection helpers repeat across RGB stages.
- `macro_payload_ir_write_byte` appears in both QMK decode and text parse paths.
- Key feedback priority exists in both semantic feedback selection and RGB rendering state selection.

These can be cleaned up opportunistically after higher-risk authority and release work is complete.

## Current Architecture Assessment

The userspace is not a random patch pile. Most compatibility behavior is centralized under `users/noah/lib/compat`, authored profile behavior is outside userspace, and RGB and macro modules are comparatively cohesive. The clutter is concentrated around runtime authority: key release planning, effect ownership, PD mode ownership, modifier masking, and combo origin recovery.

The highest-value next work is not a broad rewrite. Release behavior and modifier filtering now have smaller owner APIs. The remaining priority is to avoid calling partially resolved architecture findings done before their remaining bridge and API questions are either closed with enforcement or explicitly accepted as stable boundaries.

## Recommended Next Refactor Sequence

1. Continue splitting remaining `runtime.c` and `runtime.h` accumulator concerns only when a clear owner emerges, such as a press/tap orchestration module.
2. Tighten `qmk_combo_origin` later, after the user is ready to work on combo compatibility.
3. Clean up low-risk duplication only after the authority and release work is stable.

## 2026-04-28 Partial Finding Audit

Prompt used: `prompts/follow-up-architecture-audit.md`.

### Findings

#### Should-Fix: Modifier emit preservation still has an action-owned policy surface

Reconciliation note after remediation: resolved. The audit-time concern was valid when action dispatch still called `keyboard_mod_state_suspend()` and `keyboard_mod_state_apply()` directly. That has now landed behind `keyboard_mod_policy_begin_preserve_all()` and `keyboard_mod_policy_end_preserve_all()` in `users/noah/lib/action/action_dispatch.c:16-50`. Masked synthetic QMK taps use `keyboard_mod_policy_begin_masked_emit()` and `keyboard_mod_policy_end_masked_emit()` in `users/noah/lib/action/action_dispatch.c:64-72`. Delayed replay and process real-mod masking also route through `keyboard_mod_policy` in `users/noah/lib/key/runtime/delayed_action.c:15-20` and `users/noah/lib/key/runtime/process.c:48-75`. `noah_emit_policy_t` remains action-dispatch intent, but it no longer owns modifier preservation mechanics.

#### Resolved: Layer-lock write-back is mechanically guarded like PD lock observation

Reconciliation note after remediation: resolved. The audit-time concern was valid when only the PD lock bridge had mechanical enforcement. `tests/host/run_feature_gate_compile_tests.sh:154-167` now prevents repo-owned production code other than `users/noah/lib/state/ownership/layer_ownership.c` from calling `key_runtime_core_layer_lock_set()`, while still allowing the core declaration/definition in `ownership_state.h/.c`.

#### Optional Cleanup: Runtime core remains broad, but further splitting needs a better target

`users/noah/lib/key/runtime/core/runtime.h:298-333` still stores the whole reducer state shape, and `users/noah/lib/key/runtime/core/runtime.c` is still 1099 lines with press-token lifecycle, tap-series lifecycle, event observation, active interruption, press handling, release handling, and scan entry points. The previous splits removed clear owner APIs for release planning, pending-release queueing, projection, scan planning, ownership state, state queries, feedback projection, and PD projection. The remaining breadth is real, but a mechanical split without a strong owner would mostly add indirection. Keep this partially resolved until a clear next module boundary appears.

### Prior Finding Status

| Prior finding | Status | Audit result |
| --- | --- | --- |
| Key Runtime Core Is an Ad-Hoc Policy Accumulator | partially resolved | Extracted modules now own release planning, effect-plan construction, projection, scan planning, ownership state, pending-release queueing, state queries, feedback projection, and PD projection. `runtime.h` still exposes the broad core state shape, and `runtime.c` remains the press/tap orchestration owner. |
| Release Semantics Are Consolidated | resolved | Code references: `core/release_planner.h`, `core/release_planner.c`, `deferred_release.c`, `core/pending_release_queue.c`, and thin `release.c` adapter. Enforcement references: release matrix, scenario, runtime debug, full host, compile gate, and firmware compile checks recorded in `progress.md`. Exact passed commands: `sh tests/host/run_key_runtime_release_matrix_tests.sh`, `sh tests/host/run_key_runtime_scenario_tests.sh`, `sh tests/host/run_runtime_debug_tests.sh`, `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`. |
| Multiple Owner Ledgers Track the Same Runtime Facts | resolved | Code references: `core/projection.c`, `core/ownership_state.c`, `layer_ownership.c`, `pd_mode_key_runtime_bridge.c`, and `pd_mode_state.c`. Enforcement references: ownership authority map in `docs/KEY_RUNTIME.md`, layer-lock bridge and PD-lock bridge checks in `tests/host/run_feature_gate_compile_tests.sh`, layer ownership tests, layer-lock integration tests, runtime debug tests, full host, and firmware compile. Exact passed commands include `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_layer_ownership_tests.sh`, `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`, `sh tests/host/run_runtime_debug_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`. |
| PD Mode Authority Boundary Is Explicit | resolved | Code references: `core/pd_projection.c`, `pd_mode_state.c`, `pd_mode_key_runtime_bridge.c`, and `ownership_state.c`. Enforcement references: `tests/host/run_feature_gate_compile_tests.sh:129-152` plus PD/key-runtime and PD runtime tests recorded in `progress.md`. Exact passed commands: `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_pd_runtime_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`. |
| Combo Origin Is a Large Shadow Compatibility Patch | open | Not audited in depth in this pass because the user explicitly deferred combo work. The finding remains open. |
| Modifier Masking and Replay Are Spread Across Subsystems | resolved | Code references: `keyboard_mod_policy.h`, `keyboard_mod_policy.c`, `action_dispatch.c`, `delayed_action.c`, `process.c`, `runtime.c`, `transition.c`, `deferred_release.c`, and `pd_mode_pinch.c`. Enforcement references: direct keyboard mod policy tests in `keyboard_mod_ownership_test.c`, action dispatch tests, delayed action tests, modifier-hold integration, PD mode tests, PD/key-runtime integration, feature gate, full host, and firmware compile. Exact passed commands include `sh tests/host/run_keyboard_mod_ownership_tests.sh`, `sh tests/host/run_action_dispatch_tests.sh`, `sh tests/host/run_delayed_action_tests.sh`, `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`, `sh tests/host/run_pd_mode_tests.sh`, `sh tests/host/run_pd_runtime_tests.sh`, `sh tests/host/run_pd_mode_handlers_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_key_runtime_scenario_tests.sh`, `sh tests/host/run_runtime_trace_tests.sh`, `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`, `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`. |
| Compatibility Fallback Macros Keep Old Charybdis Names Alive | open | Still open and intentionally lower priority than the partial runtime findings. |
| Repeated Small Helpers | open | Optional cleanup only; not part of this audit pass. |

### Current Conclusion

After modifier and owner-ledger remediation, only the runtime accumulator partial remains accurate. Further runtime accumulator cleanup should wait for a clearer next owner boundary.

### Remaining Open Findings

- Keep runtime accumulator cleanup focused on a real owner boundary, not a generic file-size split.
- Leave combo-origin compatibility work for a later pass, per user preference.

## Per-File Inventory

Status key:

- `high-risk`: likely ad-hoc overlap, split authority, or duplicated behavior.
- `watch`: intentional behavior but a future clutter or drift risk.
- `compat`: compatibility bridge; acceptable if centralized and tested.
- `clean`: no meaningful ad-hoc patch concern found in this pass.

### Root Userspace Files

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/config.h` | watch | Repo-owned config plus old Charybdis drag-scroll fallback names. |
| `users/noah/hooks.c` | clean | Weak QMK hook chainers; intentional boundary surface. |
| `users/noah/keymap_materialize.h` | clean | Data materialization macros; no policy conflict seen. |
| `users/noah/noah_keymap.h` | clean | Keymap authoring bundle; should remain out of runtime modules. |
| `users/noah/noah_keymap_ids.h` | clean | Shared IDs safe for runtime use. |
| `users/noah/noah_runtime.h` | clean | Runtime public bundle; documents keymap hook finalization contract. |
| `users/noah/runtime_init.c` | watch | One-line init stage list is compact enough to obscure runtime ordering. |

### Action

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/lib/action/action_dispatch.c` | watch | Action emission also handles modifier preservation and settling. |
| `users/noah/lib/action/action_dispatch.h` | watch | Public action API exposes modifier policy knobs used by multiple subsystems. |
| `users/noah/lib/action/action_kind.c` | clean | Action kind metadata. |
| `users/noah/lib/action/action_kind_dispatch.c` | clean | Dispatch table integration. |
| `users/noah/lib/action/action_kind_dispatch_internal.h` | clean | Internal action dispatch declarations. |
| `users/noah/lib/action/action_kind_internal.h` | clean | Internal action kind declarations. |
| `users/noah/lib/action/action_kind_registry_list.h` | clean | Registry list include surface. |
| `users/noah/lib/action/action_lifecycle.c` | clean | Lifecycle helper surface; no duplicated authority found. |
| `users/noah/lib/action/action_lifecycle.h` | clean | Lifecycle API. |
| `users/noah/lib/action/owned_keycode.c` | clean | Owned keycode helper. |
| `users/noah/lib/action/owned_keycode.h` | clean | Owned keycode API. |
| `users/noah/lib/action/synthetic_record.c` | clean | Synthetic record construction. |
| `users/noah/lib/action/synthetic_record.h` | clean | Synthetic record API. |

### Compatibility

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/lib/compat/qmk_auto_mouse_contract.h` | compat | Compile-time auto mouse contract. |
| `users/noah/lib/compat/qmk_combo_origin.c` | high-risk | Large shadow state bridge for QMK combo origin recovery. |
| `users/noah/lib/compat/qmk_combo_origin.h` | high-risk | Public combo origin contract for the shadow bridge. |
| `users/noah/lib/compat/qmk_contract.c` | compat | Centralized QMK contract checks. |
| `users/noah/lib/compat/qmk_mod_contract.c` | watch | QMK modifier contract surface tied to runtime modifier policy. |
| `users/noah/lib/compat/qmk_mod_contract.h` | watch | Modifier contract declarations. |
| `users/noah/lib/compat/qmk_pointing_contract.h` | compat | Central pointing-device contract. |
| `users/noah/lib/compat/qmk_via_contract.c` | compat | VIA contract checks. |
| `users/noah/lib/compat/qmk_via_playback_contract.h` | compat | VIA playback compile contract. |
| `users/noah/lib/compat/qmk_via_split_sync.c` | compat | VIA split-sync compatibility surface. |
| `users/noah/lib/compat/qmk_via_split_sync.h` | compat | VIA split-sync API. |
| `users/noah/lib/compat/qmk_via_storage_contract.h` | compat | VIA storage contract. |
| `users/noah/lib/compat/split_half.h` | clean | Split-half definitions. |
| `users/noah/lib/compat/split_role.c` | clean | Split role helper. |

### Key Interaction

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/lib/key/interaction/handled_key.h` | watch | Authored key behavior surface feeds release/runtime complexity. |
| `users/noah/lib/key/interaction/handled_key_defaults.c` | clean | Weak defaults for authored profile data. |
| `users/noah/lib/key/interaction/handled_key_internal.h` | clean | Internal handled-key declarations. |
| `users/noah/lib/key/interaction/handled_key_lookup.c` | clean | Lookup helper. |
| `users/noah/lib/key/interaction/handled_key_materialize.c` | clean | Materialization helper. |
| `users/noah/lib/key/interaction/handled_key_policy.h` | clean | Policy constants and helpers. |
| `users/noah/lib/key/interaction/handled_key_resolution_accessors.c` | clean | Accessors only. |
| `users/noah/lib/key/interaction/handled_key_transparency.c` | clean | Transparency resolution helper. |
| `users/noah/lib/key/interaction/key_behavior.h` | clean | Authored behavior data types. |
| `users/noah/lib/key/interaction/key_behavior_lookup.c` | clean | Key behavior lookup. |
| `users/noah/lib/key/interaction/key_behavior_lookup.h` | clean | Lookup API. |
| `users/noah/lib/key/interaction/keymap_validation.c` | clean | Validation pass for authored profile data. |
| `users/noah/lib/key/interaction/keymap_validation.h` | clean | Validation API. |

### Key Ownership

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/lib/key/ownership/held_action.c` | watch | Separate held-action owner ledger parallel to core leases. |
| `users/noah/lib/key/ownership/held_action.h` | watch | Held-action ownership API. |
| `users/noah/lib/key/ownership/held_repeat.c` | watch | Separate held-repeat owner ledger parallel to core leases. |
| `users/noah/lib/key/ownership/held_repeat.h` | watch | Held-repeat ownership API. |

### Key Runtime

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/lib/key/runtime/api.c` | clean | Thin public API wrapper. |
| `users/noah/lib/key/runtime/api.h` | clean | Public runtime API. |
| `users/noah/lib/key/runtime/core/effect_plan.c` | watch | Internal effect-plan construction, tap-commit feedback filtering, and release-plan transfer. |
| `users/noah/lib/key/runtime/core/effect_plan.h` | clean | Internal effect-plan construction declarations shared by core modules. |
| `users/noah/lib/key/runtime/core/feedback_projection.c` | watch | Core-side feedback pulse projection and pulse queueing. |
| `users/noah/lib/key/runtime/core/feedback_projection.h` | clean | Internal core feedback projection declaration. |
| `users/noah/lib/key/runtime/core/ownership_state.c` | watch | Reducer-owned lease and persistent-intent mechanics, shadow projection recomputation, held/repeat feedback visibility queries, and lock observation updates over core state storage. |
| `users/noah/lib/key/runtime/core/ownership_state.h` | clean | Internal ownership-state helper declarations shared by runtime, release, and scan planning code. |
| `users/noah/lib/key/runtime/core/pending_release_queue.c` | watch | Pending-release queue allocation, ordering, drain snapshots, and released-token pending-emission cleanup over core state storage. |
| `users/noah/lib/key/runtime/core/pending_release_queue.h` | clean | Internal pending-release queue declaration used by projection and release transport. |
| `users/noah/lib/key/runtime/core/projection.c` | watch | Runtime effect execution, pending-release dispatch projection, projection snapshot capture/comparison, and trace projection checkpoints. |
| `users/noah/lib/key/runtime/core/projection.h` | watch | Projection execution and snapshot API declarations. |
| `users/noah/lib/key/runtime/core/pd_projection.c` | watch | Core-side PD held-action preemption and PD lock-tap projection. |
| `users/noah/lib/key/runtime/core/pd_projection.h` | clean | Internal core PD projection declarations. |
| `users/noah/lib/key/runtime/core/release_internal.h` | watch | Internal release planning declarations and narrow helper exports for the planner. |
| `users/noah/lib/key/runtime/core/release_planner.c` | watch | Active-release and pending multi-tap release resolution plus effect planning. |
| `users/noah/lib/key/runtime/core/release_planner.h` | watch | Central release semantics helper introduced on 2026-04-28. |
| `users/noah/lib/key/runtime/core/runtime.c` | high-risk | Core state reducer, event observation, press/tap orchestration, and scan orchestration. |
| `users/noah/lib/key/runtime/core/runtime.h` | high-risk | Broad core state type plus reducer entry surface; debug/query and ownership-observation declarations have moved to narrower headers. |
| `users/noah/lib/key/runtime/core/scan_planner.c` | watch | Scan-time hold promotion, fallback hold settlement, release-hold-pending marking, and pending multi-tap scan planning. |
| `users/noah/lib/key/runtime/core/scan_planner.h` | clean | Internal scan planning declaration and scan resolution contract. |
| `users/noah/lib/key/runtime/core/state_query.c` | watch | Core state inspection, debug, feedback, blocker, preview-owner, and pending-fallback query surface over reducer-owned storage. |
| `users/noah/lib/key/runtime/core/state_query.h` | clean | Internal core state-query declarations used by debug, feedback, projection, preflight, and deferred-release transport. |
| `users/noah/lib/key/runtime/core/tap_series.h` | watch | Internal tap-series helper declarations. |
| `users/noah/lib/key/runtime/core/tap_series_flush.c` | watch | Pending multi-tap flush resolution, branch-confirm windows, and same-key/foreign/global flush planning. |
| `users/noah/lib/key/runtime/core/trace.c` | clean | Core trace helpers. |
| `users/noah/lib/key/runtime/core/trace.h` | clean | Core trace API. |
| `users/noah/lib/key/runtime/debug.c` | watch | Debug API reaches into broad runtime state; useful for tests. |
| `users/noah/lib/key/runtime/deferred_release.c` | clean | Central adapter for blocked-release dispatch deferral and deferred-release draining. |
| `users/noah/lib/key/runtime/deferred_release.h` | clean | Deferred release adapter API. |
| `users/noah/lib/key/runtime/delayed_action.c` | watch | Delayed dispatch includes modifier preservation policy. |
| `users/noah/lib/key/runtime/delayed_action.h` | watch | Delayed action API. |
| `users/noah/lib/key/runtime/effects/effect.h` | clean | Effect value type. |
| `users/noah/lib/key/runtime/effects/effect_queue.h` | clean | Effect queue helper. |
| `users/noah/lib/key/runtime/feedback.c` | watch | Feedback priority selection could drift from RGB feedback rendering priority. |
| `users/noah/lib/key/runtime/feedback.h` | watch | Feedback API. |
| `users/noah/lib/key/runtime/feedback_kind.h` | clean | Feedback kind enum. |
| `users/noah/lib/key/runtime/interaction.h` | watch | Slot interaction cache no longer exposes derived release contracts; authored behavior contracts still feed release planning. |
| `users/noah/lib/key/runtime/keypos_codec.h` | clean | Key-position codec helper. |
| `users/noah/lib/key/runtime/origin_registry.c` | clean | Origin registry helper. |
| `users/noah/lib/key/runtime/origin_registry.h` | clean | Origin registry API. |
| `users/noah/lib/key/runtime/preflight.c` | clean | Runtime preflight pass. |
| `users/noah/lib/key/runtime/press.c` | clean | Press wrapper around core runtime. |
| `users/noah/lib/key/runtime/process.c` | watch | Process hook handles PD modifier masking and managed modifier restore. |
| `users/noah/lib/key/runtime/process_internal.h` | clean | Internal process declarations. |
| `users/noah/lib/key/runtime/release.c` | clean | Thin release-event adapter. |
| `users/noah/lib/key/runtime/scan.c` | clean | Scan hook wrapper. |
| `users/noah/lib/key/runtime/trace.c` | clean | Runtime trace helpers. |
| `users/noah/lib/key/runtime/trace.h` | clean | Runtime trace API. |
| `users/noah/lib/key/runtime/transition.c` | clean | Transition wrapper around core effect planning and execution. |
| `users/noah/lib/key/runtime/transition.h` | clean | Transition API. |
| `users/noah/lib/key/runtime/types.h` | clean | Runtime type definitions. |

### Macro

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/lib/macro/macro_dispatch.c` | clean | Macro dispatch entry point. |
| `users/noah/lib/macro/macro_dispatch.h` | clean | Macro dispatch API. |
| `users/noah/lib/macro/macro_payload.c` | clean | Payload facade. |
| `users/noah/lib/macro/macro_payload.h` | clean | Payload API. |
| `users/noah/lib/macro/macro_payload_decode_qmk.c` | watch | QMK byte-stream decode path duplicates small IR write helper shape. |
| `users/noah/lib/macro/macro_payload_encode.c` | clean | IR-to-QMK encoding path. |
| `users/noah/lib/macro/macro_payload_internal.h` | clean | Internal payload types. |
| `users/noah/lib/macro/macro_payload_keycodes.c` | clean | Large keycode name table; data-heavy but not ad-hoc policy. |
| `users/noah/lib/macro/macro_payload_parse.c` | watch | Text parse path duplicates small IR write helper shape. |
| `users/noah/lib/macro/macro_payload_run.c` | clean | IR execution path. |
| `users/noah/lib/macro/macro_slot_provider.c` | clean | Shared macro slot cache/provider. |
| `users/noah/lib/macro/macro_slot_provider.h` | clean | Slot provider API. |
| `users/noah/lib/macro/via_macro_defaults.c` | clean | VIA macro default seeding. |
| `users/noah/lib/macro/via_macro_defaults.h` | clean | VIA default API. |
| `users/noah/lib/macro/via_macro_provider.c` | clean | VIA macro provider bridge. |
| `users/noah/lib/macro/via_macro_provider.h` | clean | VIA macro provider API. |

### Pointing and PD Modes

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/lib/pointing/defs/pd_mode_flags.h` | clean | PD mode flags. |
| `users/noah/lib/pointing/defs/pd_mode_manifest.h` | clean | Authored PD mode manifest. |
| `users/noah/lib/pointing/defs/pd_modes.h` | watch | Broad PD mode public declarations. |
| `users/noah/lib/pointing/modes/pd_mode_arrow.c` | watch | Local Alt/Shift modifier policy during pointing mode. |
| `users/noah/lib/pointing/modes/pd_mode_brightness.c` | clean | Brightness pointing mode. |
| `users/noah/lib/pointing/modes/pd_mode_dragscroll.c` | watch | Old Charybdis macro fallback mapping. |
| `users/noah/lib/pointing/modes/pd_mode_handler_common.h` | clean | Shared handler helper declarations. |
| `users/noah/lib/pointing/modes/pd_mode_handlers.h` | clean | Handler API. |
| `users/noah/lib/pointing/modes/pd_mode_pinch.c` | watch | Local GUI modifier masking policy. |
| `users/noah/lib/pointing/modes/pd_mode_volume.c` | clean | Volume pointing mode. |
| `users/noah/lib/pointing/modes/pd_mode_zoom.c` | clean | Zoom pointing mode. |
| `users/noah/lib/pointing/policy/pd_mode_policy.h` | clean | PD mode policy definitions. |
| `users/noah/lib/pointing/policy/pointer_layer_policy.c` | clean | Pointer layer policy appears centralized. |
| `users/noah/lib/pointing/policy/pointer_layer_policy.h` | clean | Pointer layer policy API. |
| `users/noah/lib/pointing/runtime/pd_mode_buffered_tap_internal.h` | watch | Internal buffered tap surface tied to release behavior. |
| `users/noah/lib/pointing/runtime/pd_mode_internal.h` | watch | Internal PD runtime surface. |
| `users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c` | watch | Narrow local PD lock observation bridge into key runtime. |
| `users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.h` | watch | Public declaration for the PD/key-runtime observation bridge. |
| `users/noah/lib/pointing/runtime/pd_mode_keyboard_event_internal.h` | watch | Keyboard event bridge into PD behavior. |
| `users/noah/lib/pointing/runtime/pd_mode_lifecycle.c` | clean | PD lifecycle helper. |
| `users/noah/lib/pointing/runtime/pd_mode_registry.c` | clean | PD registry. |
| `users/noah/lib/pointing/runtime/pd_mode_registry_internal.h` | clean | Internal registry declarations. |
| `users/noah/lib/pointing/runtime/pd_mode_runtime_shared_state_internal.h` | watch | Shared PD state surface. |
| `users/noah/lib/pointing/runtime/pd_mode_snapshot.c` | clean | PD snapshot helper. |
| `users/noah/lib/pointing/runtime/pd_mode_state.c` | high-risk | PD owner ledger, exclusivity, commands, and lock feedback into key runtime. |
| `users/noah/lib/pointing/runtime/pd_runtime.c` | watch | Runtime processing bridge for pointing behavior. |

### RGB

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/lib/rgb/automouse/rgb_automouse.c` | clean | Auto-mouse RGB behavior. |
| `users/noah/lib/rgb/automouse/rgb_automouse.h` | clean | Auto-mouse RGB API. |
| `users/noah/lib/rgb/automouse/rgb_automouse_stage.c` | watch | Reads driver buffer for base-effect capture when enabled. |
| `users/noah/lib/rgb/automouse/rgb_automouse_stage.h` | clean | Auto-mouse stage API. |
| `users/noah/lib/rgb/core/rgb_config_defaults.c` | clean | Weak RGB config defaults. |
| `users/noah/lib/rgb/core/rgb_config_helpers.h` | clean | RGB config helper declarations. |
| `users/noah/lib/rgb/core/rgb_helpers.h` | clean | Shared RGB helpers. |
| `users/noah/lib/rgb/core/rgb_runtime.c` | clean | RGB stage orchestrator; comparatively cohesive. |
| `users/noah/lib/rgb/core/rgb_runtime.h` | clean | RGB runtime API. |
| `users/noah/lib/rgb/core/rgb_validation.c` | clean | RGB authored-data validation. |
| `users/noah/lib/rgb/core/rgb_validation.h` | clean | RGB validation API. |
| `users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c` | watch | Combo RGB feedback plus repeated LED intersection helper shape. |
| `users/noah/lib/rgb/stages/rgb_combo_feedback_stage.h` | clean | Combo feedback stage API. |
| `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c` | watch | Key feedback render priority could drift from semantic feedback priority. |
| `users/noah/lib/rgb/stages/rgb_key_feedback_stage.h` | clean | Key feedback stage API. |
| `users/noah/lib/rgb/stages/rgb_layer_stage.c` | clean | Layer RGB stage. |
| `users/noah/lib/rgb/stages/rgb_layer_stage.h` | clean | Layer stage API. |
| `users/noah/lib/rgb/stages/rgb_pd_mode_stage.c` | watch | PD RGB stage plus repeated LED intersection helper shape. |
| `users/noah/lib/rgb/stages/rgb_pd_mode_stage.h` | clean | PD mode stage API. |
| `users/noah/lib/rgb/stages/rgb_preview_stage.c` | watch | Preview stage plus repeated LED intersection helper shape. |
| `users/noah/lib/rgb/stages/rgb_preview_stage.h` | clean | Preview stage API. |

### State, Ownership, Trace, and Split Sync

| File | Status | Notes |
| --- | --- | --- |
| `users/noah/lib/state/ownership/keyboard_mod_ownership.c` | watch | Keyboard modifier owner ledger parallel to action and process masking policy. |
| `users/noah/lib/state/ownership/keyboard_mod_ownership.h` | watch | Modifier ownership API. |
| `users/noah/lib/state/ownership/layer_ownership.c` | watch | Layer owner ledger parallel to core leases. |
| `users/noah/lib/state/ownership/layer_ownership.h` | watch | Layer ownership API. |
| `users/noah/lib/state/runtime/keyboard_mod_state.c` | watch | Low-level suspend/apply modifier API used by several higher policies. |
| `users/noah/lib/state/runtime/keyboard_mod_state.h` | watch | Keyboard modifier state API. |
| `users/noah/lib/state/runtime/runtime_context_internal.h` | high-risk | Aggregates multiple owner ledgers and shared runtime surfaces. |
| `users/noah/lib/state/runtime/runtime_debug.h` | watch | Broad debug declarations expose many runtime internals. |
| `users/noah/lib/state/runtime/runtime_diag.c` | clean | Diagnostic counters/helpers. |
| `users/noah/lib/state/runtime/runtime_diag.h` | clean | Diagnostic API. |
| `users/noah/lib/state/runtime/runtime_reset.h` | clean | Reset API. |
| `users/noah/lib/state/runtime/runtime_shared_state.c` | clean | Shared state facade. |
| `users/noah/lib/state/runtime/runtime_shared_state_internal.h` | watch | Internal shared state declarations. |
| `users/noah/lib/state/runtime/runtime_trace.c` | clean | Runtime trace storage. |
| `users/noah/lib/state/runtime/runtime_trace.h` | clean | Runtime trace API. |
| `users/noah/lib/state/runtime/split_runtime_sync.c` | watch | Intentional multi-surface split mirror for base, combo, key feedback, and heartbeat packets. |
| `users/noah/lib/state/runtime/split_runtime_sync.h` | watch | Split runtime sync packet contract. |
