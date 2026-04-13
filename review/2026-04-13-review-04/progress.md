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

### Implementation pass: explicit slot interaction resolution nesting

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Removed the anonymous direct-field mirror from
  `users/noah/lib/key/runtime/key_runtime_interaction.h` so
  `key_runtime_slot_interaction_t` now exposes only explicit
  `.resolution` plus cached `.policy`.
- Updated the release, feedback, scan, pending-multi-tap, and slot-policy
  reducers to read authored semantics through `interaction.resolution` instead
  of treating cached slot interaction as a flat handled-key struct.
- Updated the host transition, slot, and runtime-debug suites so tests that
  need tweaked cached interaction state rebuild from a modified
  `handled_key_resolution_t` instead of mutating mirrored cached fields in
  place.
- Updated the maintainer runtime doc to describe the real slot storage shape:
  owner/lifecycle/timer/pending-multi-tap plus cached interaction
  `resolution` and `policy`.

Contracts touched in this pass:

- `key_runtime_slot_interaction_t`
- `key_runtime_slot_release_contract(...)`
- runtime consumers of `key_runtime_slot_cached_interaction(...)`
- host cached-interaction setup helpers in the key-runtime suites

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

### Implementation pass: slot-owned branch contract and cached release semantics

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Narrowed
  `users/noah/lib/key/runtime/key_runtime_interaction.h`
  so `key_runtime_slot_interaction_t` no longer stores the full authored
  `handled_key_resolution_t`. It now stores a slot-owned
  `key_runtime_slot_binding_t`, top-level slot semantic fields
  (`hold_strategy`, `layer`, `pd_mode`, `flags`), cached hold policy, and a
  cached release contract.
- Added press-time caching of `key_runtime_slot_release_contract_t` inside the
  slot interaction contract so release reducers execute the cached contract
  instead of reconstructing it from resolution fields at release time.
- Updated release, feedback, scan, pending-multi-tap, and slot-policy
  reducers to consume `interaction.binding`, cached semantic fields, and the
  cached release contract.
- Kept `key_runtime_slot_interaction_to_resolution(...)` as a reconstruction
  helper for compatibility and host setup, then updated the transition/slot
  host suites and runtime-debug assertions to work with the new slot-owned
  branch shape.
- Updated the maintainer/runtime review docs to describe cached interaction as
  slot-owned binding plus cached policy/release semantics rather than a stored
  authored resolution object.

Contracts touched in this pass:

- `key_runtime_slot_binding_t`
- `key_runtime_slot_interaction_t`
- `key_runtime_slot_release_contract_t`
- `key_runtime_slot_release_contract(...)`
- runtime consumers of `key_runtime_slot_cached_interaction(...)`

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- continue finding 1 by deciding whether `handled_key_resolution_t` itself
  should keep carrying derived runtime-facing policy flags/hold semantics or be
  narrowed further toward authored branch selection only
- decide whether the remaining
  `key_runtime_slot_interaction_to_resolution(...)` compatibility helper should
  stay for tests/debugging or be replaced with dedicated host/runtime snapshot
  helpers
- after that, make pd-mode exclusivity explicit in the public state model and
  split the remaining compatibility seams by owning subsystem

### Implementation pass: authored-resolution narrowing and slot interaction API cleanup

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Narrowed `handled_key_resolution_t` in
  `users/noah/lib/key/interaction/handled_key.h` so authored lookup now stores
  keycode, tap-count branch selection, chosen authored step, timing, layer/pd
  metadata, and structural flags instead of cached tap action, tap repeat
  count, hold behavior, and hold strategy.
- Moved the remaining derived tap/hold semantics behind
  `handled_key_resolution_*` accessors in
  `users/noah/lib/key/interaction/handled_key.c` and updated
  `key_runtime_slot_interaction_from_resolution(...)` to build slot-owned
  binding/policy/release state from that slimmer authored contract.
- Added explicit authored branch selection storage to
  `key_runtime_slot_interaction_t` through
  `key_runtime_slot_selection_t`, so the slot contract now caches both the
  authored branch snapshot and the derived slot binding without reverting to a
  full stored authored resolution object.
- Reworked
  `users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c`
  so press-time overrides now mutate the slot-owned interaction contract rather
  than widening `handled_key_resolution_t` again.
- Changed `key_runtime_slot_interaction(...)` in
  `users/noah/lib/key/runtime/key_runtime_state.h` /
  `users/noah/lib/key/runtime/slot/key_runtime_slot.c` to return the slot-owned
  `key_runtime_slot_interaction_t` directly. Tests that want an authored
  reconstruction now have to call
  `key_runtime_slot_interaction_to_resolution(...)` explicitly.
- Updated the key-runtime host harnesses and stubs to either construct the
  slimmer authored resolution shape directly or inspect cached slot binding
  through the slot interaction contract where they really mean "live slot
  semantics".
- Updated `docs/KEY_RUNTIME.md` and the active review so the maintainer docs
  now describe authored resolution as a branch-selection contract and slot
  interaction as the live per-press semantic cache.

Contracts touched in this pass:

