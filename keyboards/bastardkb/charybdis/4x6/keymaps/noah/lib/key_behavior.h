// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Schema + Lookup Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// Shared schema for key behavior config plus small lookup helpers.
//
// key_config.h authors key behavior through a single key_behaviors[] table and
// also defines key_behavior_count.  This header provides the types, forward
// declarations for that data, and runtime helpers that fold in non-table facts
// such as "is this key an MO() key?".
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

#define KEY_BEHAVIOR_MAX_TAP_COUNT 3

// ─── Hold Tiers ─────────────────────────────────────────────────────────────
//
// A hold tier is an action plus its timing mode.
//
// immediate = true:  fire at threshold and keep registered while held.
// immediate = false: resolve on release based on elapsed time.

typedef struct {
    bool     present;
    uint16_t action;
    bool     immediate;
} hold_behavior_t;

// ─── Per-Tap-Count Behavior ─────────────────────────────────────────────────
//
// Represents what a key does for one tap count.
//
// tap_overrides_default:
//   false — use the key's normal tap behavior for this count
//   true  — use tap_action instead
//
// For tap_count = 1, "normal tap behavior" means:
//   - plain keys: the keycode itself
//   - MO() keys: no tap output unless overridden
//
// For tap_count >= 2, present steps should set tap_overrides_default = true.

typedef struct {
    bool            present;
    bool            tap_overrides_default;
    uint16_t        tap_action;
    hold_behavior_t hold;
    hold_behavior_t longer_hold;
} key_behavior_step_t;

// ─── Key Behavior Config ────────────────────────────────────────────────────
//
// A single authored behavior row for one physical keycode.
//
// steps[0] = single press
// steps[1] = double tap
// steps[2] = triple tap
//
// Unused entries stay zero-initialized.

typedef struct {
    uint16_t            keycode;
    key_behavior_step_t steps[KEY_BEHAVIOR_MAX_TAP_COUNT];
} key_behavior_t;

// ─── Authoring DSL ──────────────────────────────────────────────────────────
//
// Small initializer macros for authoring key_behaviors[] data in key_config.h.
// This keeps the config file declarative while the nested struct layout stays
// encapsulated here.

#define PRESS_AND_HOLD(action_) \
    { .present = true, .action = (action_), .immediate = true }

#define SEND_AFTER_HOLD(action_) \
    { .present = true, .action = (action_), .immediate = false }

#define KEY_TAP_WITH(hold_) \
    { .present = true, .hold = hold_ }

#define KEY_TAP_WITH_LONG(hold_, long_hold_) \
    {                                         \
        .present     = true,                  \
        .hold        = hold_,                 \
        .longer_hold = long_hold_,            \
    }

#define TAP_AS(tap_action_) \
    { .present = true, .tap_overrides_default = true, .tap_action = (tap_action_) }

#define TAP_AS_WITH(tap_action_, hold_) \
    {                                    \
        .present               = true,   \
        .tap_overrides_default = true,   \
        .tap_action            = (tap_action_), \
        .hold                  = hold_,  \
    }

#define TAP_AS_WITH_LONG(tap_action_, hold_, long_hold_) \
    {                                                     \
        .present               = true,                    \
        .tap_overrides_default = true,                    \
        .tap_action            = (tap_action_),           \
        .hold                  = hold_,                   \
        .longer_hold           = long_hold_,              \
    }

// key_config.h defines these with internal linkage.
static const key_behavior_t key_behaviors[];
static const uint8_t        key_behavior_count;

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
    for (uint8_t i = 0; i < key_behavior_count; i++)
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
