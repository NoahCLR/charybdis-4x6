// ────────────────────────────────────────────────────────────────────────────
// PD Mode Runtime Shared State Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Concrete pd-mode runtime storage owned by the userspace runtime context.
// This layout is internal to the pd/runtime owner layer.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../defs/pd_mode_flags.h"

typedef struct {
    pd_mode_mask_t local_active_mode;
    pd_mode_mask_t local_locked_mode;
    pd_mode_mask_t remote_display_active_mode;
    pd_mode_mask_t remote_display_locked_mode;
    bool           synthetic_auto_mouse_anchor_active;
} pd_mode_runtime_shared_state_t;

pd_mode_runtime_shared_state_t *pd_mode_runtime_shared_state(void);
