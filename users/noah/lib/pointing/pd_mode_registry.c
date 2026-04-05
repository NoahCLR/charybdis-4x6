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

static void pd_mode_auto_mouse_sync_anchor(bool should_anchor) {
    static bool pd_mode_auto_mouse_anchor_active = false;

    if (pd_mode_auto_mouse_anchor_active == should_anchor) {
        return;
    }

    // Pd-mode handlers freeze the outgoing mouse report, so locked modes need
    // an explicit auto-mouse anchor to behave like their physically-held form.
    auto_mouse_keyevent(should_anchor);
    pd_mode_auto_mouse_anchor_active = should_anchor;
}

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

// Per-mode pointer DPI overrides. Override any of these in config.h.
// 0 = no override: normal pointer DPI is used while that mode is active.
// Dragscroll and pinch are excluded — Charybdis manages their CPI internally.
#ifndef PD_MODE_VOLUME_DPI
#    define PD_MODE_VOLUME_DPI 0
#endif
#ifndef PD_MODE_BRIGHTNESS_DPI
#    define PD_MODE_BRIGHTNESS_DPI 0
#endif
#ifndef PD_MODE_ZOOM_DPI
#    define PD_MODE_ZOOM_DPI 0
#endif
#ifndef PD_MODE_ARROW_DPI
#    define PD_MODE_ARROW_DPI 0
#endif

const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {
    {PD_MODE_DRAGSCROLL, DRAGSCROLL,      LOCK_PD_MODE(DRAGSCROLL),      NULL,                   NULL,                  NULL,                  0                    },
    {PD_MODE_VOLUME,     VOLUME_MODE,     LOCK_PD_MODE(VOLUME_MODE),     handle_volume_mode,     NULL,                  reset_volume_mode,     PD_MODE_VOLUME_DPI   },
    {PD_MODE_BRIGHTNESS, BRIGHTNESS_MODE, LOCK_PD_MODE(BRIGHTNESS_MODE), handle_brightness_mode, NULL,                  reset_brightness_mode, PD_MODE_BRIGHTNESS_DPI},
    {PD_MODE_ZOOM,       ZOOM_MODE,       LOCK_PD_MODE(ZOOM_MODE),       handle_zoom_mode,        NULL,                  reset_zoom_mode,       PD_MODE_ZOOM_DPI     },
    {PD_MODE_ARROW,      ARROW_MODE,      LOCK_PD_MODE(ARROW_MODE),      handle_arrow_mode,      handle_arrow_mode_key, reset_arrow_mode,      PD_MODE_ARROW_DPI    },
    {PD_MODE_PINCH,      PINCH_MODE,      LOCK_PD_MODE(PINCH_MODE),      NULL,                   NULL,                  NULL,                  0                    },
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

void pd_mode_apply_active_dpi(void) {
    // Sniping and dragscroll manage their own CPI through Charybdis internals.
    if (charybdis_get_pointer_dragscroll_enabled()) {
        return;
    }

    if (charybdis_get_pointer_sniping_enabled()) {
        return;
    }

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag) && pd_modes[i].dpi != 0) {
            pointing_device_set_cpi(pd_modes[i].dpi);
            return;
        }
    }

    // No active mode with a custom DPI — restore Charybdis's configured default.
    pointing_device_set_cpi(charybdis_get_pointer_default_dpi());
}

void pd_mode_activate(uint8_t mode) {
    bool was_any_mode_active = pd_any_mode_active();

    pd_mode_set(mode);

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (!was_any_mode_active) {
        pd_mode_auto_mouse_sync_anchor(true);
    }
#endif

    if (mode == PD_MODE_DRAGSCROLL || mode == PD_MODE_PINCH) {
        charybdis_set_pointer_dragscroll_enabled(true);
        // Charybdis sets CHARYBDIS_DRAGSCROLL_DPI via maybe_update_pointing_device_cpi().
    } else {
        pd_mode_apply_active_dpi();
    }
    if (mode == PD_MODE_PINCH) {
        pinch_mode_register_command();
    }
}

void pd_mode_deactivate(uint8_t mode) {
    bool was_any_mode_active = pd_any_mode_active();

    pd_mode_clear(mode);

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (was_any_mode_active && !pd_any_mode_active()) {
        pd_mode_auto_mouse_sync_anchor(false);
    }
#endif

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
        // Charybdis restores normal pointer DPI via maybe_update_pointing_device_cpi().
    } else {
        pd_mode_apply_active_dpi();
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
