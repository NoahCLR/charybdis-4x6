// ────────────────────────────────────────────────────────────────────────────
// PD Mode Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────
//
// Narrow pd-mode runtime slice owned by the userspace runtime context.
// Pd runtime modules should use this surface instead of depending on the
// aggregate runtime storage layout.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../defs/pd_mode_flags.h"

typedef struct {
    pd_mode_mask_t local_active_mode;
    pd_mode_mask_t local_locked_mode;
    pd_mode_mask_t remote_display_active_mode;
    pd_mode_mask_t remote_display_locked_mode;
} pd_mode_runtime_shared_state_t;

pd_mode_runtime_shared_state_t *pd_mode_runtime_shared_state(void);
