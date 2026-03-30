// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Mode Handlers
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "../../keymap_defs.h" // custom keycodes and layer enums
#include "pointing_device_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

static inline report_mouse_t freeze_mouse(void) {
    return (report_mouse_t){0};
}

// ─── Volume mode ────────────────────────────────────────────────────────────

#    ifndef VOLUME_THRESHOLD
#        define VOLUME_THRESHOLD 60
#    endif

static int32_t vol_acc_y    = 0;
static int8_t  vol_last_dir = 0;

report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    int16_t dy = mouse_report.y;

    if (dy != 0) {
        int8_t dir = (dy > 0) ? 1 : -1;
        if (vol_last_dir != 0 && dir != vol_last_dir) vol_acc_y = 0;
        vol_last_dir = dir;

        vol_acc_y += dy;
        while (vol_acc_y >= VOLUME_THRESHOLD) {
            tap_code(KC_AUDIO_VOL_DOWN);
            vol_acc_y -= VOLUME_THRESHOLD;
        }
        while (vol_acc_y <= -VOLUME_THRESHOLD) {
            tap_code(KC_AUDIO_VOL_UP);
            vol_acc_y += VOLUME_THRESHOLD;
        }
    }

    return freeze_mouse();
}

void reset_volume_mode(void) {
    vol_acc_y    = 0;
    vol_last_dir = 0;
}

// ─── Brightness mode ────────────────────────────────────────────────────────

#    ifndef BRIGHTNESS_THRESHOLD
#        define BRIGHTNESS_THRESHOLD 60
#    endif

static int32_t bright_acc_y    = 0;
static int8_t  bright_last_dir = 0;

report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    int16_t dy = mouse_report.y;

    if (dy != 0) {
        int8_t dir = (dy > 0) ? 1 : -1;
        if (bright_last_dir != 0 && dir != bright_last_dir) bright_acc_y = 0;
        bright_last_dir = dir;

        bright_acc_y += dy;
        while (bright_acc_y >= BRIGHTNESS_THRESHOLD) {
            tap_code(KC_BRID);
            bright_acc_y -= BRIGHTNESS_THRESHOLD;
        }
        while (bright_acc_y <= -BRIGHTNESS_THRESHOLD) {
            tap_code(KC_BRIU);
            bright_acc_y += BRIGHTNESS_THRESHOLD;
        }
    }

    return freeze_mouse();
}

void reset_brightness_mode(void) {
    bright_acc_y    = 0;
    bright_last_dir = 0;
}

// ─── Zoom mode ──────────────────────────────────────────────────────────────

#    ifndef ZOOM_THRESHOLD
#        define ZOOM_THRESHOLD 80
#    endif

static int32_t zoom_acc_y    = 0;
static int8_t  zoom_last_dir = 0;

report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    int16_t dy = mouse_report.y;

    if (dy != 0) {
        int8_t dir = (dy > 0) ? 1 : -1;
        if (zoom_last_dir != 0 && dir != zoom_last_dir) zoom_acc_y = 0;
        zoom_last_dir = dir;

        zoom_acc_y += dy;
        while (zoom_acc_y >= ZOOM_THRESHOLD) {
            tap_code16(G(KC_MINS));
            zoom_acc_y -= ZOOM_THRESHOLD;
        }
        while (zoom_acc_y <= -ZOOM_THRESHOLD) {
            tap_code16(G(KC_EQL));
            zoom_acc_y += ZOOM_THRESHOLD;
        }
    }

    return freeze_mouse();
}

void reset_zoom_mode(void) {
    zoom_acc_y    = 0;
    zoom_last_dir = 0;
}

// ─── Arrow mode ─────────────────────────────────────────────────────────────

#    ifndef ARROW_THRESHOLD_X
#        define ARROW_THRESHOLD_X 40
#    endif
#    ifndef ARROW_THRESHOLD_Y
#        define ARROW_THRESHOLD_Y 50
#    endif

static int32_t arrow_acc_x         = 0;
static int32_t arrow_acc_y         = 0;
static int8_t  arrow_last_x_dir    = 0;
static int8_t  arrow_last_y_dir    = 0;
static bool    arrow_axis_is_x     = true;
static uint8_t arrow_shift_buttons = 0;
static bool    arrow_shift_held    = false;

#    define ARROW_SELECTION_SHIFT_MOD MOD_BIT(KC_RSFT)

typedef struct {
    uint8_t real;
    uint8_t weak;
    uint8_t oneshot;
    uint8_t oneshot_locked;
} arrow_saved_mods_t;

