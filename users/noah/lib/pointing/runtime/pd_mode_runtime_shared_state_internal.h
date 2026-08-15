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
#include "../../state/shared/runtime_publication.h"
#include "../defs/pd_mode_flags.h"

#define PD_MODE_OWNER_SLOT_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))

typedef struct {
    bool              active;
    pd_mode_mask_t    mode;
    keypos_t          key_pos;
    split_side_mask_t owner_sides;
} pd_mode_owner_slot_t;

// Mirrored pd-mode identity for the non-master half's UI. The split worker
// context publishes this whole group at once, so it is grouped in one struct
// rather than spread across independently written fields.
typedef struct {
    pd_mode_mask_t active_mode;
    pd_mode_mask_t locked_mode;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    split_side_mask_t owner_sides;
    uint8_t           owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
#endif
} pd_mode_remote_display_state_t;

typedef struct {
    pd_mode_mask_t local_active_mode;
    pd_mode_mask_t local_locked_mode;
    bool           local_owner_key_pos_valid;
    keypos_t       local_owner_key_pos;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    split_side_mask_t local_owner_sides;
#endif
    // Written only by the split worker context. Each publication fills the
    // slot the current generation does not select and then advances the
    // generation, so readers always copy a slot the worker is not touching.
    pd_mode_remote_display_state_t        remote_display[2];
    noah_runtime_publication_generation_t remote_display_generation;
    pd_mode_owner_slot_t                  local_key_owners[PD_MODE_OWNER_SLOT_CAPACITY];
    bool                                  synthetic_auto_mouse_anchor_active;
    bool                                  active_dpi_sync_pending;
    bool                                  auto_sniping_layer_active;
} pd_mode_runtime_shared_state_t;

pd_mode_runtime_shared_state_t *pd_mode_runtime_shared_state(void);

static inline const pd_mode_remote_display_state_t *pd_mode_remote_display_published(const pd_mode_runtime_shared_state_t *state) {
    return &state->remote_display[noah_runtime_publication_slot(state->remote_display_generation)];
}

static inline pd_mode_remote_display_state_t *pd_mode_remote_display_pending(pd_mode_runtime_shared_state_t *state) {
    return &state->remote_display[noah_runtime_publication_slot((uint8_t)(state->remote_display_generation + 1u))];
}

// Mirrored identity without the owner bitmap, for readers that render identity
// alone. Slot publication keeps the whole slot stable while a generation is
// settled, so taking part of it is exactly as coherent as taking all of it, and
// it keeps the owner bitmap off the caller's stack.
typedef struct {
    pd_mode_mask_t active_mode;
    pd_mode_mask_t locked_mode;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    split_side_mask_t owner_sides;
#endif
} pd_mode_remote_identity_t;

static inline pd_mode_remote_identity_t pd_mode_remote_display_identity_snapshot(const pd_mode_runtime_shared_state_t *state) {
    pd_mode_remote_identity_t observed = {0};

    for (uint8_t attempt = 0; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t                               generation = noah_runtime_publication_observe(&state->remote_display_generation);
        const pd_mode_remote_display_state_t *slot       = &state->remote_display[noah_runtime_publication_slot(generation)];

        observed.active_mode = slot->active_mode;
        observed.locked_mode = slot->locked_mode;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        observed.owner_sides = slot->owner_sides;
#endif

        if (noah_runtime_publication_settled(&state->remote_display_generation, generation)) {
            break;
        }
    }

    return observed;
}

// Copy the mirrored pd-mode identity out of the published slot. Readers write
// nothing shared, so this stays safe to call from either execution context. A
// publication that preempts the copy fills the other slot and only fails the
// generation re-check, and the bounded retry means the render path never
// blocks and the split worker is never delayed.
static inline pd_mode_remote_display_state_t pd_mode_remote_display_snapshot(const pd_mode_runtime_shared_state_t *state) {
    pd_mode_remote_display_state_t observed = {0};

    for (uint8_t attempt = 0; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t generation = noah_runtime_publication_observe(&state->remote_display_generation);

        observed = state->remote_display[noah_runtime_publication_slot(generation)];

        if (noah_runtime_publication_settled(&state->remote_display_generation, generation)) {
            break;
        }
    }

    return observed;
}