- `handled_key_resolution_t`
- `handled_key_resolution_*` accessor family
- `key_runtime_slot_selection_t`
- `key_runtime_slot_interaction_t`
- `key_runtime_slot_interaction(...)`
- `key_runtime_slot_interaction_to_resolution(...)`

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- keep shrinking the remaining test/debug compatibility seam around
  `key_runtime_slot_interaction_to_resolution(...)` so authored reconstruction
  stays explicit instead of becoming a de facto slot API again
- continue Finding 2 by splitting more of the remaining release hotspot into
  small typed helpers now that press-time and slot-time semantics are cleaner
- after that, move to the next architecture review target: making pd-mode
  exclusivity explicit in the public state model and splitting the remaining
  compatibility seams by owning subsystem

### Implementation pass: explicit pd-mode selection state

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Changed
  `users/noah/lib/state/runtime/runtime_shared_state.h`
  so pd-mode runtime storage now carries explicit selected-mode fields for
  `local_active_mode`, `local_locked_mode`, `remote_display_active_mode`, and
  `remote_display_locked_mode`, while the older `*_flags` fields remain as
  compatibility mirrors.
- Changed
  `users/noah/lib/pointing/defs/pd_mode_flags.h`
  so `pd_mode_snapshot_view_t` now exposes `active_mode`, `locked_mode`,
  `active_index`, and `locked_index` instead of the older `first_active_*`
  / `first_locked_*` names.
- Simplified
  `users/noah/lib/pointing/runtime/pd_mode_state.c`
  around the explicit selected-mode storage so activate / deactivate / lock /
  unlock orchestration now operates on one selected active mode and one
  selected lock instead of walking bitmask-shaped local state.
- Updated
  `users/noah/lib/pointing/runtime/pd_mode_snapshot.c`
  to build the public snapshot view from explicit selected-mode storage, with
  compatibility flags derived from those selected modes.
- Moved the main pd-mode consumers off the old "first active" contract:
  `pd_runtime.c`, `pd_mode_lifecycle.c`, `rgb_pd_mode_stage.c`, and
  `pointer_layer_policy.c` now use explicit selected-mode identity.
- Removed the unused `pd_mode_first_local_active_index(...)` /
  `pd_mode_first_display_active_index(...)` registry helpers.
- Updated the pd-mode, pointer-layer-policy, RGB layer render, split-sync, and
  related host stubs to synthesize snapshots with explicit active/locked mode
  identity.
- Updated the active review and maintainer docs so the written architecture now
  describes pd-mode exclusivity as explicit runtime state instead of a
  manifest-order "first active" convention.

Contracts touched in this pass:

- `pd_mode_runtime_shared_state_t`
- `pd_mode_snapshot_view_t`
- `pd_mode_apply_command(...)`
- `pd_mode_snapshot(...)`
- pd-mode consumers in lifecycle/runtime/pointer-policy/RGB

Verification run in this pass:

- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- continue shrinking the pd-mode compatibility mirrors so `active_flags` /
  `locked_flags` become clearly derived transport/debug surfaces instead of
  looking like primary control state
- then return to Finding 2 and split more of the remaining key-runtime release
  hotspot into small typed helpers
- after that, move to the next architecture review target: splitting the
  remaining compatibility seams by owning subsystem

### Implementation pass: derived pd-mode compatibility flags only

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Removed the stored pd-mode compatibility mirrors from
  `users/noah/lib/state/runtime/runtime_shared_state.h` so shared runtime
  state now stores only explicit selected local/display mode identity.
- Simplified `users/noah/lib/pointing/runtime/pd_mode_state.c` so local
  split-sync snapshots derive directly from selected mode identity instead of
  synchronizing stored `*_flags` mirrors alongside the primary state.
- Updated `tests/host/runtime_debug_test.c` so the runtime-debug surface now
  treats explicit pd-mode selection state as the authoritative shared-state
  contract.
- Updated maintainer and review docs so the written architecture now describes
  `active_flags` / `locked_flags` as derived snapshot/transport output instead
  of stored runtime control state.

Contracts touched in this pass:

- `pd_mode_runtime_shared_state_t`
- `pd_mode_local_active_snapshot(...)`
- `pd_mode_local_locked_snapshot(...)`
- `noah_runtime_debug_snapshot_t.core.pd`

Verification run in this pass:

- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- return to Finding 2 and split more of the remaining key-runtime release
  hotspot into small typed helpers
- after that, decide whether the remaining flag-shaped pd-mode snapshot view
  should stay as the long-term split/debug contract or be narrowed further

### Implementation pass: typed tap release contract

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Narrowed
  `users/noah/lib/key/runtime/key_runtime_interaction.h`
  so `key_runtime_slot_release_contract_t` now carries a typed tap contract
  with explicit `DISPATCH_ACTION` / `BUFFER_MULTI_TAP` outcomes instead of a
  loose `tap_action` plus `buffers_multi_tap` pairing.
- Updated
  `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c`
  so active release now executes that typed tap contract through one helper
  instead of re-materializing tap behavior inside the reducer switch.
