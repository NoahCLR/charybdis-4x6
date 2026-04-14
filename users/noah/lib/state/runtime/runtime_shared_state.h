// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────
//
// Aggregate runtime-owned state shared across the split key engine and pd-mode
// modules. Key-runtime storage lives under key/runtime; this layer owns the
// cross-domain aggregate that other userspace state modules can snapshot.
// The old noah_runtime_shared_state name is kept only as a compatibility alias;
// new code should prefer the slice accessors below.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../key/runtime/key_runtime_shared_state.h"
#include "../../pointing/defs/pd_mode_flags.h"

typedef struct {
    pd_mode_mask_t local_active_mode;
    pd_mode_mask_t local_locked_mode;
    pd_mode_mask_t remote_display_active_mode;
    pd_mode_mask_t remote_display_locked_mode;
} pd_mode_runtime_shared_state_t;

typedef struct {
    key_runtime_shared_state_t     key;
    pd_mode_runtime_shared_state_t pd;
} runtime_shared_state_t;

runtime_shared_state_t *noah_runtime_shared_state_ptr(void);
pd_mode_runtime_shared_state_t *pd_mode_runtime_shared_state(void);

#define noah_runtime_shared_state (*noah_runtime_shared_state_ptr())

void runtime_shared_state_reset(runtime_shared_state_t *state);
