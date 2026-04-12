// ────────────────────────────────────────────────────────────────────────────
// Handled Key Resolution
// ────────────────────────────────────────────────────────────────────────────
//
// Resolve authored key_behavior rows into the handled-key contract consumed by
// the runtime press/release engine.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"

#include "../action/action_dispatch.h"
#include "../pointing/pd_modes.h"
#include "key_behavior_lookup.h"

uint8_t behavior_get_layer(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode) ? QK_LAYER_TAP_GET_LAYER(keycode) : QK_MOMENTARY_GET_LAYER(keycode);
}

bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static bool handled_key_keycode_is_pure_modifier(uint16_t keycode) {
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

static pd_mode_mask_t handled_key_pd_mode_for_behavior(key_behavior_view_t behavior) {
    return pd_mode_for_keycode(behavior.keycode);
}

static bool handled_key_uses_buffered_modifier_single_step(key_behavior_view_t behavior) {
    if (behavior.single.tap.present || behavior.single.hold.present || behavior.single.long_hold.present || behavior.is_momentary_layer) {
        return false;
    }

    return behavior.has_multi_tap && behavior.keycode < SAFE_RANGE && handled_key_keycode_is_pure_modifier(behavior.keycode);
}

static bool handled_key_uses_implicit_hold_behavior(key_behavior_view_t behavior) {
    return handled_key_pd_mode_for_behavior(behavior) != 0;
}

static bool handled_key_uses_fallback_hold_behavior(key_behavior_view_t behavior) {
    if (behavior.is_momentary_layer || behavior.keycode >= SAFE_RANGE) {
        return false;
    }

    if (action_dispatch_is_qmk_behavior_keycode(behavior.keycode)) {
        return false;
    }

    if (behavior.single.hold.present || behavior.single.long_hold.present) {
        return false;
    }

    return behavior.single.tap.present || behavior.has_multi_tap;
}

static hold_behavior_t handled_key_single_hold_behavior(key_behavior_view_t behavior) {
    if (behavior.single.hold.present) {
        return behavior.single.hold;
    }

    if (handled_key_uses_implicit_hold_behavior(behavior)) {
        return (hold_behavior_t){
            .present = true,
            .action  = behavior.keycode,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        };
    }

    return hold_behavior_none();
}

static uint16_t handled_key_tap_action_behavior(key_behavior_view_t behavior) {
    if (behavior.single.tap.present) return behavior.single.tap.action;
    if (handled_key_uses_implicit_hold_behavior(behavior)) return KC_NO;
    if (handled_key_uses_buffered_modifier_single_step(behavior)) return KC_NO;
    if (behavior.is_layer_tap) return QK_LAYER_TAP_GET_TAP_KEYCODE(behavior.keycode);
    if (behavior.is_momentary_layer) return KC_NO;
    if (behavior.keycode >= SAFE_RANGE) return KC_NO;
    return behavior.keycode;
}

static key_runtime_slot_hold_strategy_t handled_key_hold_strategy_behavior(key_behavior_view_t behavior) {
    if (handled_key_uses_implicit_hold_behavior(behavior)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    }

    if (handled_key_uses_fallback_hold_behavior(behavior)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK;
    }

    return KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

static uint16_t handled_key_flags_from_behavior(key_behavior_view_t behavior) {
    uint16_t flags = 0;

    if (behavior.handled) {
        flags |= HANDLED_KEY_FLAG_HANDLED;
    }
    if (behavior.has_multi_tap) {
        flags |= HANDLED_KEY_FLAG_MULTI_TAP;
    }
    if (behavior.is_momentary_layer) {
        flags |= HANDLED_KEY_FLAG_MOMENTARY_LAYER;
    }
    if (behavior.is_layer_tap) {
        flags |= HANDLED_KEY_FLAG_LAYER_TAP;
    }
    if (handled_key_uses_implicit_hold_behavior(behavior)) {
        flags |= HANDLED_KEY_FLAG_IMPLICIT_HOLD;
    }
    if (handled_key_uses_fallback_hold_behavior(behavior)) {
        flags |= HANDLED_KEY_FLAG_FALLBACK_HOLD;
    }

    return flags;
}

handled_key_view_t handled_key_lookup(uint16_t keycode) {
    key_behavior_view_t behavior = key_behavior_lookup(keycode);

    return (handled_key_view_t){
        .tap_action       = handled_key_tap_action_behavior(behavior),
        .hold             = handled_key_single_hold_behavior(behavior),
        .long_hold        = behavior.single.long_hold,
        .hold_strategy    = handled_key_hold_strategy_behavior(behavior),
        .tap_hold_term    = behavior.tap_hold_term,
        .longer_hold_term = behavior.longer_hold_term,
        .multi_tap_term   = behavior.multi_tap_term,
        .layer            = behavior.is_momentary_layer ? behavior_get_layer(behavior.keycode) : UINT8_MAX,
        .pd_mode          = handled_key_pd_mode_for_behavior(behavior),
        .flags            = handled_key_flags_from_behavior(behavior),
    };
}

bool handled_key_is_handled(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_HANDLED) != 0;
}

bool handled_key_uses_implicit_hold(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_IMPLICIT_HOLD) != 0;
}

bool handled_key_uses_fallback_hold(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_FALLBACK_HOLD) != 0;
}

bool handled_key_has_multi_tap(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0;
}

bool handled_key_is_momentary_layer(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0;
}

bool handled_key_is_layer_tap(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_LAYER_TAP) != 0;
}

hold_behavior_t handled_key_single_hold(handled_key_view_t key) {
    return key.hold;
}

hold_behavior_t handled_key_long_hold(handled_key_view_t key) {
    return key.long_hold;
}

key_runtime_slot_hold_strategy_t handled_key_hold_strategy(handled_key_view_t key) {
    return key.hold_strategy;
}

uint16_t handled_key_tap_action(handled_key_view_t key) {
    return key.tap_action;
}

uint16_t handled_key_tap_hold_term(handled_key_view_t key) {
    return key.tap_hold_term;
}

uint16_t handled_key_longer_hold_term(handled_key_view_t key) {
    return key.longer_hold_term;
}

uint16_t handled_key_multi_tap_term(handled_key_view_t key) {
    return key.multi_tap_term;
}

uint8_t handled_key_layer(handled_key_view_t key) {
    return key.layer;
}

pd_mode_mask_t handled_key_pd_mode(handled_key_view_t key) {
    return key.pd_mode;
}
