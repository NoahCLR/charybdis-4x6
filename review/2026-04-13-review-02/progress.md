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
