# Implementation Progress

This file tracks the follow-up audit recorded in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Refactor quality audit

Completed in this pass:

- started with `git status --short`
- read the newest prior review folder in `review/2026-04-14-review-13/`
- re-audited the landed key-runtime boundary and orchestration cleanup in the
  current tree
- checked the new boundary claims against the actual compile gate, headers,
  test harnesses, docs, and newest review notes
- recorded the follow-up findings in `review/2026-04-14-review-14/`

Verification run in this audit pass:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- all targeted checks above passed
- the full host suite passed
- the firmware compile passed
- no sibling workspace folders were edited

### Index boundary and host-runner cleanup

Completed in this pass:

- removed `users/noah/lib/key/runtime/key_runtime_index.h` and moved the
  read-side index accessors into
  `users/noah/lib/key/runtime/key_runtime_index_internal.h`
- updated the key-runtime owner modules and allowlisted white-box suites to use
  the explicit internal index header
- tightened `tests/host/run_feature_gate_compile_tests.sh` so the removed
  `key_runtime_index.h` name is rejected everywhere and
  `key_runtime_index_internal.h` is enforced as an internal-only seam
- added one host-only shared source helper in
  `tests/host/noah_source_manifest.sh` and migrated the reviewed runners onto
  it
- updated `docs/KEY_RUNTIME.md` and this review note to reflect the landed
  cleanup while leaving the review-integrity issue open

Verification run in this pass:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- the internal index seam is now name-accurate and compile-gated
- the reviewed public-seam runners now share one host-only source helper
- all targeted checks above passed
- the full host suite passed
- the firmware compile passed

Next steps:

- reconcile `review/2026-04-14-review-13/` so the newest prior review folder
  tells one consistent architecture story
