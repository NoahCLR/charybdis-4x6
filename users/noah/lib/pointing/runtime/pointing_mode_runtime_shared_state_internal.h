// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Runtime Shared State Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Concrete pointing-mode runtime storage owned by the userspace runtime
// context. This layout is internal to the pd/runtime owner layer.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../key/runtime/slot/keypos_codec.h"
#include "../../key/runtime/slot/origin_registry.h"
#include "../../state/shared/runtime_publication.h"
#include "../defs/pointing_mode_flags.h"

#define POINTING_MODE_OWNER_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))

// One key holding the active mode. `key` is the key's packed matrix index plus
// one, so zero marks a free entry and the zero-initialised runtime context
// starts with no owners. `owner_sides` is the side mask the key's press
// reported. An entry carries no mode: every change of the active mode clears
// the list, so every entry belongs to the active mode.
typedef struct {
    uint8_t           key;
    split_side_mask_t owner_sides;
} pointing_mode_owner_entry_t;

// Mirrored pointing-mode identity for the slave half's UI. The split worker
// context publishes this whole group at once, so it is grouped in one struct
// rather than spread across independently written fields.
typedef struct {
    pointing_mode_mask_t active_mode;
    pointing_mode_mask_t locked_mode;
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    split_side_mask_t owner_sides;
    uint8_t           owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
#endif
} pointing_mode_remote_display_state_t;

typedef struct {
    pointing_mode_mask_t local_active_mode;
    pointing_mode_mask_t local_locked_mode;
    // What the active mode runs from (pointing_mode_source_t).
    uint8_t              local_active_source;
    // The single owner key and owner sides: those of the owners below while
    // any key holds the mode, otherwise the key that locked or activated it.
    bool           local_owner_key_pos_valid;
    keypos_t       local_owner_key_pos;
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    split_side_mask_t local_owner_sides;
#endif
    // Written only by the split worker context. Each publication fills the
    // slot the current generation does not select and then advances the
    // generation, so readers always copy a slot the worker is not touching.
    pointing_mode_remote_display_state_t  remote_display[2];
    noah_runtime_publication_generation_t remote_display_generation;
    // The keys holding the active mode. A press takes the lowest free entry,
    // and the first entry in use gives the single owner key above.
    pointing_mode_owner_entry_t           local_owners[POINTING_MODE_OWNER_CAPACITY];
    bool                                  synthetic_auto_mouse_anchor_active;
    bool                                  active_dpi_sync_pending;
    bool                                  auto_sniping_layer_active;
} pointing_mode_runtime_shared_state_t;

pointing_mode_runtime_shared_state_t *pointing_mode_runtime_shared_state(void);

static inline const pointing_mode_remote_display_state_t *pointing_mode_remote_display_published(const pointing_mode_runtime_shared_state_t *state) {
    return &state->remote_display[noah_runtime_publication_slot(state->remote_display_generation)];
}

static inline pointing_mode_remote_display_state_t *pointing_mode_remote_display_pending(pointing_mode_runtime_shared_state_t *state) {
    return &state->remote_display[noah_runtime_publication_slot((uint8_t)(state->remote_display_generation + 1u))];
}

// Mirrored identity without the owner bitmap, for readers that render identity
// alone. Slot publication keeps the whole slot stable while a generation is
// settled, so taking part of it is exactly as coherent as taking all of it, and
// it keeps the owner bitmap off the caller's stack.
typedef struct {
    pointing_mode_mask_t active_mode;
    pointing_mode_mask_t locked_mode;
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    split_side_mask_t owner_sides;
#endif
} pointing_mode_remote_identity_t;

static inline pointing_mode_remote_identity_t pointing_mode_remote_display_identity_snapshot(const pointing_mode_runtime_shared_state_t *state) {
    pointing_mode_remote_identity_t observed = {0};

    for (uint8_t attempt = 0; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t                               generation = noah_runtime_publication_observe(&state->remote_display_generation);
        const pointing_mode_remote_display_state_t *slot = &state->remote_display[noah_runtime_publication_slot(generation)];

        observed.active_mode = slot->active_mode;
        observed.locked_mode = slot->locked_mode;
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
        observed.owner_sides = slot->owner_sides;
#endif

        if (noah_runtime_publication_settled(&state->remote_display_generation, generation)) {
            break;
        }
    }

    return observed;
}

// Copy the mirrored pointing-mode identity out of the published slot. Readers
// write nothing shared, so this stays safe to call from either execution
// context. A publication that preempts the copy fills the other slot and only
// fails the generation re-check, and the bounded retry means the render path
// never blocks and the split worker is never delayed.
static inline pointing_mode_remote_display_state_t pointing_mode_remote_display_snapshot(const pointing_mode_runtime_shared_state_t *state) {
    pointing_mode_remote_display_state_t observed = {0};

    for (uint8_t attempt = 0; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t generation = noah_runtime_publication_observe(&state->remote_display_generation);

        observed = state->remote_display[noah_runtime_publication_slot(generation)];

        if (noah_runtime_publication_settled(&state->remote_display_generation, generation)) {
            break;
        }
    }

    return observed;
}
