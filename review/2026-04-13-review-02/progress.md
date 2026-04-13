# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

Completed in this pass so far:

- Started from a clean worktree and confirmed the active review history under
  `review/`.
- Re-read the newest existing review before opening a distinct same-day review.
- Reviewed the current userspace runtime surfaces under `users/noah/`,
  including:
  - hook entry points and runtime init
  - source/build manifests
  - key interaction and key runtime modules
  - pointing runtime, registry, policy, and mode surfaces
  - shared state, ownership, macro, and action modules
  - maintainer docs and host-test surfaces
- Opened `review/2026-04-13-review-02/` for a deeper architecture pass focused
  on software extensibility rather than folder layout.
- Wrote a concrete userspace architecture review covering:
  - architecture and separation of concerns
  - modularity and extensibility
  - abstractions and interfaces
  - code organization
  - state management and flow
  - scalability risks
  - testing/debuggability
  - actionable refactoring recommendations

Key findings recorded in this review:

- the strongest remaining risk is semantic duplication, not directory layout
- multi-tap behavior bypasses the main handled-key abstraction
- action meaning is repeatedly inferred from raw keycodes instead of a typed
  descriptor
- `state/runtime/` still owns key-runtime internals that belong to the key
  domain
- pd-mode policy is manifest-driven at the edge but fragmented in the core
- the macro DSL implementation is structurally monolithic

Verification run so far:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git status --short`

Next steps:

- decide whether to turn the contract recommendations in
  `userspace-architecture-review.md` into an implementation review/pass
- if that work is started, prioritize the interaction-contract and
  action-descriptor seams before any new feature additions

## 2026-04-13: Recommendation 1 started

Completed in this pass:

- Landed the first implementation step for recommendation 1
  ("centralize interaction resolution").
- Extended handled-key resolution with a tap-count-aware entry point:
  `handled_key_lookup_tap_count(keycode, tap_count)`.
- Expanded `handled_key_view_t` so the resolved contract now carries:
  - tap repeat count
  - authored-step presence
  - whether higher tap counts still exist
  - whether a later tap resolves immediately on press
- Updated `multi_tap_t` to cache the resolved tap outcome for the current tap
  count instead of re-deriving it from raw behavior lookups during flush and
  hold resolution.
- Updated pending multi-tap runtime paths so later tap counts are resolved
  through the handled-key contract, while the first tap still seeds from the
  already-bound active slot state.
- Removed the remaining pending multi-tap release/scan dependency on raw
  authored behavior lookups by using cached slot binding and multi-tap
  semantics instead.
- Updated the affected host fixtures so their manual pending multi-tap state
  matches the new cached interaction contract.

Contracts touched:

- `users/noah/lib/key/interaction/handled_key.[ch]`
- `users/noah/lib/key/interaction/multi_tap_engine.[ch]`
- `users/noah/lib/key/runtime/key_runtime_state.h`
- `users/noah/lib/key/runtime/slot/key_runtime_slot*.c`

Verification run in this pass:

- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- continue recommendation 1 by moving more first-tap-only slot metadata behind
  the same resolved interaction surface instead of mixing slot binding and
  handled-key state
- start recommendation 2 so action meaning stops being rediscovered from raw
  keycodes across runtime, feedback, and policy modules

## 2026-04-13: Recommendation 1 continued

Completed in this pass:

- Broke the handled-key/runtime-state type dependency by moving
  `key_runtime_slot_hold_strategy_t` into a small shared runtime header.
- Added an active-slot interaction cache so `active_key_state_t` now stores a
  resolved handled-key snapshot alongside the older mirror fields.
- Changed `key_runtime_slot_track(...)` to seed that resolved interaction
  snapshot directly instead of splitting press setup across separate
  `track(...)` and metadata application paths.
- Moved the active-slot reducers and readers onto the new interaction
  contract, including:
  - active release
  - active scan
  - pending multi-tap release/scan
  - flush policy
  - key feedback
- Kept `binding`, `timing`, and `semantic` as synchronized compatibility
  mirrors for this pass, and added a legacy fallback path so existing manual
  host fixtures still execute through the new runtime contract while the test
  surface catches up.

Contracts touched:

- `users/noah/lib/key/runtime/key_runtime_types.h`
- `users/noah/lib/state/runtime/runtime_shared_state.h`
- `users/noah/lib/key/runtime/key_runtime_state.h`
- `users/noah/lib/key/runtime/key_runtime_feedback.c`
- `users/noah/lib/key/runtime/slot/key_runtime_slot.[ch]`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_policy.[ch]`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.c`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- remove the remaining compatibility mirrors once host fixtures and debug
  surfaces no longer rely on `binding`, `timing`, and `semantic`
- fold more static key metadata into the slot-cached interaction contract so
  active release no longer needs a legacy event-key merge path
- start recommendation 2 after recommendation 1 no longer depends on the old
  mirror state

## 2026-04-13: Recommendation 1 completed mirror removal

Completed in this pass:

- Removed the remaining active-slot compatibility mirrors from
  `active_key_state_t`, so slot-owned interaction semantics now live only in
  `interaction.view`.
- Deleted the old `binding`, `timing`, and `semantic` storage from
  `runtime_shared_state.h`, and removed the mirrored `hold_strategy` field from
  slot lifecycle state.
- Simplified `key_runtime_slot_interaction(...)` so it reads only the cached
  interaction snapshot instead of reconstructing fallback views from split slot
  fragments.
- Removed the legacy active-release merge path that used release-event handled
  key metadata to fill gaps in slot state. Active release now resolves from the
  slot-cached interaction contract only.
- Migrated the remaining host fixtures and runtime-debug expectations onto the
  slot-cached interaction contract, including release, scan, feedback,
  preflight, and snapshot coverage.
- Updated a small number of release and multi-tap fixtures so they seed pd-mode,
  layer, and multi-tap semantics on the active slot itself instead of expecting
  release-time re-resolution to infer them later.

Contracts touched:

- `users/noah/lib/state/runtime/runtime_shared_state.h`
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c`
- `tests/host/key_runtime_feedback_test.c`
- `tests/host/key_runtime_preflight_test.c`
- `tests/host/key_runtime_slot_test.c`
- `tests/host/key_runtime_transition_test.c`
- `tests/host/runtime_debug_test.c`

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- start recommendation 2 so action meaning stops being rediscovered from raw
  keycodes across runtime, feedback, and policy modules
