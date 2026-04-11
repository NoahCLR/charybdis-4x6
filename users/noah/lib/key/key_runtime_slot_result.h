// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Results
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot-event result surface for press, release, scan, interrupt, and
// pending multi-tap flush paths. This keeps transition planning on one generic
// result shape instead of translating several local event protocols directly.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "delayed_action.h"
#include "handled_key.h"
#include "key_runtime_slot_effect.h"

#define KEY_RUNTIME_SLOT_RESULT_CAPACITY 6

typedef enum {
    KEY_RUNTIME_SLOT_RESULT_EFFECT_NONE = 0,
    KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST,
    KEY_RUNTIME_SLOT_RESULT_EFFECT_DELAYED_ACTION,
    KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_PRESS,
    KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_RELEASE,
    KEY_RUNTIME_SLOT_RESULT_EFFECT_PD_MODE_LOCK_TAP,
} key_runtime_slot_result_effect_kind_t;

typedef struct {
    key_runtime_slot_result_effect_kind_t kind;
    keypos_t                              key_pos;
    union {
        key_runtime_slot_effect_request_t slot_effect_request;
        struct {
            uint16_t              action;
            delayed_action_mods_t mods;
            uint8_t               repeat_count;
        } delayed_action;
        uint8_t       layer;
        pd_mode_mask_t pd_mode;
    } data;
} key_runtime_slot_result_effect_t;

typedef struct {
    bool                             handled;
    uint8_t                          count;
    bool                             overflowed;
    key_runtime_slot_result_effect_t effects[KEY_RUNTIME_SLOT_RESULT_CAPACITY];
} key_runtime_slot_result_t;
