// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Types
// ────────────────────────────────────────────────────────────────────────────
//
// Struct definitions used by the behavior config in key_config.h.
// Processing logic that reads these types lives in key_behavior.h,
// multi_tap.h, and keymap.c.
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

// ─── Mode Tap Overrides ─────────────────────────────────────────────────────
//
// By default, tapping a pointing device mode key sends whatever is at that
// position on LAYER_BASE.  Use this to override that default.

typedef struct {
    uint16_t keycode;
    uint16_t tap;
} mode_tap_override_t;
