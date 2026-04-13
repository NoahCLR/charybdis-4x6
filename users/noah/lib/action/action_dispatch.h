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

typedef struct {
    uint16_t action;
    uint8_t  layer;
    pd_mode_mask_t pd_mode;
    bool     is_layer_lock;
    bool     is_raw_qmk_layer_action;
    bool     is_layer_tap;
    bool     is_macro;
    bool     is_qmk_behavior_keycode;
    bool     is_keymap_custom;
    bool     is_pd_mode_lock;
    bool     is_owned_momentary_layer;
} noah_action_desc_t;

static inline noah_action_desc_t noah_action_describe(uint16_t action) {
    bool is_layer_lock           = noah_action_keycode_is_layer_lock(action);
    bool is_owned_momentary_layer = noah_action_keycode_is_owned_momentary_layer(action);
    bool is_layer_tap            = noah_action_keycode_is_layer_tap(action);

    return (noah_action_desc_t){
        .action                   = action,
        .layer                    = is_owned_momentary_layer ? QK_MOMENTARY_GET_LAYER(action) : (is_layer_tap ? QK_LAYER_TAP_GET_LAYER(action) : (is_layer_lock ? (uint8_t)(action - LAYER_LOCK_BASE) : UINT8_MAX)),
        .pd_mode                  = pd_mode_for_keycode(action),
        .is_layer_lock            = is_layer_lock,
        .is_raw_qmk_layer_action  = noah_action_keycode_is_raw_qmk_layer_action(action),
        .is_layer_tap             = is_layer_tap,
        .is_macro                 = noah_action_keycode_is_macro(action),
        .is_qmk_behavior_keycode  = noah_action_keycode_is_qmk_behavior(action),
        .is_keymap_custom         = action >= NOAH_KEYMAP_SAFE_RANGE,
        .is_pd_mode_lock          = is_pd_mode_lock_action(action),
        .is_owned_momentary_layer = is_owned_momentary_layer,
    };
}

static inline bool noah_action_desc_is_layer_action(noah_action_desc_t desc) {
    return desc.is_layer_lock || desc.is_raw_qmk_layer_action;
}

static inline bool noah_action_desc_is_press_only(noah_action_desc_t desc) {
    return desc.is_layer_lock || desc.is_pd_mode_lock || desc.is_macro;
}

static inline bool noah_action_desc_requires_per_key_hold(noah_action_desc_t desc) {
    return desc.is_owned_momentary_layer;
}

static inline bool noah_action_desc_requires_owned_dispatch(noah_action_desc_t desc) {
    return noah_action_desc_is_press_only(desc) || noah_action_desc_requires_per_key_hold(desc);
}

static inline bool noah_action_desc_uses_shared_hold(noah_action_desc_t desc) {
    return !noah_action_desc_requires_owned_dispatch(desc);
}

static inline bool noah_action_desc_is_pd_mode_action(noah_action_desc_t desc) {
    return desc.pd_mode != 0;
}

bool action_dispatch_layer_is_locked(uint8_t layer);
void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy);
void noah_emit_synthetic_qmk_tap(uint16_t keycode, noah_emit_policy_t policy);
void noah_emit_literal_tap(uint16_t keycode, noah_emit_policy_t policy);
void action_dispatch(uint16_t action);
