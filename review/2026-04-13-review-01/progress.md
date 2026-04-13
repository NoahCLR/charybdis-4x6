# Implementation Progress

This file tracks the directory-layout review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

Completed in this pass so far:

- Re-read the current architecture review and lib layout before starting the
  directory reorganization.
- Confirmed the key-runtime docs and code treat authored behavior, tap/hold,
  multi-tap, and threshold resolution as one engine, so the future key-folder
  split should follow `interaction/ownership/runtime` rather than
  `behavior/hold`.
- Opened a new review for the directory-layout migration and recorded the
  intended end-state tree plus the staged move order.

Verification run so far:

- `git status --short`

Follow-up implementation completed in this pass:

- Moved `users/noah/lib/split_role.c` to
  `users/noah/lib/compat/split_role.c` so the split-role override now lives
  with the other QMK/fork contract surfaces.
- Moved `users/noah/lib/action/macro_dispatch.c` and `.h` to
  `users/noah/lib/macro/` so hardcoded macro dispatch sits with macro payload
  and VIA default macro ownership instead of the generic action helpers.
- Moved `users/noah/lib/keymap_materialize.h` to `users/noah/` so the authored
  keymap materialization header no longer pretends to be runtime library code.
- Updated userspace includes, `source_manifest.mk`, and the handful of
  historical review links that referenced the old `keymap_materialize.h` path.

Verification run for Phase 1:

- `git status --short`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Follow-up implementation completed in this pass:

- Split `users/noah/lib/rgb/` into `core/`, `automouse/`, and `stages/`.
- Moved runtime, validation, helper, and authored-config support files into
  `users/noah/lib/rgb/core/`.
- Moved `rgb_automouse.*` and `rgb_automouse_stage.*` into
  `users/noah/lib/rgb/automouse/`.
- Moved the remaining stage renderers into `users/noah/lib/rgb/stages/`.
- Updated userspace includes, `source_manifest.mk`, direct RGB host runners,
  and RGB docs to match the new folder ownership.

Verification run for Phase 2:

- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Follow-up implementation completed in this pass:

- Split `users/noah/lib/pointing/` into `defs/`, `runtime/`, `policy/`, and
  the existing `modes/` surface.
- Moved `pd_mode_manifest.h`, `pd_mode_flags.h`, and `pd_modes.h` into
  `users/noah/lib/pointing/defs/`.
- Moved `pd_runtime.c`, `pd_mode_state.c`, `pd_mode_registry.c`,
  `pd_mode_lifecycle.c`, `pd_mode_internal.h`, and
  `pd_mode_registry_internal.h` into `users/noah/lib/pointing/runtime/`.
- Moved `pointer_layer_policy.c` and `.h` into
  `users/noah/lib/pointing/policy/`.
- Updated userspace includes, `source_manifest.mk`, direct pointing host
  runners, README, and pd-mode docs to match the new folder ownership.

Verification run for Phase 3:

- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Follow-up implementation completed in this pass:

- Split `users/noah/lib/state/` into `ownership/` and `runtime/`.
- Moved `keyboard_mod_ownership.*` and `layer_ownership.*` into
  `users/noah/lib/state/ownership/`.
- Moved `keyboard_mod_state.*`, `runtime_debug.*`, `runtime_shared_state.*`,
  `runtime_trace.*`, and `split_runtime_sync.*` into
  `users/noah/lib/state/runtime/`.
- Updated userspace includes, `source_manifest.mk`, direct state host runners,
  and maintainer docs to match the new folder ownership.

Verification run for Phase 4:

- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_layer_ownership_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Follow-up implementation completed in this pass:

- Split `users/noah/lib/key/` into `interaction/`, `ownership/`, and
  `runtime/`.
- Moved `key_behavior.*`, `key_behavior_lookup.*`, `handled_key.*`,
  `keymap_validation.*`, and `multi_tap_engine.*` into
  `users/noah/lib/key/interaction/`.
- Moved `held_action.*` and `held_repeat.*` into
  `users/noah/lib/key/ownership/`.
- Moved delayed-action and top-level key-runtime files into
  `users/noah/lib/key/runtime/`.
- Moved shared effect headers into `users/noah/lib/key/runtime/effects/`.
- Moved slot reducers, slot result helpers, and slot-local headers into
  `users/noah/lib/key/runtime/slot/`.
- Updated userspace includes, `source_manifest.mk`, direct key-runtime host
  runners, and maintainer docs to match the new folder ownership.

Verification run for Phase 5:

- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- directory-layout migration complete for the current `users/noah/lib/` plan
- keep future work move-only only if another structure pass is opened; do not
  mix these folder moves with logic refactors retroactively
