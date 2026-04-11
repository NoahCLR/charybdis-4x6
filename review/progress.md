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
- Split the keymap header surface into:
  - `users/noah/noah_keymap_ids.h` for shared layer ids, userspace keycodes, and materialized authored data symbols consumed by runtime modules
  - `users/noah/noah_keymap.h` as the authoring surface for keymap-owned translation units
- Removed the `noah_runtime.h` re-export from `users/noah/noah_keymap.h`.
- Switched runtime modules off the authoring header and onto narrower dependencies:
  - `noah_keymap_ids.h` in runtime modules that only need ids or materialized authored data
  - `noah_runtime.h` in runtime test paths that actually call `noah_*` hook helpers
- Confirmed that only authoring translation units and authoring-oriented validation tests still include `noah_keymap.h` directly.
- Added a dedicated VIA compatibility layer for default-macro seeding and command classification:
  - `users/noah/lib/compat/qmk_via_contract.h`
  - `users/noah/lib/compat/qmk_via_contract.c`
- Moved the remaining fork-coupled VIA seeding assumptions out of `users/noah/lib/macro/via_macro_defaults.c`:
  - dynamic macro seed-capacity calculation
  - macro-buffer writes
  - post-init EEPROM validity decision
  - VIA command id classification for RGB invalidation vs macro reseeding
- Reduced `users/noah/lib/macro/via_macro_defaults.c` to userspace policy and scheduling logic over the new compat surface.
- Added focused host coverage for the VIA default-macro path:
  - `tests/host/via_macro_defaults_test.c`
  - `tests/host/run_via_macro_defaults_tests.sh`
- Added host stub headers needed by the new VIA compat path:
  - `tests/host/include/via.h`
  - `tests/host/include/eeprom.h`
  - `tests/host/include/nvm_eeprom_eeconfig_internal.h`
  - `tests/host/include/nvm_eeprom_via_internal.h`
  - updated `tests/host/include/dynamic_keymap.h` with `dynamic_keymap_macro_set_buffer(...)`
- Fixed a real-firmware compile mismatch in the VIA compat layer by aligning `noah_qmk_via_macro_set_buffer(...)` to QMK's non-const `uint8_t *` buffer signature.
- Started the Phase 3 pd-mode trait/plugin refactor by moving pointer-layer and registry policy off hard-coded mode identities and onto manifest-defined traits:
  - added `pd_mode_traits_t` and `PD_MODE_TRAIT_*` flags in `users/noah/lib/pointing/pd_mode_manifest.h`
  - extended `pd_mode_def_t` with manifest-defined `traits`
  - added `pd_mode_has_trait(...)` and `pd_any_active_mode_has_trait(...)` query helpers in `users/noah/lib/pointing/pd_mode_registry.c`
  - rewired `users/noah/lib/pointing/pointer_layer_policy.c` to consume trait queries instead of special-casing `PD_MODE_ARROW` and implicit "all non-arrow modes anchor auto-mouse"
  - rewired dragscroll backend, pinch GUI ownership, and locked auto-mouse toggle policy in `users/noah/lib/pointing/pd_mode_registry.c` to consume manifest traits instead of checking for `DRAGSCROLL` or `PINCH` directly
- Updated pd-mode manifest consumers for the new seven-field row shape:
  - `users/noah/lib/pointing/pd_mode_flags.h`
  - `users/noah/noah_keymap_ids.h`
  - `tests/host/real_profile_validation_test.c`
- Fixed host harness fallout introduced by the trait refactor:
  - added trait-aware stubs to `tests/host/pointer_layer_policy_test.c`
  - fixed a macro-parameter substitution bug in `tests/host/real_profile_validation_test.c` where `.traits` was being preprocessed into an invalid designated field
- Added direct trait coverage in `tests/host/pd_mode_test.c` for manifest traits and active-mode trait queries.
- Added a header-boundary guard to `tests/host/run_feature_gate_compile_tests.sh` so:
  - runtime modules under `users/noah/` cannot silently drift back to `noah_keymap.h`
  - keymap-owned translation units under `keyboards/.../keymaps/noah/` cannot start depending on `noah_runtime.h`
- Documented the intended split directly in:
  - `users/noah/noah_keymap.h`
  - `users/noah/noah_runtime.h`
- Swept `users/noah/lib/` for remaining hard-coded pd-mode identity checks after the trait refactor and confirmed that the remaining named-mode references are manifest/default definitions rather than central policy branches.
- Started Phase 4 of the key-runtime refactor by replacing the raw `active_key` field layout with a slot container in shared state:
  - `users/noah/lib/state/runtime_shared_state.h` now stores `active_slots[KEY_RUNTIME_ACTIVE_SLOT_CAPACITY]`
  - slot capacity is now `2`, so the runtime can keep two handled-key state slots alive concurrently instead of forcing every new key through the primary slot
