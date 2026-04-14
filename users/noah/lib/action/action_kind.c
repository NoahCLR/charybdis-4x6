// ────────────────────────────────────────────────────────────────────────────
// Action Kind Metadata
// ────────────────────────────────────────────────────────────────────────────
//
// Shared action-kind metadata for authored validation, handled-key policy,
// descriptor classification, and lifecycle dispatch.
// ────────────────────────────────────────────────────────────────────────────

#include "action_kind_internal.h"

#include "noah_keymap_ids.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "owned_keycode.h"
#include "synthetic_record.h"
#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/layer_ownership.h"

#define NOAH_ACTION_DISPATCH_STANDARD \
    (NOAH_ACTION_DISPATCH_MACRO_PREFLIGHT | NOAH_ACTION_DISPATCH_PD_PRESS_INTERCEPT | NOAH_ACTION_DISPATCH_PD_RELEASE_INTERCEPT)

static void noah_action_log_unsupported_layer_action(noah_action_desc_t desc) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported raw QMK layer action 0x%04X; use LOCK_LAYER(...) for persistent changes or PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for owned momentary holds\n", (unsigned int)desc.action);
#else
    (void)desc;
#endif
}

static void noah_action_tap_noop(noah_action_desc_t desc) {
    (void)desc;
}

static void noah_action_press_noop(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    (void)key_pos;
}

static void noah_action_release_noop(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    (void)key_pos;
}

static void noah_action_tap_literal(noah_action_desc_t desc) {
    pointer_layer_policy_note_action(desc.action, true);
    tap_code16(desc.action);
    pointer_layer_policy_note_action(desc.action, false);
}

static void noah_action_press_literal(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;

    // Route literal keycodes, including QK_MODS such as S(KC_1), through the
    // shared owned-keycode contract so held actions and macro dispatch cannot
    // drift apart.
    if (owned_keycode_register(desc.action)) {
        return;
    }

    register_code16(desc.action);
}

static void noah_action_release_literal(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;

    if (owned_keycode_unregister(desc.action)) {
        return;
    }

    unregister_code16(desc.action);
}

static void noah_action_tap_unsupported_layer(noah_action_desc_t desc) {
    noah_action_log_unsupported_layer_action(desc);
}

static void noah_action_press_unsupported_layer(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_log_unsupported_layer_action(desc);
}

static void noah_action_release_unsupported_layer(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_log_unsupported_layer_action(desc);
}

static void noah_action_toggle_layer_lock(noah_action_desc_t desc) {
    layer_ownership_toggle_lock_state(desc.layer);
}

static void noah_action_toggle_pd_mode_lock(noah_action_desc_t desc) {
    const pd_mode_def_t *def = pd_mode_lock_action_lookup(desc.action);
    if (def) {
        pd_mode_toggle_lock_state(def->mode_flag);
    }
}

static void noah_action_tap_layer_lock(noah_action_desc_t desc) {
    noah_action_toggle_layer_lock(desc);
}

static void noah_action_press_layer_lock(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_toggle_layer_lock(desc);
}

static void noah_action_tap_pd_mode_lock(noah_action_desc_t desc) {
    noah_action_toggle_pd_mode_lock(desc);
}

static void noah_action_press_pd_mode_lock(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_toggle_pd_mode_lock(desc);
}

static void noah_action_press_owned_momentary_layer(noah_action_desc_t desc, keypos_t key_pos) {
    layer_ownership_momentary_press(key_pos, desc.layer);
}

static void noah_action_release_owned_momentary_layer(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    layer_ownership_momentary_release(key_pos);
}

static void noah_action_tap_keymap_custom(noah_action_desc_t desc) {
    noah_dispatch_synthetic_tap(desc.action);
}

static void noah_action_press_keymap_custom(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_dispatch_synthetic_record(desc.action, true);
}

static void noah_action_release_keymap_custom(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_dispatch_synthetic_record(desc.action, false);
}

static void noah_action_tap_qmk_behavior(noah_action_desc_t desc) {
    noah_dispatch_synthetic_qmk_tap(desc.action);
}

static void noah_action_press_qmk_behavior(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_dispatch_synthetic_qmk_record(desc.action, true, 0);
}

static void noah_action_release_qmk_behavior(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_dispatch_synthetic_qmk_record(desc.action, false, 0);
}

