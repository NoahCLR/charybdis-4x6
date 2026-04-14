# Implementation Progress

This file tracks the audit captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Follow-up quality review of the review-05 implementation

Completed in this pass:

- started with `git status --short`
- re-read the newest existing review folder,
  `review/2026-04-14-review-05/`, before reviewing the landed refactor work
- audited the current tree with focus on:
  - the handled-key materialization/refresh seam
  - the mutation-maintained key-runtime index and its public read surface
  - the new action-kind table and its test/build consequences
  - the host handled-key fixture and runtime-suite coupling
  - review/documentation integrity after the review-05 implementation
- wrote a new review folder for this follow-up audit

Key findings recorded in this review:

- no must-fix production correctness failure found
- should-fix: the new action-kind seam is over-coupled to runtime side effects,
  and the shared weak host-stub layer is now masking that coupling; the shim
  has already drifted from real function signatures
- should-fix: the key-runtime index is mutation-maintained, but its public API
  still exposes raw storage/layout and core consumers still depend on those raw
  arrays directly
- should-fix: the shared host handled-key fixture still exposes an internal
  “refresh overridden materialized contract” path that higher-level runtime
  suites rely on
- optional cleanup: `review/2026-04-14-review-05/progress.md` still reads as if
  the final full-suite/build verification had not yet happened

Areas assessed as solid in this pass:

- the handled-key contract is now authoritative on
  `handled_key_materialized_t.contract`
- the runtime index is genuinely mutation-maintained in production
- the dedicated index regression suite materially improves coverage
- the public action descriptor is smaller after removing `caps`

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
  `charybdis-4x6/review/2026-04-14-review-06/`

Next steps:

- if you want to act on this audit, the highest-value cleanup is to split the
  action-kind metadata seam from the executable dispatch-hook seam, or at least
  make the shared host stubs type-check against the real headers
- after that, narrow the key-runtime index read surface so raw slot-index
  arrays are not part of the public contract
- finally, reduce the shared host handled-key fixture back to the public
  authored/materialized seam and keep internal override-refresh helpers local
  to constructor-level tests

### Implementation follow-up for the review-06 audit

Completed in this pass:

- started from the existing in-flight refactor state with `git status --short`
- split action-kind metadata from side-effectful dispatch execution:
  - `users/noah/lib/action/action_kind.c` now owns classification and metadata
  - `users/noah/lib/action/action_kind_dispatch.c` now owns tap/press/release
    dispatch hooks
  - metadata-only host runners no longer link the shared dispatch stub layer
- corrected the shared dispatch host stubs to type-check against the real
  headers and limited their use to runners that actually execute dispatch hooks
- narrowed the key-runtime index public seam:
  - removed the public raw index snapshot accessor from
    `users/noah/lib/key/runtime/key_runtime_index.h`
  - moved slot-pointer snapshot helpers behind
    `users/noah/lib/key/runtime/key_runtime_index_internal.h`
  - updated runtime consumers to use semantic accessors or internal snapshot
    helpers instead of copying raw slot-index arrays directly
- trimmed the shared handled-key host fixture back to the public authored ->
  materialized -> interaction seam
- localized the internal materialized-contract refresh path to the
  constructor-level slot test and removed it from the higher-level transition
  suite
- removed the stale handled-key fixture include from
  `tests/host/key_runtime_admission_test.c`
- added semantic runtime-debug index helpers so runtime-debug assertions no
  longer need to read raw slot-index arrays directly

Key architectural outcomes:

- the action-kind metadata seam is now independent from dispatch-time runtime
  side effects
- the mutation-maintained key-runtime registry is consumed through semantic
  accessors in production code
- higher-level runtime suites now use authored resolutions and the production
  `handled_key_materialize(...)` seam instead of a shared internal override
  helper
- the runtime-debug surface now owns slot-index decoding for tests that need
  index ordering visibility

Verification run in this implementation pass:

- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- all targeted host suites above passed
- the full host suite passed
- the firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope:

- no sibling workspace folders were edited
- implementation changes are confined to `users/noah/`, `tests/host/`, and
  `review/2026-04-14-review-06/`

Next steps:

- follow-up review work in `review/2026-04-14-review-07/` tightened the
  remaining host/test seam drift after this pass
- the next cleanup after review-06 was:
  - making host suites compile against the real synthetic-record dispatch
    signature
  - moving the higher-level integration/scenario harnesses onto semantic
    runtime-debug helpers backed by the real `runtime_debug.c` seam
  - adding an explicit action-kind metadata/dispatch coverage guard
