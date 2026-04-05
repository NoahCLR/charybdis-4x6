// ────────────────────────────────────────────────────────────────────────────
// Key Runtime State
// ────────────────────────────────────────────────────────────────────────────
//
// Shared active-key and multi-tap state for the split key runtime modules.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "key_behavior.h"
#include "multi_tap_engine.h"

typedef struct {
    uint16_t        timer;
    uint16_t        keycode;
    keypos_t        key_pos;
    bool            hold_fired;
    uint16_t        held_action_keycode;
    uint16_t        tap_action;
    uint16_t        tap_hold_term;
    uint16_t        longer_hold_term;
    uint16_t        multi_tap_term;
    bool            hold_one_shot_fired;
    bool            implicit_pd_mode_hold;
    bool            pd_mode_was_locked_on_press;
    bool            layer_interrupted;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
} active_key_state_t;

#define ACTIVE_KEY_STATE_INIT                           \
    {                                                   \
        .keycode             = KC_NO,                   \
        .held_action_keycode = KC_NO,                   \
        .tap_hold_term       = CUSTOM_TAP_HOLD_TERM,    \
        .longer_hold_term    = CUSTOM_LONGER_HOLD_TERM, \
        .multi_tap_term      = CUSTOM_MULTI_TAP_TERM,   \
    }

extern active_key_state_t active_key;
extern multi_tap_t        multi_tap;

void noah_key_runtime_scan(void);

uint8_t behavior_get_layer(uint16_t keycode);
bool    is_layer_key(uint16_t keycode);
bool    active_key_matches(uint16_t keycode, keypos_t key_pos);

void active_key_reset(void);
void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired);
