// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Lookup
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime lookup helpers for authored key behavior rows.
// ────────────────────────────────────────────────────────────────────────────

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
        .handled            = config || is_mo,
        .is_momentary_layer = is_mo || custom_lt,
        .is_layer_tap       = custom_lt,
        .has_multi_tap      = key_behavior_has_multi_tap_in_config(config),
        .tap_hold_term      = tap_term,
        .longer_hold_term   = longer_term,
        .multi_tap_term     = multi_term,
        .single             = config ? config->tap_counts[0] : key_behavior_step_none(),
    };
}
