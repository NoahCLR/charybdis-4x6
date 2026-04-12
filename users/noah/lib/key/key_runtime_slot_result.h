// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Results
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot-event result surface for press, release, scan, interrupt, and
// pending multi-tap flush paths. This keeps transition planning on one generic
// result shape instead of translating several local event protocols directly.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_effect.h"

#define KEY_RUNTIME_SLOT_RESULT_CAPACITY 8

typedef key_runtime_effect_kind_t key_runtime_slot_result_effect_kind_t;
typedef key_runtime_effect_t      key_runtime_slot_result_effect_t;

#define KEY_RUNTIME_SLOT_RESULT_EFFECT_NONE KEY_RUNTIME_EFFECT_NONE
#define KEY_RUNTIME_SLOT_RESULT_EFFECT_DELAYED_ACTION KEY_RUNTIME_EFFECT_DELAYED_ACTION
#define KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_PRESS KEY_RUNTIME_EFFECT_LAYER_PRESS
#define KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_RELEASE KEY_RUNTIME_EFFECT_LAYER_RELEASE
#define KEY_RUNTIME_SLOT_RESULT_EFFECT_PD_MODE_LOCK_TAP KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP

typedef struct {
    bool                             handled;
    uint8_t                          count;
    bool                             overflowed;
    key_runtime_slot_result_effect_t effects[KEY_RUNTIME_SLOT_RESULT_CAPACITY];
} key_runtime_slot_result_t;
