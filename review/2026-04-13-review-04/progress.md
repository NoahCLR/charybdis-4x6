# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

### Review pass

Completed in this pass:

- Started with `git status --short` and confirmed the worktree was clean
  before opening a new review.
- Confirmed the active review lineage under `review/` and created the next
  sortable same-day review folder:
  `review/2026-04-13-review-04/`.
- Re-read the newest existing architecture review in
  `review/2026-04-13-review-03/` so this pass would evaluate the live codebase
  after the earlier contract cleanup work rather than duplicating its already
  landed recommendations.
- Reviewed the current architecture and docs across:
  - userspace entry points, hook wiring, source manifests, and runtime init
  - handled-key lookup, key-behavior lookup, slot reducers, transition plans,
    feedback, and runtime shared state
  - action dispatch/lifecycle and compat boundaries
  - pd-mode defs, snapshot/state/lifecycle/runtime, pointer policy, and
    pd-mode RGB surfaces
  - macro dispatch, payload parsing/IR, VIA default seeding, and VIA playback
    compatibility
  - runtime debug/trace surfaces and representative host harnesses/tests
- Wrote a new review focused on the next architectural constraints in the live
  userspace.

Key findings recorded in this review:

- `handled_key_view_t` still conflates authored lookup, slot-owned runtime
  interaction, and feedback-policy input
- release behavior is still resolved by a monolithic feature matrix instead of
  a narrower release contract
- pd-mode state still uses composable-looking bitmasks even though the runtime
  enforces one effective active mode and one effective lock
- `compat/` still groups unrelated fork dependencies under overly broad
  surfaces
- macro semantics still cross partially separate repo IR and VIA bytecode
  models
- the host suite is strong, but too much higher-level coverage still depends on
  raw storage and internal state layout

Verification run in this pass:

- `git status --short`

Checks intentionally skipped in this pass:

- host test runners
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Why they were skipped:

- this pass only adds internal review documents under `review/`
- no runtime code, source manifests, authored keymap data, tests, or build
  wiring changed

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to
  `charybdis-4x6/review/2026-04-13-review-04/`

### Implementation pass: slot-owned interaction contract

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Added
  `users/noah/lib/key/runtime/key_runtime_interaction.h`
  with the new slot-owned `key_runtime_slot_interaction_t` contract plus
  default/conversion helpers.
- Moved active slot storage in
  `users/noah/lib/key/runtime/key_runtime_shared_state.h`
  from raw `handled_key_view_t` to cached `key_runtime_slot_interaction_t`.
- Added `key_runtime_slot_cached_interaction(...)` as the key-runtime-local
  read surface while keeping `key_runtime_slot_interaction(...)` as a
  compatibility wrapper for callers that still need a reconstructed
  `handled_key_view_t`.
- Updated slot press, scan, feedback, pending multi-tap, release, and slot
  policy reducers to consume cached slot interaction state and the stored
  `handled_key_interaction_policy_t` instead of repeatedly re-deriving policy
  from `handled_key_view_t`.
- Updated host fixtures to construct valid cached slot interactions rather than
  assigning raw handled-key views directly into active-slot storage.

Contracts touched in this pass:

- `key_runtime_slot_interaction_t`
- `key_runtime_slot_cached_interaction(...)`
- `key_runtime_slot_track(...)`
- the release/feedback/scan/pending-multi-tap consumers of cached slot policy

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- continue finding 1 by splitting authored handled-key resolution from the
  remaining fields still mirrored into `key_runtime_slot_interaction_t`
- next, extract a typed release contract so new hold/release behaviors stop
  inflating `key_runtime_slot_release_active.c`
- after that, make pd-mode exclusivity explicit in the public state model and
  split the remaining compatibility seams by owning subsystem
