// ────────────────────────────────────────────────────────────────────────────
// Action Kind Metadata
// ────────────────────────────────────────────────────────────────────────────
//
// Shared action-kind metadata for authored validation, handled-key policy,
// and action descriptor classification.
// ────────────────────────────────────────────────────────────────────────────

#include "action_kind_internal.h"

#include "noah_keymap_ids.h"

static const noah_action_kind_def_t noah_action_kind_defs[NOAH_ACTION_KIND_COUNT] = {
    [NOAH_ACTION_KIND_LITERAL] =
        {
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
        },
    [NOAH_ACTION_KIND_LAYER_LOCK] =
        {
            .caps =
                NOAH_ACTION_CAP_PRESS_ONLY |
                NOAH_ACTION_CAP_LAYER_AFFECTING |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED |
                NOAH_ACTION_CAP_CONSUMES_DIRECT_PRESS,
        },
    [NOAH_ACTION_KIND_LAYER_HOLD] =
        {
            .caps =
                NOAH_ACTION_CAP_REQUIRES_KEY_OWNER |
                NOAH_ACTION_CAP_LAYER_AFFECTING |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED,
            .preview_layer_uses_desc_layer = true,
        },
    [NOAH_ACTION_KIND_LAYER_TAP] =
        {
            .caps =
                NOAH_ACTION_CAP_LAYER_AFFECTING |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED,
        },
    [NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION] =
        {
            .caps = NOAH_ACTION_CAP_LAYER_AFFECTING,
        },
    [NOAH_ACTION_KIND_MACRO] =
        {
            .caps =
                NOAH_ACTION_CAP_PRESS_ONLY |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
        },
    [NOAH_ACTION_KIND_QMK_BEHAVIOR] =
        {
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
        },
    [NOAH_ACTION_KIND_KEYMAP_CUSTOM] =
        {
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
        },
    [NOAH_ACTION_KIND_PD_MODE_HOLD] =
        {
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_PD_MODE_AFFECTING |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
        },
    [NOAH_ACTION_KIND_PD_MODE_LOCK] =
        {
            .caps =
                NOAH_ACTION_CAP_PRESS_ONLY |
                NOAH_ACTION_CAP_PD_MODE_AFFECTING |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED |
                NOAH_ACTION_CAP_CONSUMES_DIRECT_PRESS,
        },
};

const noah_action_kind_def_t *noah_action_kind_def(noah_action_kind_t kind) {
    if (kind >= NOAH_ACTION_KIND_COUNT) {
        kind = NOAH_ACTION_KIND_LITERAL;
    }

    return &noah_action_kind_defs[kind];
}

static noah_action_desc_t noah_action_desc_build(noah_action_kind_t kind, uint16_t action, uint8_t layer, pd_mode_mask_t pd_mode) {
    const noah_action_kind_def_t *def = noah_action_kind_def(kind);

    return (noah_action_desc_t){
        .kind    = kind,
        .action  = action,
        .caps    = def ? def->caps : 0,
        .layer   = layer,
        .pd_mode = pd_mode,
    };
}

noah_action_desc_t noah_action_describe(uint16_t action) {
    pd_mode_mask_t pd_mode = pd_mode_for_keycode(action);

    if (noah_action_keycode_is_layer_lock(action)) {
        return noah_action_desc_build(NOAH_ACTION_KIND_LAYER_LOCK, action, (uint8_t)(action - LAYER_LOCK_BASE), 0);
    }

    if (noah_action_keycode_is_owned_momentary_layer(action)) {
        return noah_action_desc_build(NOAH_ACTION_KIND_LAYER_HOLD, action, QK_MOMENTARY_GET_LAYER(action), 0);
    }

    if (noah_action_keycode_is_layer_tap(action)) {
        return noah_action_desc_build(NOAH_ACTION_KIND_LAYER_TAP, action, QK_LAYER_TAP_GET_LAYER(action), 0);
    }

    if (noah_action_keycode_is_raw_qmk_layer_action(action)) {
        return noah_action_desc_build(NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION, action, 0, 0);
    }

    if (pd_mode != 0) {
        return noah_action_desc_build(NOAH_ACTION_KIND_PD_MODE_HOLD, action, 0, pd_mode);
    }

    if (is_pd_mode_lock_action(action)) {
        return noah_action_desc_build(NOAH_ACTION_KIND_PD_MODE_LOCK, action, 0, 0);
    }

    if (noah_action_keycode_is_macro(action)) {
        return noah_action_desc_build(NOAH_ACTION_KIND_MACRO, action, 0, 0);
    }

    if (noah_action_keycode_is_qmk_behavior(action)) {
        return noah_action_desc_build(NOAH_ACTION_KIND_QMK_BEHAVIOR, action, 0, 0);
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
        return noah_action_desc_build(NOAH_ACTION_KIND_KEYMAP_CUSTOM, action, 0, 0);
    }

    return noah_action_desc_build(NOAH_ACTION_KIND_LITERAL, action, 0, 0);
}
