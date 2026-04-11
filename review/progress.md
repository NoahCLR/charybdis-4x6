# Implementation Progress

This file tracks concrete follow-up work from
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-11

Completed in this pass:

- Added duplicate `key_behaviors[]` keycode validation in `users/noah/lib/key/key_behavior_lookup.c`.
- Added a new authored RGB validation module in `users/noah/lib/rgb/rgb_validation.c` and wired it into RGB runtime post-init.
- Added validation coverage for:
  - duplicate or missing `pd_mode_colors[]` rows
  - unknown `pd_mode_colors[]` mode ids
  - invalid `layer_led_groups[]` layer ids
  - invalid LED indices in layer and pd-mode LED-group tables
  - unknown `pd_mode_led_groups[]` mode ids
- Added host tests:
  - `tests/host/key_behavior_validation_test.c`
  - `tests/host/rgb_validation_test.c`
- Added optional key-runtime tracing in `users/noah/lib/key/key_runtime_trace.c`.
- Wired trace hooks through:
  - `process_record_user()` flow
  - preflight interruption and multi-tap flush points
  - press/release/scan transition plans
  - per-effect execution inside the transition executor
- Added a traced-build compile gate in `tests/host/run_feature_gate_compile_tests.sh`.
- Materialized combo output metadata in `users/noah/lib/keymap_materialize.h` so authored combo outputs are available to shared validation code without depending on QMK combo internals.
- Extended `users/noah/lib/key/keymap_validation.c` with profile-level checks for:
  - unreachable `key_behaviors[]` rows that are not referenced by `keymaps[][]` or combo outputs
  - invalid combo outputs that use raw QMK layer actions and would bypass userspace layer ownership
- Added host coverage for the new keymap-level validation path:
  - `tests/host/keymap_validation_test.c`
  - `tests/host/run_keymap_validation_tests.sh`
- Added a real-profile host harness that compiles the authored keymap and RGB config translation units directly:
  - `tests/host/include/noah_real_profile_keyboard.h`
  - `tests/host/real_profile_validation_test.c`
  - `tests/host/run_real_profile_validation_tests.sh`
- Used the real-profile harness to remove three dead `key_behaviors[]` rows from `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`:
  - `KC_EQL`
  - `KC_GRV`
  - `ARROW_MODE`
  Those rows were no longer referenced by `keymaps[][]` or active combo outputs.
- Added a compatibility layer under `users/noah/lib/compat/` to centralize fork-specific QMK contracts:
  - `qmk_contract.h`
  - `qmk_contract.c`
  - `qmk_mod_contract.c`
- Moved the copied VIA dynamic macro playback contract out of `users/noah/lib/action/action_lifecycle.c` and into `users/noah/lib/compat/qmk_contract.c`.
- Moved the `register_mods()` / `unregister_mods()` symbol override contract out of `users/noah/lib/state/keyboard_mod_ownership.c` and into `users/noah/lib/compat/qmk_mod_contract.c`.
- Switched fork-specific auto-mouse API usage to the compatibility surface in:
  - `users/noah/lib/pointing/pd_runtime.c`
  - `users/noah/lib/pointing/pd_mode_registry.c`
  - `users/noah/lib/pointing/pointer_layer_policy.c`
  - `users/noah/lib/state/split_runtime_sync.c`
  - `users/noah/lib/rgb/rgb_runtime.c`
  - `users/noah/lib/rgb/rgb_automouse.c`

Verification completed in this pass:

- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`

Follow-up wiring completed during verification:

- updated `tests/host/run_rgb_layer_render_tests.sh` to link the new `rgb_validation.c` module so the existing RGB runtime integration test still links against the same runtime shape as firmware builds
- updated the host runners that link key-runtime modules so they also link `key_runtime_trace.c`

Next recommended step:

- decide whether the next architectural slice should be:
  - finish Phase 1 by moving remaining fork-coupled VIA seeding assumptions behind the compatibility boundary
  - start Phase 2 by splitting `users/noah/noah_keymap.h` into authoring and runtime-integration headers
