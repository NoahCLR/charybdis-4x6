# Implementation Progress

This file tracks the follow-up quality review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Follow-up audit after review-02 implementation

Completed in this pass:

- started with `git status --short`
- re-read the newest existing review folder,
  `review/2026-04-14-review-02/`, before auditing the current code
- audited the landed handled-key cleanup directly
- audited the landed macro-provider default-seeding cleanup directly
- checked host-test seams and review-note accuracy
- wrote a new review folder for this follow-up audit

Key findings recorded in this review:

- no new must-fix correctness regressions found
- should-fix: `macro_slot_provider_encode_write(...)` still reparses raw payload
  text instead of serializing provider-owned cached IR
- should-fix: `key_runtime_slot_interaction_from_resolution(...)` and
  `key_runtime_slot_binding_from_resolution(...)` are now test-only authored
  bypass helpers, but they still live in the public runtime header
- optional cleanup: `review/2026-04-14-review-02/progress.md` slightly
  overstates completion

Areas assessed as solid in this pass:

- the transparent multi-tap correctness fix is landed and covered
- handled-key public surface is cleaner after removing the old
  `*_at_position(...)` helpers
- duplicate runtime hold-policy state is gone
- pd-mode identity sync and runtime debug registry exposure remain solid

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
  `charybdis-4x6/review/2026-04-14-review-03/`

Next steps:

- decide whether the macro provider encode path should become truly IR-backed
  or be narrowed/documented as payload-backed
- internalize or remove the authored-to-interaction constructors from the
  public runtime header
- once those are addressed, update the latest review notes so the newest review
  folder remains the primary source of truth

### Audit follow-up implementation

Completed in this pass:

- changed `macro_slot_provider_encode_write(...)` to serialize cached provider
  IR with `macro_payload_encode_ir_write(...)`
- changed the VIA defaults provider to compile authored payload text into real
  IR during `load_ir(...)`, so validation and seeding now share one artifact
- removed the public runtime authored-bypass helpers
  `key_runtime_slot_binding_from_resolution(...)` and
  `key_runtime_slot_interaction_from_resolution(...)`
- added one shared host helper that turns authored
  `handled_key_resolution_t` into runtime interaction through the host
  handled-key materialization seam
- cut the remaining runtime host suites over to the materialized interaction
  seam
- added macro host assertions that encode/write uses cached IR serialization,
  skips payload lookup, and reloads after cache invalidation
- added a corrective note to `review/2026-04-14-review-02/progress.md` so the
  older implementation entry no longer overstates completion

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- all targeted host suites passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- source changes are confined to `users/noah/`, `tests/host/`, and review notes

Next steps:

- none
