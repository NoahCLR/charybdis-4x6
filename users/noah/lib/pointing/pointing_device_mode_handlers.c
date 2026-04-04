// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Mode Handlers
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../state/keyboard_mod_state.h"
#include "pointing_device_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

static inline report_mouse_t freeze_mouse(void) {
    return (report_mouse_t){0};
}

typedef void (*pd_mode_tap_fn_t)(uint16_t keycode);

typedef struct {
    int32_t accumulator;
    int8_t  last_dir;
} pd_mode_axis_state_t;

static void pd_mode_tap_code(uint16_t keycode) {
    tap_code(keycode);
}

static void pd_mode_tap_code16(uint16_t keycode) {
    tap_code16(keycode);
}

static void pd_mode_axis_reset(pd_mode_axis_state_t *state) {
    state->accumulator = 0;
    state->last_dir    = 0;
}

static void pd_mode_axis_emit(pd_mode_axis_state_t *state, int16_t delta, uint16_t positive, uint16_t negative, uint16_t threshold, pd_mode_tap_fn_t dispatch) {
    if (delta != 0) {
        int8_t dir = (delta > 0) ? 1 : -1;
        if (state->last_dir != 0 && dir != state->last_dir) state->accumulator = 0;
        state->last_dir = dir;
    }

    state->accumulator += delta;
    while (state->accumulator >= threshold) {
        dispatch(positive);
        state->accumulator -= threshold;
    }
    while (state->accumulator <= -(int32_t)threshold) {
        dispatch(negative);
        state->accumulator += threshold;
    }
}

static report_mouse_t pd_mode_handle_vertical_threshold(report_mouse_t mouse_report, pd_mode_axis_state_t *state, uint16_t positive, uint16_t negative, uint16_t threshold, pd_mode_tap_fn_t dispatch) {
    pd_mode_axis_emit(state, mouse_report.y, positive, negative, threshold, dispatch);
    return freeze_mouse();
}

// ─── Volume mode ────────────────────────────────────────────────────────────

#    ifndef VOLUME_THRESHOLD
#        define VOLUME_THRESHOLD 60
#    endif

static pd_mode_axis_state_t volume_axis = {0};

report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    return pd_mode_handle_vertical_threshold(mouse_report, &volume_axis, KC_AUDIO_VOL_DOWN, KC_AUDIO_VOL_UP, VOLUME_THRESHOLD, pd_mode_tap_code);
}

void reset_volume_mode(void) {
    pd_mode_axis_reset(&volume_axis);
}

// ─── Brightness mode ────────────────────────────────────────────────────────

#    ifndef BRIGHTNESS_THRESHOLD
#        define BRIGHTNESS_THRESHOLD 60
#    endif

static pd_mode_axis_state_t brightness_axis = {0};

report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    return pd_mode_handle_vertical_threshold(mouse_report, &brightness_axis, KC_BRID, KC_BRIU, BRIGHTNESS_THRESHOLD, pd_mode_tap_code);
}

void reset_brightness_mode(void) {
    pd_mode_axis_reset(&brightness_axis);
}

// ─── Zoom mode ──────────────────────────────────────────────────────────────

#    ifndef ZOOM_THRESHOLD
#        define ZOOM_THRESHOLD 80
#    endif

static pd_mode_axis_state_t zoom_axis = {0};

report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    return pd_mode_handle_vertical_threshold(mouse_report, &zoom_axis, G(KC_MINS), G(KC_EQL), ZOOM_THRESHOLD, pd_mode_tap_code16);
}

void reset_zoom_mode(void) {
    pd_mode_axis_reset(&zoom_axis);
}

// ─── Arrow mode ─────────────────────────────────────────────────────────────

#    ifndef ARROW_THRESHOLD_X
#        define ARROW_THRESHOLD_X 40
#    endif
#    ifndef ARROW_THRESHOLD_Y
#        define ARROW_THRESHOLD_Y 50
#    endif

static pd_mode_axis_state_t arrow_x_axis       = {0};
static pd_mode_axis_state_t arrow_y_axis       = {0};
static bool                 arrow_axis_is_x     = true;
static uint8_t              arrow_shift_buttons = 0;
static bool                 arrow_shift_held    = false;

#    define ARROW_SELECTION_SHIFT_MOD MOD_BIT(KC_RSFT)

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

static void arrow_send_shortcut(uint16_t shortcut) {
    keyboard_mod_state_t saved = keyboard_mod_state_suspend();
    tap_code16(shortcut);
    keyboard_mod_state_apply(saved);
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
        pd_mode_axis_emit(&arrow_x_axis, dx, KC_RIGHT, KC_LEFT, ARROW_THRESHOLD_X, pd_mode_tap_code);

        if (dy != 0) {
            pd_mode_axis_reset(&arrow_y_axis);
        }
    } else {
        pd_mode_axis_emit(&arrow_y_axis, dy, KC_DOWN, KC_UP, ARROW_THRESHOLD_Y, pd_mode_tap_code);

        if (dx != 0) {
            pd_mode_axis_reset(&arrow_x_axis);
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
    pd_mode_axis_reset(&arrow_x_axis);
    pd_mode_axis_reset(&arrow_y_axis);
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
