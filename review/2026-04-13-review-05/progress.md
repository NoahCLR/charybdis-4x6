# Implementation Progress

This file tracks the audit review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

### Follow-up quality audit

Completed in this pass:

- Started with `git status --short` and confirmed the worktree was clean
  before opening a new review.
- Confirmed the active review lineage under `review/` and created the next
  sortable same-day review folder:
  `review/2026-04-13-review-05/`.
- Re-read the newest existing review in
  `review/2026-04-13-review-04/` so this pass would audit the live code
  against the intended architecture and the work already claimed as landed.
- Reviewed the current implementation across:
  - handled-key lookup, cached interaction, release reduction, pending
    multi-tap, and scan/hold policy
  - pd-mode runtime storage, snapshot/state flow, pointer policy, and RGB
    pd-mode consumers
  - compat surfaces for QMK, VIA, pointing, and auto-mouse
  - macro IR compilation/encoding/playback and VIA default seeding
  - host harnesses, runtime-debug/trace surfaces, and maintainer docs
- Wrote a follow-up audit focused on whether the landed refactor actually
  achieved its design goals in the live tree.

Key findings recorded in this review:

- must-fix: pending multi-tap release currently drops the long-hold-only
  release path, so a branch with release-resolved `long_hold` and no primary
  `hold` does not preserve the documented semantics
- should-fix: the cached release phase-contract table is still invariant global
  policy duplicated into every slot rather than a truly slot-owned contract
- should-fix: cached interaction storage still exposes dead `valid` / `view`
  wrapper state that tests and debug code treat as public slot API
- optional cleanup: the RGB layer-render host suite still models pd-mode with
  obsolete composable flag semantics, and one snapshot comment still reflects
  the old "first active" framing

Areas assessed as solid in this pass:

- authored handled-key resolution vs slot-owned interaction is materially
  cleaner than before
- explicit pd-mode selection state is a genuine improvement over the old flag
  model
- compat seams are cleaner and more feature-owned
- macro semantics now have one canonical IR path instead of parallel semantic
  pipelines

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
- all changes are confined to
  `charybdis-4x6/review/2026-04-13-review-05/`

Next steps:

- fix the pending multi-tap long-hold-only release path and add a host
  regression that proves it
- trim the cached release contract back to slot-owned data instead of storing
  invariant phase-policy tables per slot
- remove or justify the cached interaction `valid` / `view` wrapper so tests
  stop treating dead slot storage detail as public contract
- update the RGB render host stubs to use the exclusive pd-mode state model

### Implementation pass: release contract and runtime/test seam cleanup

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-05/` audit.
- Fixed pending multi-tap release resolution in
  `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`
  so release-time long-hold actions now win even when a tap-count branch has
  no primary release-hold action.
- Added a regression in
  `tests/host/key_runtime_slot_test.c`
  that covers the previously missing long-hold-only release path for pending
  multi-tap.
- Removed the invariant release phase-policy table from the cached slot
  contract by shrinking
  `users/noah/lib/key/runtime/key_runtime_interaction.h`
  and moving the phase-policy table back into
  `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c`
  as reducer-local immutable policy.
- Removed the stale cached-interaction wrapper from
  `users/noah/lib/key/runtime/key_runtime_shared_state.h`
  so active slot storage now carries `key_runtime_slot_interaction_t`
  directly, then updated the affected key-runtime host suites to use that
  direct slot-owned contract.
- Updated
  `tests/host/rgb_layer_render_test.c`
  so its pd-mode stubs now model exclusive active/locked mode identity for
  local state and collapse remote transport flags the way the runtime does.
- Updated
  `docs/KEY_RUNTIME.md`
  and the active review notes so the maintainer docs now describe the slimmer
  cached release contract and direct slot interaction storage accurately.

Contracts touched in this pass:

- `key_runtime_slot_pending_multi_tap_handle_release(...)`
- `key_runtime_slot_release_contract_t`
- `active_key_state_t.interaction`
- reducer-local release phase-policy selection in
  `key_runtime_slot_release_active.c`
- RGB pd-mode host snapshot/model stubs

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted key-runtime, RGB, and feature-gate checks passed
- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- all code, doc, test, and review changes are confined to `charybdis-4x6/`

Next steps:

- keep moving higher-level tests toward semantic harness helpers where that
  reduces storage-coupling noise without weakening low-level reducer coverage

### Implementation pass: semantic release matrix coverage

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-05/` implementation log.
- Added a new end-to-end semantic release matrix suite in
  `tests/host/key_runtime_release_matrix_test.c`
  built on the scenario harness instead of raw slot layout mutation.
- Covered cross-path release semantics for active single-key release and final
  pending-multi-tap release across:
  - primary release-hold actions
  - long-only release-hold actions
  - primary-vs-long release selection after `longer_hold_term`
  - threshold hold followed by release-resolved long hold
  - held lifecycle followed by release-resolved long hold
- Added pending-only edge coverage for the two cases that are not directly
  cross-path equivalent:
  - quick release preserving a still-open multi-tap chain
  - no-scan pending hold release synthesizing held register/unregister
