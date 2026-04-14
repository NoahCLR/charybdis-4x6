// ────────────────────────────────────────────────────────────────────────────
// Action Kind Metadata
// ────────────────────────────────────────────────────────────────────────────
//
// Shared action-kind metadata for authored validation, handled-key policy,
// and descriptor classification.
// ────────────────────────────────────────────────────────────────────────────

#include "action_kind_internal.h"

#include "noah_keymap_ids.h"

#define NOAH_ACTION_DISPATCH_STANDARD (NOAH_ACTION_DISPATCH_MACRO_PREFLIGHT | NOAH_ACTION_DISPATCH_PD_PRESS_INTERCEPT | NOAH_ACTION_DISPATCH_PD_RELEASE_INTERCEPT)

static const noah_action_kind_def_t noah_action_kind_defs[NOAH_ACTION_KIND_COUNT] = {
#define NOAH_ACTION_KIND_DEF(name, priority, matcher, cap_mask, feedback_kept, uses_desc_layer_preview, dispatch_mask, policy_mask, tap_impl, press_impl, release_impl) \
    [NOAH_ACTION_KIND_##name] = {                                                                                                                                       \
        .defined                       = true,                                                                                                                          \
        .caps                          = (uint16_t)(cap_mask),                                                                                                          \
        .keeps_registered_feedback     = (feedback_kept),                                                                                                               \
        .preview_layer_uses_desc_layer = (uses_desc_layer_preview),                                                                                                     \
        .dispatch_flags                = (uint8_t)(dispatch_mask),                                                                                                      \
        .policy_flags                  = (uint16_t)(policy_mask),                                                                                                       \
        .match_priority                = (uint8_t)(priority),                                                                                                           \
        .match                         = matcher,                                                                                                                       \
    },
    NOAH_ACTION_KIND_REGISTRY(NOAH_ACTION_KIND_DEF)
#undef NOAH_ACTION_KIND_DEF
};

static const noah_action_kind_def_t *noah_action_desc_kind_def(noah_action_desc_t desc) {
    return noah_action_kind_def(desc.kind);
}

static bool noah_action_desc_has_dispatch_flag(noah_action_desc_t desc, noah_action_dispatch_flag_t flag) {
    const noah_action_kind_def_t *def = noah_action_desc_kind_def(desc);
    return def && (def->dispatch_flags & (uint8_t)flag) != 0;
}

static bool noah_action_desc_has_policy_flag(noah_action_desc_t desc, noah_action_policy_flag_t flag) {
    const noah_action_kind_def_t *def = noah_action_desc_kind_def(desc);
    return def && (def->policy_flags & (uint16_t)flag) != 0;
}

static bool noah_action_keycode_is_pure_modifier_literal(uint16_t action) {
    switch (action) {
        case KC_LEFT_CTRL:
        case KC_LEFT_SHIFT:
        case KC_LEFT_ALT:
        case KC_LEFT_GUI:
        case KC_RIGHT_CTRL:
        case KC_RIGHT_SHIFT:
        case KC_RIGHT_ALT:
        case KC_RIGHT_GUI:
            return true;
        default:
            return false;
    }
}

bool noah_action_kind_metadata_defined(noah_action_kind_t kind) {
    const noah_action_kind_def_t *def = noah_action_kind_def(kind);
    return def && def->defined;
}

bool noah_action_desc_has_capability(noah_action_desc_t desc, noah_action_cap_t capability) {
    const noah_action_kind_def_t *def = noah_action_desc_kind_def(desc);
    return def && (def->caps & (uint16_t)capability) != 0;
}

bool noah_action_desc_is_runtime_handled_keycode(noah_action_desc_t desc) {
    return noah_action_desc_has_policy_flag(desc, NOAH_ACTION_POLICY_RUNTIME_HANDLED_KEYCODE);
}

bool noah_action_desc_is_momentary_layer_keycode(noah_action_desc_t desc) {
    return noah_action_desc_has_policy_flag(desc, NOAH_ACTION_POLICY_MOMENTARY_LAYER_KEYCODE);
}

bool noah_action_desc_uses_authored_layer_tap_contract(noah_action_desc_t desc) {
    return noah_action_desc_has_policy_flag(desc, NOAH_ACTION_POLICY_AUTHORED_LAYER_TAP_CONTRACT);
}

bool noah_action_desc_releases_momentary_layer_before_action(noah_action_desc_t desc) {
    return noah_action_desc_has_policy_flag(desc, NOAH_ACTION_POLICY_RELEASES_MOMENTARY_LAYER_BEFORE_ACTION);
}

bool noah_action_desc_uses_held_lifecycle_for_press_and_hold(noah_action_desc_t desc) {
    return noah_action_desc_has_policy_flag(desc, NOAH_ACTION_POLICY_PRESS_AND_HOLD_USES_HELD_LIFECYCLE);
}

bool noah_action_desc_default_tap_uses_layer_tap_keycode(noah_action_desc_t desc) {
    return noah_action_desc_has_policy_flag(desc, NOAH_ACTION_POLICY_DEFAULT_TAP_USES_LAYER_TAP_KEYCODE);
}

bool noah_action_desc_default_tap_uses_action_keycode(noah_action_desc_t desc) {
    return noah_action_desc_has_policy_flag(desc, NOAH_ACTION_POLICY_DEFAULT_TAP_USES_ACTION_KEYCODE);
}

