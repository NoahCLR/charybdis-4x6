# Implementation Progress

This file tracks the audit captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Follow-up quality review of the review-06 implementation

Completed in this pass:

- started with `git status --short`
- re-read the newest existing review folder,
  `review/2026-04-14-review-06/`, before reviewing the landed refactor work
- audited the current tree with focus on:
  - the action-kind metadata/dispatch split
  - dispatch/test interface cleanliness around synthetic-record hooks
  - the key-runtime index and runtime-debug/public-state boundaries
  - higher-level host harness coupling to raw runtime-debug core layout
  - review/documentation integrity after the review-06 cleanup
- wrote a new review folder for this follow-up audit

Key findings recorded in this review:

- no must-fix production correctness failure found
- should-fix: several host suites still define
  `noah_dispatch_synthetic_record(...)` with a stale `void` signature instead
  of the real `bool` signature, so the compiler is not protecting the current
  dispatch seam
- should-fix: the runtime-debug layer still exports raw `core` storage, and
  higher-level harnesses still read `snapshot.core.key.*` directly even after
  the new semantic debug helpers were added
- should-fix: action-kind behavior is still spread across the public kind enum,
  the metadata table, and the dispatch ops table with no completeness guard
- optional cleanup: `review/2026-04-14-review-06/progress.md` now overstates
  finality with “no immediate follow-up is required”

Areas assessed as solid in this pass:

- `action_kind.c` is now genuinely metadata-only
- `action_lifecycle.c` owns action preflight/intercept ordering cleanly
- the key-runtime index remains mutation-maintained in production
- the handled-key host fixture is cleaner and higher-level runtime suites are
  back on the public authored/materialized seam

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
  `charybdis-4x6/review/2026-04-14-review-07/`

Next steps:

- make the affected host suites include the real synthetic-record dispatch
  header and update their local test doubles to the real signatures
- continue shrinking the runtime-debug/public-state boundary so higher-level
  harnesses can stop reading `snapshot.core.key.*` directly
- add a completeness guard or coverage check that keeps the action-kind enum,
  metadata table, and dispatch ops table in sync

### Shipping hardening pass for review-07 should-fix items

Completed in this pass:

- kept the current action-kind metadata/dispatch split, but hardened the seam
  instead of reopening the broader action-model refactor
- updated host suites that define `noah_dispatch_synthetic_record(...)` to
  include the real `users/noah/lib/action/synthetic_record.h` header and match
  the production `bool` signature
- added semantic key-runtime slot helpers to
  `users/noah/lib/state/runtime/runtime_debug.{h,c}` for:
  - slot copy by `keypos_t`
  - owner keycode
  - held-action keycode
  - pending multi-tap count
  - pending multi-tap holding state
  - slot-has-pending-multi-tap
  - slot-hold-complete
- removed the weak aggregate `noah_runtime_debug_snapshot(...)` implementation
  from `tests/host/key_runtime_integration_harness.c`
- linked the real `users/noah/lib/state/runtime/runtime_debug.c` into the
  integration runners that use the shared harness
- kept only narrow weak stubs in the integration harness for missing reset or
  sub-snapshot providers that some isolated runners do not link
- updated `tests/host/key_runtime_integration_harness.c`,
  `tests/host/key_runtime_scenario_harness.c`, and
  `tests/host/runtime_debug_test.c` to use semantic runtime-debug helpers
  instead of raw `snapshot.core.key.slots_by_position[...]` access for routine
  key-runtime assertions
- removed the remaining `tests/host/action_kind_host_stubs.c` usage from:
  - `tests/host/run_key_runtime_layer_lock_integration_tests.sh`
  - `tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- added action-kind completeness guards by:
  - marking each metadata row as explicitly defined in
    `users/noah/lib/action/action_kind.c`
  - exposing internal completeness queries for metadata and dispatch coverage
  - making `action_kind_dispatch.c` fail loudly in host builds and degrade to a
    logged noop dispatch path in non-host builds if a kind row is missing
  - adding a host assertion in `tests/host/action_lifecycle_test.c` that every
    `NOAH_ACTION_KIND_*` value has both metadata and dispatch coverage
- updated the review history so `review/2026-04-14-review-06/progress.md` no
  longer overstates closure

Contracts touched in this pass:

- `users/noah/lib/action/synthetic_record.h`
- `users/noah/lib/action/action_kind_internal.h`
- `users/noah/lib/action/action_kind_dispatch_internal.h`
- `users/noah/lib/state/runtime/runtime_debug.h`

Verification run during this pass:

- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
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
  `review/2026-04-14-review-07/`, plus this corrective note in
  `review/2026-04-14-review-06/progress.md`

Next steps:

- the review-07 should-fix items are closed by this pass
- if we want a later optional cleanup, the next non-blocking step would be to
  split the raw aggregate `noah_runtime_debug_snapshot_t.core` from the
  semantic high-level debug surface entirely instead of just avoiding it in the
  main harnesses