static const noah_action_kind_def_t noah_action_kind_defs[NOAH_ACTION_KIND_COUNT] = {
    [NOAH_ACTION_KIND_LITERAL] =
        {
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
            .dispatch_flags            = NOAH_ACTION_DISPATCH_STANDARD,
            .tap                       = noah_action_tap_literal,
            .press                     = noah_action_press_literal,
            .release                   = noah_action_release_literal,
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
            .tap     = noah_action_tap_layer_lock,
            .press   = noah_action_press_layer_lock,
            .release = noah_action_release_noop,
        },
    [NOAH_ACTION_KIND_LAYER_HOLD] =
        {
            .caps =
                NOAH_ACTION_CAP_REQUIRES_KEY_OWNER |
                NOAH_ACTION_CAP_LAYER_AFFECTING |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED,
            .preview_layer_uses_desc_layer = true,
            .dispatch_flags                = NOAH_ACTION_DISPATCH_STANDARD,
            .tap                           = noah_action_tap_unsupported_layer,
            .press                         = noah_action_press_owned_momentary_layer,
            .release                       = noah_action_release_owned_momentary_layer,
        },
    [NOAH_ACTION_KIND_LAYER_TAP] =
        {
            .caps =
                NOAH_ACTION_CAP_LAYER_AFFECTING |
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED,
            .dispatch_flags = NOAH_ACTION_DISPATCH_STANDARD,
            .tap            = noah_action_tap_unsupported_layer,
            .press          = noah_action_press_unsupported_layer,
            .release        = noah_action_release_unsupported_layer,
        },
    [NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION] =
        {
            .caps           = NOAH_ACTION_CAP_LAYER_AFFECTING,
            .dispatch_flags = NOAH_ACTION_DISPATCH_STANDARD,
            .tap            = noah_action_tap_unsupported_layer,
            .press          = noah_action_press_unsupported_layer,
            .release        = noah_action_release_unsupported_layer,
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
            .dispatch_flags            = NOAH_ACTION_DISPATCH_STANDARD,
            .tap                       = noah_action_tap_noop,
            .press                     = noah_action_press_noop,
            .release                   = noah_action_release_noop,
        },
    [NOAH_ACTION_KIND_QMK_BEHAVIOR] =
        {
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
            .dispatch_flags            = NOAH_ACTION_DISPATCH_STANDARD,
            .tap                       = noah_action_tap_qmk_behavior,
            .press                     = noah_action_press_qmk_behavior,
            .release                   = noah_action_release_qmk_behavior,
        },
    [NOAH_ACTION_KIND_KEYMAP_CUSTOM] =
        {
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .keeps_registered_feedback = true,
            .dispatch_flags            = NOAH_ACTION_DISPATCH_STANDARD,
            .tap                       = noah_action_tap_keymap_custom,
            .press                     = noah_action_press_keymap_custom,
            .release                   = noah_action_release_keymap_custom,
        },
    [NOAH_ACTION_KIND_PD_MODE_HOLD] =
        {
            .caps =
                NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED |
                NOAH_ACTION_CAP_PD_MODE_AFFECTING |
                NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED |
                NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED,
            .dispatch_flags = NOAH_ACTION_DISPATCH_STANDARD,
            .tap            = noah_action_tap_literal,
            .press          = noah_action_press_literal,
            .release        = noah_action_release_literal,
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
            .tap     = noah_action_tap_pd_mode_lock,
            .press   = noah_action_press_pd_mode_lock,
            .release = noah_action_release_noop,
        },
};

static const noah_action_kind_def_t *noah_action_desc_kind_def(noah_action_desc_t desc) {
    return noah_action_kind_def(desc.kind);
}

static bool noah_action_desc_has_dispatch_flag(noah_action_desc_t desc, noah_action_dispatch_flag_t flag) {
    const noah_action_kind_def_t *def = noah_action_desc_kind_def(desc);
    return def && (def->dispatch_flags & (uint8_t)flag) != 0;
}

const noah_action_kind_def_t *noah_action_kind_def(noah_action_kind_t kind) {
    if (kind >= NOAH_ACTION_KIND_COUNT) {
        kind = NOAH_ACTION_KIND_LITERAL;
    }

    return &noah_action_kind_defs[kind];
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

void noah_action_desc_tap_dispatch(noah_action_desc_t desc) {
    const noah_action_kind_def_t *def = noah_action_desc_kind_def(desc);
    def->tap(desc);
}

void noah_action_desc_press_dispatch(noah_action_desc_t desc, keypos_t key_pos) {
    const noah_action_kind_def_t *def = noah_action_desc_kind_def(desc);
    def->press(desc, key_pos);
}

void noah_action_desc_release_dispatch(noah_action_desc_t desc, keypos_t key_pos) {
    const noah_action_kind_def_t *def = noah_action_desc_kind_def(desc);
    def->release(desc, key_pos);
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
