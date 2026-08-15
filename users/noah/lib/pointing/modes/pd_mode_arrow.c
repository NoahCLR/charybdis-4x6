// ────────────────────────────────────────────────────────────────────────────
// PD Mode Arrow
// ────────────────────────────────────────────────────────────────────────────

#include "pd_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

#    include "../../action/action_dispatch.h"
#    include "../../action/owned_keycode.h"
#    include "pd_mode_handler_common.h"

#    define ARROW_VERTICAL_MASKED_MODS (MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_RIGHT_ALT))
#    define ARROW_SELECTION_SHIFT_KEYCODE KC_RIGHT_SHIFT

#    ifndef ARROW_THRESHOLD_X
#        define ARROW_THRESHOLD_X 40
#    endif
#    ifndef ARROW_THRESHOLD_Y
#        define ARROW_THRESHOLD_Y 50
#    endif

PD_MODE_VALIDATE_AXIS_THRESHOLD(ARROW_THRESHOLD_X);
PD_MODE_VALIDATE_AXIS_THRESHOLD(ARROW_THRESHOLD_Y);

static pd_mode_axis_state_t  arrow_x_axis        = {0};
static pd_mode_axis_state_t  arrow_y_axis        = {0};
static bool                  arrow_axis_is_x     = true;
static uint8_t               arrow_shift_buttons = 0;
static owned_keycode_lease_t arrow_shift_lease   = {0};

static void arrow_vertical_tap_code(uint16_t keycode) {
    noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(keycode, ARROW_VERTICAL_MASKED_MODS, true);
}

static void arrow_shift_sync(void) {
    bool should_hold_shift = arrow_shift_buttons != 0;

    if (should_hold_shift && !arrow_shift_lease.active) {
        (void)owned_keycode_acquire(ARROW_SELECTION_SHIFT_KEYCODE, &arrow_shift_lease);
    } else if (!should_hold_shift && arrow_shift_lease.active) {
        (void)owned_keycode_release(&arrow_shift_lease);
    }
}

static void arrow_send_shortcut(uint16_t shortcut) {
    noah_emit_literal_tap(shortcut, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS);
}

static void arrow_update_dominant_axis(int16_t dx, int16_t dy) {
    int32_t ax = dx;
    int32_t ay = dy;

    if (ax < 0) {
        ax = -ax;
    }
    if (ay < 0) {
        ay = -ay;
    }

    if (ax > ay && ax > 0) {
        arrow_axis_is_x = true;
    } else if (ay > ax && ay > 0) {
        arrow_axis_is_x = false;
    }
}

report_mouse_t handle_arrow_mode(report_mouse_t mouse_report) {
    int16_t dx               = mouse_report.x;
    int16_t dy               = mouse_report.y;
    uint8_t remaining_budget = NOAH_PD_MODE_MAX_TAPS_PER_TICK;

    arrow_update_dominant_axis(dx, dy);

    if (arrow_axis_is_x) {
        (void)pd_mode_axis_emit(&arrow_x_axis, dx, KC_RIGHT, KC_LEFT, ARROW_THRESHOLD_X, &remaining_budget, pd_mode_tap_code);

        if (dy != 0) {
            pd_mode_axis_reset(&arrow_y_axis);
        }
    } else {
        (void)pd_mode_axis_emit(&arrow_y_axis, dy, KC_DOWN, KC_UP, ARROW_THRESHOLD_Y, &remaining_budget, arrow_vertical_tap_code);

        if (dx != 0) {
            pd_mode_axis_reset(&arrow_x_axis);
        }
    }

    return pd_mode_freeze_mouse();
}

bool handle_arrow_mode_key(uint16_t keycode, keyrecord_t *record) {
    if (!IS_MOUSEKEY_BUTTON(keycode)) {
        return false;
    }

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

void pd_mode_arrow_debug_snapshot(pd_mode_arrow_debug_snapshot_t *out) {
    if (out == NULL) {
        return;
    }

    pd_mode_axis_debug_snapshot(&arrow_x_axis, ARROW_THRESHOLD_X, &out->horizontal);
    pd_mode_axis_debug_snapshot(&arrow_y_axis, ARROW_THRESHOLD_Y, &out->vertical);
    out->selected_axis_is_horizontal = arrow_axis_is_x;
}

#else

report_mouse_t handle_arrow_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

bool handle_arrow_mode_key(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void reset_arrow_mode(void) {}

void pd_mode_arrow_debug_snapshot(pd_mode_arrow_debug_snapshot_t *out) {
    if (out != NULL) {
        *out = (pd_mode_arrow_debug_snapshot_t){0};
    }
}

#endif // POINTING_DEVICE_ENABLE
