// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Aggregate runtime-owned storage shared across the split key engine and
// pd-mode modules. This layout is internal to the runtime owner layer.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../pointing/runtime/pd_mode_runtime_shared_state_internal.h"
#include "../../key/runtime/core/runtime.h"

typedef struct {
    pd_mode_runtime_shared_state_t pd;
    runtime_v2_state_t             v2;
} runtime_shared_state_t;
