// ────────────────────────────────────────────────────────────────────────────
// PD Mode Snapshot
// ────────────────────────────────────────────────────────────────────────────
//
// Central computed view of pd-mode runtime state. This is the read-only
// contract for consumers that need effective local/display state plus derived
// policy metadata such as active traits and selected mode identity.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

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

static pd_mode_snapshot_view_t pd_mode_snapshot_build_view(pd_mode_mask_t active_mode, pd_mode_mask_t locked_mode, split_half_t owner_half) {
    pd_mode_snapshot_view_t view = {
        .active_mode  = active_mode,
        .locked_mode  = locked_mode,
        .active_index = pd_mode_snapshot_mode_index(active_mode),
        .locked_index = pd_mode_snapshot_mode_index(locked_mode),
        .owner_half   = owner_half,
    };

    if (view.active_index < PD_MODE_COUNT) {
        view.active_traits = pd_modes[view.active_index].traits;
    }

    return view;
}

pd_mode_snapshot_t pd_mode_snapshot(void) {
    const pd_mode_runtime_shared_state_t *state = pd_mode_runtime_shared_state();
    pd_mode_snapshot_t                    snapshot;
    split_half_t                         local_owner_half   = SPLIT_HALF_NONE;
    split_half_t                         display_owner_half = SPLIT_HALF_NONE;

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    local_owner_half = state->local_owner_half;
#endif

    snapshot.local = pd_mode_snapshot_build_view(state->local_active_mode, state->local_locked_mode, local_owner_half);

    if (is_keyboard_master()) {
        snapshot.display = snapshot.local;
        return snapshot;
    }

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    display_owner_half = state->remote_display_owner_half;
#endif

    snapshot.display = pd_mode_snapshot_build_view(state->remote_display_active_mode, state->remote_display_locked_mode, display_owner_half);
    return snapshot;
}
