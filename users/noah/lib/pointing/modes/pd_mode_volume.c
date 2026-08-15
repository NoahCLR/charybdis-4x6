// ────────────────────────────────────────────────────────────────────────────
// PD Mode Volume
// ────────────────────────────────────────────────────────────────────────────

#include "pd_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

#    include "pd_mode_handler_common.h"

#    ifndef VOLUME_THRESHOLD
#        define VOLUME_THRESHOLD 60
#    endif

PD_MODE_VALIDATE_AXIS_THRESHOLD(VOLUME_THRESHOLD);

static pd_mode_axis_state_t volume_axis = {0};

report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    return pd_mode_handle_vertical_threshold(mouse_report, &volume_axis, KC_AUDIO_VOL_DOWN, KC_AUDIO_VOL_UP, VOLUME_THRESHOLD, pd_mode_tap_code);
}

void reset_volume_mode(void) {
    pd_mode_axis_reset(&volume_axis);
}

void pd_mode_volume_debug_snapshot(pd_mode_axis_debug_snapshot_t *out) {
    pd_mode_axis_debug_snapshot(&volume_axis, VOLUME_THRESHOLD, out);
}

#else

report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

void reset_volume_mode(void) {}

void pd_mode_volume_debug_snapshot(pd_mode_axis_debug_snapshot_t *out) {
    if (out != NULL) {
        *out = (pd_mode_axis_debug_snapshot_t){0};
    }
}

#endif // POINTING_DEVICE_ENABLE
