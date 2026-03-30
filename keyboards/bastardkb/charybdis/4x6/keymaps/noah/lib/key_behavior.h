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

// ─── Hold Tiers ─────────────────────────────────────────────────────────────
//
// A hold tier is an action plus its timing mode.
//
// PRESS_AND_HOLD_UNTIL_RELEASE:
//   fire at threshold and keep registered while held.
//
// TAP_AT_HOLD_THRESHOLD:
//   fire once at threshold.
//
// TAP_ON_RELEASE_AFTER_HOLD:
//   wait until release, then fire once.

typedef enum {
    HOLD_BEHAVIOR_NONE = 0,
    HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE,
    HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD,
    HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD,
} hold_behavior_mode_t;

typedef struct {
    bool                 present;
    uint16_t             action;
    hold_behavior_mode_t mode;
} hold_behavior_t;

// ─── Tap Behavior ───────────────────────────────────────────────────────────
//
// Tap can either use the key's normal tap behavior or send a different action.
//
// present = false:
//   Use the key's normal tap behavior for this step.
//
// present = true:
//   Send action instead.

typedef struct {
    bool     present;
    uint16_t action;
} tap_behavior_t;

// ─── Per-Tap-Count Behavior ─────────────────────────────────────────────────
//
// Represents what a key does for one tap count.
//
// tap:
//   What happens on a quick release.
//   If omitted, the key keeps its normal tap behavior for that step.
//
// hold:
//   What happens after the first hold threshold.
//
// long_hold:
//   What happens after the longer hold threshold.

typedef struct {
    tap_behavior_t  tap;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
} key_behavior_step_t;

// ─── Key Behavior Config ────────────────────────────────────────────────────
//
// A single authored behavior row for one physical keycode.
//
// tap_counts[0] = single press
// tap_counts[1] = double tap
// tap_counts[2] = triple tap
//
// Unused entries stay zero-initialized.

typedef struct {
    uint16_t            keycode;
    uint16_t            tap_hold_term;    // 0 = TAPPING_TERM for LT(), CUSTOM_TAP_HOLD_TERM otherwise
    uint16_t            longer_hold_term; // 0 = CUSTOM_LONGER_HOLD_TERM
    uint16_t            multi_tap_term;   // 0 = CUSTOM_MULTI_TAP_TERM
    key_behavior_step_t tap_counts[KEY_BEHAVIOR_MAX_TAP_COUNT];
} key_behavior_t;

// ─── Authoring Helpers ──────────────────────────────────────────────────────
//
// Small initializer macros for the nested tap/hold values used in
// key_behaviors[].

#define TAP_SENDS(action_) \
    { .present = true, .action = (action_) }

#define PRESS_AND_HOLD_UNTIL_RELEASE(action_) \
    { .present = true, .action = (action_), .mode = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE }

#define TAP_AT_HOLD_THRESHOLD(action_) \
    { .present = true, .action = (action_), .mode = HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD }

#define TAP_ON_RELEASE_AFTER_HOLD(action_) \
    { .present = true, .action = (action_), .mode = HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD }

// key_config.h defines these with internal linkage.
static const key_behavior_t key_behaviors[];
static const uint8_t        key_behavior_count;

typedef struct {
    const key_behavior_t *config;
    uint16_t              keycode;
    bool                  handled;
    bool                  is_momentary_layer; // MO() or LT() — engine handles layer_on/off
    bool                  is_layer_tap;       // specifically LT() — has embedded tap key
    bool                  has_multi_tap;
    uint16_t              tap_hold_term;      // resolved: per-key → TAPPING_TERM for LT → CUSTOM_TAP_HOLD_TERM
    uint16_t              longer_hold_term;   // resolved: per-key → CUSTOM_LONGER_HOLD_TERM
    uint16_t              multi_tap_term;     // resolved: per-key → CUSTOM_MULTI_TAP_TERM
    key_behavior_step_t   single;
} key_behavior_view_t;

static inline hold_behavior_t hold_behavior_none(void) {
    return (hold_behavior_t){0};
}

static inline bool hold_fires_at_threshold(hold_behavior_t hold) {
    return hold.present &&
           (hold.mode == HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE ||
            hold.mode == HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD);
}

static inline bool hold_registers_while_held(hold_behavior_t hold) {
    return hold.present && hold.mode == HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE;
}

static inline bool hold_sends_on_release(hold_behavior_t hold) {
    return hold.present && hold.mode == HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD;
}

static inline tap_behavior_t tap_behavior_none(void) {
    return (tap_behavior_t){0};
}

static inline key_behavior_step_t key_behavior_step_none(void) {
    return (key_behavior_step_t){0};
}

static inline bool key_behavior_step_present(key_behavior_step_t step) {
    return step.tap.present || step.hold.present || step.long_hold.present;
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

    return config->tap_counts[tap_count - 1];
}

static inline bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    const key_behavior_t *config = key_behavior_config_lookup(keycode);

    if (!config || count >= KEY_BEHAVIOR_MAX_TAP_COUNT) return false;

    for (uint8_t i = count; i < KEY_BEHAVIOR_MAX_TAP_COUNT; i++)
        if (key_behavior_step_present(config->tap_counts[i])) return true;

    return false;
}

static inline bool key_behavior_has_multi_tap(uint16_t keycode) {
    return key_behavior_has_more_taps(keycode, 1);
}

static inline key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    const key_behavior_t *config = key_behavior_config_lookup(keycode);
    bool                  is_mo  = IS_QK_MOMENTARY(keycode);
    bool                  is_lt  = IS_QK_LAYER_TAP(keycode);

    uint16_t tap_term    = CUSTOM_TAP_HOLD_TERM;
    if      (config && config->tap_hold_term)    tap_term    = config->tap_hold_term;
    else if (is_lt)                              tap_term    = TAPPING_TERM;

    uint16_t longer_term = config && config->longer_hold_term ? config->longer_hold_term : CUSTOM_LONGER_HOLD_TERM;
    uint16_t multi_term  = config && config->multi_tap_term   ? config->multi_tap_term   : CUSTOM_MULTI_TAP_TERM;

    return (key_behavior_view_t){
        .config             = config,
        .keycode            = keycode,
        .handled            = config || is_mo || is_lt,
        .is_momentary_layer = is_mo || is_lt,
        .is_layer_tap       = is_lt,
        .has_multi_tap      = config ? key_behavior_has_multi_tap(keycode) : false,
        .tap_hold_term      = tap_term,
        .longer_hold_term   = longer_term,
        .multi_tap_term     = multi_term,
        .single             = config ? config->tap_counts[0] : key_behavior_step_none(),
    };
}
