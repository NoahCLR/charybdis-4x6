// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Schema
// ────────────────────────────────────────────────────────────────────────────
//
// Shared schema for key behavior config and authoring helpers.
//
// keymap.c authors key behavior through a single key_behaviors[] table and
// materializes key_behavior_count alongside the other derived keymap symbols.
// Runtime interpretation helpers live in
// key_behavior_lookup.h / key_behavior_lookup.c.
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
// A single authored behavior row for one keycode.
//
// tap_counts[0] = single press
// tap_counts[1] = double tap
// tap_counts[2] = triple tap
// tap_counts[3] = quadruple tap
// tap_counts[4] = quintuple tap
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

#define TAP_SENDS(action_) {.present = true, .action = (action_)}

#define PRESS_AND_HOLD_UNTIL_RELEASE(action_) {.present = true, .action = (action_), .mode = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE}

#define TAP_AT_HOLD_THRESHOLD(action_) {.present = true, .action = (action_), .mode = HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD}

#define TAP_ON_RELEASE_AFTER_HOLD(action_) {.present = true, .action = (action_), .mode = HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD}

// Defined once in keymap.c.
extern const key_behavior_t key_behaviors[];
extern const uint8_t        key_behavior_count;

static inline hold_behavior_t hold_behavior_none(void) {
    return (hold_behavior_t){0};
}

static inline bool hold_fires_at_threshold(hold_behavior_t hold) {
    return hold.present && (hold.mode == HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE || hold.mode == HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD);
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
