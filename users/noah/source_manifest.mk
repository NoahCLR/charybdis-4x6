# Canonical source inventory for the noah userspace.
#
# Keep userspace runtime sources here so the firmware build and host compile
# gates consume one shared manifest instead of mirroring file lists manually.

NOAH_COMMON_SOURCES := \
    runtime_init.c \
    hooks.c \
    lib/compat/qmk_contract.c \
    lib/compat/qmk_mod_contract.c \
    lib/compat/qmk_via_contract.c \
    lib/compat/qmk_via_storage_regions.c \
    lib/compat/qmk_via_sync_metadata.c \
    lib/compat/qmk_via_sync_protocol.c \
    lib/compat/qmk_via_sync_state.c \
    lib/compat/qmk_via_split_sync.c \
    lib/action/action_kind.c \
    lib/action/action_kind_dispatch.c \
    lib/action/action_lifecycle.c \
    lib/action/synthetic_record.c \
    lib/compat/split_role.c \
    lib/key/behavior/handled_key_defaults.c \
    lib/key/behavior/handled_key_resolution_accessors.c \
    lib/key/behavior/handled_key_lookup.c \
    lib/key/behavior/handled_key_transparency.c \
    lib/key/behavior/handled_key_materialize.c \
    lib/key/behavior/key_behavior_lookup.c \
    lib/key/behavior/keymap_validation.c \
    lib/key/runtime/api.c \
    lib/key/runtime/preflight.c \
    lib/key/runtime/process.c \
    lib/key/runtime/slot/origin_registry.c \
    lib/key/runtime/press.c \
    lib/key/runtime/release.c \
    lib/key/runtime/deferred_release.c \
    lib/key/runtime/scan.c \
    lib/key/runtime/transition.c \
    lib/key/runtime/trace.c \
    lib/key/runtime/delayed_action.c \
    lib/compat/qmk_combo_origin.c \
    lib/key/ownership/held_action.c \
    lib/key/ownership/held_repeat.c \
    lib/action/action_dispatch.c \
    lib/action/owned_keycode.c \
    lib/macro/macro_dispatch.c \
    lib/macro/macro_slot_provider.c \
    lib/macro/macro_payload.c \
    lib/macro/macro_payload_decode_qmk.c \
    lib/macro/macro_payload_keycodes.c \
    lib/macro/macro_payload_parse.c \
    lib/macro/macro_payload_run.c \
    lib/macro/macro_payload_encode.c \
    lib/macro/via_macro_provider.c \
    lib/macro/via_macro_defaults.c \
    lib/key/runtime/feedback.c \
    lib/state/modifiers/keyboard_mod_state.c \
    lib/state/modifiers/keyboard_mod_policy.c \
    lib/state/diagnostics/runtime_diag.c \
    lib/state/ownership/keyboard_mod_ownership.c \
    lib/state/ownership/layer_ownership.c \
    lib/key/runtime/debug.c \
    lib/state/diagnostics/runtime_trace.c \
    lib/state/shared/runtime_shared_state.c \
    lib/split/runtime_sync_dirty.c \
    lib/split/runtime_sync.c \
    lib/key/runtime/reducer/runtime.c \
    lib/key/runtime/planning/effect_plan.c \
    lib/key/runtime/reducer/state_query.c \
    lib/key/runtime/reducer/ownership_state.c \
    lib/key/runtime/queue/pending_release_queue.c \
    lib/key/runtime/projection/feedback_projection.c \
    lib/key/runtime/projection/pd_projection.c \
    lib/key/runtime/projection/projection.c \
    lib/key/runtime/planning/release_planner.c \
    lib/key/runtime/planning/scan_planner.c \
    lib/key/runtime/planning/tap_series_flush.c \
    lib/key/runtime/trace/core_trace.c \
    lib/rgb/core/rgb_runtime.c \
    lib/rgb/stages/rgb_combo_feedback_stage.c \
    lib/rgb/stages/rgb_key_feedback_stage.c \
    lib/rgb/stages/rgb_layer_stage.c \
    lib/rgb/automouse/rgb_automouse_stage.c \
    lib/rgb/stages/rgb_pd_mode_stage.c \
    lib/rgb/stages/rgb_preview_stage.c \
    lib/rgb/core/rgb_config_defaults.c \
    lib/rgb/core/rgb_validation.c

NOAH_POINTING_SOURCES := \
    lib/pointing/runtime/pd_runtime.c \
    lib/pointing/runtime/pd_mode_snapshot.c \
    lib/pointing/runtime/pd_mode_key_runtime_bridge.c \
    lib/pointing/runtime/pd_mode_state.c \
    lib/pointing/runtime/pd_mode_registry.c \
    lib/pointing/runtime/pd_mode_lifecycle.c \
    lib/pointing/policy/pointer_layer_policy.c \
    lib/pointing/modes/pd_mode_dragscroll.c \
    lib/pointing/modes/pd_mode_pinch.c \
    lib/pointing/modes/pd_mode_volume.c \
    lib/pointing/modes/pd_mode_brightness.c \
    lib/pointing/modes/pd_mode_zoom.c \
    lib/pointing/modes/pd_mode_arrow.c

NOAH_AUTOMOUSE_SOURCES := \
    lib/rgb/automouse/rgb_automouse.c

NOAH_RGB_KEYMAP_SOURCES := \
    $(KEYMAP_PATH)/rgb_config.c