bool noah_action_desc_is_pure_modifier_literal(noah_action_desc_t desc) {
    return desc.kind == NOAH_ACTION_KIND_LITERAL && noah_action_keycode_is_pure_modifier_literal(desc.action);
}

bool noah_action_desc_source_sets_momentary_layer_flag(noah_action_desc_t desc) {
    return noah_action_desc_source_layer_uses_desc_layer(desc);
}

bool noah_action_desc_source_sets_layer_tap_flag(noah_action_desc_t desc) {
    return noah_action_desc_default_tap_uses_layer_tap_keycode(desc);
}

bool noah_action_desc_supports_fallback_hold(noah_action_desc_t desc) {
    return noah_action_desc_has_policy_flag(desc, NOAH_ACTION_POLICY_SUPPORTS_FALLBACK_HOLD);
}

bool noah_action_desc_source_layer_uses_desc_layer(noah_action_desc_t desc) {
    return noah_action_desc_has_policy_flag(desc, NOAH_ACTION_POLICY_SOURCE_LAYER_USES_DESC_LAYER);
}

uint8_t noah_action_desc_source_layer(noah_action_desc_t desc) {
    return noah_action_desc_source_layer_uses_desc_layer(desc) ? desc.layer : UINT8_MAX;
}

uint16_t noah_action_desc_default_tap_action(noah_action_desc_t desc) {
    if (noah_action_desc_default_tap_uses_layer_tap_keycode(desc)) {
        return QK_LAYER_TAP_GET_TAP_KEYCODE(desc.action);
    }

    if (noah_action_desc_default_tap_uses_action_keycode(desc)) {
        return desc.action;
    }

    return KC_NO;
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

const noah_action_kind_def_t *noah_action_kind_def(noah_action_kind_t kind) {
    return kind < NOAH_ACTION_KIND_COUNT ? &noah_action_kind_defs[kind] : NULL;
}

static noah_action_desc_t noah_action_desc_build(noah_action_kind_t kind, uint16_t action, uint8_t layer, pd_mode_mask_t pd_mode) {
    return (noah_action_desc_t){
        .kind    = kind,
        .action  = action,
        .layer   = layer,
        .pd_mode = pd_mode,
    };
}

bool noah_action_kind_match_literal(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    if (!out) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_LITERAL, action, 0, pd_mode);
    return true;
}

bool noah_action_kind_match_layer_lock(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    (void)pd_mode;

    if (!(out && noah_action_keycode_is_layer_lock(action))) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_LAYER_LOCK, action, (uint8_t)(action - LAYER_LOCK_BASE), 0);
    return true;
}

bool noah_action_kind_match_layer_hold(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    (void)pd_mode;

    if (!(out && noah_action_keycode_is_owned_momentary_layer(action))) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_LAYER_HOLD, action, QK_MOMENTARY_GET_LAYER(action), 0);
    return true;
}

bool noah_action_kind_match_layer_tap(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    (void)pd_mode;

    if (!(out && noah_action_keycode_is_layer_tap(action))) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_LAYER_TAP, action, QK_LAYER_TAP_GET_LAYER(action), 0);
    return true;
}

bool noah_action_kind_match_unsupported_layer_action(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    (void)pd_mode;

    if (!(out && noah_action_keycode_is_raw_qmk_layer_action(action))) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION, action, 0, 0);
    return true;
}

bool noah_action_kind_match_macro(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    (void)pd_mode;

    if (!(out && noah_action_keycode_is_macro(action))) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_MACRO, action, 0, 0);
    return true;
}

bool noah_action_kind_match_qmk_behavior(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    (void)pd_mode;

    if (!(out && noah_action_keycode_is_qmk_behavior(action))) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_QMK_BEHAVIOR, action, 0, 0);
    return true;
}

bool noah_action_kind_match_keymap_custom(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    (void)pd_mode;

    if (!(out && action >= NOAH_KEYMAP_SAFE_RANGE)) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_KEYMAP_CUSTOM, action, 0, 0);
    return true;
}

bool noah_action_kind_match_pd_mode_hold(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    if (!(out && pd_mode != 0)) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_PD_MODE_HOLD, action, 0, pd_mode);
    return true;
}

bool noah_action_kind_match_pd_mode_lock(uint16_t action, pd_mode_mask_t pd_mode, noah_action_desc_t *out) {
    (void)pd_mode;

    if (!(out && is_pd_mode_lock_action(action))) {
        return false;
    }

    *out = noah_action_desc_build(NOAH_ACTION_KIND_PD_MODE_LOCK, action, 0, 0);
    return true;
}

noah_action_desc_t noah_action_describe(uint16_t action) {
    const noah_action_kind_def_t *best_def  = NULL;
    noah_action_desc_t            best_desc = noah_action_desc_build(NOAH_ACTION_KIND_LITERAL, action, 0, 0);
    pd_mode_mask_t                pd_mode   = pd_mode_for_keycode(action);

    for (uint8_t kind = 0; kind < NOAH_ACTION_KIND_COUNT; kind++) {
        const noah_action_kind_def_t *def = noah_action_kind_def((noah_action_kind_t)kind);
        noah_action_desc_t            candidate;

        if (!(def && def->defined && def->match)) {
            continue;
        }

        if (!def->match(action, pd_mode, &candidate)) {
            continue;
        }

        if (!best_def || def->match_priority > best_def->match_priority) {
            best_def  = def;
            best_desc = candidate;
        }
    }

    return best_desc;
}
