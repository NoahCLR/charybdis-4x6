// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Effect Queue Vocabulary
// ────────────────────────────────────────────────────────────────────────────
//
// Shared field layout for effect-bearing handled-key runtime surfaces. Slot
// reducers and transition planning still use different capacities, but they
// should expose one consistent queue vocabulary to callers and tests.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "key_runtime_effect.h"

#define KEY_RUNTIME_EFFECT_QUEUE_FIELDS(capacity) \
    uint8_t              count;                   \
    bool                 overflowed;              \
    key_runtime_effect_t items[(capacity)]
