# Implementation Progress

This file tracks the audit captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Follow-up quality review of the review-04 implementation

Completed in this pass:

- started with `git status --short`
- re-read the newest existing review folder,
  `review/2026-04-14-review-04/`, before reviewing the landed refactor work
- audited the current code focused on:
  - the mutation-maintained key-runtime index
  - the new `process_record_user` stage pipeline
  - the action-kind consolidation work
  - the handled-key materialization / runtime interaction seam
  - host-test coverage of the new registry and contract boundaries
- wrote a new review folder for this follow-up audit

Key findings recorded in this review:

- no must-fix correctness failure found
- should-fix: the handled-key “materialized contract” is still duplicated and
  leaked through `key_runtime_interaction.h`
- should-fix: the action-kind consolidation is only partial, and public
  `noah_action_desc_t` still exposes internal capability storage
- should-fix: several runtime suites still manually resync the key-runtime
  index, so the new mutation-maintained registry is not directly protected by
  tests
- optional cleanup: `review/2026-04-14-review-04/progress.md` now overstates
  finality

Areas assessed as solid in this pass:

- production key-runtime index maintenance is no longer rebuild-on-read
- the ordered process pipeline is materially cleaner than the previous
  monolithic router
- the main runtime suites now exercise the real
  `handled_key_materialize(...)` seam
- full host and firmware verification remain green after the refactor

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
  `charybdis-4x6/review/2026-04-14-review-05/`

Next steps:

- if you want to act on this audit, the highest-value cleanup is to make the
  materialized handled-key contract the only runtime contract source and stop
  leaking `handled_key_policy.h` through `key_runtime_interaction.h`
- after that, either fully consolidate action-kind lifecycle ops into the
  shared action-kind definition or narrow the comments/API so the partial state
  is explicit
- finally, add mutation-path tests that assert registry membership and
  `preview_owner_slot` / `pending_fallback_slot` transitions without manual
  resync helpers

### Implementation of review-05 follow-up

Completed in this pass:

- made `handled_key_materialized_t.contract` the authoritative runtime-facing
  handled-key contract and added
  `handled_key_materialized_refresh_contract(...)` as the internal refresh seam
- removed the `handled_key_policy.h` leak from
  `users/noah/lib/key/runtime/key_runtime_interaction.h`
- changed runtime override construction to mutate a materialized handled key,
  refresh its contract through handled-key internals, and only then cache a
  runtime interaction
- internalized `key_runtime_index_sync_slot(...)` behind
  `users/noah/lib/key/runtime/key_runtime_index_internal.h`
- cut runtime host suites over to production slot mutators so index membership,
  preview-owner selection, and pending-fallback selection are exercised without
  manual resync helpers
- added a dedicated host regression unit in
  `tests/host/key_runtime_index_test.c` and wired
  `tests/host/run_key_runtime_index_tests.sh` into
  `tests/host/run_all_host_tests.sh`
- finished the action-kind consolidation by removing `caps` from public
  `noah_action_desc_t`, moving lifecycle ops into the shared internal
  `noah_action_kind_def_t`, and routing public capability queries through the
  action-kind definition instead of descriptor storage
- added shared weak host stubs in `tests/host/action_kind_host_stubs.c` and
  updated isolated host runners so `action_kind.c` can own lifecycle hooks
  without requiring the full firmware link surface

Verification run during implementation:

- `git status --short`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`

Verification results so far:

- all targeted host suites above passed
- compile gates passed after updating isolated host runners to include the
  shared weak action-kind stubs

Workspace scope:

- no sibling workspace folders were edited
- implementation changes are confined to `users/noah/`, `tests/host/`,
  `review/2026-04-14-review-05/`, and the corrective note below in
  `review/2026-04-14-review-04/progress.md`

Next steps:

- run the final full host suite and firmware build
- if those stay green, this audit follow-up is complete
