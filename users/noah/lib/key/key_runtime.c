// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────
//
// Shared state and helper functions for the split key runtime modules.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"
#include "key_runtime_state.h"
#include "../action/action_dispatch.h"
#include "../pointing/pd_modes.h"
#include "held_action.h"

uint8_t behavior_get_layer(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode) ? QK_LAYER_TAP_GET_LAYER(keycode) : QK_MOMENTARY_GET_LAYER(keycode);
}

bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static bool key_runtime_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

bool active_key_matches(uint16_t keycode, keypos_t key_pos) {
    return active_key.keycode == keycode && key_runtime_keypos_equal(active_key.key_pos, key_pos);
}

bool key_runtime_activate_pending_fallback_hold(void) {
    if (!active_key.fallback_hold_pending || active_key.held_action_keycode != KC_NO || active_key.keycode == KC_NO) {
        return false;
    }

    held_action_register(active_key.key_pos, active_key.keycode);
    active_key.held_action_keycode = active_key.keycode;
    active_key.hold_fired          = true;
    return true;
}

void active_key_reset(void) {
    active_key = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
}

void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired) {
    active_key = (active_key_state_t){
        .timer               = timer_read(),
        .keycode             = keycode,
        .key_pos             = key_pos,
        .hold_fired          = hold_fired,
        .held_action_keycode = KC_NO,
        .tap_action          = tap_action,
        .tap_hold_term       = tap_hold_term,
        .longer_hold_term    = longer_hold_term,
        .multi_tap_term      = multi_tap_term,
        .hold                = hold,
        .long_hold           = long_hold,
    };
}

handled_key_view_t handled_key_lookup(uint16_t keycode) {
    return (handled_key_view_t){
        .behavior = key_behavior_lookup(keycode),
    };
}

static bool key_runtime_keycode_is_pure_modifier(uint16_t keycode) {
    switch (keycode) {
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

static bool handled_key_uses_buffered_modifier_single_step(handled_key_view_t key) {
    if (key.behavior.single.tap.present || key.behavior.single.hold.present || key.behavior.single.long_hold.present || key.behavior.is_momentary_layer) {
        return false;
    }

    return key.behavior.has_multi_tap && key.behavior.keycode < SAFE_RANGE && key_runtime_keycode_is_pure_modifier(key.behavior.keycode);
}

bool handled_key_uses_implicit_hold(handled_key_view_t key) {
    return pd_mode_for_keycode(key.behavior.keycode) != 0;
}

bool handled_key_uses_fallback_hold(handled_key_view_t key) {
    if (key.behavior.is_momentary_layer || key.behavior.keycode >= SAFE_RANGE) {
        return false;
    }

    if (action_dispatch_is_qmk_behavior_keycode(key.behavior.keycode)) {
        return false;
    }

    if (key.behavior.single.hold.present || key.behavior.single.long_hold.present) {
        return false;
    }

    return key.behavior.single.tap.present || key.behavior.has_multi_tap;
}

hold_behavior_t handled_key_single_hold(handled_key_view_t key) {
    if (key.behavior.single.hold.present) {
        return key.behavior.single.hold;
    }

    if (pd_mode_for_keycode(key.behavior.keycode)) {
        return (hold_behavior_t){
            .present = true,
            .action  = key.behavior.keycode,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        };
    }

    return hold_behavior_none();
}

uint16_t handled_key_tap_action(handled_key_view_t key) {
    if (key.behavior.single.tap.present) return key.behavior.single.tap.action;
    if (pd_mode_for_keycode(key.behavior.keycode)) return KC_NO;
    if (handled_key_uses_buffered_modifier_single_step(key)) return KC_NO;
    if (key.behavior.is_layer_tap) return QK_LAYER_TAP_GET_TAP_KEYCODE(key.behavior.keycode);
    if (key.behavior.is_momentary_layer) return KC_NO;
    if (key.behavior.keycode >= SAFE_RANGE) return KC_NO;
    return key.behavior.keycode;
}

bool handled_key_multi_tap_repress(handled_key_view_t key, uint16_t keycode) {
    return multi_tap_active(&multi_tap) && multi_tap.keycode == keycode && key.behavior.has_multi_tap;
}

uint16_t handled_key_advance_multi_tap(uint16_t keycode) {
    return multi_tap_advance(&multi_tap, keycode, key_behavior_step_lookup, key_behavior_has_more_taps);
}
