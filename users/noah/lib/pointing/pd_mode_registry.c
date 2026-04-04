// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Mode Registry
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "noah_keymap.h"
#include "pointing_device_modes.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#endif

#include "pointing_device_mode_handlers.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static bool scroll_mode_auto_mouse_owned = false;

static void scroll_mode_lock_attach_auto_mouse(void) {
    if (get_auto_mouse_toggle()) {
        scroll_mode_auto_mouse_owned = false;
        return;
    }

    auto_mouse_toggle();
    scroll_mode_auto_mouse_owned = true;
}

static void scroll_mode_lock_detach_auto_mouse(void) {
    if (scroll_mode_auto_mouse_owned && get_auto_mouse_toggle()) {
        auto_mouse_toggle();
    }

    scroll_mode_auto_mouse_owned = false;
}
#endif

static bool pinch_command_registered = false;

static void pinch_mode_register_command(void) {
    if (pinch_command_registered) {
        return;
    }

    add_weak_mods(MOD_BIT(KC_LEFT_GUI));
    send_keyboard_report();
    pinch_command_registered = true;
}

static void pinch_mode_unregister_command(void) {
    if (!pinch_command_registered) {
        return;
    }

    del_weak_mods(MOD_BIT(KC_LEFT_GUI));
    send_keyboard_report();
    pinch_command_registered = false;
}

const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {
    {PD_MODE_DRAGSCROLL, DRAGSCROLL, LOCK_PD_MODE(DRAGSCROLL), NULL, NULL, NULL},
    {PD_MODE_VOLUME, VOLUME_MODE, LOCK_PD_MODE(VOLUME_MODE), handle_volume_mode, NULL, reset_volume_mode},
    {PD_MODE_BRIGHTNESS, BRIGHTNESS_MODE, LOCK_PD_MODE(BRIGHTNESS_MODE), handle_brightness_mode, NULL, reset_brightness_mode},
    {PD_MODE_ZOOM, ZOOM_MODE, LOCK_PD_MODE(ZOOM_MODE), handle_zoom_mode, NULL, reset_zoom_mode},
    {PD_MODE_ARROW, ARROW_MODE, LOCK_PD_MODE(ARROW_MODE), handle_arrow_mode, handle_arrow_mode_key, reset_arrow_mode},
    {PD_MODE_PINCH, PINCH_MODE, LOCK_PD_MODE(PINCH_MODE), NULL, NULL, NULL},
};

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
    if (mode == PD_MODE_DRAGSCROLL || mode == PD_MODE_PINCH) {
        charybdis_set_pointer_dragscroll_enabled(true);
    }
    if (mode == PD_MODE_PINCH) {
        pinch_mode_register_command();
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

    if (mode == PD_MODE_PINCH) {
        pinch_mode_unregister_command();
    }

    if (mode == PD_MODE_DRAGSCROLL || mode == PD_MODE_PINCH) {
        charybdis_set_pointer_dragscroll_enabled(false);
    }
}

void pd_mode_lock(uint8_t mode) {
    pd_mode_set_locked(mode);
    pd_mode_activate(mode);

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (mode == PD_MODE_DRAGSCROLL || mode == PD_MODE_PINCH) {
        scroll_mode_lock_attach_auto_mouse();
    }
#endif
}

void pd_mode_unlock(uint8_t mode) {
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (mode == PD_MODE_DRAGSCROLL || mode == PD_MODE_PINCH) {
        scroll_mode_lock_detach_auto_mouse();
    }
#endif

    pd_mode_clear_locked(mode);
    pd_mode_deactivate(mode);
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

uint8_t pd_mode_first_active_index(void) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag)) return i;
    }

    return PD_MODE_COUNT;
}
