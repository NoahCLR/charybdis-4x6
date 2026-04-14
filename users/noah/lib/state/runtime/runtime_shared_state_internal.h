// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Aggregate runtime-owned storage shared across the split key engine and
// pd-mode modules. This layout is internal to the runtime owner layer.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../key/runtime/key_runtime_shared_state.h"
#include "../../pointing/runtime/pd_mode_runtime_shared_state.h"

typedef struct {
    key_runtime_shared_state_t     key;
    pd_mode_runtime_shared_state_t pd;
} runtime_shared_state_t;
