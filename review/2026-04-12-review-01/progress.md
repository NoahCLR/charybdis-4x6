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
- Implemented the first handled-key follow-up slice from this review:
  - expanded `handled_key_view_t` into a richer resolved handled-key surface
    with resolved tap, hold, long-hold, hold strategy, timing, layer,
    pd-mode, and capability flags
  - moved handled-key press context and handled-release reduction to consume
    that resolved object instead of re-deriving key semantics from raw
    `key_behavior_view_t`
  - threaded the resolved handled-key release shape through pending multi-tap
    release handling and transition planning
  - updated the affected host harnesses for the richer handled-key contract
- Implemented the second handled-key follow-up slice from this review:
  - added one shared runtime effect vocabulary in
    `users/noah/lib/key/key_runtime_effect.h`
  - changed `key_runtime_slot_result_t` to store executable runtime effects
    directly instead of storing a second slot-result-only effect protocol
  - moved slot-result request expansion into `key_runtime_slot_result.c` so
    reducers now emit the same effect shape that the transition plan executes
  - simplified `key_runtime_transition.c` to queue and execute shared runtime
    effects instead of translating slot-result effects into another enum/union
  - updated slot-runtime host assertions to validate the direct shared effect
    ordering and payloads
- Implemented the feedback/debug follow-up slice from this review:
  - chose cached slot semantic metadata as the remaining feedback/debug seam
    instead of re-running handled-key resolution against mutable slot state
  - added `active_key_state_t.semantic` to cache resolved multi-tap, layer,
    pd-mode, and preview-layer hints when a handled press begins
  - moved feedback preview-layer lookup onto that cached slot metadata while
    keeping a binding-derived fallback for manual host fixtures
  - updated the affected host harnesses, including admission and preflight
    stubs, for the richer slot contract

Verification run in this pass:

- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted handled-key host tests passed
- feature-gate compile tests passed
- full host suite passed
- firmware build passed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Recommended next implementation work:

1. Add a small pd-mode lifecycle policy seam before the next unusual mode
   pushes more central trait branches into the registry.
2. Add one shared runtime snapshot/reset surface for debugging and host tests.
