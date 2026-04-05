// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────
//
// Shared state and helper functions for the split key runtime modules.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_internal.h"

active_key_state_t active_key = ACTIVE_KEY_STATE_INIT;
multi_tap_t        multi_tap  = {0};

uint8_t behavior_get_layer(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode) ? QK_LAYER_TAP_GET_LAYER(keycode) : QK_MOMENTARY_GET_LAYER(keycode);
}

bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

bool keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

bool active_key_matches(uint16_t keycode, keypos_t key_pos) {
    return active_key.keycode == keycode && keypos_equal(active_key.key_pos, key_pos);
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

bool handled_key_uses_implicit_pd_mode_hold(handled_key_view_t key) {
    return !key.behavior.single.hold.present && pd_mode_for_keycode(key.behavior.keycode);
}

hold_behavior_t handled_key_single_hold(handled_key_view_t key) {
    if (key.behavior.single.hold.present) {
        return key.behavior.single.hold;
    }

    if (handled_key_uses_implicit_pd_mode_hold(key)) {
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

void handled_key_dispatch_tap_or_begin_multi_tap(uint16_t keycode, handled_key_view_t key) {
    uint16_t tap_action = handled_key_tap_action(key);
    if (key.behavior.has_multi_tap) {
        multi_tap_begin(&multi_tap, keycode, tap_action, key.behavior.tap_hold_term, key.behavior.multi_tap_term);
    } else if (tap_action != KC_NO) {
        action_dispatch(tap_action);
    }
}
