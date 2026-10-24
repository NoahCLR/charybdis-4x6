// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Aggregate runtime-owned storage shared across the split key engine and
// pointing-mode modules. This layout is internal to the runtime owner layer.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../pointing/runtime/pointing_mode_runtime_shared_state_internal.h"
#include "../../key/runtime/reducer/runtime.h"

typedef struct {
    pointing_mode_runtime_shared_state_t pd;
    key_runtime_core_state_t             core;
} runtime_shared_state_t;