static void arrow_shift_sync(void) {
    bool should_hold_shift = arrow_shift_buttons != 0;

    if (should_hold_shift && !arrow_shift_held) {
        register_mods(ARROW_SELECTION_SHIFT_MOD);
        arrow_shift_held = true;
    } else if (!should_hold_shift && arrow_shift_held) {
        unregister_mods(ARROW_SELECTION_SHIFT_MOD);
        arrow_shift_held = false;
    }
}

static arrow_saved_mods_t arrow_suspend_mods(void) {
    arrow_saved_mods_t saved = {
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };

    clear_mods();
    clear_weak_mods();
    clear_oneshot_mods();
    clear_oneshot_locked_mods();
    send_keyboard_report();

    return saved;
}

static void arrow_restore_mods(arrow_saved_mods_t saved) {
    set_mods(saved.real);
    set_weak_mods(saved.weak);
    set_oneshot_mods(saved.oneshot);
    set_oneshot_locked_mods(saved.oneshot_locked);
    send_keyboard_report();
}

static void arrow_send_shortcut(uint16_t shortcut) {
    arrow_saved_mods_t saved = arrow_suspend_mods();
    tap_code16(shortcut);
    arrow_restore_mods(saved);
}

static inline void arrow_send_key(uint16_t keycode) {
    tap_code(keycode);
}

static int32_t arrow_emit_many(int32_t delta, uint16_t positive, uint16_t negative, uint16_t threshold) {
    while (delta >= threshold) {
        arrow_send_key(positive);
        delta -= threshold;
    }
    while (delta <= -(int32_t)threshold) {
        arrow_send_key(negative);
        delta += threshold;
    }
    return delta;
}

static void arrow_update_dominant_axis(int16_t dx, int16_t dy) {
    int16_t ax = dx >= 0 ? dx : -dx;
    int16_t ay = dy >= 0 ? dy : -dy;
    if (ax > ay && ax > 0)
        arrow_axis_is_x = true;
    else if (ay > ax && ay > 0)
        arrow_axis_is_x = false;
}

report_mouse_t handle_arrow_mode(report_mouse_t mouse_report) {
    int16_t dx = mouse_report.x;
    int16_t dy = mouse_report.y;

    arrow_update_dominant_axis(dx, dy);

    if (arrow_axis_is_x) {
        if (dx != 0) {
            int8_t dir = (dx > 0) ? 1 : -1;
            if (arrow_last_x_dir != 0 && dir != arrow_last_x_dir) arrow_acc_x = 0;
            arrow_last_x_dir = dir;
        }
        arrow_acc_x += dx;
        arrow_acc_x = arrow_emit_many(arrow_acc_x, KC_RIGHT, KC_LEFT, ARROW_THRESHOLD_X);

        if (dy != 0) {
            arrow_acc_y      = 0;
            arrow_last_y_dir = 0;
        }
    } else {
        if (dy != 0) {
            int8_t dir = (dy > 0) ? 1 : -1;
            if (arrow_last_y_dir != 0 && dir != arrow_last_y_dir) arrow_acc_y = 0;
            arrow_last_y_dir = dir;
        }
        arrow_acc_y += dy;
        arrow_acc_y = arrow_emit_many(arrow_acc_y, KC_DOWN, KC_UP, ARROW_THRESHOLD_Y);

        if (dx != 0) {
            arrow_acc_x      = 0;
            arrow_last_x_dir = 0;
        }
    }

    return freeze_mouse();
}

bool handle_arrow_mode_key(uint16_t keycode, keyrecord_t *record) {
    if (!IS_MOUSEKEY_BUTTON(keycode)) return false;

    switch (keycode) {
        case MS_BTN1: {
            uint8_t button_mask = (uint8_t)(1u << (keycode - QK_MOUSE_BUTTON_1));
            if (record->event.pressed) {
                arrow_shift_buttons |= button_mask;
            } else {
                arrow_shift_buttons &= (uint8_t)~button_mask;
            }
            arrow_shift_sync();
            return true;
        }
        case MS_BTN2:
            if (record->event.pressed) {
                arrow_send_shortcut(G(KC_C));
            }
            return true;
        case MS_BTN3:
            if (record->event.pressed) {
                arrow_send_shortcut(G(KC_V));
            }
            return true;
        default:
            return false;
    }
}

void reset_arrow_mode(void) {
    arrow_acc_x         = 0;
    arrow_acc_y         = 0;
    arrow_last_x_dir    = 0;
    arrow_last_y_dir    = 0;
    arrow_axis_is_x     = true;
    arrow_shift_buttons = 0;
    arrow_shift_sync();
}

#else

report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_arrow_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

bool handle_arrow_mode_key(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void reset_volume_mode(void) {}
void reset_brightness_mode(void) {}
void reset_zoom_mode(void) {}
void reset_arrow_mode(void) {}

#endif // POINTING_DEVICE_ENABLE
