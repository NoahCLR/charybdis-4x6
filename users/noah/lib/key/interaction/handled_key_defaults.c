// ────────────────────────────────────────────────────────────────────────────
// Handled Key Defaults
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key_internal.h"

#include "../../pointing/defs/pd_modes.h"

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

pd_mode_mask_t handled_key_pd_mode_for_behavior(key_behavior_view_t behavior) {
    return pd_mode_for_keycode(behavior.keycode);
}

bool handled_key_resolution_step_present(handled_key_resolution_t resolution) {
    return key_behavior_step_present(resolution.step);
}

static bool handled_key_resolution_uses_buffered_modifier_single_step(handled_key_resolution_t resolution) {
    if (handled_key_resolution_step_present(resolution) || handled_key_resolution_is_momentary_layer(resolution)) {
        return false;
    }

    return handled_key_resolution_has_multi_tap(resolution) && resolution.keycode < SAFE_RANGE && handled_key_keycode_is_pure_modifier(resolution.keycode);
}

bool handled_key_resolution_uses_fallback_hold_behavior(handled_key_resolution_t resolution) {
    noah_action_desc_t desc;

    if (resolution.tap_count != 1) {
        return false;
    }

    if (handled_key_resolution_is_momentary_layer(resolution) || resolution.keycode >= SAFE_RANGE) {
        return false;
    }

    desc = noah_action_describe(resolution.keycode);
    if (noah_action_desc_is_qmk_behavior_keycode(desc)) {
        return false;
    }

    if (resolution.step.hold.present || resolution.step.long_hold.present) {
        return false;
    }

    return resolution.step.tap.present || handled_key_resolution_has_multi_tap(resolution);
}

static uint16_t handled_key_default_tap_action(handled_key_resolution_t resolution) {
    noah_action_desc_t desc = noah_action_describe(resolution.keycode);

    if (resolution.pd_mode != 0) {
        return KC_NO;
    }

    if (noah_action_desc_is_layer_tap(desc)) {
        return QK_LAYER_TAP_GET_TAP_KEYCODE(resolution.keycode);
    }

    if (handled_key_resolution_is_momentary_layer(resolution) || resolution.keycode >= SAFE_RANGE) {
        return KC_NO;
    }

    return resolution.keycode;
}

static uint16_t handled_key_single_tap_action(handled_key_resolution_t resolution) {
    if (resolution.step.tap.present) {
        return resolution.step.tap.action;
    }

    if (handled_key_resolution_uses_buffered_modifier_single_step(resolution)) {
        return KC_NO;
    }

    return handled_key_default_tap_action(resolution);
}

hold_behavior_t handled_key_hold_behavior(handled_key_resolution_t resolution) {
    if (resolution.step.hold.present) {
        return resolution.step.hold;
    }

    if (handled_key_resolution_uses_implicit_hold(resolution)) {
        return (hold_behavior_t){
            .present = true,
            .action  = resolution.keycode,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        };
    }

    return hold_behavior_none();
}

uint16_t handled_key_tap_action_behavior(handled_key_resolution_t resolution) {
    if (resolution.tap_count == 1) {
        return handled_key_single_tap_action(resolution);
    }

    if (resolution.step.tap.present) {
        return resolution.step.tap.action;
    }

    return handled_key_single_tap_action(resolution);
}

uint8_t handled_key_tap_repeat_count_behavior(handled_key_resolution_t resolution, uint16_t tap_action) {
    if (tap_action == KC_NO) {
        return 0;
    }

    if (resolution.tap_count == 1 || resolution.step.tap.present) {
        return 1;
    }

    return resolution.tap_count;
}

key_runtime_slot_hold_strategy_t handled_key_hold_strategy_behavior(handled_key_resolution_t resolution) {
    if (handled_key_resolution_uses_implicit_hold(resolution)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    }

    if (handled_key_resolution_uses_fallback_hold(resolution)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK;
    }

    return KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

uint16_t handled_key_flags_from_behavior(key_behavior_view_t behavior) {
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

    return flags;
}

bool handled_key_tap_resolves_on_press_behavior(handled_key_resolution_t resolution) {
    return resolution.tap_count > 1 && handled_key_resolution_step_present(resolution) && !resolution.step.hold.present && !resolution.step.long_hold.present && !resolution.has_more_taps;
}
