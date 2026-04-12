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
    lib/action/action_lifecycle.c \
    lib/action/synthetic_record.c \
    lib/split_role.c \
    lib/key/handled_key.c \
    lib/key/key_behavior_lookup.c \
    lib/key/keymap_validation.c \
    lib/key/key_runtime.c \
    lib/key/key_runtime_admission.c \
    lib/key/key_runtime_slot_policy.c \
    lib/key/key_runtime_slot_pending_multi_tap.c \
    lib/key/key_runtime_slot_release_reduce.c \
    lib/key/key_runtime_slot_result.c \
    lib/key/key_runtime_slot_scan_reduce.c \
    lib/key/key_runtime_slot_step.c \
    lib/key/key_runtime_slot.c \
    lib/key/key_runtime_preflight.c \
    lib/key/key_runtime_process.c \
    lib/key/key_runtime_press.c \
    lib/key/key_runtime_release.c \
    lib/key/key_runtime_scan.c \
    lib/key/key_runtime_transition.c \
    lib/key/key_runtime_trace.c \
    lib/key/delayed_action.c \
    lib/key/held_action.c \
    lib/key/held_repeat.c \
    lib/action/action_dispatch.c \
    lib/action/owned_keycode.c \
    lib/action/macro_dispatch.c \
    lib/macro/macro_payload.c \
    lib/macro/via_macro_defaults.c \
    lib/key/multi_tap_engine.c \
    lib/key/key_runtime_feedback.c \
    lib/state/keyboard_mod_state.c \
    lib/state/keyboard_mod_ownership.c \
    lib/state/layer_ownership.c \
    lib/state/runtime_debug.c \
    lib/state/runtime_shared_state.c \
    lib/state/split_runtime_sync.c \
    lib/rgb/rgb_runtime.c \
    lib/rgb/rgb_key_feedback_stage.c \
    lib/rgb/rgb_layer_stage.c \
    lib/rgb/rgb_automouse_stage.c \
    lib/rgb/rgb_pd_mode_stage.c \
    lib/rgb/rgb_preview_stage.c \
    lib/rgb/rgb_config_defaults.c \
    lib/rgb/rgb_validation.c

NOAH_POINTING_SOURCES := \
    lib/pointing/pd_runtime.c \
    lib/pointing/pd_mode_state.c \
    lib/pointing/pd_mode_registry.c \
    lib/pointing/pointer_layer_policy.c \
    lib/pointing/pd_mode_handlers.c

NOAH_AUTOMOUSE_SOURCES := \
    lib/rgb/rgb_automouse.c

NOAH_RGB_KEYMAP_SOURCES := \
    $(KEYMAP_PATH)/rgb_config.c
