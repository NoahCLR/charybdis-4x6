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
