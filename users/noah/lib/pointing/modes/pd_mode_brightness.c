// ────────────────────────────────────────────────────────────────────────────
// PD Mode Brightness
// ────────────────────────────────────────────────────────────────────────────

#include "pd_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

#    include "pd_mode_handler_common.h"

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

#else

report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

void reset_brightness_mode(void) {}

#endif // POINTING_DEVICE_ENABLE
