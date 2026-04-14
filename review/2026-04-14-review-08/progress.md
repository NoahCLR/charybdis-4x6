# Implementation Progress

This file tracks the audit captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Follow-up quality review of the review-07 implementation

Completed in this pass:

- started with `git status --short`
- re-read the newest existing review folder,
  `review/2026-04-14-review-07/`, before reviewing the landed refactor work
- audited the current tree with focus on:
  - the post-hardening action-kind metadata/dispatch contract
  - the runtime-debug/public-state boundary after the semantic helper pass
  - higher-level integration/scenario harness coupling after the review-07
    cleanup
  - remaining weak compatibility shims in harness-based host runners
  - review/documentation integrity after the shipping hardening pass
- wrote a new review folder for this follow-up audit

Key findings recorded in this review:

- no must-fix production correctness failure found
- should-fix: the shared integration harness still hides incomplete subsystem
  linkage behind weak debug/reset shims, so harness-based runners can still
  test against zeroed runtime surfaces instead of being forced to link the real
  subsystem
- should-fix: the runtime-debug cleanup still exports raw slot layout through
  `noah_runtime_debug_slot_copy(...)`, so the higher-level semantic seam is not
  finished yet
- should-fix: the action-kind guard improved current-table coverage, but the
  invalid-kind contract is still inconsistent between metadata and dispatch and
  degrades silently in normal non-console firmware builds

Areas assessed as solid in this pass:

- the review-07 pass did close its stated should-fix items
- stale synthetic-record signature drift is gone from the host suites
- the main harnesses now use the real `runtime_debug.c` seam
- the action-kind table coverage check for current enum rows is real
- the key-runtime index and handled-key materialization seams remain solid

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
  `charybdis-4x6/review/2026-04-14-review-08/`

Next steps:

- decide whether to harden the shared integration harness by removing the weak
  debug/reset fallback layer and localizing any remaining stubs to individual
  runners
- finish the runtime-debug boundary cleanup by replacing public raw slot-copy
  usage with semantic helpers
- normalize the invalid-kind policy across action metadata and dispatch, then
  add one focused host regression for that policy

### Implementation follow-up for the review-08 should-fix items

Completed in this pass:

- removed the shared weak debug/reset fallback layer from
  `tests/host/key_runtime_integration_harness.c`
- localized the remaining zeroed debug/reset stubs to the specific harness
  suites that still need them:
  - `tests/host/key_runtime_layer_lock_integration_test.c`
  - `tests/host/pd_mode_key_runtime_integration_test.c`
  - `tests/host/real_profile_thumb_layer_lock_integration_test.c`
  - `tests/host/key_runtime_modifier_hold_integration_test.c`
- finished the runtime-debug boundary cleanup for higher-level harnesses by:
  - removing public `noah_runtime_debug_slot_copy(...)` from
    `users/noah/lib/state/runtime/runtime_debug.h`
  - moving raw slot-copy logic to a private helper inside
    `users/noah/lib/state/runtime/runtime_debug.c`
  - updating `tests/host/key_runtime_scenario_harness.c` to use semantic
    runtime-debug queries instead of materializing raw slot structs
  - updating `tests/host/runtime_debug_test.c` so routine slot assertions use
    semantic helpers while explicit aggregate-shape checks stay on the raw
    `core` snapshot
- normalized invalid action-kind handling to a single reject policy by:
  - making `noah_action_kind_def(...)` return `NULL` for invalid kinds instead
    of normalizing back to the literal row
  - making metadata/policy helpers treat invalid descriptors as unsupported
  - keeping dispatch on the same reject/noop path and recording an internal
    dispatch fault marker before the host-test hard fail or firmware noop path
  - adding focused invalid-kind regression coverage in
    `tests/host/action_lifecycle_test.c`

Contracts touched in this pass:

- `tests/host/key_runtime_integration_harness.c`
- `users/noah/lib/state/runtime/runtime_debug.h`
- `users/noah/lib/action/action_kind_internal.h`
- `users/noah/lib/action/action_kind_dispatch_internal.h`

Verification run during this pass:

- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- all targeted suites above passed
- the feature-gate compile suite passed
- the full host suite passed
- the firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope:

- no sibling workspace folders were edited
- implementation changes remain confined to `users/noah/`, `tests/host/`, and
  `review/2026-04-14-review-08/`

Next steps:

- the review-08 should-fix items are closed by this pass
- if we want a later optional cleanup, the next non-blocking step would be to
  shrink `noah_runtime_debug_snapshot_t.core` itself once we are ready to
  redesign the low-level aggregate debug snapshot, but that is no longer a
  shipping blocker

### Optional host-surface cleanup

Completed in this pass:

- removed the dead shared host stub file
  `tests/host/action_kind_host_stubs.c`
- confirmed that the remaining host runners enumerate their required action and
  runtime-debug sources explicitly and no longer reference that file

Verification run during this pass:

- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- the action and authored-profile host runners above passed after removing the
  dead file
- the full host suite passed
- the firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope:

- no sibling workspace folders were edited
- this cleanup touched `tests/host/` and
  `review/2026-04-14-review-08/progress.md` only

Next steps:

- no further cleanup is required for the removed shared host stub
- if we want another non-shipping tidy-up later, the next reasonable target is
  the low-level `noah_runtime_debug_snapshot_t.core` aggregate itself

### Optional runtime-debug aggregate redesign

Completed in this pass:

- removed the public `runtime_shared_state_t` leak from
  `users/noah/lib/state/runtime/runtime_debug.h` by redesigning
  `noah_runtime_debug_snapshot_t` as a semantic snapshot
- split the main snapshot into semantic key/pd debug views instead of exposing
  the raw `snapshot.core` aggregate
- added semantic slot snapshot state for:
  - owner keycode
  - tap action
  - held-action keycode
  - pending multi-tap count
  - pending multi-tap holding
  - has-pending-multi-tap
  - hold-complete
- kept snapshot-based runtime-debug helpers stable, but reimplemented them on
  the semantic key snapshot instead of raw `active_key_state_t`
- updated `tests/host/runtime_debug_test.c` to validate the semantic debug
  surface directly instead of reaching through `snapshot.core`

Contracts touched in this pass:

- `users/noah/lib/state/runtime/runtime_debug.h`
- `users/noah/lib/state/runtime/runtime_debug.c`
- `tests/host/runtime_debug_test.c`

Verification run during this pass:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- all targeted runtime-debug and integration suites above passed
- the feature-gate compile suite passed
- the full host suite passed
- the firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope:

- no sibling workspace folders were edited
- this cleanup touched `users/noah/`, `tests/host/`, and
  `review/2026-04-14-review-08/progress.md`

Next steps:

- no further runtime-debug cleanup is required for shipping
- if we later want a lower-level raw runtime aggregate for specialized tests,
  add it as an explicit opt-in raw snapshot surface instead of reattaching it
  to the main semantic debug API
