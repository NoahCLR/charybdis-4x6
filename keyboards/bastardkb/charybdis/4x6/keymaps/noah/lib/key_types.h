// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Types
// ────────────────────────────────────────────────────────────────────────────
//
// Struct definitions used by the config tables in key_config.h.
// Processing logic that reads these tables lives in keymap.c.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

// ─── Hold Keys ──────────────────────────────────────────────────────────────
//
// On tap (< CUSTOM_TAP_HOLD_TERM), sends the keycode itself.
// On hold (>= CUSTOM_TAP_HOLD_TERM), sends the hold keycode.
//
// immediate = true:  hold fires at threshold without waiting for release.
// immediate = false: action is determined on release based on elapsed time.

typedef struct {
    uint16_t keycode;
    uint16_t hold;
    bool     immediate;
} hold_key_t;

// ─── Longer Hold Keys ───────────────────────────────────────────────────────
//
// Third-tier action when held past CUSTOM_LONGER_HOLD_TERM.
// The key must also appear in hold_keys for the base hold behavior.
//
// immediate = true:  longer hold fires at threshold without waiting for release.
// immediate = false: action is determined on release based on elapsed time.

typedef struct {
    uint16_t keycode;
    uint16_t longer_hold;
    bool     immediate;
} longer_hold_key_t;

// ─── Double-Tap Keys ────────────────────────────────────────────────────────
//
// Sends an action when tapped twice within CUSTOM_DOUBLE_TAP_TERM.
// Note: adds a small delay to single taps (waiting for potential second press).

typedef struct {
    uint16_t keycode;
    uint16_t action;
} double_tap_key_t;

// ─── Triple-Tap Keys ────────────────────────────────────────────────────────
//
// Third-tap action for keys that already appear in double_tap_keys[].
// Adds one extra deferral window to double-taps while waiting for a
// potential third press.

typedef struct {
    uint16_t keycode;
    uint16_t action;
} triple_tap_key_t;

// ─── Mode Tap Overrides ─────────────────────────────────────────────────────
//
// By default, tapping a pointing device mode key sends whatever is at that
// position on LAYER_BASE.  Use this to override that default.

typedef struct {
    uint16_t keycode;
    uint16_t tap;
} mode_tap_override_t;
