// ────────────────────────────────────────────────────────────────────────────
// Action Kind Metadata
// ────────────────────────────────────────────────────────────────────────────
//
// Shared action-kind metadata for authored validation, handled-key policy,
// and descriptor classification.
// ────────────────────────────────────────────────────────────────────────────

#include "action_kind_internal.h"

#include "noah_keymap_ids.h"

#define NOAH_ACTION_DISPATCH_STANDARD \
    (NOAH_ACTION_DISPATCH_MACRO_PREFLIGHT | NOAH_ACTION_DISPATCH_PD_PRESS_INTERCEPT | NOAH_ACTION_DISPATCH_PD_RELEASE_INTERCEPT)

static const noah_action_kind_def_t noah_action_kind_defs[NOAH_ACTION_KIND_COUNT] = {
    [NOAH_ACTION_KIND_LITERAL] =
        {
            .defined = true,
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
            .dispatch_flags            = NOAH_ACTION_DISPATCH_STANDARD,
        },
    [NOAH_ACTION_KIND_LAYER_LOCK] =
        {
            .defined = true,
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
            .defined = true,
            .caps =
                NOAH_ACTION_CAP_REQUIRES_KEY_OWNER |
                NOAH_ACTION_CAP_LAYER_AFFECTING |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED,
            .preview_layer_uses_desc_layer = true,
            .dispatch_flags                = NOAH_ACTION_DISPATCH_STANDARD,
        },
    [NOAH_ACTION_KIND_LAYER_TAP] =
        {
            .defined = true,
            .caps =
                NOAH_ACTION_CAP_LAYER_AFFECTING |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED,
            .dispatch_flags = NOAH_ACTION_DISPATCH_STANDARD,
        },
    [NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION] =
        {
            .defined = true,
            .caps           = NOAH_ACTION_CAP_LAYER_AFFECTING,
            .dispatch_flags = NOAH_ACTION_DISPATCH_STANDARD,
        },
    [NOAH_ACTION_KIND_MACRO] =
        {
            .defined = true,
            .caps =
                NOAH_ACTION_CAP_PRESS_ONLY |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
            .dispatch_flags            = NOAH_ACTION_DISPATCH_STANDARD,
        },
    [NOAH_ACTION_KIND_QMK_BEHAVIOR] =
        {
            .defined = true,
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
            .dispatch_flags            = NOAH_ACTION_DISPATCH_STANDARD,
        },
    [NOAH_ACTION_KIND_KEYMAP_CUSTOM] =
        {
            .defined = true,
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
            .dispatch_flags            = NOAH_ACTION_DISPATCH_STANDARD,
        },
    [NOAH_ACTION_KIND_PD_MODE_HOLD] =
        {
            .defined = true,
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_PD_MODE_AFFECTING |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .dispatch_flags = NOAH_ACTION_DISPATCH_STANDARD,
        },
    [NOAH_ACTION_KIND_PD_MODE_LOCK] =
        {
            .defined = true,
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

static const noah_action_kind_def_t *noah_action_desc_kind_def(noah_action_desc_t desc) {
    return noah_action_kind_def(desc.kind);
}

static bool noah_action_desc_has_dispatch_flag(noah_action_desc_t desc, noah_action_dispatch_flag_t flag) {
    const noah_action_kind_def_t *def = noah_action_desc_kind_def(desc);
    return def && (def->dispatch_flags & (uint8_t)flag) != 0;
}

bool noah_action_kind_metadata_defined(noah_action_kind_t kind) {
    return kind < NOAH_ACTION_KIND_COUNT && noah_action_kind_defs[kind].defined;
}

const noah_action_kind_def_t *noah_action_kind_def(noah_action_kind_t kind) {
    return noah_action_kind_metadata_defined(kind) ? &noah_action_kind_defs[kind] : NULL;
}

bool noah_action_desc_has_capability(noah_action_desc_t desc, noah_action_cap_t capability) {
    const noah_action_kind_def_t *def = noah_action_desc_kind_def(desc);
    return def && (def->caps & (uint16_t)capability) != 0;
}

bool noah_action_desc_dispatches_macro_preflight(noah_action_desc_t desc) {
    return noah_action_desc_has_dispatch_flag(desc, NOAH_ACTION_DISPATCH_MACRO_PREFLIGHT);
}

bool noah_action_desc_intercepts_pd_mode_press(noah_action_desc_t desc) {
    return noah_action_desc_has_dispatch_flag(desc, NOAH_ACTION_DISPATCH_PD_PRESS_INTERCEPT);
}

bool noah_action_desc_intercepts_pd_mode_release(noah_action_desc_t desc) {
    return noah_action_desc_has_dispatch_flag(desc, NOAH_ACTION_DISPATCH_PD_RELEASE_INTERCEPT);
}

static noah_action_desc_t noah_action_desc_build(noah_action_kind_t kind, uint16_t action, uint8_t layer, pd_mode_mask_t pd_mode) {
    return (noah_action_desc_t){
        .kind    = kind,
        .action  = action,
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
