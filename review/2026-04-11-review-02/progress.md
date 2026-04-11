# Implementation Progress

This file tracks the review pass captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-11

Completed in this pass:

- Audited the current `noah` userspace architecture after the follow-up work
  already captured in [review-01](../2026-04-11-review-01/userspace-architecture-review.md).
- Re-read the current authoring/runtime boundary, hook surface, key runtime,
  pd-mode runtime, RGB runtime, ownership modules, docs, and host verification
  scripts.
- Wrote a new review focused on:
  - separation of concerns
  - extensibility cost
  - abstraction quality
  - state management
  - long-term scalability
  - testing/debuggability
- Identified the main remaining architectural pressure points as:
  - the handled-key runtime still being an implicit FSM
  - the hard two-slot handled-key ceiling
  - pd-mode cross-cutting policy still requiring central consumers for novel behavior
  - duplicated build-source inventories between firmware build wiring and host compile gates

Verification run in this pass:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- host suite passed
- firmware build passed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Recommended next implementation work:

1. Convert the handled-key runtime toward an explicit slot reducer / FSM.
2. Separate key-runtime slot-capacity policy from lifecycle policy.
3. Unify build wiring and compile-gate source manifests.
4. Add one structured scenario harness for key-runtime event traces.

## 2026-04-11 Follow-Up Implementation

Completed in this pass:

- Implemented a canonical userspace source manifest in
  [`users/noah/source_manifest.mk`](../../users/noah/source_manifest.mk).
- Rewired the firmware build to consume that manifest from
  [`users/noah/rules.mk`](../../users/noah/rules.mk).
- Rewired the host compile gate to consume the same manifest through
  [`tests/host/noah_source_manifest.sh`](../../tests/host/noah_source_manifest.sh)
  instead of maintaining a second manual userspace source inventory in
  [`tests/host/run_feature_gate_compile_tests.sh`](../../tests/host/run_feature_gate_compile_tests.sh).
- Added the first structured key-runtime scenario harness:
  - [`tests/host/key_runtime_scenario_harness.h`](../../tests/host/key_runtime_scenario_harness.h)
  - [`tests/host/key_runtime_scenario_harness.c`](../../tests/host/key_runtime_scenario_harness.c)
  - [`tests/host/key_runtime_scenario_test.c`](../../tests/host/key_runtime_scenario_test.c)
  - [`tests/host/run_key_runtime_scenario_tests.sh`](../../tests/host/run_key_runtime_scenario_tests.sh)
- Wired the new scenario suite into the default host regression pass in
  [`tests/host/run_all_host_tests.sh`](../../tests/host/run_all_host_tests.sh).
- Extended the host compile stubs to cover the broader syntax surface now
  exercised by the unified compile gate:
  - [`tests/host/include/qmk_stub.h`](../../tests/host/include/qmk_stub.h)
  - [`tests/host/include/quantum_keycodes.h`](../../tests/host/include/quantum_keycodes.h)
  - [`tests/host/include/send_string.h`](../../tests/host/include/send_string.h)

Why this pass landed first:

- it removes recurring build/compile-gate drift without changing runtime behavior
- it adds a higher-level event-trace regression surface before any handled-key
  reducer/FSM refactor

Verification run in this pass:

- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted key-runtime scenario tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed
