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

typedef struct {
    bool                 handled;
    uint8_t              count;
    bool                 overflowed;
    key_runtime_effect_t effects[KEY_RUNTIME_SLOT_RESULT_CAPACITY];
} key_runtime_slot_result_t;
