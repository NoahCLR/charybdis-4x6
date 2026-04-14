# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Architecture review of the current userspace

Completed in this pass:

- started with `git status --short`
- re-read the newest existing review folder,
  `review/2026-04-14-review-03/`, before reviewing the current architecture
- audited the current userspace structure across:
  - key runtime orchestration and slot/index state
  - handled-key resolution/materialization boundaries
  - action classification and lifecycle dispatch
  - macro provider/cache structure
  - pointing runtime and split runtime sync
  - host test seams and runtime observability
- wrote a new review folder for this architecture-focused pass

Key findings recorded in this review:

- no must-fix architecture break found
- should-fix: the key-runtime index is still a rebuild-on-read cache rather
  than a mutation-maintained registry
- should-fix: `noah_process_record_user(...)` remains a hard-coded ordered
  router, so new input features still require editing core dispatch logic
- should-fix: the action abstraction is split across descriptor, lifecycle,
  validation, and handled-key policy modules, which makes new action kinds
  cross-cutting changes
- should-fix: several host runtime suites still bypass the production
  `handled_key_materialize(...)` implementation through a test-owned fixture
- optional cleanup: `handled_key.h` still carries too much public inline policy

Areas assessed as solid in this pass:

- authored keymap data and reusable userspace runtime code are separated well
- handled-key materialization is a meaningful seam now
- key-runtime slot reducers are more navigable than the older monolithic flow
- compat boundaries are centralized usefully under `users/noah/lib/compat/`
- runtime debug and trace surfaces make the system unusually observable for
  firmware code

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- full host suite passed
- firmware build passed

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all changes in this pass are confined to
  `charybdis-4x6/review/2026-04-14-review-04/`

Next steps:

- decide whether to treat the key-runtime index as accepted scan-based cache
  debt or finish the original registry design
- decide whether to introduce a table-driven record-handler pipeline before
  adding more input features
- if new action kinds are planned, consolidate the action-kind contract before
  extending it further

### Implementation of review-04 follow-up

Completed in this pass:

- landed a mutation-maintained key-runtime index in
  `users/noah/lib/key/runtime/key_runtime_index.c`
- routed slot/index membership updates through slot mutation sites instead of
  rebuild-on-read calls from consumers
- replaced the hard-coded `noah_process_record_user(...)` chain with a private
  ordered handler pipeline in `users/noah/lib/key/runtime/key_runtime_process.c`
- cut the main runtime host suites over to the production
  `handled_key_materialize(...)` seam instead of test-owned materialization
  reimplementation
- split handled-key authored lookup and resolution accessors into dedicated
  translation units so runtime suites can link the real materializer cleanly
- fixed runtime interaction materialization so effective transparent
  `layer` / `pd_mode` / `flags` come from the materialized handled key rather
  than the authored row
- moved handled-key hold/release contract builders out of public
  `handled_key.h` into internal `handled_key_policy.h`
- introduced shared action-kind metadata in
  `users/noah/lib/action/action_kind.c` so action classification, capability
  flags, and handled-key policy hints are defined in one place
- moved `noah_action_describe(...)` onto that shared action-kind metadata seam
- updated `action_lifecycle.c` so layer-lock and pd-lock behavior stays kind
  owned while preserving the existing macro preflight ordering
- updated host runners and userspace source manifest for the new runtime and
  action units

Implementation note:

- the action-model cleanup landed as shared metadata/classification plus
  internal handled-key policy hints; tap/press/release ops still live in
  `action_lifecycle.c` rather than in the shared action-kind table

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- all targeted host suites above passed
- full host suite passed
- firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope:

- no sibling workspace folders were edited
- implementation changes stayed inside `users/noah/`, `tests/host/`, and
  `review/2026-04-14-review-04/`

Next steps:

- if another action-kind expansion is needed later, consider moving lifecycle
  ops into the shared action-kind definition as a separate cleanup
- review-05 immediately followed this pass and closed three remaining seams:
  the duplicated handled-key contract path, the registry test bypasses, and
  the partial action-kind lifecycle consolidation
- treat this pass as a major architecture improvement, but not the final
  cleanup point for those remaining review-05 follow-ups
