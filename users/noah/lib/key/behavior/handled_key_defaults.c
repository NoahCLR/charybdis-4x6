// ────────────────────────────────────────────────────────────────────────────
// Handled Key Defaults
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key_internal.h"

#include "noah_keymap_ids.h"
#include "../../action/custom_key_shortcut.h"

__attribute__((weak)) key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    (void)keycode;
    (void)tap_count;
    return key_behavior_step_none();
}

__attribute__((weak)) bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    (void)keycode;
    (void)count;
    return false;
}

__attribute__((weak)) bool key_behavior_future_tap_path_has_foreign_pointing_mode(uint16_t keycode, uint8_t count, pointing_mode_mask_t base_mode) {
    (void)keycode;
    (void)count;
    (void)base_mode;
    return false;
}

__attribute__((weak)) key_behavior_step_t key_behavior_view_step(const key_behavior_view_t *behavior, uint8_t tap_count) {
    return behavior ? key_behavior_step_lookup(behavior->keycode, tap_count) : key_behavior_step_none();
}

__attribute__((weak)) bool key_behavior_view_has_more_taps(const key_behavior_view_t *behavior, uint8_t count) {
    return behavior && key_behavior_has_more_taps(behavior->keycode, count);
}

uint8_t behavior_get_layer(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode) ? QK_LAYER_TAP_GET_LAYER(keycode) : noah_action_keycode_momentary_layer(keycode);
}

bool is_layer_key(uint16_t keycode) {
    return noah_action_keycode_is_owned_momentary_layer(keycode) || IS_QK_LAYER_TAP(keycode);
}

pointing_mode_mask_t handled_key_pointing_mode_for_behavior(key_behavior_view_t behavior) {
    return pointing_mode_for_keycode(behavior.keycode);
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
    return resolution && resolution->tap_count == 1 && resolution->pointing_mode != 0 && resolution->step.tap.present && key_behavior_future_tap_path_has_foreign_pointing_mode(resolution->keycode, resolution->tap_count, resolution->pointing_mode);
}

// A pressed custom key with a shortcut and no authored row of its own, or a
// bypassed one, is a native key (D-F26): the runtime holds its shortcut from
// the press until the release, so a quick tap is a press and a release, never
// a second tap after it. Read through a transparent cell it is the lower key's
// keycode, as an ordinary key is: its shortcut is the tap, and its hold is the
// fallback hold.
static bool handled_key_resolution_holds_custom_shortcut_natively(const handled_key_resolution_t *resolution) {
    return resolution && resolution->tap_count == 1 && resolution->authored_tap_depth == 0u && !resolution->step.hold.present && (resolution->flags & HANDLED_KEY_FLAG_TRANSPARENT_SOURCE) == 0u && NOAH_KEYCODE_IS_CUSTOM_KEY(resolution->keycode) && noah_custom_key_has_shortcut(resolution->keycode);
}

static uint16_t handled_key_default_tap_action(const handled_key_resolution_t *resolution) {
    if (handled_key_resolution_holds_custom_shortcut_natively(resolution)) {
        return KC_NO;
    }
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

    if (resolution->tap_count == 1 && resolution->pointing_mode != 0 && !handled_key_resolution_uses_deferred_stacked_pd_hold(resolution)) {
        return (hold_behavior_t){
            .present = true,
            .action  = resolution->keycode,
            .style   = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        };
    }

    if (handled_key_resolution_holds_custom_shortcut_natively(resolution)) {
        return (hold_behavior_t){
            .present = true,
            .action  = resolution->keycode,
            .style   = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        };
    }

    // LM(n, mods) holds its modifiers from the press, as QMK's LM() does; the
    // layer is the key's own momentary hold.
    if (resolution->tap_count == 1 && noah_action_keycode_is_layer_mod(resolution->keycode)) {
        return (hold_behavior_t){
            .present = true,
            .action  = noah_action_keycode_layer_mod_mods(resolution->keycode),
            .style   = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        };
    }

    if (resolution->tap_count == 1 && noah_action_keycode_dual_role_hold(resolution->keycode) != KC_NO) {
        return (hold_behavior_t){
            .present = true,
            .action  = noah_action_keycode_dual_role_hold(resolution->keycode),
            .style   = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE,
        };
    }

    if (handled_key_resolution_uses_deferred_stacked_pd_hold(resolution)) {
        // Keep stacked pointing modes out of the immediate activation path
        // while the tap-count branch is unresolved.
        return (hold_behavior_t){
            .present = true,
            .action  = resolution->keycode,
            .style   = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE,
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

    // An empty tap at a later count repeats the first tap: the one the row sets
    // there, or the key's own (D-F33).
    if (resolution->first_tap.present) {
        return resolution->first_tap.action;
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
    if (resolution && resolution->tap_count == 1 && resolution->pointing_mode != 0 && !handled_key_resolution_uses_deferred_stacked_pd_hold(resolution)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    }
    if (resolution && resolution->tap_count == 1 && !resolution->step.hold.present && noah_action_keycode_is_layer_mod(resolution->keycode)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    }
    if (handled_key_resolution_holds_custom_shortcut_natively(resolution)) {
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
