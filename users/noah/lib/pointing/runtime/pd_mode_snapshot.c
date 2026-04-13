// ────────────────────────────────────────────────────────────────────────────
// PD Mode Snapshot
// ────────────────────────────────────────────────────────────────────────────
//
// Central computed view of pd-mode runtime state. This is the read-only
// contract for consumers that need effective local/display state plus derived
// policy metadata such as active traits and first active mode selection.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../defs/pd_modes.h"
#include "../../state/runtime/runtime_shared_state.h"

static pd_mode_snapshot_view_t pd_mode_snapshot_build_view(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags) {
    pd_mode_snapshot_view_t view = {
        .active_flags       = active_flags,
        .locked_flags       = locked_flags,
        .first_active_index = PD_MODE_COUNT,
        .first_locked_index = PD_MODE_COUNT,
    };

    for (uint8_t index = 0; index < PD_MODE_COUNT; index++) {
        pd_mode_mask_t mode = pd_modes[index].mode_flag;

        if ((active_flags & mode) != 0) {
            if (view.first_active_index == PD_MODE_COUNT) {
                view.first_active_index = index;
                view.first_active_mode  = mode;
            }

            view.active_traits |= pd_modes[index].traits;
        }

        if ((locked_flags & mode) != 0 && view.first_locked_index == PD_MODE_COUNT) {
            view.first_locked_index = index;
            view.first_locked_mode  = mode;
        }
    }

    return view;
}

pd_mode_snapshot_t pd_mode_snapshot(void) {
    pd_mode_snapshot_t snapshot;

    snapshot.local = pd_mode_snapshot_build_view(noah_runtime_shared_state.pd.local_active_flags, noah_runtime_shared_state.pd.local_locked_flags);

    if (is_keyboard_master()) {
        snapshot.display = snapshot.local;
        return snapshot;
    }

    snapshot.display = pd_mode_snapshot_build_view(noah_runtime_shared_state.pd.remote_display_active_flags, noah_runtime_shared_state.pd.remote_display_locked_flags);
    return snapshot;
}
