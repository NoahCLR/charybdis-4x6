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

bool key_runtime_slot_activate_pending_fallback_hold(active_key_state_t *slot) {
    if (!(slot && key_runtime_slot_uses_fallback_hold(slot) && slot->held_action_keycode == KC_NO && slot->keycode != KC_NO)) {
        return false;
    }

    slot->held_action_keycode = slot->keycode;
    key_runtime_slot_commit_hold_phase(slot, true);
    held_action_register(slot->key_pos, slot->keycode);
    return true;
}

bool key_runtime_activate_pending_fallback_hold(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        if (key_runtime_slot_activate_pending_fallback_hold(key_runtime_slot_at(index))) {
            return true;
        }
    }

    return false;
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
