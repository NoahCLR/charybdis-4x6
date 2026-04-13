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

static pd_mode_snapshot_view_t pd_mode_snapshot_build_view(pd_mode_mask_t active_mode, pd_mode_mask_t locked_mode) {
    pd_mode_snapshot_view_t view = {
        .active_flags  = active_mode,
        .locked_flags  = locked_mode,
        .active_mode   = active_mode,
        .locked_mode   = locked_mode,
        .active_index  = pd_mode_snapshot_mode_index(active_mode),
        .locked_index  = pd_mode_snapshot_mode_index(locked_mode),
    };

    if (view.active_index < PD_MODE_COUNT) {
        view.active_traits = pd_modes[view.active_index].traits;
    }

    return view;
}

pd_mode_snapshot_t pd_mode_snapshot(void) {
    pd_mode_snapshot_t snapshot;

    snapshot.local = pd_mode_snapshot_build_view(noah_runtime_shared_state.pd.local_active_mode, noah_runtime_shared_state.pd.local_locked_mode);

    if (is_keyboard_master()) {
        snapshot.display = snapshot.local;
        return snapshot;
    }

    snapshot.display = pd_mode_snapshot_build_view(noah_runtime_shared_state.pd.remote_display_active_mode, noah_runtime_shared_state.pd.remote_display_locked_mode);
    return snapshot;
}
