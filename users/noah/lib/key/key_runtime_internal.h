// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Private shared state and helpers for the split key runtime modules.
// Keep this header limited to declarations shared across key_runtime*.c.
// Engine-local helpers should stay file-local in their owning module.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

#include "noah_keymap.h"
#include "../action/action_dispatch.h"
#include "../pointing/pd_mode_key_runtime.h"
#include "../pointing/pointing_device_modes.h"
#include "delayed_action.h"
#include "key_behavior_lookup.h"
#include "held_action.h"
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
    bool            layer_interrupted;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
} active_key_state_t;

typedef struct {
    key_behavior_view_t behavior;
    uint8_t             pd_mode;
} handled_key_view_t;

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

uint8_t behavior_get_layer(uint16_t keycode);
bool    is_layer_key(uint16_t keycode);
bool    keypos_equal(keypos_t lhs, keypos_t rhs);
bool    active_key_matches(uint16_t keycode, keypos_t key_pos);

void active_key_reset(void);
void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired);

handled_key_view_t handled_key_lookup(uint16_t keycode);
uint16_t           handled_key_tap_action(handled_key_view_t key, keyrecord_t *record);
bool               handled_key_multi_tap_repress(handled_key_view_t key, uint16_t keycode);
uint16_t           handled_key_advance_multi_tap(uint16_t keycode);
void               handled_key_dispatch_tap_or_begin_multi_tap(uint16_t keycode, handled_key_view_t key, keyrecord_t *record);
