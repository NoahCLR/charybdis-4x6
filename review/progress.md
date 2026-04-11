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
- Continued Phase 4 again by collapsing the slot storage model itself:
  - `users/noah/lib/state/runtime_shared_state.h` no longer stores parallel `active_slots[]` and `multi_tap_slots[]` arrays
  - each `active_slots[]` entry now owns both:
    - the active press/hold state
    - the deferred `pending_multi_tap` chain for that same physical key position
  - `users/noah/lib/key/key_runtime_slot.c` now resolves slot-to-pending-multi-tap ownership directly from the slot object instead of cross-indexing into a separate array
  - this removes the last storage-level split between “pressed key state” and “released but still owned tap-chain state”, so the next refactor can focus on behavior/FSM cleanup instead of container plumbing
- Fixed one regression introduced during the storage collapse:
  - `key_runtime_slot_track(...)` now preserves an in-flight `pending_multi_tap` sequence when a multi-tap repress updates the slot's active press fields
  - without that preservation, the third-tap hold path in `key_runtime_modifier_hold_integration_test.c` dropped the pending hold state while retaking ownership of the pressed key
- Continued Phase 4 by promoting the slot storage model into a clearer per-slot lifecycle API:
  - `users/noah/lib/key/key_runtime_slot.c` now exposes slot-owned helpers for:
    - slot idle / ownership queries
    - pending multi-tap presence, match, hold-pending, and expiry queries
    - beginning, advancing, resolving, and clearing a slot-owned pending multi-tap chain
  - `users/noah/lib/key/key_runtime_transition.c` now consumes those slot helpers instead of coordinating `pending_multi_tap` as a raw embedded struct
  - `users/noah/lib/key/key_runtime_preflight.c` and `users/noah/lib/key/key_runtime_feedback.c` now also query pending tap-chain state through slot helpers instead of directly calling `multi_tap_*` on raw slot fields
- Narrowed the handled-key helper boundary again:
  - removed the last multi-tap progression helpers from `users/noah/lib/key/handled_key.h` and `users/noah/lib/key/key_runtime.c`
  - `handled_key.*` is back to behavior interpretation only, while slot lifecycle owns the per-slot tap-chain progression rules
- Added direct host coverage for the new slot lifecycle surface:
  - `tests/host/key_runtime_slot_test.c`
  - `tests/host/run_key_runtime_slot_tests.sh`
  - updated `tests/host/run_all_host_tests.sh` to keep the new slot-level suite in the default host regression pass
- Updated smaller host harnesses to match the refactored slot module's real dependency surface:
  - `tests/host/run_key_runtime_preflight_tests.sh` now links `multi_tap_engine.c`
  - `tests/host/key_runtime_preflight_test.c` and `tests/host/key_runtime_feedback_test.c` now provide the `key_behavior_step_lookup(...)` / `key_behavior_has_more_taps(...)` stubs that `key_runtime_slot.c` legitimately depends on
- Continued Phase 4 by moving the remaining active-slot release/scan policy out of `users/noah/lib/key/key_runtime_transition.c` and into slot-local helpers:
  - added slot-local resolution types and helper entry points in `users/noah/lib/key/key_runtime_state.h`
  - added slot-local resolution helpers in `users/noah/lib/key/key_runtime_slot.c` for:
    - active-key release outcomes
    - active-key scan outcomes
    - pending multi-tap scan outcomes
  - reduced `users/noah/lib/key/key_runtime_transition.c` to applying slot-local resolutions into transition-plan effects instead of owning those decision trees directly
- Extended slot-level host coverage for the new helper surface in `tests/host/key_runtime_slot_test.c`, including direct checks for:
  - immediate-hold quick-release tap resolution
  - locked pd-mode tap resolution
  - long-hold scan promotion
  - pending multi-tap layer-lock scan resolution
- Updated `tests/host/key_runtime_feedback_test.c` with the layer-key and layer-lock stubs now required by the slot module's explicit pending-multi-tap scan dependency surface
- Continued Phase 4 by moving more slot-owned mutation work out of `users/noah/lib/key/key_runtime_transition.c` and into `users/noah/lib/key/key_runtime_slot.c`:
  - added slot-level effect-request types in `users/noah/lib/key/key_runtime_state.h` so slot helpers can mutate state and hand transition-planning data back without depending on `key_runtime_transition.h`
  - moved fallback-hold activation request generation into the slot layer
  - moved hold-threshold and long-hold promotion state mutation into slot helpers that now own:
    - owned-hold/repeat cleanup
    - held/repeat binding activation
    - hold feedback-level decisions
  - moved pending multi-tap scan application into the slot layer so pending-chain reset and long-hold assignment are no longer transition-file-local
  - moved pending multi-tap hold-release consumption into the slot layer so the slot helper now:
    - resolves the released action
    - decides delayed-action vs held-lifecycle replay
    - captures delayed-action modifier state
    - clears the slot after consumption
