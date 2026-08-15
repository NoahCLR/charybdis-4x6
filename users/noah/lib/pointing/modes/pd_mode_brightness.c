// ────────────────────────────────────────────────────────────────────────────
// PD Mode Brightness
// ────────────────────────────────────────────────────────────────────────────

#include "pd_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

#    include "pd_mode_handler_common.h"

#    ifndef BRIGHTNESS_THRESHOLD
#        define BRIGHTNESS_THRESHOLD 60
#    endif

PD_MODE_VALIDATE_AXIS_THRESHOLD(BRIGHTNESS_THRESHOLD);

static pd_mode_axis_state_t brightness_axis = {0};

report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    return pd_mode_handle_vertical_threshold(mouse_report, &brightness_axis, KC_BRID, KC_BRIU, BRIGHTNESS_THRESHOLD, pd_mode_tap_code);
}

void reset_brightness_mode(void) {
    pd_mode_axis_reset(&brightness_axis);
}

void pd_mode_brightness_debug_snapshot(pd_mode_axis_debug_snapshot_t *out) {
    pd_mode_axis_debug_snapshot(&brightness_axis, BRIGHTNESS_THRESHOLD, out);
}

#else

report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

void reset_brightness_mode(void) {}

void pd_mode_brightness_debug_snapshot(pd_mode_axis_debug_snapshot_t *out) {
    if (out != NULL) {
        *out = (pd_mode_axis_debug_snapshot_t){0};
    }
}

#endif // POINTING_DEVICE_ENABLE
