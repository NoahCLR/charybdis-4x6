// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Lookup
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime lookup helpers for authored key behavior rows.
// ────────────────────────────────────────────────────────────────────────────

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "../action/action_dispatch.h"
#include "../pointing/pd_modes.h"
#include "key_behavior_lookup.h"

static const key_behavior_t *key_behavior_config_lookup(uint16_t keycode) {
    for (uint8_t i = 0; i < key_behavior_count; i++) {
        if (key_behaviors[i].keycode == keycode) return &key_behaviors[i];
    }

    return NULL;
}

static key_behavior_step_t key_behavior_step_lookup_in_config(const key_behavior_t *config, uint8_t tap_count) {
    if (!config || tap_count == 0 || tap_count > KEY_BEHAVIOR_MAX_TAP_COUNT) {
        return key_behavior_step_none();
    }

    return config->tap_counts[tap_count - 1];
}

static bool key_behavior_has_more_taps_in_config(const key_behavior_t *config, uint8_t count) {
    if (!config || count >= KEY_BEHAVIOR_MAX_TAP_COUNT) return false;

    for (uint8_t i = count; i < KEY_BEHAVIOR_MAX_TAP_COUNT; i++) {
        if (key_behavior_step_present(config->tap_counts[i])) return true;
    }

    return false;
}

static bool key_behavior_has_multi_tap_in_config(const key_behavior_t *config) {
    return key_behavior_has_more_taps_in_config(config, 1);
}

static bool key_behavior_keycode_supported(uint16_t keycode) {
    if (!action_dispatch_is_raw_qmk_layer_action(keycode)) {
        return true;
    }

    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static bool key_behavior_action_supported(uint16_t action, hold_behavior_mode_t hold_mode) {
    // All non-layer actions stay valid authored surfaces. The runtime only
    // rejects raw QMK layer actions here, because layer ownership is the one
    // area where bypassing userspace is always incorrect. Held QK_MODS actions
    // remain supported and are decomposed by the shared owned_keycode.c helper.
    if (!action_dispatch_is_raw_qmk_layer_action(action)) {
        return true;
    }

    return IS_QK_MOMENTARY(action) && hold_mode == HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE;
}

static void key_behavior_log_invalid_keycode(uint8_t index, uint16_t keycode) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported key_behaviors[%u].keycode 0x%04X; only MO(...) and LT(...) layer keycodes are supported in the custom runtime\n", (unsigned int)index, (unsigned int)keycode);
#else
    (void)index;
    (void)keycode;
#endif
}

static void key_behavior_log_invalid_action(uint8_t index, uint8_t tap_count, const char *field, uint16_t action, hold_behavior_mode_t hold_mode) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported key_behaviors[%u].tap_counts[%u].%s action 0x%04X; raw QMK layer actions are only supported as PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer))\n", (unsigned int)index, (unsigned int)tap_count, field, (unsigned int)action);
#else
    (void)index;
    (void)tap_count;
    (void)field;
    (void)action;
    (void)hold_mode;
#endif
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    return key_behavior_step_lookup_in_config(key_behavior_config_lookup(keycode), tap_count);
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    return key_behavior_has_more_taps_in_config(key_behavior_config_lookup(keycode), count);
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    const key_behavior_t *config    = key_behavior_config_lookup(keycode);
    bool                  is_mo     = IS_QK_MOMENTARY(keycode);
    bool                  is_lt     = IS_QK_LAYER_TAP(keycode);
    bool                  is_pd_mode = pd_mode_for_keycode(keycode) != 0;
    bool                  custom_lt = is_lt && config;

    uint16_t tap_term = CUSTOM_TAP_HOLD_TERM;
    if (config && config->tap_hold_term) {
        tap_term = config->tap_hold_term;
    } else if (custom_lt) {
        tap_term = TAPPING_TERM;
    }

    uint16_t longer_term = config && config->longer_hold_term ? config->longer_hold_term : CUSTOM_LONGER_HOLD_TERM;
    uint16_t multi_term  = config && config->multi_tap_term ? config->multi_tap_term : CUSTOM_MULTI_TAP_TERM;

    return (key_behavior_view_t){
        .config             = config,
        .keycode            = keycode,
        .handled            = config || is_mo || is_pd_mode,
        .is_momentary_layer = is_mo || custom_lt,
        .is_layer_tap       = custom_lt,
        .has_multi_tap      = key_behavior_has_multi_tap_in_config(config),
        .tap_hold_term      = tap_term,
        .longer_hold_term   = longer_term,
        .multi_tap_term     = multi_term,
        .single             = config ? config->tap_counts[0] : key_behavior_step_none(),
    };
}

void key_behavior_validate_all(void) {
    for (uint8_t i = 0; i < key_behavior_count; i++) {
        const key_behavior_t *config = &key_behaviors[i];

        if (!key_behavior_keycode_supported(config->keycode)) {
            key_behavior_log_invalid_keycode(i, config->keycode);
        }

        for (uint8_t tap_index = 0; tap_index < KEY_BEHAVIOR_MAX_TAP_COUNT; tap_index++) {
            key_behavior_step_t step = config->tap_counts[tap_index];

            if (step.tap.present && !key_behavior_action_supported(step.tap.action, HOLD_BEHAVIOR_NONE)) {
                key_behavior_log_invalid_action(i, tap_index, "tap", step.tap.action, HOLD_BEHAVIOR_NONE);
            }

            if (step.hold.present && !key_behavior_action_supported(step.hold.action, step.hold.mode)) {
                key_behavior_log_invalid_action(i, tap_index, "hold", step.hold.action, step.hold.mode);
            }

            if (step.long_hold.present && !key_behavior_action_supported(step.long_hold.action, step.long_hold.mode)) {
                key_behavior_log_invalid_action(i, tap_index, "long_hold", step.long_hold.action, step.long_hold.mode);
            }
        }
    }
}