- Wired the new suite into
  `tests/host/run_key_runtime_release_matrix_tests.sh`
  and the full host runner so release semantics are part of the default
  verification surface.

Contracts touched in this pass:

- semantic equivalence between active release and pending multi-tap release
  for release-time hold resolution
- pending multi-tap quick-release chain preservation
- pending multi-tap no-scan held-lifecycle fallback
- full-suite host runner coverage for release matrix verification

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- release-matrix host suite passed
- existing key-runtime scenario suite passed
- full host suite passed with the release-matrix runner wired in
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- extend the same semantic-matrix approach to other stateful runtime seams
  where path coverage is decent but cross-product semantic coverage is still
  thin

### Implementation pass: RGB and pd-mode behavioral coverage

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-05/` implementation log.
- Extended
  `tests/host/rgb_layer_render_test.c`
  with higher-level behavioral assertions for:
  - the documented stage order between preview overlays, pd-mode right-half
    overlays, pd-mode LED groups, and key-behavior feedback
  - `MULTI_TAP_PENDING` feedback repainting both halves after lower RGB stages
  - `HOLD_PENDING` feedback repainting both halves after lower RGB stages
- Extended
  `tests/host/pd_mode_handlers_test.c`
  with raw human-facing behavior tests for `VOLUME_MODE`,
  `BRIGHTNESS_MODE`, and `ZOOM_MODE`, including discrete threshold stepping,
  opposite-direction behavior, and direction-change reset semantics.
- Extended
  `tests/host/pd_mode_key_runtime_integration_test.c`
  with authored entry-path coverage for pd modes:
  - authored single-press rows preserving the default momentary pd-mode hold
  - authored hold actions that momentarily activate a pd mode while held
  - authored double-tap lock behavior for a pd-mode key
  - authored second-press hold branching into a different pd mode
- Tightened the pd-mode integration harness so delayed and tapped actions flow
  through `action_dispatch(...)`, which lets the host suite assert the real
  user-visible result of multi-tap lock actions instead of only internal slot
  state.

Contracts touched in this pass:

- RGB stage-order contract described in `docs/RGB_CONFIG.md`
- RGB consumption of `KEY_FEEDBACK_FLAG_MULTI_TAP_PENDING` and
  `KEY_FEEDBACK_FLAG_HOLD_PENDING`
- raw threshold/step semantics for volume, brightness, and zoom pd handlers
- authored pd-mode entry-path equivalence through key-runtime integration

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted RGB and pd-mode behavioral suites passed
- neighboring pd-mode policy and split-sync suites passed
- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- add one full-scene slave-side RGB assertion that combines remote preview,
  remote pd-mode display state, and remote key-feedback state in a single
  rendered frame
- consider a small pd-runtime task-user suite so active-mode pointer dispatch
  is asserted at the same behavioral level as the raw handler tests

### Implementation pass: remote RGB scene and pd-runtime hook coverage

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-05/` implementation log.
- Extended
  `tests/host/rgb_layer_render_test.c`
  with full-scene slave-side assertions that now combine:
  - remote preview-layer sync
  - remote locked/displayed pd-mode state
  - remote key-feedback state
- Added one slave scene where flash-gated hold feedback is present but hidden,
  proving the rendered frame still matches the human-visible expectation:
  preview on the keyboard half, locked pd-mode color on the pointing half, and
  pd-mode LED-group overrides where they overlap.
- Added one slave scene where remote `MULTI_TAP_PENDING` feedback repaints both
  halves after preview and pd-mode overlays, so the documented last-stage
  override is covered on the mirrored split path as well.
- Added a dedicated
  `tests/host/pd_runtime_test.c`
  suite for the top-level pointing hooks in
  `users/noah/lib/pointing/runtime/pd_runtime.c`.
- Covered the hook-level contracts for:
  - `noah_pointing_device_init_user()` enabling auto-mouse and restoring the
    configured default layer
  - `noah_pointing_device_task_user()` returning untouched reports when no
    local mode is active
  - `noah_pointing_device_task_user()` ignoring slave-side mirrored display
    state and dispatching only the current local active mode
  - `noah_layer_state_set_user()` restoring active-mode DPI after sniping
    drops, stripping the pointer layer for arrow mode, and respecting the
    sniping-layer short-circuit
  - `noah_is_mouse_record_user()` delegating the pointer-layer policy contract
- Wired the new runtime suite into
  `tests/host/run_pd_runtime_tests.sh`
  and the default full host runner.

Contracts touched in this pass:

- slave-side RGB stage order across remote preview, remote pd-mode display
  state, pd-mode LED groups, and remote key-feedback overlays
- `noah_pointing_device_init_user()`
- `noah_pointing_device_task_user()`
- `noah_layer_state_set_user()`
- `noah_is_mouse_record_user()`
- full-suite host runner coverage for top-level pd-runtime behavior

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted RGB and pd-runtime suites passed
- neighboring pd-mode and pointer-policy suites passed
- full host suite passed with the new runtime runner wired in
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- no additional must-have behavioral gaps remain from this README-alignment
  pass; add new full-scene tests only when docs introduce another cross-stage
  interaction that is not already pinned semantically