- Reduced `users/noah/lib/key/key_runtime_transition.c` further toward pure effect-plan assembly:
  - transition code now converts slot effect requests into queued plan effects
  - transition code no longer owns the state mutations for fallback hold activation, threshold hold fire, long-hold promotion, pending multi-tap scan application, or pending multi-tap hold-release consumption
- Extended `tests/host/key_runtime_slot_test.c` again with direct coverage for the new mutation helpers, including:
  - repeat-hold threshold activation requests
  - long-hold promotion replacing an existing owned hold
  - pending multi-tap scan application consuming the pending chain
  - pending multi-tap hold-release returning held-lifecycle replay data
- Updated the smaller host harnesses that link `key_runtime_slot.c` so they provide the newly required shared-runtime stubs:
  - `tests/host/key_runtime_slot_test.c`
  - `tests/host/key_runtime_feedback_test.c`
  - `tests/host/key_runtime_preflight_test.c`
  - `tests/host/pd_mode_key_runtime_integration_test.c`
- Continued Phase 4 again by extracting the remaining interrupt/scan/flush bookkeeping from `users/noah/lib/key/key_runtime_transition.c` into slot helpers:
  - added new slot helpers in `users/noah/lib/key/key_runtime_slot.c` for:
    - interrupt-time fallback/layer bookkeeping
    - immediate-hold threshold commit state updates
    - active-slot flush cleanup and effect selection
  - extended slot effect requests in `users/noah/lib/key/key_runtime_state.h` with explicit held-unregister support so transition planning can stay data-driven even when a flush must drop a held action
  - rewired `users/noah/lib/key/key_runtime_transition.c` to route those helper results into the transition plan instead of mutating slot state inline
- Removed the last duplicated fallback-hold activation logic from `users/noah/lib/key/key_runtime.c` by reusing the slot-layer fallback-hold request helper for the direct runtime path
- Extended `tests/host/key_runtime_slot_test.c` with direct coverage for the new bookkeeping helpers, including:
  - interrupt-time layer interruption plus fallback-hold activation
  - immediate-hold threshold commit feedback requests
  - held-action flush unregistration
  - unheld tap flush dispatch
- Continued Phase 4 once more by moving more of the handled-key press path into slot-local helpers:
  - added `key_runtime_slot_begin_press(...)` in `users/noah/lib/key/key_runtime_slot.c` so slot lifecycle now owns:
    - active-slot tracking for a new handled-key press
    - implicit/fallback/pd-lock metadata initialization
    - immediate-hold-on-press registration decisions
  - added `key_runtime_slot_take_pending_multi_tap_flush(...)` so pending tap-chain flush resolution now lives in the slot layer instead of inside `key_runtime_transition.c`
  - rewired `users/noah/lib/key/key_runtime_transition.c` to use those slot helpers for:
    - regular handled-key press setup
    - matching pending-multi-tap repress setup
    - pending multi-tap flush plan assembly
- Extended `tests/host/key_runtime_slot_test.c` again with direct coverage for the new press-path helper surface:
  - pending multi-tap flush fallback replay
  - begin-press metadata plus immediate-hold registration

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
- `sh tests/host/run_key_runtime_slot_tests.sh`
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

Additional verification completed for the continued Phase 4 pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Additional verification completed for the continued Phase 4 mutation-extraction pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Additional verification completed for the continued Phase 4 bookkeeping-extraction pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Additional verification completed for the continued Phase 4 press-path extraction pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Continued Phase 4 again by collapsing more handled-key press orchestration into slot-local planning:

- added `key_runtime_slot_press_plan_t` and `key_runtime_slot_prepare_handled_press(...)` so the slot layer now owns:
  - matching pending-multi-tap repress preparation
  - foreign pending-multi-tap flush resolution
  - conflicting active-slot reclaim/flush selection
  - press-time begin-request generation for the selected slot
