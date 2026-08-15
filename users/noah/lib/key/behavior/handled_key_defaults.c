// ────────────────────────────────────────────────────────────────────────────
// Handled Key Defaults
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key_internal.h"

__attribute__((weak)) bool key_behavior_future_tap_path_has_foreign_pd_mode(uint16_t keycode, uint8_t count, pd_mode_mask_t base_mode) {
    (void)keycode;
    (void)count;
    (void)base_mode;
    return false;
}

uint8_t behavior_get_layer(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode) ? QK_LAYER_TAP_GET_LAYER(keycode) : QK_MOMENTARY_GET_LAYER(keycode);
}

bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

pd_mode_mask_t handled_key_pd_mode_for_behavior(key_behavior_view_t behavior) {
    return pd_mode_for_keycode(behavior.keycode);
}

bool handled_key_resolution_step_present(const handled_key_resolution_t *resolution) {
    return resolution && key_behavior_step_present(resolution->step);
}

static bool handled_key_resolution_uses_buffered_modifier_single_step(const handled_key_resolution_t *resolution) {
    noah_action_desc_t desc;

    if (!resolution) {
        return false;
    }

    desc = noah_action_describe(resolution->keycode);

    if (handled_key_resolution_step_present(resolution) || (resolution->flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0u) {
        return false;
    }

    return (resolution->flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0u && noah_action_desc_is_pure_modifier_literal(desc);
}

bool handled_key_resolution_uses_fallback_hold_behavior(const handled_key_resolution_t *resolution) {
    if (!resolution || resolution->tap_count != 1) {
        return false;
    }

    if (!noah_action_desc_supports_fallback_hold(noah_action_describe(resolution->keycode))) {
        return false;
    }

    if (resolution->step.hold.present || resolution->step.long_hold.present) {
        return false;
    }

    return resolution->step.tap.present || (resolution->flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0u;
}

bool handled_key_resolution_uses_deferred_stacked_pd_hold(const handled_key_resolution_t *resolution) {
    return resolution && resolution->tap_count == 1 && resolution->pd_mode != 0 && resolution->step.tap.present && key_behavior_future_tap_path_has_foreign_pd_mode(resolution->keycode, resolution->tap_count, resolution->pd_mode);
}

static uint16_t handled_key_default_tap_action(const handled_key_resolution_t *resolution) {
    return resolution ? noah_action_desc_default_tap_action(noah_action_describe(resolution->keycode)) : KC_NO;
}

static uint16_t handled_key_single_tap_action(const handled_key_resolution_t *resolution) {
    if (!resolution) {
        return KC_NO;
    }

    if (resolution->step.tap.present) {
        return resolution->step.tap.action;
    }

    if (handled_key_resolution_uses_buffered_modifier_single_step(resolution)) {
        return KC_NO;
    }

    return handled_key_default_tap_action(resolution);
}

hold_behavior_t handled_key_hold_behavior(const handled_key_resolution_t *resolution) {
    if (!resolution) {
        return hold_behavior_none();
    }

    if (resolution->step.hold.present) {
        return resolution->step.hold;
    }

    if (resolution->tap_count == 1 && resolution->pd_mode != 0 && !handled_key_resolution_uses_deferred_stacked_pd_hold(resolution)) {
        return (hold_behavior_t){
            .present = true,
            .action  = resolution->keycode,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        };
    }

    if (handled_key_resolution_uses_deferred_stacked_pd_hold(resolution)) {
        // Keep stacked PD modes out of the immediate activation path while the
        // tap-count branch is unresolved.
        return (hold_behavior_t){
            .present = true,
            .action  = resolution->keycode,
            .mode    = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE,
        };
    }

    return hold_behavior_none();
}

uint16_t handled_key_tap_action_behavior(const handled_key_resolution_t *resolution) {
    if (!resolution) {
        return KC_NO;
    }

    if (resolution->tap_count == 1) {
        return handled_key_single_tap_action(resolution);
    }

    if (resolution->step.tap.present) {
        return resolution->step.tap.action;
    }

    return handled_key_single_tap_action(resolution);
}

uint8_t handled_key_tap_repeat_count_behavior(const handled_key_resolution_t *resolution, uint16_t tap_action) {
    if (tap_action == KC_NO) {
        return 0;
    }

    if (!resolution || resolution->tap_count == 1 || resolution->step.tap.present) {
        return 1;
    }

    return resolution->tap_count;
}

key_runtime_slot_hold_strategy_t handled_key_hold_strategy_behavior(const handled_key_resolution_t *resolution) {
    if (resolution && resolution->tap_count == 1 && resolution->pd_mode != 0 && !handled_key_resolution_uses_deferred_stacked_pd_hold(resolution)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    }

    if (handled_key_resolution_uses_fallback_hold_behavior(resolution)) {
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

bool handled_key_tap_resolves_on_press_behavior(const handled_key_resolution_t *resolution) {
    (void)resolution;
    // Keep the selected tap branch visible through the normal pending window
    // so feedback can show the branch before any optional tap-commit pulse.
    return false;
}