- Added a dedicated slot helper module in `users/noah/lib/key/key_runtime_slot.c` for:
  - primary slot access
  - first-active slot lookup
  - free-slot lookup
  - slot-active and slot-match queries
  - slot reset and slot tracking
- Narrowed `users/noah/lib/key/key_runtime.c` back to handled-key behavior helpers plus the fallback-hold activation bridge that still depends on `held_action`
- Rewired the main key-runtime flow to consume slot helpers instead of raw shared-state fields:
  - `users/noah/lib/key/key_runtime_press.c`
  - `users/noah/lib/key/key_runtime_preflight.c`
  - `users/noah/lib/key/key_runtime_feedback.c`
  - `users/noah/lib/key/key_runtime_transition.c`
- Made `multi_tap` physical-key-aware by adding `key_pos` ownership in:
  - `users/noah/lib/key/multi_tap_engine.h`
  - `users/noah/lib/key/multi_tap_engine.c`
- Added direct overlap coverage for the two-slot runtime in:
  - `tests/host/key_runtime_transition_test.c`
  - `tests/host/key_runtime_feedback_test.c`
  This now covers:
  - using a free secondary slot instead of eagerly flushing the first key
  - reclaiming/flushing only when both slots are already occupied
  - releasing a secondary-slot key without disturbing the primary slot
  - scanning and feedback for a non-primary active slot
- Updated host/runtime wiring for the new slot module:
  - `tests/host/run_key_runtime_feedback_tests.sh`
  - `tests/host/run_key_runtime_preflight_tests.sh`
  - `tests/host/run_key_runtime_transition_tests.sh`
  - `tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `tests/host/run_feature_gate_compile_tests.sh`
- Fixed real firmware build wiring for the new slot module by adding:
  - `users/noah/lib/key/key_runtime_slot.c` to `users/noah/rules.mk`
- Fixed test harness coupling to the old storage layout in:
  - `tests/host/pd_mode_key_runtime_integration_test.c`
  - `tests/host/key_runtime_preflight_test.c`
- Continued Phase 4 by removing the last single-threaded `multi_tap` bottleneck from the handled-key runtime:
  - `users/noah/lib/state/runtime_shared_state.h` now stores `multi_tap_slots[KEY_RUNTIME_ACTIVE_SLOT_CAPACITY]` instead of one shared `multi_tap`
  - `users/noah/lib/key/key_runtime_slot.c` now owns the slot-to-multi-tap mapping helpers and press-slot selection policy for:
    - reusing a slot's own pending tap chain by `keypos_t`
    - preferring truly free slots first
    - reclaiming an inactive slot with a pending tap chain before flushing an unrelated active slot
  - `users/noah/lib/key/key_runtime_transition.c` now resolves multi-tap state per slot for:
    - repress/advance
    - release-to-pending-multi-tap promotion
    - pending-hold release
    - scan-time hold/long-hold promotion
    - forced reclaim and flush
  - `users/noah/lib/key/key_runtime_feedback.c` now reports pending multi-tap feedback across all runtime slots instead of only slot 0
  - `users/noah/lib/key/key_runtime_preflight.c` now keeps foreign pending multi-tap chains alive for handled-key presses and only force-flushes them on non-handled presses
- Tightened host coverage around the new slot-local multi-tap behavior:
  - `tests/host/key_runtime_transition_test.c` now covers:
    - flushing multiple active multi-tap slots in deterministic slot order
    - starting an independent secondary-slot multi-tap chain while another slot already has one pending
    - reclaiming an inactive pending-multi-tap slot before flushing an unrelated active key slot
  - `tests/host/key_runtime_preflight_test.c` now covers the new preflight interruption contract:
    - handled-key presses do not flush foreign pending multi-tap slots
    - non-handled presses still do
  - `tests/host/key_runtime_feedback_test.c` now verifies the multi-tap pending feedback bit can come from a non-primary slot

Verification completed in this pass:

- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Follow-up wiring completed during verification:

- updated `tests/host/run_rgb_layer_render_tests.sh` to link the new `rgb_validation.c` module so the existing RGB runtime integration test still links against the same runtime shape as firmware builds
- updated the host runners that link key-runtime modules so they also link `key_runtime_trace.c`

Next recommended step:

- continue Phase 4 by collapsing the remaining split between active-slot state and per-slot pending-multi-tap state into a single clearer per-slot FSM surface, so release-time tap deferral and pressed-key hold resolution are modeled as one owned state object instead of parallel arrays.
