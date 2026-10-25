// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Snapshot
// ────────────────────────────────────────────────────────────────────────────
//
// Central computed view of pointing-mode runtime state. This is the read-only
// contract for consumers that need effective local/display state plus derived
// policy metadata such as active traits and selected mode identity.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../key/runtime/slot/origin_registry.h"
#include "../defs/pointing_modes.h"
#include "pointing_mode_runtime_shared_state_internal.h"

static uint8_t pointing_mode_snapshot_mode_index(pointing_mode_mask_t mode) {
    if (mode == 0) {
        return POINTING_MODE_COUNT;
    }

    for (uint8_t index = 0; index < POINTING_MODE_COUNT; index++) {
        if (pointing_modes[index].mode_flag == mode) {
            return index;
        }
    }

    return POINTING_MODE_COUNT;
}

// Filled through a pointer rather than returned by value: this runs on the
// bounded main-loop scan path, where a returned view costs an extra struct
// temporary and its copy from the deliberately retained reviewed-path policy
// reserve. That policy margin is distinct from the physical stack boundary.
static void pointing_mode_snapshot_fill_view(pointing_mode_snapshot_view_t *view, pointing_mode_mask_t active_mode, pointing_mode_mask_t locked_mode, split_side_mask_t owner_sides) {
    view->active_mode   = active_mode;
    view->locked_mode   = locked_mode;
    view->active_index  = pointing_mode_snapshot_mode_index(active_mode);
    view->locked_index  = pointing_mode_snapshot_mode_index(locked_mode);
    view->owner_sides   = owner_sides;
    view->active_traits = pointing_mode_effective_traits(active_mode);
    view->active_source = POINTING_MODE_SOURCE_NONE;
    view->active_light  = false;
}

// A mode its layer started shows the layer's lighting unless its slot's light
// switch is on (D-F62); a mode its key runs always shows its own.
static void pointing_mode_snapshot_fill_local_source(pointing_mode_snapshot_view_t *view, const pointing_mode_runtime_shared_state_t *state) {
    view->active_source = view->active_mode != 0 ? (pointing_mode_source_t)state->local_active_source : POINTING_MODE_SOURCE_NONE;
    view->active_light  = view->active_source == POINTING_MODE_SOURCE_KEY || (view->active_source == POINTING_MODE_SOURCE_LAYER && pointing_mode_layer_started_light(view->active_mode));
}

pointing_mode_snapshot_t pointing_mode_snapshot(void) {
    const pointing_mode_runtime_shared_state_t *state = pointing_mode_runtime_shared_state();
    pointing_mode_snapshot_t                    snapshot;
    pointing_mode_remote_identity_t             display;
    split_side_mask_t                           local_owner_sides   = SPLIT_SIDE_MASK_NONE;
    split_side_mask_t                           display_owner_sides = SPLIT_SIDE_MASK_NONE;

#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    local_owner_sides = state->local_owner_sides;
#endif

    pointing_mode_snapshot_fill_view(&snapshot.local, state->local_active_mode, state->local_locked_mode, local_owner_sides);
    pointing_mode_snapshot_fill_local_source(&snapshot.local, state);

    if (is_keyboard_master()) {
        snapshot.display = snapshot.local;
        return snapshot;
    }

    // The split worker publishes the mirrored identity from another execution
    // context, so take the group as one published generation instead of reading
    // fields a publication may currently be halfway through writing. This
    // reader does not render owner keys, so it leaves the owner bitmap in the
    // slot rather than copying it onto a hot main-loop stack path.
    display = pointing_mode_remote_display_identity_snapshot(state);

#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    display_owner_sides = display.owner_sides;
#endif

    pointing_mode_snapshot_fill_view(&snapshot.display, display.active_mode, display.locked_mode, display_owner_sides);
    snapshot.display.active_light = display.active_mode != 0 && display.active_light;
    return snapshot;
}

// Consumers that render mirrored identity and owner keys together must take
// both from the same published generation. Calling pointing_mode_snapshot() and
// pointing_mode_display_owner_bitmap_snapshot() separately is coherent per call
// but can still pair identity from one publication with an owner bitmap from
// the next.
pointing_mode_snapshot_t pointing_mode_snapshot_with_owner_bitmap(uint8_t *out_owner_bitmap, bool *out_has_owner_keys) {
    const pointing_mode_runtime_shared_state_t *state = pointing_mode_runtime_shared_state();
    pointing_mode_snapshot_t                    snapshot;
    pointing_mode_remote_display_state_t        display;
    split_side_mask_t                           local_owner_sides   = SPLIT_SIDE_MASK_NONE;
    split_side_mask_t                           display_owner_sides = SPLIT_SIDE_MASK_NONE;
    bool                                        has_owner_keys      = false;

    key_origin_bitmap_clear(out_owner_bitmap);

#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    local_owner_sides = state->local_owner_sides;
#endif

    pointing_mode_snapshot_fill_view(&snapshot.local, state->local_active_mode, state->local_locked_mode, local_owner_sides);
    pointing_mode_snapshot_fill_local_source(&snapshot.local, state);

    if (is_keyboard_master()) {
        snapshot.display = snapshot.local;
        has_owner_keys   = pointing_mode_local_owner_bitmap_snapshot(out_owner_bitmap);
    } else {
        display = pointing_mode_remote_display_snapshot(state);

#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
        display_owner_sides = display.owner_sides;
        key_origin_bitmap_copy(out_owner_bitmap, display.owner_bitmap);
        has_owner_keys = key_origin_bitmap_has_any(out_owner_bitmap);
#endif

        pointing_mode_snapshot_fill_view(&snapshot.display, display.active_mode, display.locked_mode, display_owner_sides);
        snapshot.display.active_light = display.active_mode != 0 && display.active_light;
    }

    if (out_has_owner_keys) {
        *out_has_owner_keys = has_owner_keys;
    }

    return snapshot;
}
