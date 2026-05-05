// ────────────────────────────────────────────────────────────────────────────
// PD Mode Runtime Shared State Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Concrete pd-mode runtime storage owned by the userspace runtime context.
// This layout is internal to the pd/runtime owner layer.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../key/runtime/slot/origin_registry.h"
#include "../defs/pd_mode_flags.h"

#define PD_MODE_OWNER_SLOT_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))

typedef struct {
    bool              active;
    pd_mode_mask_t    mode;
    keypos_t          key_pos;
    split_side_mask_t owner_sides;
} pd_mode_owner_slot_t;

typedef struct {
    pd_mode_mask_t local_active_mode;
    pd_mode_mask_t local_locked_mode;
    pd_mode_mask_t remote_display_active_mode;
    pd_mode_mask_t remote_display_locked_mode;
    bool           local_owner_key_pos_valid;
    keypos_t       local_owner_key_pos;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    split_side_mask_t local_owner_sides;
    split_side_mask_t remote_display_owner_sides;
    uint8_t           remote_display_owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
#endif
    pd_mode_owner_slot_t local_key_owners[PD_MODE_OWNER_SLOT_CAPACITY];
    bool                 synthetic_auto_mouse_anchor_active;
    bool                 active_dpi_sync_pending;
    bool                 auto_sniping_layer_active;
} pd_mode_runtime_shared_state_t;

pd_mode_runtime_shared_state_t *pd_mode_runtime_shared_state(void);
