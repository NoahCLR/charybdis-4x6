// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Dispatches authored key actions, including custom layer-lock and
// pointing-device lock actions.
//
// Actions are keycode-like values. Keymap-local custom keycodes starting at
// NOAH_KEYMAP_SAFE_RANGE are routed back through process_record_user().
// Non-layer QMK behavior keycodes such as OSM()/MT() need explicit handling
// too, because tap_code16/register_code16 only model plain key press/release.
// Raw QMK layer actions are intentionally handled separately so they cannot
// bypass the userspace layer ownership model.
//
// Scope note: this module intentionally keeps authored tap actions on normal
// QMK tap semantics. Ownership-aware literal keycode dispatch, including held
// QK_MODS actions such as S(KC_1), lives in owned_keycode.c and is reused by
// the overlap-sensitive synthetic paths this userspace runtime owns.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "action_kind_registry_list.h"
#include "../pointing/defs/pd_modes.h"

typedef struct {
    bool settle_pending_fallback_holds;
    bool preserve_keyboard_mod_state;
} noah_emit_policy_t;

#define NOAH_EMIT_POLICY_NONE ((noah_emit_policy_t){0})
#define NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS ((noah_emit_policy_t){.settle_pending_fallback_holds = true})
#define NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS ((noah_emit_policy_t){.settle_pending_fallback_holds = true, .preserve_keyboard_mod_state = true})

static inline bool noah_action_keycode_is_layer_lock(uint16_t action) {
    return action >= LAYER_LOCK_BASE && action < LAYER_LOCK_BASE + LAYER_COUNT;
}

static inline bool noah_action_keycode_is_owned_momentary_layer(uint16_t action) {
    return IS_QK_MOMENTARY(action);
}

static inline bool noah_action_keycode_is_layer_tap(uint16_t action) {
    return IS_QK_LAYER_TAP(action);
}

static inline bool noah_action_keycode_is_raw_qmk_layer_action(uint16_t action) {
    return IS_QK_TO(action) || IS_QK_MOMENTARY(action) || IS_QK_DEF_LAYER(action) || IS_QK_TOGGLE_LAYER(action) || IS_QK_ONE_SHOT_LAYER(action) || IS_QK_LAYER_TAP_TOGGLE(action) || IS_QK_LAYER_MOD(action) || IS_QK_LAYER_TAP(action);
}

static inline bool noah_action_keycode_is_macro(uint16_t action) {
    return (action >= MACRO_0 && action <= MACRO_15) || IS_QK_MACRO(action);
}

static inline bool noah_action_keycode_is_qmk_behavior(uint16_t action) {
    return IS_QK_ONE_SHOT_MOD(action) || IS_QK_MOD_TAP(action);
}

typedef enum {
#define NOAH_ACTION_KIND_ENUM(name, priority, matcher, caps, keeps_feedback, preview_uses_desc_layer, dispatch_flags, policy_flags, tap, press, release) NOAH_ACTION_KIND_##name,
    NOAH_ACTION_KIND_REGISTRY(NOAH_ACTION_KIND_ENUM)
#undef NOAH_ACTION_KIND_ENUM
        NOAH_ACTION_KIND_COUNT,
} noah_action_kind_t;

typedef enum {
    NOAH_ACTION_CAP_PRESS_ONLY                             = (1u << 0),
    NOAH_ACTION_CAP_REQUIRES_KEY_OWNER                     = (1u << 1),
    NOAH_ACTION_CAP_LAYER_AFFECTING                        = (1u << 2),
    NOAH_ACTION_CAP_PD_MODE_AFFECTING                      = (1u << 3),
    NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED             = (1u << 4),
    NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED                 = (1u << 5),
    NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED = (1u << 6),
    NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED          = (1u << 7),
    NOAH_ACTION_CAP_CONSUMES_DIRECT_PRESS                  = (1u << 8),
} noah_action_cap_t;

typedef enum {
    NOAH_ACTION_AUTHORED_USE_TAP = 0,
    NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD,
    NOAH_ACTION_AUTHORED_USE_HOLD_OTHER,
} noah_action_authored_use_t;

typedef struct {
    noah_action_kind_t kind;
    uint16_t           action;
    uint8_t            layer;
    pd_mode_mask_t     pd_mode;
} noah_action_desc_t;

