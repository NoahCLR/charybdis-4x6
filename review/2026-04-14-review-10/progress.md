# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Deep userspace architecture review

Completed in this pass:

- started with `git status --short`
- read the newest existing review folder before beginning a new review pass
- mapped the current userspace structure under `users/noah/`
- reviewed the authored keymap boundary under
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`
- inspected the main runtime seams for:
  - hook entry points
  - handled-key runtime state and transitions
  - action classification and dispatch
  - pd-mode registry/state/policy
  - layer ownership and held-action ownership
  - RGB stage orchestration
  - test and compile-gate coverage
- wrote a new review folder:
  `review/2026-04-14-review-10/`

Key findings recorded in this review:

- should-fix: runtime state ownership is split between the global
  `runtime_shared_state` aggregate and several module-private static state
  stores, which keeps reset/debug/integration work non-local
- should-fix: action extensibility is still a manually synchronized closed set
  across multiple core files, unlike the cleaner manifest-driven pd-mode design
- should-fix: top-level userspace orchestration is still a hard-coded ordered
  pipeline in `process_record`, init/scan wiring, and RGB stage composition
- optional: the authored keymap surface is data-driven but too consolidated in
  one large `keymap.c`

Areas assessed as solid in this pass:

- the `noah_runtime.h` vs `noah_keymap.h` boundary is clear and enforced
- the handled-key engine is a real reducer/effect-plan state machine rather
  than ad hoc QMK hook logic
- the pd-mode subsystem is the strongest extensibility model in the tree
- test and compile-gate discipline is strong enough to support incremental
  architecture work safely

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- full host suite passed
- firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all changes in this pass are confined to
  `charybdis-4x6/review/2026-04-14-review-10/`

### Runtime context Milestone 1

Completed in this pass:

- added `users/noah/lib/state/runtime/runtime_context.h` as the canonical
  singleton-owned runtime storage surface
- rehomed mutable state for:
  - `runtime_shared_state`
  - layer ownership
  - held-action ownership
  - held-repeat ownership
  - keyboard modifier ownership
  - runtime trace
- preserved the existing module APIs and the
  `runtime_shared_state.h` surface as compatibility wrappers into the runtime
  context
- changed `noah_runtime_debug_snapshot()` into a true context-backed aggregate
  snapshot instead of a manual cross-module callback collector
- changed `noah_runtime_reset_for_test()` to reset the singleton context, while
  preserving the QMK layer/mod/report cleanup behavior
- moved repo-owned host tests off
  `runtime_shared_state_reset(&noah_runtime_shared_state)`
- removed local host-test `*_debug_snapshot()` / `*_reset_for_test()` fallback
  stubs that were only compensating for the old split ownership model
- updated standalone host runners that now need the context-backed shared-state
  object linked in

Architecture result after this pass:

- runtime-owned mutable state now has one canonical owner
- public behavior and authored keymap semantics are unchanged
- the remaining state debt is no longer “private statics everywhere”; it is
  boundary cleanup around the compatibility alias and a few direct shared-state
  reads that still need to be narrowed in Milestone 2

### Runtime context Milestone 2 boundary cleanup

Completed in this pass:

- added `pd_mode_runtime_shared_state()` as the pd-mode slice accessor in
  `users/noah/lib/state/runtime/runtime_shared_state.h`
- moved `pd_mode_state.c` and `pd_mode_snapshot.c` off direct
  `noah_runtime_shared_state` reads/writes and onto the pd-mode slice accessor
- tightened the compatibility intent in `runtime_shared_state.h` so new code is
  steered toward slice accessors instead of the legacy aggregate alias

Architecture result after this pass:

- repo-owned runtime code no longer reads the shared-state compatibility alias
  directly
- the compatibility shim still exists, but it is now isolated to the runtime
  layer instead of leaking into pd-mode call sites
- the next runtime-state step is now mostly policy and cleanup: keep the shim
  narrow rather than chasing more hidden storage owners

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted pd-mode and compile-gate checks passed
- full host suite passed
- firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_layer_ownership_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- all targeted Milestone 1 runners passed
- full host suite passed
- firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope for this pass:

- no sibling workspace folders were edited
- changes are confined to `charybdis-4x6/`
- runtime files, host tests, host runners, and the active review folder were
  updated together

Next steps:

- keep `runtime_shared_state.h` narrow as an explicit compatibility shim and
  avoid adding new direct aggregate call sites
- extend the new action-kind registry so hold-preview, feedback, and authored
  policy hooks can live with kind definitions instead of only in downstream
  runtime policy files
- only then tackle declarative hook registration and authored keymap file
  decomposition

### Action-kind registry foundation

Completed in this pass:

- added `users/noah/lib/action/action_kind_registry_list.h` as the single-source
  action-kind definition list
- moved the `noah_action_kind_t` enum generation in
  `users/noah/lib/action/action_dispatch.h` onto that registry list
- moved metadata initialization and descriptor classification in
  `users/noah/lib/action/action_kind.c` onto the same registry rows
- moved dispatch-op initialization in
  `users/noah/lib/action/action_kind_dispatch.c` onto the same registry rows
- kept metadata ownership in `action_kind.c` and dispatch-op ownership in
  `action_kind_dispatch.c` so existing host runners that intentionally compile
  only the metadata surface do not need broader link wiring
- changed `noah_action_describe()` from a hard-coded ordered branch chain to a
  registry-driven matcher/priority scan without changing authored behavior

Architecture result after this pass:

- enum identity, classification order, metadata, and dispatch ops are now
  synchronized from one list instead of parallel hand-maintained tables
- the action-kind core is materially easier to extend safely than it was at the
  start of the day
- the remaining action extensibility debt has shifted downstream: hold-preview,
  feedback, and authored-policy semantics still live outside the registry in
  central runtime policy files

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted action, validation, integration, and compile-gate runners passed
- full host suite passed
- firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope for this pass:

- no sibling workspace folders were edited
- changes are confined to `charybdis-4x6/`
- action metadata, action dispatch, and the active review folder were updated
  together

Next steps:

- keep the registry as the single source for action-kind identity and wiring
- pull the remaining fallback-hold and default-action policy into
  registry-backed hooks or policy descriptors
- only after that, move on to declarative hook registration

### Action policy flags

Completed in this pass:

- extended `users/noah/lib/action/action_kind_registry_list.h` with explicit
  action policy flags for:
  - direct runtime-handled keycodes
  - momentary-layer keycodes
  - authored layer-tap contracts
  - release-layer-before-action behavior
  - press-and-hold held-lifecycle behavior
  - default tap extraction for layer taps
- added public descriptor helpers in
  `users/noah/lib/action/action_dispatch.h` /
  `users/noah/lib/action/action_kind.c` so downstream runtime code can ask the
  action registry for those semantics directly
- moved handled-key lookup onto those helpers in
  `users/noah/lib/key/interaction/key_behavior_lookup.c`
- moved handled-key hold semantics onto those helpers in
  `users/noah/lib/key/interaction/handled_key_policy.h`
- moved pending multi-tap layer-release ordering onto those helpers in
  `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`
- moved default layer-tap tap-key extraction onto those helpers in
  `users/noah/lib/key/interaction/handled_key_defaults.c`
- added host assertions for the new helper surface in
  `tests/host/action_dispatch_test.c` and `tests/host/action_lifecycle_test.c`

Architecture result after this pass:

- the action registry now owns more than enum/metadata/dispatch; it also owns
  the first layer of handled-key/runtime action policy
- handled-key runtime code no longer needs to hard-code several action-family
  facts like “layer lock releases the momentary layer first” or “pd-mode hold
  is directly handled”
- the remaining action-policy debt is now narrower: fallback-hold eligibility
  and some transparency/default-action derivation still live downstream

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted action, key-behavior, integration, and compile-gate runners passed
- full host suite passed
- firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope for this pass:

- no sibling workspace folders were edited
- changes are confined to `charybdis-4x6/`
- action metadata, handled-key runtime call sites, host tests, and the active
  review folder were updated together

Next steps:

- keep the action registry as the primary source of action-family semantics
- move fallback-hold eligibility and remaining transparency/default-action rules
  behind registry-backed policy hooks
- only after that, move on to declarative hook registration
