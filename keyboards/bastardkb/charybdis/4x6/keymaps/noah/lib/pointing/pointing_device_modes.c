// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Modes
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "../../keymap_defs.h"
#include "pointing_device_modes.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#endif

#include "pointing_device_mode_handlers.h"

uint8_t pd_mode_flags        = 0;
uint8_t pd_mode_locked_flags = 0;

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static bool dragscroll_auto_mouse_owned = false;

static void dragscroll_lock_attach_auto_mouse(void) {
    if (get_auto_mouse_toggle()) {
        dragscroll_auto_mouse_owned = false;
        return;
    }

    auto_mouse_toggle();
    dragscroll_auto_mouse_owned = true;
}

static void dragscroll_lock_detach_auto_mouse(void) {
    if (dragscroll_auto_mouse_owned && get_auto_mouse_toggle()) {
        auto_mouse_toggle();
    }

    dragscroll_auto_mouse_owned = false;
}
#endif

const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {
    {PD_MODE_DRAGSCROLL, DRAGSCROLL, LOCK_PD_MODE(DRAGSCROLL), NULL, NULL, NULL}, {PD_MODE_VOLUME, VOLUME_MODE, LOCK_PD_MODE(VOLUME_MODE), handle_volume_mode, NULL, reset_volume_mode}, {PD_MODE_BRIGHTNESS, BRIGHTNESS_MODE, LOCK_PD_MODE(BRIGHTNESS_MODE), handle_brightness_mode, NULL, reset_brightness_mode}, {PD_MODE_ZOOM, ZOOM_MODE, LOCK_PD_MODE(ZOOM_MODE), handle_zoom_mode, NULL, reset_zoom_mode}, {PD_MODE_ARROW, ARROW_MODE, LOCK_PD_MODE(ARROW_MODE), handle_arrow_mode, handle_arrow_mode_key, reset_arrow_mode},
};

void pd_mode_set(uint8_t mode) {
    pd_mode_flags |= mode;
}

void pd_mode_clear(uint8_t mode) {
    pd_mode_flags &= (uint8_t)~mode;
}

void pd_mode_set_locked(uint8_t mode) {
    pd_mode_locked_flags |= mode;
}

void pd_mode_clear_locked(uint8_t mode) {
    pd_mode_locked_flags &= (uint8_t)~mode;
}

bool pd_mode_active(uint8_t mode) {
    return (pd_mode_flags & mode) != 0;
}

bool pd_mode_locked(uint8_t mode) {
    return (pd_mode_locked_flags & mode) != 0;
}

bool pd_any_mode_locked(void) {
    return pd_mode_locked_flags != 0;
}

void pd_mode_apply_remote_snapshot(uint8_t active_flags, uint8_t locked_flags) {
    // Remote sync only mirrors mode state for the non-master half's policy/UI.
    // Do not replay local side effects such as dragscroll or auto-mouse
    // ownership changes from this path.
    pd_mode_locked_flags = locked_flags;
    pd_mode_flags        = active_flags | locked_flags;
}

const pd_mode_def_t *pd_mode_lookup(uint8_t mode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].mode_flag == mode) return &pd_modes[i];
    }
    return NULL;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].lock_action != KC_NO && pd_modes[i].lock_action == action) return &pd_modes[i];
    }
    return NULL;
}

bool pd_mode_is_lockable(uint8_t mode) {
    const pd_mode_def_t *def = pd_mode_lookup(mode);
    return def && def->lock_action != KC_NO;
}

bool is_pd_mode_lock_action(uint16_t action) {
    return pd_mode_lock_action_lookup(action) != NULL;
}

void pd_mode_activate(uint8_t mode) {
    pd_mode_set(mode);
    if (mode == PD_MODE_DRAGSCROLL) {
        charybdis_set_pointer_dragscroll_enabled(true);
    }
}

void pd_mode_deactivate(uint8_t mode) {
    pd_mode_clear(mode);

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].mode_flag == mode && pd_modes[i].reset) {
            pd_modes[i].reset();
            break;
        }
    }

    if (mode == PD_MODE_DRAGSCROLL) {
        charybdis_set_pointer_dragscroll_enabled(false);
    }
}

void pd_mode_lock(uint8_t mode) {
    pd_mode_set_locked(mode);
    pd_mode_activate(mode);

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (mode == PD_MODE_DRAGSCROLL) {
        dragscroll_lock_attach_auto_mouse();
    }
#endif
}

void pd_mode_unlock(uint8_t mode) {
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (mode == PD_MODE_DRAGSCROLL) {
        dragscroll_lock_detach_auto_mouse();
    }
#endif

    pd_mode_clear_locked(mode);
    pd_mode_deactivate(mode);
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

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag) && pd_modes[i].key_handler && pd_modes[i].key_handler(keycode, record)) {
            return true;
        }
    }
    return false;
}

uint8_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) return pd_modes[i].mode_flag;
    }
    return 0;
}