bool     noah_action_desc_has_capability(noah_action_desc_t desc, noah_action_cap_t capability);
bool     noah_action_desc_is_runtime_handled_keycode(noah_action_desc_t desc);
bool     noah_action_desc_is_momentary_layer_keycode(noah_action_desc_t desc);
bool     noah_action_desc_uses_authored_layer_tap_contract(noah_action_desc_t desc);
bool     noah_action_desc_releases_momentary_layer_before_action(noah_action_desc_t desc);
bool     noah_action_desc_uses_held_lifecycle_for_press_and_hold(noah_action_desc_t desc);
bool     noah_action_desc_default_tap_uses_layer_tap_keycode(noah_action_desc_t desc);
bool     noah_action_desc_default_tap_uses_action_keycode(noah_action_desc_t desc);
bool     noah_action_desc_is_pure_modifier_literal(noah_action_desc_t desc);
bool     noah_action_desc_source_sets_momentary_layer_flag(noah_action_desc_t desc);
bool     noah_action_desc_source_sets_layer_tap_flag(noah_action_desc_t desc);
bool     noah_action_desc_supports_fallback_hold(noah_action_desc_t desc);
bool     noah_action_desc_source_layer_uses_desc_layer(noah_action_desc_t desc);
uint8_t  noah_action_desc_source_layer(noah_action_desc_t desc);
uint16_t noah_action_desc_default_tap_action(noah_action_desc_t desc);

static inline bool noah_action_desc_is_layer_lock(noah_action_desc_t desc) {
    return desc.kind == NOAH_ACTION_KIND_LAYER_LOCK;
}

static inline bool noah_action_desc_is_owned_momentary_layer(noah_action_desc_t desc) {
    return desc.kind == NOAH_ACTION_KIND_LAYER_HOLD;
}

static inline bool noah_action_desc_is_layer_tap(noah_action_desc_t desc) {
    return desc.kind == NOAH_ACTION_KIND_LAYER_TAP;
}

static inline bool noah_action_desc_is_raw_qmk_layer_action(noah_action_desc_t desc) {
    return desc.kind == NOAH_ACTION_KIND_LAYER_HOLD || desc.kind == NOAH_ACTION_KIND_LAYER_TAP || desc.kind == NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION;
}

static inline bool noah_action_desc_is_macro(noah_action_desc_t desc) {
    return desc.kind == NOAH_ACTION_KIND_MACRO;
}

static inline bool noah_action_desc_is_qmk_behavior_keycode(noah_action_desc_t desc) {
    return desc.kind == NOAH_ACTION_KIND_QMK_BEHAVIOR;
}

static inline bool noah_action_desc_is_keymap_custom(noah_action_desc_t desc) {
    return desc.kind == NOAH_ACTION_KIND_KEYMAP_CUSTOM;
}

static inline bool noah_action_desc_is_pd_mode_lock(noah_action_desc_t desc) {
    return desc.kind == NOAH_ACTION_KIND_PD_MODE_LOCK;
}

static inline bool noah_action_desc_is_layer_action(noah_action_desc_t desc) {
    return noah_action_desc_has_capability(desc, NOAH_ACTION_CAP_LAYER_AFFECTING);
}

static inline bool noah_action_desc_is_press_only(noah_action_desc_t desc) {
    return noah_action_desc_has_capability(desc, NOAH_ACTION_CAP_PRESS_ONLY);
}

static inline bool noah_action_desc_requires_per_key_hold(noah_action_desc_t desc) {
    return noah_action_desc_has_capability(desc, NOAH_ACTION_CAP_REQUIRES_KEY_OWNER);
}

static inline bool noah_action_desc_requires_owned_dispatch(noah_action_desc_t desc) {
    return noah_action_desc_is_press_only(desc) || noah_action_desc_requires_per_key_hold(desc);
}

static inline bool noah_action_desc_uses_shared_hold(noah_action_desc_t desc) {
    return !noah_action_desc_requires_owned_dispatch(desc);
}

static inline bool noah_action_desc_is_pd_mode_action(noah_action_desc_t desc) {
    return noah_action_desc_has_capability(desc, NOAH_ACTION_CAP_PD_MODE_AFFECTING);
}

static inline bool noah_action_desc_supported_as_behavior_keycode(noah_action_desc_t desc) {
    return noah_action_desc_has_capability(desc, NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED);
}

static inline bool noah_action_desc_supported_as_authored_action(noah_action_desc_t desc, noah_action_authored_use_t use) {
    switch (use) {
        case NOAH_ACTION_AUTHORED_USE_TAP:
            return noah_action_desc_has_capability(desc, NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED);
        case NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD:
            return noah_action_desc_has_capability(desc, NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED);
        case NOAH_ACTION_AUTHORED_USE_HOLD_OTHER:
            return noah_action_desc_has_capability(desc, NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED);
        default:
            return false;
    }
}

static inline bool noah_action_desc_consumes_direct_press(noah_action_desc_t desc) {
    return noah_action_desc_has_capability(desc, NOAH_ACTION_CAP_CONSUMES_DIRECT_PRESS);
}

noah_action_desc_t noah_action_describe(uint16_t action);
void               noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy);
void               noah_emit_synthetic_qmk_tap(uint16_t keycode, noah_emit_policy_t policy);
void               noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(uint16_t keycode, uint8_t masked_mods, bool settle_pending_fallback_holds);
void               noah_emit_literal_tap(uint16_t keycode, noah_emit_policy_t policy);
void               action_dispatch(uint16_t action);
