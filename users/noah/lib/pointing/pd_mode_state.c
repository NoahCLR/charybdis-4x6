// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Mode State
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "pointing_device_modes.h"

static uint8_t pd_mode_active_flags_state = 0;
static uint8_t pd_mode_locked_flags_state = 0;

void pd_mode_set(uint8_t mode) {
    pd_mode_active_flags_state |= mode;
}

void pd_mode_clear(uint8_t mode) {
    pd_mode_active_flags_state &= (uint8_t)~mode;
}

void pd_mode_set_locked(uint8_t mode) {
    pd_mode_locked_flags_state |= mode;
}

void pd_mode_clear_locked(uint8_t mode) {
    pd_mode_locked_flags_state &= (uint8_t)~mode;
}

uint8_t pd_mode_active_snapshot(void) {
    return pd_mode_active_flags_state;
}

uint8_t pd_mode_locked_snapshot(void) {
    return pd_mode_locked_flags_state;
}

bool pd_mode_active(uint8_t mode) {
    return (pd_mode_active_flags_state & mode) != 0;
}

bool pd_mode_locked(uint8_t mode) {
    return (pd_mode_locked_flags_state & mode) != 0;
}

bool pd_any_mode_active(void) {
    return pd_mode_active_flags_state != 0;
}

bool pd_any_mode_locked(void) {
    return pd_mode_locked_flags_state != 0;
}

void pd_mode_apply_remote_snapshot(uint8_t active_flags, uint8_t locked_flags) {
    // Remote sync only mirrors mode state for the non-master half's policy/UI.
    // Do not replay local side effects such as dragscroll or auto-mouse
    // ownership changes from this path.
    pd_mode_locked_flags_state = locked_flags;
    pd_mode_active_flags_state = active_flags | locked_flags;
}

bool pd_mode_set_lock_state(uint8_t mode, bool locked) {
    if (locked) {
        bool changed = false;

        for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
            uint8_t other_mode = pd_modes[i].mode_flag;
            if (other_mode != mode && pd_mode_locked(other_mode)) {
                pd_mode_unlock(other_mode);
                changed = true;
            }
        }

        if (!pd_mode_locked(mode)) {
            pd_mode_lock(mode);
            changed = true;
        }

        return changed;
    }

    if (!pd_mode_locked(mode)) return false;

    pd_mode_unlock(mode);

    return true;
}

bool pd_mode_toggle_lock_state(uint8_t mode) {
    return pd_mode_set_lock_state(mode, !pd_mode_locked(mode));
}

bool pd_mode_unlock_other_locks(uint8_t keep_mode) {
    bool changed = false;

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        uint8_t mode = pd_modes[i].mode_flag;
        if (mode != keep_mode) {
            changed |= pd_mode_set_lock_state(mode, false);
        }
    }

    return changed;
}

bool pd_mode_deactivate_other_unlocked(uint8_t keep_mode) {
    bool changed = false;

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        uint8_t mode = pd_modes[i].mode_flag;
        if (mode != keep_mode && pd_mode_active(mode) && !pd_mode_locked(mode)) {
            pd_mode_deactivate(mode);
            changed = true;
        }
    }

    return changed;
}

void pd_mode_update(uint8_t mode, bool active) {
    if (active) {
        pd_mode_activate(mode);
    } else if (!pd_mode_locked(mode)) {
        pd_mode_deactivate(mode);
    }
}
