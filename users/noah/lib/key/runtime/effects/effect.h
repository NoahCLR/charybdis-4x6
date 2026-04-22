// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Effects
// ────────────────────────────────────────────────────────────────────────────
//
// Shared runtime effect vocabulary emitted by the handled-key reducers and
// executed by the transition plan.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../../../pointing/defs/pd_mode_flags.h"
#include "../delayed_action.h"

typedef enum {
    KEY_RUNTIME_EFFECT_NONE = 0,
    KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
    KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER,
    KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER,
    KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
    KEY_RUNTIME_EFFECT_REPEAT_START,
    KEY_RUNTIME_EFFECT_LAYER_PRESS,
    KEY_RUNTIME_EFFECT_LAYER_RELEASE,
    KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
    KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP,
    KEY_RUNTIME_EFFECT_DELAYED_ACTION,
} key_runtime_effect_kind_t;

typedef struct {
    key_runtime_effect_kind_t kind;
    union {
        uint16_t action;
        struct {
            keypos_t key_pos;
            uint16_t action;
        } held_action;
        keypos_t key_pos;
        struct {
            keypos_t key_pos;
            uint16_t action;
            uint16_t repeat_hz;
        } repeat;
        struct {
            keypos_t key_pos;
            uint8_t  layer;
        } layer_press;
        struct {
            keypos_t key_pos;
            bool     long_hold_level;
        } feedback_pulse;
        struct {
            pd_mode_mask_t pd_mode;
            keypos_t       key_pos;
        } pd_mode_lock_tap;
        struct {
            uint16_t              action;
            delayed_action_mods_t mods;
            uint8_t               repeat_count;
        } delayed_action;
    } data;
} key_runtime_effect_t;