- use the now-single interaction contract as the migration base for typed action
  descriptors instead of adding another parallel metadata seam

## 2026-04-13: Recommendation 2 started

Completed in this pass:

- Introduced a first typed action-descriptor surface on
  `users/noah/lib/action/action_dispatch.h` so callers can classify authored
  actions through `noah_action_desc_t` instead of re-checking raw keycodes in
  multiple modules.
- Re-based the existing action-dispatch predicate helpers on that descriptor
  surface where it was safe to do so, keeping the old boolean helper API intact
  for compatibility.
- Migrated `action_lifecycle.c` to resolve one descriptor per action and route
  tap/press/release behavior from that descriptor instead of repeatedly
  branching on raw keycode tests.
- Migrated direct-action preflight dispatch to use the descriptor for
  layer-lock and pd-lock routing.
- Added direct host coverage for descriptor classification in the action
  dispatch test harness.
- Kept qmk-behavior classification behind the existing
  `action_dispatch_is_qmk_behavior_keycode()` seam for now so the lightweight
  host harnesses that intentionally stub that classification still compile and
  exercise behavior without needing a broader test rewrite.

Contracts touched:

- `users/noah/lib/action/action_dispatch.[ch]`
- `users/noah/lib/action/action_lifecycle.c`
- `users/noah/lib/key/runtime/key_runtime_preflight.c`
- `tests/host/action_dispatch_test.c`
- `tests/host/key_runtime_preflight_test.c`

Verification run in this pass:

- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- migrate the remaining action-policy callers onto the descriptor surface,
  especially key-runtime slot policy and feedback paths that still infer action
  meaning from raw keycodes
- decide whether qmk-behavior classification should move fully into the
  descriptor once the host harness stubs are consolidated enough to support it

## 2026-04-13: Recommendation 2 continued

Completed in this pass:

- Moved the key-runtime slot policy layer onto `noah_action_desc_t` for the
  remaining hold-threshold and pulse decisions that still reclassified raw
  action keycodes.
- Moved key-runtime feedback onto the descriptor surface for held-action layer
  and pd-mode classification instead of mixing direct raw-keycode tests with
  the new descriptor path.
- Moved the pending multi-tap release/scan policy checks that still depended on
  raw action classification onto the descriptor surface too.
- Adjusted the lightweight host harnesses so the shared inline descriptor can
  still rely on the existing classifier seams (`layer lock`, `macro`,
  `raw layer action`, `qmk behavior`, `pd lock`, `pd mode`) without forcing the
  runtime tests to link the full action-dispatch module.
- Preserved the previous VIA macro contract by keeping
  `noah_qmk_contract_try_play_via_macro()` unconditional inside lifecycle tap
  handling while leaving the broader descriptor migration intact.

Contracts touched:

- `users/noah/lib/action/action_dispatch.[ch]`
- `users/noah/lib/action/action_lifecycle.c`
- `users/noah/lib/key/runtime/key_runtime_feedback.c`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`
- `tests/host/action_dispatch_test.c`
- `tests/host/action_lifecycle_test.c`
- `tests/host/key_runtime_feedback_test.c`
- `tests/host/key_runtime_modifier_hold_integration_test.c`
- `tests/host/key_runtime_preflight_test.c`
- `tests/host/key_runtime_slot_test.c`
- `tests/host/key_runtime_transition_test.c`
- `tests/host/via_macro_action_lifecycle_test.c`

Verification run in this pass:

- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- decide whether to move `layer lock`, `raw layer action`, and `macro`
  classification fully into the descriptor internals once the remaining host
  harness stubs are consolidated enough to stop depending on those helper seams
- continue recommendation 2 by migrating any remaining runtime helper code that
  still consults `noah_action_hold_kind()` or direct action predicates where a
  descriptor is now the clearer contract

## 2026-04-13: Recommendation 2 ownership follow-up

Completed in this pass:

- Added descriptor helpers for shared-vs-owned dispatch semantics so action
  ownership code can ask the action descriptor directly whether a hold uses
  shared dispatch.
- Moved `held_action.c` off the lifecycle hold-kind query for ownership
  refcount decisions. The ownership layer now depends on the action descriptor
  instead of re-entering lifecycle to classify dispatch mode.
- Reworked the held-action host test to model per-key and press-only behavior
  with real action shapes (`MO(...)` and `LOCK_LAYER(...)`) instead of a fake
  lifecycle-only hold-kind stub.
- Filled in the remaining runtime-debug host harness classifier shims so the
  full host suite can link `held_action.c` through the new descriptor path.

Contracts touched:

- `users/noah/lib/action/action_dispatch.h`
- `users/noah/lib/key/ownership/held_action.c`
- `tests/host/held_action_test.c`
- `tests/host/runtime_debug_test.c`

Verification run in this pass:

- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- decide whether `noah_action_hold_kind()` should remain as a public lifecycle
  compatibility wrapper or collapse into a thin helper around the descriptor
  semantics everywhere
- continue recommendation 2 by removing any remaining production callers that
  still need lifecycle-owned action classification instead of descriptor-owned
  action classification

## 2026-04-13: Recommendation 2 completed

Completed in this pass:

- Made `noah_action_desc_t` the canonical action-classification contract.
  `noah_action_describe()` now computes layer-lock, raw layer-action, macro,
  qmk-behavior, layer-tap, owned-momentary-layer, pd-mode, and pd-lock
  semantics directly instead of delegating that work to older boolean helper
  seams.
- Rebased the older exported action predicate helpers on the canonical
  classifier so they now act as compatibility projections rather than as a
  second independently-maintained classification system.
- Migrated the remaining production call sites that still inferred action
  meaning directly from helper predicates:
  `handled_key.c`, `key_behavior_lookup.c`, and `keymap_validation.c` now read
  descriptor semantics instead.
- Extended the host QMK stub surface with real encodable `TO(...)`, `OSM(...)`,
  `MT(...)`, and related layer/mod ranges so action tests no longer rely on
  fake “pretend qmk behavior” keycodes.
- Reworked the affected host fixtures to use real encodable action families
  where recommendation 2 had previously been masked by stub-only
  classification, including action lifecycle, keymap validation, slot/transition
  runtime tests, and real-profile host runners.
- Fixed the real-profile host validation/integration runners to honor the
  authored keymap's real `LAYER_COUNT` via `QMK_STUB_SUPPRESS_LAYER_COUNT`
  instead of silently iterating beyond the authored layer array.

Contracts touched:

- `users/noah/lib/action/action_dispatch.[ch]`
- `users/noah/lib/key/interaction/handled_key.c`
- `users/noah/lib/key/interaction/key_behavior_lookup.c`
- `users/noah/lib/key/interaction/keymap_validation.c`
- `tests/host/include/qmk_stub.h`
- `tests/host/action_dispatch_test.c`
- `tests/host/action_lifecycle_test.c`
- `tests/host/key_behavior_lookup_test.c`
- `tests/host/key_behavior_validation_test.c`
- `tests/host/keymap_validation_test.c`
- `tests/host/key_runtime_slot_test.c`
- `tests/host/key_runtime_transition_test.c`
- `tests/host/pd_mode_key_runtime_integration_test.c`
- `tests/host/pointer_layer_policy_test.c`
- `tests/host/real_profile_validation_test.c`
- `tests/host/run_real_profile_validation_tests.sh`
- `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`

Verification run in this pass:

- `git diff --check`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- treat `action_dispatch_is_*()` and `noah_action_hold_kind()` as compatibility
  projections only and avoid introducing any new production callers
- move on to recommendation 3, since recommendation 2 is now structurally
  complete and production action classification is descriptor-owned

## 2026-04-13: Recommendation 2 compatibility cleanup

Completed in this pass:

- Removed the remaining exported compatibility classifier layer from
  `action_dispatch.[ch]`. The older `action_dispatch_is_*()` helpers no longer
  exist as a public API; action classification is descriptor-owned only.
- Removed `noah_action_hold_kind()` and its enum from `action_lifecycle.[ch]`.
  Hold/dispatch-shape classification now lives entirely on the descriptor
  helper surface.
- Reworked the remaining host assertions and harness code to use
  `noah_action_describe(...)`, `noah_action_desc_is_*()`, and the inline
  keycode-family helpers directly instead of rebuilding or stubbing the removed
  wrapper API by name.
- Cleaned the last test-local fallback-hold reconstruction path in
  `key_runtime_slot_test.c` so it now asks the descriptor whether a keycode is
  a qmk-behavior action instead of consulting a removed wrapper symbol.

Contracts touched:

- `users/noah/lib/action/action_dispatch.[ch]`
- `users/noah/lib/action/action_lifecycle.[ch]`
- `tests/host/action_lifecycle_test.c`
- `tests/host/held_action_test.c`
- `tests/host/key_behavior_lookup_test.c`
- `tests/host/key_behavior_validation_test.c`
- `tests/host/key_runtime_admission_test.c`
- `tests/host/key_runtime_feedback_test.c`
- `tests/host/key_runtime_modifier_hold_integration_test.c`
- `tests/host/key_runtime_preflight_test.c`
- `tests/host/key_runtime_scenario_harness.c`
- `tests/host/key_runtime_slot_test.c`
- `tests/host/key_runtime_transition_test.c`
- `tests/host/pd_mode_key_runtime_integration_test.c`
- `tests/host/real_profile_validation_test.c`
- `tests/host/runtime_debug_test.c`
- `tests/host/via_macro_action_lifecycle_test.c`

Verification run in this pass:

- `git diff --check`
- `rg -n "action_dispatch_is_layer_action\\(|action_dispatch_is_layer_lock\\(|action_dispatch_is_raw_qmk_layer_action\\(|action_dispatch_is_macro\\(|action_dispatch_is_qmk_behavior_keycode\\(|noah_action_hold_kind\\(" users/noah/lib tests/host`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- start recommendation 3 without carrying the removed compatibility action
  contracts into the runtime-storage refactor

## 2026-04-13: Recommendation 3 landed

Completed in this pass:

- Moved the key-runtime-owned storage layout out of
  `state/runtime/runtime_shared_state.h` into new
  `key/runtime/key_runtime_shared_state.h`.
- That new key-runtime header now owns:
  `key_runtime_slot_phase_t`, slot owner/lifecycle/interaction storage types,
  `active_key_state_t`, `key_runtime_feedback_state_t`,
  `key_runtime_shared_state_t`, `ACTIVE_KEY_STATE_INIT`, and the slot-table
  capacity constant.
- Slimmed `state/runtime/runtime_shared_state.h` down to the aggregate wrapper
  only: it now composes `key_runtime_shared_state_t` with
  `pd_mode_runtime_shared_state_t` instead of defining key-runtime internals
  itself.
- Reworked `runtime_shared_state_reset()` so the aggregate layer delegates
  key-state initialization back to `key_runtime_shared_state_reset(...)`
  instead of directly knowing how slots are initialized.

Contracts touched:

- `users/noah/lib/key/runtime/key_runtime_shared_state.h`
- `users/noah/lib/state/runtime/runtime_shared_state.h`
- `users/noah/lib/state/runtime/runtime_shared_state.c`

Verification run in this pass:

- `git diff --check`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- move on to recommendation 4

## 2026-04-13: Recommendation 3 include cleanup landed

Completed in this pass:

- Narrowed the include direction around the runtime aggregate:
  `key_runtime_state.h` no longer re-exports
  `state/runtime/runtime_shared_state.h`.
- Added a minimal `key_runtime_shared_state()` seam so key-runtime modules can
  reach the `key` slice of userspace state without importing the aggregate
  `runtime_shared_state_t` contract.
- Updated the remaining key-runtime implementation files that were still
  piercing the aggregate directly:
  `key_runtime_feedback.c` and `slot/key_runtime_slot.c`.
- Updated host tests that actually need the aggregate singleton to include
  `runtime_shared_state.h` explicitly instead of getting it transitively
  through `key_runtime_state.h`.
- Replaced raw aggregate zeroing in those host tests with
  `runtime_shared_state_reset(&noah_runtime_shared_state)` so test resets now
  follow the same initialization contract as production code.

Contracts touched:

- `users/noah/lib/key/runtime/key_runtime_shared_state.h`
- `users/noah/lib/key/runtime/key_runtime_state.h`
- `users/noah/lib/key/runtime/key_runtime.c`
- `users/noah/lib/key/runtime/key_runtime_feedback.c`
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c`
- `users/noah/lib/state/runtime/runtime_shared_state.c`
- `tests/host/key_runtime_admission_test.c`
- `tests/host/key_runtime_feedback_test.c`
- `tests/host/key_runtime_layer_lock_integration_test.c`
- `tests/host/key_runtime_modifier_hold_integration_test.c`
- `tests/host/key_runtime_preflight_test.c`
- `tests/host/key_runtime_slot_test.c`
- `tests/host/key_runtime_transition_test.c`
- `tests/host/pd_mode_key_runtime_integration_test.c`
- `tests/host/pd_mode_test.c`
- `tests/host/real_profile_thumb_layer_lock_integration_test.c`

Verification run in this pass:

- `git diff --check`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Next steps:

- move on to recommendation 4
