// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime-state transitions for the handled-key engine. These helpers mutate
// the shared key runtime state and emit an ordered plan of side effects, but do
// not execute those effects themselves.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../pointing/pd_mode_flags.h"
#include "delayed_action.h"
#include "handled_key.h"

typedef enum {
    KEY_RUNTIME_TRANSITION_EFFECT_NONE = 0,
    KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION,
    KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_REGISTER,
    KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_UNREGISTER,
    KEY_RUNTIME_TRANSITION_EFFECT_RELEASE_HELD_ACTION_OWNED_BY_KEY,
    KEY_RUNTIME_TRANSITION_EFFECT_LAYER_PRESS,
    KEY_RUNTIME_TRANSITION_EFFECT_LAYER_RELEASE,
    KEY_RUNTIME_TRANSITION_EFFECT_FEEDBACK_PULSE,
    KEY_RUNTIME_TRANSITION_EFFECT_PD_MODE_LOCK_TAP,
    KEY_RUNTIME_TRANSITION_EFFECT_DELAYED_ACTION,
} key_runtime_transition_effect_kind_t;

typedef struct {
    key_runtime_transition_effect_kind_t kind;
    union {
        uint16_t action;
        struct {
            keypos_t  key_pos;
            uint16_t  action;
        } held_action;
        keypos_t key_pos;
        struct {
            keypos_t key_pos;
            uint8_t  layer;
        } layer_press;
        bool long_hold_level;
        pd_mode_mask_t pd_mode;
        struct {
            uint16_t              action;
            delayed_action_mods_t mods;
            uint8_t               repeat_count;
        } delayed_action;
    } data;
} key_runtime_transition_effect_t;

#define KEY_RUNTIME_TRANSITION_PLAN_CAPACITY 10

typedef struct {
    key_runtime_transition_effect_t effects[KEY_RUNTIME_TRANSITION_PLAN_CAPACITY];
    uint8_t                         count;
} key_runtime_transition_plan_t;

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan);
void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan);

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan);
bool key_runtime_transition_handled_key_press(uint16_t keycode, keyrecord_t *record, handled_key_view_t key, bool active_held_action_survives_flush, key_runtime_transition_plan_t *plan);
bool key_runtime_transition_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_view_t key, key_runtime_transition_plan_t *plan);
void key_runtime_transition_scan(key_runtime_transition_plan_t *plan);