- rewired `users/noah/lib/key/key_runtime_transition.c` so `key_runtime_transition_handled_key_press(...)` now queues plan effects from the returned slot-owned press plan instead of encoding those branch trees inline
- pushed handled-key press slot selection up into `users/noah/lib/key/key_runtime_press.c` so the press flow chooses the slot once before transition planning
- updated `tests/host/key_runtime_transition_test.c` for the new handled-key press entrypoint
- extended `tests/host/key_runtime_slot_test.c` with direct coverage for:
  - matching pending-multi-tap press reuse
  - foreign pending-multi-tap flush plus immediate-hold begin

Additional verification completed for the continued Phase 4 press-plan collapse pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Continued Phase 4 once more by collapsing the last press-specific transition glue around the slot-owned press plan:

- rewired `users/noah/lib/key/key_runtime_transition.c` so handled-key press transitions now:
  - accept `keypos_t` instead of the full `keyrecord_t`
  - apply the returned `key_runtime_slot_press_plan_t` through a dedicated transition helper instead of open-coding delayed-action, layer-press, reclaim, and begin-request routing inline
- added small generic transition helpers for:
  - slot-effect-request presence checks
  - pending-multi-tap flush application
  - slot press-plan application
- updated `users/noah/lib/key/key_runtime_press.c` and `tests/host/key_runtime_transition_test.c` for the narrowed handled-key press entrypoint

Additional verification completed for the continued Phase 4 press-transition narrowing pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Continued Phase 4 again by moving more release/scan application out of `users/noah/lib/key/key_runtime_transition.c` and into slot-local helpers:

- added `key_runtime_slot_take_active_release(...)` plus `key_runtime_slot_release_apply_t` so the slot layer now owns:
  - matched active-key release reset timing
  - tap-to-pending-multi-tap promotion
  - release-time dispatch vs pd-mode-lock routing for matched releases
- added `key_runtime_slot_apply_scan_resolution(...)` plus `key_runtime_slot_scan_apply_t` so the slot layer now owns:
  - immediate-hold commit request generation
  - fallback-hold activation request generation during scan
  - hold-threshold / long-hold promotion request selection during scan
- rewired `users/noah/lib/key/key_runtime_transition.c` so active release and active scan transitions now apply those slot-owned helpers instead of branching over raw release/scan resolution structs inline
- extended `tests/host/key_runtime_slot_test.c` with direct coverage for:
  - matched active release promoting to a pending multi-tap chain
  - matched active release producing a locked pd-mode tap plan
  - scan application returning both immediate-hold and long-hold requests
  - scan application returning fallback-hold activation

Additional verification completed for the continued Phase 4 release-scan application pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Continued Phase 4 once more by collapsing the remaining pending-multi-tap scan/expiry routing into slot-owned planning:

- added `key_runtime_slot_pending_multi_tap_plan_t` and `key_runtime_slot_take_pending_multi_tap_plan(...)` so the slot layer now owns:
  - deciding between pending-hold scan application vs expired-chain flush
  - packaging the key position plus layer-release-before-action requirement for pending-chain scan effects
  - packaging expired pending-chain delayed-action replay as a slot-owned flush result
- rewired `users/noah/lib/key/key_runtime_transition.c` so pending multi-tap scan/expiry now routes through:
  - a dedicated pending-multi-tap plan application helper
  - a dedicated pending-multi-tap hold-release application helper
  instead of open-coding those branch trees inline
- extended slot-level coverage in `tests/host/key_runtime_slot_test.c` for:
  - pending-multi-tap scan returning a layer-release-before-lock effect request
  - pending-multi-tap scan returning an expired-chain flush plan
- extended transition coverage in `tests/host/key_runtime_transition_test.c` for:
  - `key_runtime_transition_scan(...)` replaying an expired pending multi-tap chain as a delayed action

Additional verification completed for the continued Phase 4 pending-multi-tap routing pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next recommended step:

- continue Phase 4 with final transition cleanup and boundary tightening, especially:
  - reducing the remaining slot lookup/orchestration wrappers in `key_runtime_transition.c`
  - deciding whether the remaining release-entry discovery can move beside press/release/scan process helpers or is now small enough to leave alone
  - doing a final Phase 4 sweep for dead helpers, naming cleanup, and docs
  so Phase 4 can be called complete before switching to the RGB/runtime decomposition work.
