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

// ─── Multi-Tap Keys ─────────────────────────────────────────────────────────
//
// Sends an action when tapped N times within CUSTOM_MULTI_TAP_TERM.
// Each entry maps a (keycode, tap_count) pair to an action.
// A key can have entries at multiple tap counts (e.g. 2 and 3).
//
// hold_action (optional): if set (!= KC_NO), distinguishes between
// "multi-tap and release" (fires action) vs "multi-tap and hold past
// CUSTOM_TAP_HOLD_TERM" (fires hold_action).  Omit or set to KC_NO
// to disable — existing entries with 3 fields get KC_NO automatically
// via C99 zero-initialization.
//
// Note: any key in this table gets a small delay on single taps
// (waiting for a potential next press).

typedef struct {
    uint16_t keycode;
    uint8_t  tap_count;   // 2 = double-tap, 3 = triple-tap, etc.
    uint16_t action;       // fires on tap (quick release after multi-tap)
    uint16_t hold_action;  // fires on hold (KC_NO = disabled)
} tap_action_t;

// ─── Mode Tap Overrides ─────────────────────────────────────────────────────
//
// By default, tapping a pointing device mode key sends whatever is at that
// position on LAYER_BASE.  Use this to override that default.

typedef struct {
    uint16_t keycode;
    uint16_t tap;
} mode_tap_override_t;