- Extended
  `tests/host/key_runtime_slot_test.c`
  and
  `tests/host/key_runtime_transition_test.c`
  to assert the new cached tap-contract seam directly for normal tap dispatch,
  multi-tap buffering, and modifier multi-tap buffering with `KC_NO`.
- Updated
  `docs/KEY_RUNTIME.md`
  and the active review so the maintainer-facing description matches the
  narrower release-contract shape.

Contracts touched in this pass:

- `key_runtime_slot_release_tap_contract_t`
- `key_runtime_slot_release_contract_t`
- `key_runtime_slot_release_contract_build(...)`
- `key_runtime_slot_reduce_active_release(...)`

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- keep shrinking the remaining phase-specific branch choreography inside
  `key_runtime_slot_release_active.c`
- then decide whether the next Finding 2 slice should move more branch
  selection into the cached release contract or into small phase-specific
  helpers

### Implementation pass: release phase-contract table

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Replaced the bespoke active-release phase resolver matrix in
  `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c`
  with a compact typed phase-contract table plus one generic interpreter for
  buffered tap, quick tap, pd-mode quick lock, fallback suppression,
  release-hold selection, long-hold takeover, and nonquick tap dispatch.
- Kept the cached release contract and tap contract unchanged at the outer
  seam, but moved the remaining phase selection logic out of dedicated
  per-phase resolver functions and into data-shaped phase policy.
- Updated
  `docs/KEY_RUNTIME.md`
  and the active review so the maintainer-facing description now reflects that
  release routing uses typed phase contracts rather than one bespoke resolver
  per slot phase.

Contracts touched in this pass:

- `key_runtime_slot_release_phase_contract_t`
- `key_runtime_slot_release_resolve_phase_contract(...)`
- `key_runtime_slot_reduce_active_release(...)`

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- decide whether the next Finding 2 slice should move more branch
  selection into the cached release contract or continue shrinking the generic
  phase interpreter
- if the generic interpreter stays, consider pushing long-hold takeover and
  hold-action selection into tiny shared helpers so scan and release keep
  converging on one semantic vocabulary

### Implementation pass: shared release-hold contract

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Narrowed
  `users/noah/lib/key/runtime/key_runtime_interaction.h`
  so `key_runtime_slot_release_contract_t` now carries a typed
  `key_runtime_slot_release_hold_contract_t` for primary release-hold action
  and long-hold takeover selection.
- Updated
  `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c`
  so active release now reads hold-action availability and long-hold takeover
  through that shared cached hold contract instead of directly through loose
  `release_hold_action` / `release_long_hold_action` fields.
- Updated
  `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`
  so pending multi-tap release reuses the same cached release-hold selection
  helper instead of the older `key_runtime_slot_policy_select_release_hold_action(...)`
  path.
- Removed the old slot-policy-specific release-hold selector from
  `users/noah/lib/key/runtime/slot/key_runtime_slot_policy.[ch]`.
- Extended
  `tests/host/key_runtime_slot_test.c`
  to assert the new cached hold-contract seam directly for release-hold and
  long-hold takeover behavior.
- Updated
  `docs/KEY_RUNTIME.md`
  and the active review so the maintainer-facing description now matches the
  shared release-hold contract shape.

Contracts touched in this pass:

- `key_runtime_slot_release_hold_contract_t`
- `key_runtime_slot_release_hold_contract_select_action(...)`
- `key_runtime_slot_release_contract_t.hold`
- pending multi-tap release hold selection

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- decide whether the next Finding 2 slice should push more of the generic
  phase ordering into cached contracts or stop after adding decision tracing
- if release behavior is stable enough structurally now, the next high-value
  move after that is probably Finding 4: split `compat/` into
  feature-owned surfaces

### Implementation pass: cached release phase contracts

Completed in this pass:

- Started with `git status --short` and continued from the active
  `review/2026-04-13-review-04/` architecture review.
- Narrowed
  `users/noah/lib/key/runtime/key_runtime_interaction.h`
  so `key_runtime_slot_release_contract_t` now owns the phase-policy data
  itself through cached per-phase release contracts instead of relying on a
  local static phase table inside the active-release reducer.
- Updated
  `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c`
  so the reducer now selects phase policy from the cached release contract and
  no longer owns a separate static phase-contract table.
- Extended
  `tests/host/key_runtime_slot_test.c`
  to assert the cached phase-contract seam directly for tap-window and
  release-hold-pending behavior.
- Updated
  `docs/KEY_RUNTIME.md`
  and the active review so the maintainer-facing description now treats
  tap/hold/phase release policy as fully slot-owned cached contract state.

Contracts touched in this pass:

- `key_runtime_slot_release_phase_contract_t`
- `key_runtime_slot_release_phase_contracts_t`
- `key_runtime_slot_release_contract_t.phases`
- `key_runtime_slot_reduce_active_release(...)`

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/`

Next steps:

- treat Finding 2 as structurally landed and choose between:
  adding decision tracing for key-runtime release branches, or moving on to
  Finding 4 and splitting `compat/` into feature-owned surfaces
