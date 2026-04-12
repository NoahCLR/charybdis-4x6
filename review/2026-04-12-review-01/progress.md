# Implementation Progress

This file tracks the review pass captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-12

Completed in this pass:

- Audited the current `noah` userspace architecture after the handled-key
  reducer, source-manifest, scenario-harness, and multi-scan regression work
  already recorded in
  [2026-04-11-review-02](../2026-04-11-review-02/userspace-architecture-review.md).
- Re-read the authored/runtime boundary, key runtime, action lifecycle,
  ownership modules, pd-mode runtime, RGB runtime, docs, and host verification
  surfaces.
- Wrote a new review focused on:
  - architecture and separation of concerns
  - modularity and extensibility cost
  - abstraction quality and leakage
  - code organization and discoverability
  - state management and flow
  - scalability risks
  - testing and debuggability
- Identified the main remaining architectural pressure points as:
  - handled-key lifecycle semantics still spanning multiple protocol layers
  - hidden cross-module coupling through action dispatch and effect execution
  - pd-mode extensibility still depending on central trait consumers for novel
    behavior
  - distributed runtime state making higher-level debugging and test reset
    setup more manual than necessary

Verification run in this pass:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- full host suite passed
- firmware build passed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Recommended next implementation work:

1. Introduce a richer `resolved_handled_key_t` so handled-key semantics stop
   being re-derived across multiple runtime modules.
2. Collapse the handled-key slot result and transition plan toward one shared
   effect vocabulary.
3. Add a small pd-mode lifecycle policy seam before the next unusual mode
   pushes more central trait branches into the registry.
4. Add one shared runtime snapshot/reset surface for debugging and host tests.
