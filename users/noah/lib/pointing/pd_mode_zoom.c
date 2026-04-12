// ────────────────────────────────────────────────────────────────────────────
// PD Mode Zoom
// ────────────────────────────────────────────────────────────────────────────

#include "pd_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

#    include "pd_mode_handler_common.h"

#    ifndef ZOOM_THRESHOLD
#        define ZOOM_THRESHOLD 80
#    endif

static pd_mode_axis_state_t zoom_axis = {0};

report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    return pd_mode_handle_vertical_threshold(mouse_report, &zoom_axis, G(KC_MINS), G(KC_EQL), ZOOM_THRESHOLD, pd_mode_tap_code);
}

void reset_zoom_mode(void) {
    pd_mode_axis_reset(&zoom_axis);
}

#else

report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

void reset_zoom_mode(void) {}

#endif // POINTING_DEVICE_ENABLE
