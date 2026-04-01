// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Lookup
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime helpers that interpret authored key_behavior rows into resolved
// behavior views for the key engine.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_behavior.h"

typedef struct {
    const key_behavior_t *config;
    uint16_t              keycode;
    bool                  handled;
    bool                  is_momentary_layer; // MO() or LT() — engine handles layer_on/off
    bool                  is_layer_tap;       // specifically LT() — has embedded tap key
    bool                  has_multi_tap;
    uint16_t              tap_hold_term;    // resolved: per-key → TAPPING_TERM for LT → CUSTOM_TAP_HOLD_TERM
    uint16_t              longer_hold_term; // resolved: per-key → CUSTOM_LONGER_HOLD_TERM
    uint16_t              multi_tap_term;   // resolved: per-key → CUSTOM_MULTI_TAP_TERM
    key_behavior_step_t   single;
} key_behavior_view_t;

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count);
bool                key_behavior_has_more_taps(uint16_t keycode, uint8_t count);
key_behavior_view_t key_behavior_lookup(uint16_t keycode);
