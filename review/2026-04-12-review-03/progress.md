# Implementation Progress

This file tracks the review pass captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-12

Completed in this pass:

- Audited the current `noah` userspace after the follow-up work already
  recorded in
  [2026-04-12-review-02](../2026-04-12-review-02/userspace-architecture-review.md).
- Re-read the authored/runtime split, handled-key runtime flow, ownership
  modules, pd-mode manifest/registry/state/handlers, RGB staging, build
  manifest wiring, compile gates, and maintainer docs.
- Wrote a new review focused on long-term software structure for a fixed board
  rather than generic firmware advice.
- Confirmed that the current architecture should stay board-specific and
  data-driven rather than moving toward a plugin framework or hardware-generic
  abstraction layer.
- Identified the main current architecture risks as:
  - handled-key public contracts still spanning several adjacent reduction and
    effect surfaces
  - pd-mode state conflating authoritative local state with mirrored split-sync
    display state
  - `pd_mode_handlers.c` continuing to accumulate per-mode state in one file
  - missing saturation/debug coverage around effect-plan overflow and
    cross-subsystem live tracing

Verification run in this pass:

- `git status --short`

Verification intentionally not run in this pass:

- no host tests
- no firmware compile

Reason:

- this pass only added review documentation under `review/`
- no runtime, keymap, compat, or build-surface source files changed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Follow-up implementation completed in this pass:

- Split pd-mode shared state into explicit local-authority and mirrored
  display fields in `runtime_shared_state`.
- Replaced ambiguous pd-mode state queries with explicit local/display query
  names and updated pd-mode policy, split sync, pointer-layer policy, key
  runtime, and RGB consumers to use the correct surface.
- Kept split-runtime packets authoritative by syncing local pd-mode state from
  the master half while applying remote snapshots only to mirrored display
  state on the slave half.
- Extended host coverage so pd-mode tests assert local vs display semantics,
  RGB rendering uses mirrored slave-half pd-mode state, automouse rendering
  clamps on mirrored locked display state, and runtime debug snapshots cover
  both local and mirrored pd-mode storage.

Verification run for the follow-up implementation:

- `git status --short`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Follow-up implementation completed after the pd-mode split:

- Moved handled-key resolution into `users/noah/lib/key/handled_key.c` and
  wired that source into `source_manifest.mk` plus the host runners that build
  the real key-runtime userspace surface.
- Made `handled_key_view_t` a resolved-only public contract by removing raw
  authored behavior exposure and the `resolved` flag from the public type.
- Updated key-runtime callers to consume explicit handled-key flags/accessors
  instead of reading authored `behavior` fields through the handled-key view.
- Updated key-runtime host tests and integration runners to build against the
  resolved handled-key contract, including pd-mode release cases that now
  resolve after authored pd-mode mappings are installed.
- Deliberately left the remaining handled-key effect vocabulary cleanup for a
  later pass so this change stayed focused on the contract boundary itself.

Verification run for the handled-key follow-up:

- `git status --short`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Follow-up implementation completed after the handled-key contract cleanup:

- Introduced a shared handled-key effect-queue vocabulary in
  `key_runtime_effect_queue.h` so slot results and transition plans now expose
  the same `items/count/overflowed` fields.
- Updated the slot-result and transition-plan implementations plus trace output
  to use the shared queue vocabulary instead of separate `effects` storage
  names.
- Added first-overflow logging to `key_runtime_slot_result.c` so slot-result
  saturation now reports the dropped effect the same way transition-plan
  saturation already did.
- Extended the slot and transition host suites to intentionally fill both queue
  capacities and assert the overflow flag plus first-overflow logging path.
- Enabled `CONSOLE_ENABLE` in the dedicated slot/transition host runners so the
  overflow logging contract is covered in normal host verification.

Verification run for the effect-queue follow-up:

- `git status --short`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Recommended next implementation work:

Follow-up implementation completed after the handled-key effect-queue cleanup:

- Split the former `pd_mode_handlers.c` monolith into per-mode translation
  units: `pd_mode_volume.c`, `pd_mode_brightness.c`, `pd_mode_zoom.c`, and
  `pd_mode_arrow.c`.
- Added `pd_mode_handler_common.h` for the shared vertical-axis helper surface
  while keeping arrow-mode-specific state, key interception, and modifier
  policy isolated inside `pd_mode_arrow.c`.
- Updated the pointing source manifest and the dedicated pd-mode handler host
  runner so the build/test surface matches the new per-mode ownership layout.
- Preserved the existing manifest-driven pd-mode registry API; this pass only
  changed implementation ownership, not pd-mode identity or public contracts.

Verification run for the pd-mode per-file split:

- `git status --short`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Recommended next implementation work:

1. Add a small shared runtime trace sink for cross-subsystem debugging so key
   runtime, pd mode, ownership, and split-sync events can be inspected through
   one vocabulary.
2. If another handled-key lifecycle feature lands, split
   `key_runtime_slot_step.c` and `key_runtime_slot_release_reduce.c` by
   ownership seam instead of layering more local helpers into either file.
3. When the next meaningful pd-mode lifecycle or policy feature lands, split
   `pd_mode_registry.c` by ownership seam instead of letting manifest
   materialization, lifecycle hooks, and state transitions keep growing in one
   file.
