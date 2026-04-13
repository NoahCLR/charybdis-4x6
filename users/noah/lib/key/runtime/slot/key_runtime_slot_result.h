// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Results
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot-event result surface for press, release, scan, interrupt, and
// pending multi-tap flush paths. This keeps transition planning on one generic
// result shape instead of translating several local event protocols directly.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../effects/key_runtime_effect_queue.h"

#define KEY_RUNTIME_SLOT_RESULT_CAPACITY 8

typedef struct {
    bool handled;
    KEY_RUNTIME_EFFECT_QUEUE_FIELDS(KEY_RUNTIME_SLOT_RESULT_CAPACITY);
} key_runtime_slot_result_t;
