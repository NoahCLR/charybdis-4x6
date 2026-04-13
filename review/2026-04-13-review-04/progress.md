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

### Implementation pass: active-release contract extraction

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Added `key_runtime_slot_release_contract_t` plus release-contract helpers in
  `users/noah/lib/key/runtime/key_runtime_interaction.h` so authored
  release-time semantics are represented as an explicit typed contract instead
  of being reinterpreted inline inside the reducer.
- Updated
  `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c`
  to build and execute that contract for quick taps, immediate-hold quick
  release, fallback suppression, release-hold selection, pd-mode quick lock,
  and multi-tap buffering.
- Narrowed the active-release reducer surface by removing the now-unused raw
  handled-key parameter from `key_runtime_slot_reduce_active_release(...)`.

Contracts touched in this pass:

- `key_runtime_slot_release_contract_t`
- `key_runtime_slot_release_contract(...)`
- `key_runtime_slot_release_contract_select_hold_action(...)`
- `key_runtime_slot_reduce_active_release(...)`

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

### Implementation pass: explicit handled-key resolution type

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Renamed the authored handled-key lookup struct in
  `users/noah/lib/key/interaction/handled_key.h`
  to `handled_key_resolution_t` and kept `handled_key_view_t` as a
  compatibility typedef while call sites migrate.
- Updated handled-key lookup and policy helpers to use the explicit resolution
  type as the authored lookup surface.
- Reshaped
  `users/noah/lib/key/runtime/key_runtime_interaction.h`
  so `key_runtime_slot_interaction_t` carries an explicit
  `.resolution` object plus cached policy, rather than only mirroring the
  authored fields as unnamed runtime-owned state.
- Moved process-record wiring in
  `users/noah/lib/key/runtime/key_runtime_process.c`
  to name the authored lookup result as `handled_key_resolution_t`.

Contracts touched in this pass:

- `handled_key_resolution_t`
- `handled_key_lookup(...)`
- `handled_key_lookup_tap_count(...)`
- `key_runtime_slot_interaction_t.resolution`

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

### Implementation pass: runtime and host seam migration

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Migrated key-runtime orchestration headers and reducers to use
  `handled_key_resolution_t` directly for authored lookup surfaces:
  `key_runtime_process.h`, `key_runtime_transition.h`,
  `key_runtime_state.h`, `key_runtime_slot_step.h`,
  `key_runtime_slot_press_reduce.h`, and
  `key_runtime_slot_release_reduce.h`.
- Updated the corresponding runtime implementations to pass authored lookup
  results through process, transition, press, release, preflight, and
  multi-tap advancement as `handled_key_resolution_t`.
- Migrated the host integration harness and the remaining key-runtime host
  suites to use `handled_key_resolution_t` directly instead of relying on the
  `handled_key_view_t` compatibility typedef.

Contracts touched in this pass:

- `key_runtime_process_handled_key_press(...)`
- `key_runtime_process_handled_key_release(...)`
- `key_runtime_transition_handled_key_press(...)`
- `key_runtime_transition_handled_key_release(...)`
- `key_runtime_slot_reduce_handled_press(...)`
- `key_runtime_slot_reduce_handled_release(...)`
- `key_runtime_slot_interaction(...)`
- host integration harness handled-key entry points

Verification run in this pass:

- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

### Implementation pass: handled-key compatibility cleanup

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Removed the last public `handled_key_view_t` compatibility layer from
  `users/noah/lib/key/interaction/handled_key.h` and
  `users/noah/lib/key/interaction/handled_key.c`.
- Renamed the handled-key authored-resolution accessors to the explicit
  `handled_key_resolution_*` surface so call sites no longer depend on
  view-era naming for authored lookup results.
- Removed the last slot interaction compatibility wrappers in
  `users/noah/lib/key/runtime/key_runtime_interaction.h`, leaving
  `key_runtime_slot_interaction_from_resolution(...)` and
  `key_runtime_slot_interaction_to_resolution(...)` as the explicit
  conversion seam between authored lookup output and slot-owned runtime state.
- Updated the remaining key-runtime reducers and host suites to use the new
  authored-resolution accessor names directly.

Contracts touched in this pass:

- `handled_key_resolution_*` accessor family
- `key_runtime_slot_interaction_from_resolution(...)`
- `key_runtime_slot_interaction_to_resolution(...)`
- the remaining key-runtime and host-test authored-resolution call sites

Verification run in this pass:

- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- continue finding 1 by deciding whether the direct-field compatibility in
  `key_runtime_slot_interaction_t` should remain for pragmatism or be narrowed
  further behind policy/resolution accessors
- if release-contract work continues, move contract construction earlier so the
  slot interaction can cache release semantics at press/tap-branch resolution
- after that, make pd-mode exclusivity explicit in the public state model and
  split the remaining compatibility seams by owning subsystem
