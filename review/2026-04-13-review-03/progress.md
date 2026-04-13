# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

Completed in this pass:

- Started with `git status --short` and confirmed the worktree was clean
  before opening a new review.
- Confirmed the active review lineage under `review/` and created the next
  sortable same-day review folder:
  `review/2026-04-13-review-03/`.
- Re-read the newest existing architecture review in
  `review/2026-04-13-review-02/` to avoid repeating already-landed contract
  work.
- Reviewed the live userspace structure and key runtime entrypoints,
  including:
  - `users/noah/noah_runtime.h`, `runtime_init.c`, and `hooks.c`
  - key interaction, key runtime, slot, transition, and feedback modules
  - action dispatch/lifecycle and owned-keycode surfaces
  - pd-mode defs, snapshot, state, lifecycle, policy, and runtime files
  - macro dispatch, payload, and VIA-default seeding modules
  - runtime shared-state, debug, trace, and split-sync surfaces
  - keymap authoring/materialization surfaces
  - host test runners, debug tests, pd-mode tests, and key-runtime scenario
    harnesses
- Wrote a new review focused on the remaining software-architecture bottlenecks
  after the earlier contract cleanup work.

Key findings recorded in this review:

- `noah_action_desc_t` is still a boolean-heavy descriptor rather than a
  durable action-family contract
- pd-mode read-side state is centralized through `pd_mode_snapshot()`, but the
  pd-mode write path and split-sync ownership are still fragmented
- key interaction is resolved centrally, but hold/release/feedback policy is
  still interpreted in several runtime reducers
- hardcoded macros are still runtime-string-interpreted instead of compiled as
  static authored data
- the host suite is strong, but too much integration coverage still depends on
  raw storage layout

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
- all changes are confined to `charybdis-4x6/review/2026-04-13-review-03/`

Next steps:

- if implementation work starts from this review, prioritize the action-family
  contract first because it affects key interaction, validation, lifecycle,
  and future feature growth
- next, centralize pd-mode writes behind a single command/apply seam so mode
  lifecycle, sync, and state mutation stop being split across modules
- after that, compute resolved hold policy once per slot interaction and move
  more integration tests onto semantic builders instead of raw storage setup

### Implementation Pass: Action Contract And Resolved Hold Policy

Completed in this pass:

- Landed Recommendation 1 from the review by upgrading
  `noah_action_desc_t` from overlapping booleans to a tagged action-family
  contract with explicit `kind`, capability bits, and typed layer / pd-mode
  payload.
- Updated the main action-family consumers to use helper queries derived from
  that normalized descriptor instead of reading descriptor booleans directly:
  - `users/noah/lib/action/action_lifecycle.c`
  - `users/noah/lib/key/interaction/key_behavior_lookup.c`
  - `users/noah/lib/key/interaction/keymap_validation.c`
  - `users/noah/lib/key/runtime/key_runtime_preflight.c`
  - tests that lock the action taxonomy in place
- Preserved the direct-action distinction for pd-mode lock keys by keeping
  `desc.pd_mode != 0` as the handled-key admission rule in
  `key_behavior_lookup.c`, while still classifying pd-mode locks as their own
  action family in the descriptor.
- Landed the first implementation slice of Recommendation 3 by introducing
  resolved interaction-policy helpers in `users/noah/lib/key/interaction/handled_key.h`:
  - `handled_key_hold_contract_t`
  - `handled_key_interaction_policy_t`
  - `handled_key_resolve_policy(...)`
- Moved threshold / release / feedback policy onto that shared resolved
  interaction contract in:
  - `users/noah/lib/key/runtime/key_runtime_feedback.c`
  - `users/noah/lib/key/runtime/slot/key_runtime_slot.c`
  - `users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c`
  - `users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.c`
  - `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`
- Kept the layer-release-before-lock rule physically keyed off the source slot
  owner in pending multi-tap scan handling, because that decision still
  depends on the owning key position rather than only on the resolved handled
  interaction view.
- Added host-test coverage for the new taxonomy and the pd-mode-lock handled
  boundary in:
  - `tests/host/action_dispatch_test.c`
  - `tests/host/key_behavior_lookup_test.c`

Contracts touched in this pass:

- action family contract:
  `users/noah/lib/action/action_dispatch.h`
- direct action / lifecycle routing:
  `users/noah/lib/action/action_lifecycle.c`,
  `users/noah/lib/key/runtime/key_runtime_preflight.c`
