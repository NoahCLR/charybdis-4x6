// ────────────────────────────────────────────────────────────────────────────
// PD Mode Snapshot
// ────────────────────────────────────────────────────────────────────────────
//
// Central computed view of pd-mode runtime state. This is the read-only
// contract for consumers that need effective local/display state plus derived
// policy metadata such as active traits and selected mode identity.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../key/runtime/slot/origin_registry.h"
#include "../defs/pd_modes.h"
#include "pd_mode_runtime_shared_state_internal.h"

static uint8_t pd_mode_snapshot_mode_index(pd_mode_mask_t mode) {
    if (mode == 0) {
        return PD_MODE_COUNT;
    }

    for (uint8_t index = 0; index < PD_MODE_COUNT; index++) {
        if (pd_modes[index].mode_flag == mode) {
            return index;
        }
    }

    return PD_MODE_COUNT;
}

// Filled through a pointer rather than returned by value: this runs on the
// bounded main-loop scan path, where a returned view costs an extra struct
// temporary and its copy on a reviewed stack path with little headroom.
static void pd_mode_snapshot_fill_view(pd_mode_snapshot_view_t *view, pd_mode_mask_t active_mode, pd_mode_mask_t locked_mode, split_side_mask_t owner_sides) {
    view->active_mode   = active_mode;
    view->locked_mode   = locked_mode;
    view->active_index  = pd_mode_snapshot_mode_index(active_mode);
    view->locked_index  = pd_mode_snapshot_mode_index(locked_mode);
    view->owner_sides   = owner_sides;
    view->active_traits = view->active_index < PD_MODE_COUNT ? pd_modes[view->active_index].traits : (pd_mode_traits_t)0;
}

pd_mode_snapshot_t pd_mode_snapshot(void) {
    const pd_mode_runtime_shared_state_t *state = pd_mode_runtime_shared_state();
    pd_mode_snapshot_t                    snapshot;
    pd_mode_remote_identity_t             display;
    split_side_mask_t                     local_owner_sides   = SPLIT_SIDE_MASK_NONE;
    split_side_mask_t                     display_owner_sides = SPLIT_SIDE_MASK_NONE;

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    local_owner_sides = state->local_owner_sides;
#endif

    pd_mode_snapshot_fill_view(&snapshot.local, state->local_active_mode, state->local_locked_mode, local_owner_sides);

    if (is_keyboard_master()) {
        snapshot.display = snapshot.local;
        return snapshot;
    }

    // The split worker publishes the mirrored identity from another execution
    // context, so take the group as one published generation instead of reading
    // fields a publication may currently be halfway through writing. This
    // reader does not render owner keys, so it leaves the owner bitmap in the
    // slot rather than copying it onto a hot main-loop stack path.
    display = pd_mode_remote_display_identity_snapshot(state);

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    display_owner_sides = display.owner_sides;
#endif

    pd_mode_snapshot_fill_view(&snapshot.display, display.active_mode, display.locked_mode, display_owner_sides);
    return snapshot;
}

// Consumers that render mirrored identity and owner keys together must take
// both from the same published generation. Calling pd_mode_snapshot() and
// pd_mode_display_owner_bitmap_snapshot() separately is coherent per call but
// can still pair identity from one publication with an owner bitmap from the
// next.
pd_mode_snapshot_t pd_mode_snapshot_with_owner_bitmap(uint8_t *out_owner_bitmap, bool *out_has_owner_keys) {
    const pd_mode_runtime_shared_state_t *state = pd_mode_runtime_shared_state();
    pd_mode_snapshot_t                    snapshot;
    pd_mode_remote_display_state_t        display;
    split_side_mask_t                     local_owner_sides   = SPLIT_SIDE_MASK_NONE;
    split_side_mask_t                     display_owner_sides = SPLIT_SIDE_MASK_NONE;
    bool                                  has_owner_keys      = false;

    key_origin_bitmap_clear(out_owner_bitmap);

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    local_owner_sides = state->local_owner_sides;
#endif

    pd_mode_snapshot_fill_view(&snapshot.local, state->local_active_mode, state->local_locked_mode, local_owner_sides);

    if (is_keyboard_master()) {
        snapshot.display = snapshot.local;
        has_owner_keys   = pd_mode_local_owner_bitmap_snapshot(out_owner_bitmap);
    } else {
        display = pd_mode_remote_display_snapshot(state);

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        display_owner_sides = display.owner_sides;
        key_origin_bitmap_copy(out_owner_bitmap, display.owner_bitmap);
        has_owner_keys = key_origin_bitmap_has_any(out_owner_bitmap);
#endif

        pd_mode_snapshot_fill_view(&snapshot.display, display.active_mode, display.locked_mode, display_owner_sides);
    }

    if (out_has_owner_keys) {
        *out_has_owner_keys = has_owner_keys;
    }

    return snapshot;
}
