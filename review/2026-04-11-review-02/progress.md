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
