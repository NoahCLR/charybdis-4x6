// ────────────────────────────────────────────────────────────────────────────
// PD Mode State
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../state/runtime_shared_state.h"
#include "../state/split_runtime_sync.h"
#include "pd_mode_internal.h"

#define PD_MODE_LOCAL_ACTIVE_FLAGS (noah_runtime_shared_state.pd.local_active_flags)
#define PD_MODE_LOCAL_LOCKED_FLAGS (noah_runtime_shared_state.pd.local_locked_flags)
#define PD_MODE_REMOTE_DISPLAY_ACTIVE_FLAGS (noah_runtime_shared_state.pd.remote_display_active_flags)
#define PD_MODE_REMOTE_DISPLAY_LOCKED_FLAGS (noah_runtime_shared_state.pd.remote_display_locked_flags)

static pd_mode_mask_t pd_mode_first_snapshot_match(pd_mode_mask_t flags) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if ((flags & pd_modes[i].mode_flag) != 0) {
            return pd_modes[i].mode_flag;
        }
    }

    return 0;
}

void pd_mode_set(pd_mode_mask_t mode) {
    PD_MODE_LOCAL_ACTIVE_FLAGS |= mode;
}

void pd_mode_clear(pd_mode_mask_t mode) {
    PD_MODE_LOCAL_ACTIVE_FLAGS &= (pd_mode_mask_t)~mode;
}

void pd_mode_set_locked(pd_mode_mask_t mode) {
    PD_MODE_LOCAL_LOCKED_FLAGS |= mode;
}

void pd_mode_clear_locked(pd_mode_mask_t mode) {
    PD_MODE_LOCAL_LOCKED_FLAGS &= (pd_mode_mask_t)~mode;
}

static inline bool pd_mode_display_uses_local_state(void) {
    return is_keyboard_master();
}

pd_mode_mask_t pd_mode_local_active_snapshot(void) {
    return PD_MODE_LOCAL_ACTIVE_FLAGS;
}

pd_mode_mask_t pd_mode_local_locked_snapshot(void) {
    return PD_MODE_LOCAL_LOCKED_FLAGS;
}

pd_mode_mask_t pd_mode_display_active_snapshot(void) {
    return pd_mode_display_uses_local_state() ? PD_MODE_LOCAL_ACTIVE_FLAGS : PD_MODE_REMOTE_DISPLAY_ACTIVE_FLAGS;
}

pd_mode_mask_t pd_mode_display_locked_snapshot(void) {
    return pd_mode_display_uses_local_state() ? PD_MODE_LOCAL_LOCKED_FLAGS : PD_MODE_REMOTE_DISPLAY_LOCKED_FLAGS;
}

bool pd_mode_local_active(pd_mode_mask_t mode) {
    return (PD_MODE_LOCAL_ACTIVE_FLAGS & mode) != 0;
}

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    return (PD_MODE_LOCAL_LOCKED_FLAGS & mode) != 0;
}

bool pd_mode_display_active(pd_mode_mask_t mode) {
    return (pd_mode_display_active_snapshot() & mode) != 0;
}

bool pd_mode_display_locked(pd_mode_mask_t mode) {
    return (pd_mode_display_locked_snapshot() & mode) != 0;
}

bool pd_any_local_mode_active(void) {
    return PD_MODE_LOCAL_ACTIVE_FLAGS != 0;
}

bool pd_any_local_mode_locked(void) {
    return PD_MODE_LOCAL_LOCKED_FLAGS != 0;
}

bool pd_any_display_mode_active(void) {
    return pd_mode_display_active_snapshot() != 0;
}

bool pd_any_display_mode_locked(void) {
    return pd_mode_display_locked_snapshot() != 0;
}

void pd_mode_apply_remote_snapshot(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags) {
    // Remote sync only mirrors mode state for the non-master half's policy/UI.
    // Do not replay local side effects such as dragscroll or auto-mouse
    // ownership changes from this path. Keep only one effective mode so the
    // mirrored UI matches the local exclusivity invariant.
    pd_mode_mask_t locked_mode = pd_mode_first_snapshot_match(locked_flags);
    pd_mode_mask_t active_mode = locked_mode ? locked_mode : pd_mode_first_snapshot_match(active_flags);

    PD_MODE_REMOTE_DISPLAY_LOCKED_FLAGS = locked_mode;
    PD_MODE_REMOTE_DISPLAY_ACTIVE_FLAGS = active_mode;
}

bool pd_mode_set_lock_state(pd_mode_mask_t mode, bool locked) {
    if (locked) {
        bool changed = false;

        for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
            pd_mode_mask_t other_mode = pd_modes[i].mode_flag;
            if (other_mode != mode && pd_mode_local_locked(other_mode)) {
                pd_mode_unlock(other_mode);
                changed = true;
            }
        }

        changed |= pd_mode_deactivate_other_unlocked(mode);

        if (!pd_mode_local_locked(mode) || !pd_mode_local_active(mode)) {
            pd_mode_lock(mode);
            changed = true;
        }

        return changed;
    }

    if (!pd_mode_local_locked(mode)) return false;

    pd_mode_unlock(mode);

    return true;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    return pd_mode_set_lock_state(mode, !pd_mode_local_locked(mode));
}

bool pd_mode_unlock_other_locks(pd_mode_mask_t keep_mode) {
    bool changed = false;

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        pd_mode_mask_t mode = pd_modes[i].mode_flag;
        if (mode != keep_mode) {
            changed |= pd_mode_set_lock_state(mode, false);
        }
    }

    return changed;
}

bool pd_mode_deactivate_other_unlocked(pd_mode_mask_t keep_mode) {
    bool changed = false;

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        pd_mode_mask_t mode = pd_modes[i].mode_flag;
        if (mode != keep_mode && pd_mode_local_active(mode) && !pd_mode_local_locked(mode)) {
            pd_mode_deactivate(mode);
            changed = true;
        }
    }

    return changed;
}

void pd_mode_update(pd_mode_mask_t mode, bool active) {
    if (active) {
        pd_mode_activate(mode);
    } else if (!pd_mode_local_locked(mode)) {
        pd_mode_deactivate(mode);
    }
}

bool pd_mode_handle_keycode_press(uint16_t keycode) {
    pd_mode_mask_t mode = pd_mode_for_keycode(keycode);
    if (!mode) {
        return false;
    }

    bool state_changed = false;

    state_changed |= pd_mode_unlock_other_locks(mode);
    state_changed |= pd_mode_deactivate_other_unlocked(mode);

    if (!pd_mode_local_active(mode)) {
        pd_mode_activate(mode);
        state_changed = true;
    }

    if (state_changed) {
        split_runtime_sync();
    }

    return true;
}

bool pd_mode_handle_keycode_release(uint16_t keycode) {
    pd_mode_mask_t mode = pd_mode_for_keycode(keycode);
    if (!mode) {
        return false;
    }

    if (pd_mode_local_active(mode) && !pd_mode_local_locked(mode)) {
        pd_mode_deactivate(mode);
        split_runtime_sync();
    }

    return true;
}

#undef PD_MODE_LOCAL_ACTIVE_FLAGS
#undef PD_MODE_LOCAL_LOCKED_FLAGS
#undef PD_MODE_REMOTE_DISPLAY_ACTIVE_FLAGS
#undef PD_MODE_REMOTE_DISPLAY_LOCKED_FLAGS
