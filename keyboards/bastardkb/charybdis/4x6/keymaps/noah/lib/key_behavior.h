// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Lookup Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// key_config.h now authors key behavior through a single key_behaviors[]
// table.  This header provides small lookup helpers and a runtime view that
// folds in non-table facts such as "is this key an MO() key?".
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

#include "key_types.h"

#define KEY_BEHAVIOR_COUNT (sizeof(key_behaviors) / sizeof(key_behaviors[0]))

typedef struct {
    const key_behavior_t *config;
    uint16_t              keycode;
    bool                  handled;
    bool                  is_momentary_layer;
    bool                  has_multi_tap;
    key_behavior_step_t   single;
} key_behavior_view_t;

static inline hold_behavior_t hold_behavior_none(void) {
    return (hold_behavior_t){0};
}

static inline key_behavior_step_t key_behavior_step_none(void) {
    return (key_behavior_step_t){0};
}

static inline const key_behavior_t *key_behavior_config_lookup(uint16_t keycode) {
    for (uint8_t i = 0; i < KEY_BEHAVIOR_COUNT; i++)
        if (key_behaviors[i].keycode == keycode) return &key_behaviors[i];
    return NULL;
}

static inline key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    const key_behavior_t *config = key_behavior_config_lookup(keycode);

    if (!config || tap_count == 0 || tap_count > KEY_BEHAVIOR_MAX_TAP_COUNT) {
        return key_behavior_step_none();
    }

    return config->steps[tap_count - 1];
}

static inline bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    const key_behavior_t *config = key_behavior_config_lookup(keycode);

    if (!config || count >= KEY_BEHAVIOR_MAX_TAP_COUNT) return false;

    for (uint8_t i = count; i < KEY_BEHAVIOR_MAX_TAP_COUNT; i++)
        if (config->steps[i].present) return true;

    return false;
}

static inline bool key_behavior_has_multi_tap(uint16_t keycode) {
    return key_behavior_has_more_taps(keycode, 1);
}

static inline key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    const key_behavior_t *config = key_behavior_config_lookup(keycode);
    bool                  is_mo  = IS_QK_MOMENTARY(keycode);

    return (key_behavior_view_t){
        .config             = config,
        .keycode            = keycode,
        .handled            = config || is_mo,
        .is_momentary_layer = is_mo,
        .has_multi_tap      = config ? key_behavior_has_multi_tap(keycode) : false,
        .single             = config ? config->steps[0] : key_behavior_step_none(),
    };
}