- handled-key resolved hold policy:
  `users/noah/lib/key/interaction/handled_key.h`
- slot / feedback consumers of the resolved hold policy:
  `users/noah/lib/key/runtime/key_runtime_feedback.c`,
  `users/noah/lib/key/runtime/slot/key_runtime_slot.c`,
  `users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c`,
  `users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.c`,
  `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_owned_keycode_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- code changes stayed inside `charybdis-4x6/`

Next steps:

- Recommendation 2 remains open: add a pd-mode write controller so mutation,
  lifecycle side effects, and split-sync intent stop being distributed across
  `pd_mode_state.c`, `pd_mode_lifecycle.c`, and action entrypoints.
- Recommendation 4 remains open: add macro IR for static authored macros so
  hardcoded macros stop paying runtime parse / reinterpret cost.
- Recommendation 5 remains open: move more integration fixtures onto semantic
  builders so storage layout stops acting like a public test API.

### Implementation Pass: PD-Mode Write Controller

Completed in this pass:

- Landed Recommendation 2 from the review by adding a typed pd-mode write
  contract in `users/noah/lib/pointing/defs/pd_modes.h`:
  - `pd_mode_command_t`
  - `pd_mode_apply_result_t`
  - `pd_mode_apply_command(...)`
- Reworked `users/noah/lib/pointing/runtime/pd_mode_state.c` into the
  centralized pd-mode write controller:
  - local activate / deactivate / lock / unlock orchestration now runs through
    one command dispatcher
  - remote display snapshots now route through the same command/result seam
  - split-sync intent is computed centrally from before/after snapshots
  - public wrappers such as `pd_mode_handle_keycode_press(...)`,
    `pd_mode_handle_keycode_release(...)`, `pd_mode_set_lock_state(...)`, and
    `pd_mode_toggle_lock_state(...)` are now thin adapters around the command
    controller
- Split single-mode lifecycle effects away from cross-mode orchestration in
  `users/noah/lib/pointing/runtime/pd_mode_lifecycle.c` by making the
  lifecycle file own only leaf transitions:
  - `pd_mode_transition_activate(...)`
  - `pd_mode_transition_deactivate(...)`
  - `pd_mode_transition_lock(...)`
  - `pd_mode_transition_unlock(...)`
- Preserved the historical internal convenience API used by host tests through
  `pd_mode_activate(...)`, `pd_mode_deactivate(...)`, `pd_mode_lock(...)`, and
  `pd_mode_unlock(...)`, but changed those helpers to route through the new
  controller instead of performing distributed orchestration themselves.
- Removed the last pd-mode split-sync branch from
  `users/noah/lib/action/action_lifecycle.c`; pd-mode lock taps now rely on
  the public pd-mode wrapper to own sync behavior instead of having action
  lifecycle coordinate it.
- Added command/result host coverage at the new seam in
  `tests/host/pd_mode_test.c`, including:
  - before/after snapshot reporting for key-press commands
  - display-only remote snapshot application
  - handled-without-sync behavior for locked-mode releases
- Updated the action-lifecycle host stub in
  `tests/host/action_lifecycle_test.c` so the stub mirrors the production
  contract where the pd-mode public wrapper owns split-runtime sync.
- Updated the active review document with an implementation-status note so the
  review folder no longer reads as though Recommendations 1 and 2 are still
  entirely unlanded.

Contracts touched in this pass:

- pd-mode write contract:
  `users/noah/lib/pointing/defs/pd_modes.h`
- pd-mode write controller and public pd-mode state wrappers:
  `users/noah/lib/pointing/runtime/pd_mode_state.c`
- pd-mode leaf lifecycle transitions:
  `users/noah/lib/pointing/runtime/pd_mode_internal.h`,
  `users/noah/lib/pointing/runtime/pd_mode_lifecycle.c`
- action lifecycle / pd-mode lock ownership boundary:
  `users/noah/lib/action/action_lifecycle.c`

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- code changes stayed inside `charybdis-4x6/`

Next steps:

- Recommendation 4 remains open: add macro IR for static authored macros so
  hardcoded macros stop paying runtime parse / reinterpret cost.
- Recommendation 5 remains open: move more integration fixtures onto semantic
  builders so storage layout stops acting like a public test API.
- The resolved hold-policy work can still be pushed further upward into more
  semantic test helpers, but the major remaining architectural gap is now in
  macro execution and fixture shape rather than pd-mode write ownership.
