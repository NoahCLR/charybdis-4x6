// ────────────────────────────────────────────────────────────────────────────
// Handled Key Resolution
// ────────────────────────────────────────────────────────────────────────────
//
// Resolve authored key_behavior rows into the handled-key contract consumed by
// the runtime press/release engine.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"

#include "../../action/action_dispatch.h"
#include "../../pointing/defs/pd_modes.h"
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

static bool handled_key_uses_implicit_hold_behavior(key_behavior_view_t behavior, uint8_t tap_count) {
    return tap_count == 1 && handled_key_pd_mode_for_behavior(behavior) != 0;
}

static bool handled_key_uses_fallback_hold_behavior(key_behavior_view_t behavior, uint8_t tap_count) {
    noah_action_desc_t desc;

    if (tap_count != 1) {
        return false;
    }

    if (behavior.is_momentary_layer || behavior.keycode >= SAFE_RANGE) {
        return false;
    }

    desc = noah_action_describe(behavior.keycode);
    if (noah_action_desc_is_qmk_behavior_keycode(desc)) {
        return false;
    }

    if (behavior.single.hold.present || behavior.single.long_hold.present) {
        return false;
    }

    return behavior.single.tap.present || behavior.has_multi_tap;
}

static uint16_t handled_key_default_tap_action_behavior(key_behavior_view_t behavior) {
    if (handled_key_pd_mode_for_behavior(behavior) != 0) {
        return KC_NO;
    }

    if (behavior.is_layer_tap) {
        return QK_LAYER_TAP_GET_TAP_KEYCODE(behavior.keycode);
    }

    if (behavior.is_momentary_layer || behavior.keycode >= SAFE_RANGE) {
        return KC_NO;
    }

    return behavior.keycode;
}

static uint16_t handled_key_single_tap_action_behavior(key_behavior_view_t behavior) {
    if (behavior.single.tap.present) {
        return behavior.single.tap.action;
    }

    if (handled_key_uses_buffered_modifier_single_step(behavior)) {
        return KC_NO;
    }

    return handled_key_default_tap_action_behavior(behavior);
}

static hold_behavior_t handled_key_hold_behavior(key_behavior_view_t behavior, key_behavior_step_t step, uint8_t tap_count) {
    if (step.hold.present) {
        return step.hold;
    }

    if (handled_key_uses_implicit_hold_behavior(behavior, tap_count)) {
        return (hold_behavior_t){
            .present = true,
            .action  = behavior.keycode,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        };
    }

    return hold_behavior_none();
}

static uint16_t handled_key_tap_action_behavior(key_behavior_view_t behavior, key_behavior_step_t step, uint8_t tap_count) {
    if (tap_count == 1) {
        return handled_key_single_tap_action_behavior(behavior);
    }

    if (step.tap.present) {
        return step.tap.action;
    }

    return handled_key_single_tap_action_behavior(behavior);
}

static uint8_t handled_key_tap_repeat_count_behavior(key_behavior_view_t behavior, key_behavior_step_t step, uint8_t tap_count, uint16_t tap_action) {
    if (tap_action == KC_NO) {
        return 0;
    }

    if (tap_count == 1 || step.tap.present) {
        return 1;
    }

    return tap_count;
}

static key_runtime_slot_hold_strategy_t handled_key_hold_strategy_behavior(key_behavior_view_t behavior, uint8_t tap_count) {
    if (handled_key_uses_implicit_hold_behavior(behavior, tap_count)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    }

    if (handled_key_uses_fallback_hold_behavior(behavior, tap_count)) {
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
    if (handled_key_uses_implicit_hold_behavior(behavior, 1)) {
        flags |= HANDLED_KEY_FLAG_IMPLICIT_HOLD;
    }
    if (handled_key_uses_fallback_hold_behavior(behavior, 1)) {
        flags |= HANDLED_KEY_FLAG_FALLBACK_HOLD;
    }

    return flags;
}

static bool handled_key_tap_resolves_on_press(key_behavior_step_t step, uint8_t tap_count, bool has_more_taps) {
    return tap_count > 1 && key_behavior_step_present(step) && !step.hold.present && !step.long_hold.present && !has_more_taps;
}

handled_key_view_t handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count) {
    key_behavior_view_t behavior = key_behavior_lookup(keycode);
    key_behavior_step_t step     = tap_count <= 1 ? behavior.single : key_behavior_step_lookup(keycode, tap_count);
    uint16_t            tap      = handled_key_tap_action_behavior(behavior, step, tap_count);
    bool                more     = key_behavior_has_more_taps(keycode, tap_count);
    bool                present  = key_behavior_step_present(step);

    return (handled_key_view_t){
        .tap_action       = tap,
        .tap_repeat_count = handled_key_tap_repeat_count_behavior(behavior, step, tap_count, tap),
        .hold             = handled_key_hold_behavior(behavior, step, tap_count),
        .long_hold        = step.long_hold,
        .hold_strategy    = handled_key_hold_strategy_behavior(behavior, tap_count),
        .tap_hold_term    = behavior.tap_hold_term,
        .longer_hold_term = behavior.longer_hold_term,
        .multi_tap_term   = behavior.multi_tap_term,
        .layer            = behavior.is_momentary_layer ? behavior_get_layer(behavior.keycode) : UINT8_MAX,
        .pd_mode          = handled_key_pd_mode_for_behavior(behavior),
        .step_present     = present,
        .has_more_taps    = more,
        .tap_resolves_on_press = handled_key_tap_resolves_on_press(step, tap_count, more),
        .flags            = handled_key_flags_from_behavior(behavior),
    };
}

handled_key_view_t handled_key_lookup(uint16_t keycode) {
    return handled_key_lookup_tap_count(keycode, 1);
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

uint8_t handled_key_tap_repeat_count(handled_key_view_t key) {
    return key.tap_repeat_count;
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
