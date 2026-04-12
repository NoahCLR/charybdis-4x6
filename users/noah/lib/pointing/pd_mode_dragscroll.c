// ────────────────────────────────────────────────────────────────────────────
// PD Mode Dragscroll
// ────────────────────────────────────────────────────────────────────────────

#include "pd_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

#    include "pd_mode_handler_common.h"

#    include <stdbool.h>
#    include <limits.h>

#    ifndef CHARYBDIS_DRAGSCROLL_BUFFER_SIZE
#        define CHARYBDIS_DRAGSCROLL_BUFFER_SIZE 6
#    endif
#    ifndef CHARYBDIS_SCROLL_RATE_LIMIT_MS
#        define CHARYBDIS_SCROLL_RATE_LIMIT_MS 16
#    endif
#    ifndef CHARYBDIS_SCROLL_SNAP_RATIO
#        define CHARYBDIS_SCROLL_SNAP_RATIO 3
#    endif
#    ifndef CHARYBDIS_SCROLL_STEP_DIVISOR
#        define CHARYBDIS_SCROLL_STEP_DIVISOR 8
#    endif
#    ifndef CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS
#        define CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS 100
#    endif
#    ifndef NOAH_DRAGSCROLL_THRESHOLD_H
#        define NOAH_DRAGSCROLL_THRESHOLD_H CHARYBDIS_DRAGSCROLL_BUFFER_SIZE
#    endif
#    ifndef NOAH_DRAGSCROLL_THRESHOLD_V
#        define NOAH_DRAGSCROLL_THRESHOLD_V CHARYBDIS_DRAGSCROLL_BUFFER_SIZE
#    endif
#    ifndef NOAH_DRAGSCROLL_DIVISOR_H
#        define NOAH_DRAGSCROLL_DIVISOR_H CHARYBDIS_SCROLL_STEP_DIVISOR
#    endif
#    ifndef NOAH_DRAGSCROLL_DIVISOR_V
#        define NOAH_DRAGSCROLL_DIVISOR_V CHARYBDIS_SCROLL_STEP_DIVISOR
#    endif
#    ifndef NOAH_DRAGSCROLL_LOCK_START_RATIO_NUM
#        define NOAH_DRAGSCROLL_LOCK_START_RATIO_NUM CHARYBDIS_SCROLL_SNAP_RATIO
#    endif
#    ifndef NOAH_DRAGSCROLL_LOCK_START_RATIO_DEN
#        define NOAH_DRAGSCROLL_LOCK_START_RATIO_DEN 1
#    endif
#    ifndef NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_NUM
#        define NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_NUM CHARYBDIS_SCROLL_SNAP_RATIO
#    endif
#    ifndef NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_DEN
#        define NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_DEN 1
#    endif
#    ifndef NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS
#        ifdef NOAH_DRAGSCROLL_AXIS_LOCK_TIMEOUT_MS
#            define NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS NOAH_DRAGSCROLL_AXIS_LOCK_TIMEOUT_MS
#        else
#            define NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS 40
#        endif
#    endif
#    ifndef NOAH_DRAGSCROLL_CROSS_AXIS_DECAY_DIVISOR
#        define NOAH_DRAGSCROLL_CROSS_AXIS_DECAY_DIVISOR 2
#    endif

#    ifndef MOUSE_REPORT_HV_MIN
#        define MOUSE_REPORT_HV_MIN INT8_MIN
#    endif
#    ifndef MOUSE_REPORT_HV_MAX
#        define MOUSE_REPORT_HV_MAX INT8_MAX
#    endif

typedef enum {
    DRAGSCROLL_AXIS_NONE = 0,
    DRAGSCROLL_AXIS_X,
    DRAGSCROLL_AXIS_Y,
} dragscroll_axis_t;

typedef struct {
    int32_t           buffer_x;
    int32_t           buffer_y;
    uint32_t          last_motion_time;
    uint32_t          last_scroll_time;
    dragscroll_axis_t locked_axis;
} dragscroll_state_t;

static dragscroll_state_t dragscroll_state = {0};

static int32_t dragscroll_abs32(int32_t value) {
    return value >= 0 ? value : -value;
}

static int32_t dragscroll_axis_threshold(dragscroll_axis_t axis) {
    return axis == DRAGSCROLL_AXIS_X ? NOAH_DRAGSCROLL_THRESHOLD_H : NOAH_DRAGSCROLL_THRESHOLD_V;
}

static int32_t dragscroll_axis_divisor(dragscroll_axis_t axis) {
    int32_t divisor = axis == DRAGSCROLL_AXIS_X ? NOAH_DRAGSCROLL_DIVISOR_H : NOAH_DRAGSCROLL_DIVISOR_V;
    return divisor > 0 ? divisor : 1;
}

static bool dragscroll_axis_above_threshold(dragscroll_axis_t axis, int32_t value_abs) {
    return value_abs >= dragscroll_axis_threshold(axis);
}

static bool dragscroll_axis_ratio_satisfied(int32_t axis_abs, int32_t other_abs, uint16_t ratio_num, uint16_t ratio_den) {
    return (int64_t)axis_abs * ratio_den >= (int64_t)other_abs * ratio_num;
}

static bool dragscroll_axis_meets_start(dragscroll_axis_t axis, int32_t axis_abs, int32_t other_abs) {
    return dragscroll_axis_above_threshold(axis, axis_abs) &&
           dragscroll_axis_ratio_satisfied(axis_abs, other_abs, NOAH_DRAGSCROLL_LOCK_START_RATIO_NUM, NOAH_DRAGSCROLL_LOCK_START_RATIO_DEN);
}

static bool dragscroll_axis_meets_sustain(dragscroll_axis_t axis, int32_t axis_abs, int32_t other_abs) {
    return dragscroll_axis_above_threshold(axis, axis_abs) &&
           dragscroll_axis_ratio_satisfied(axis_abs, other_abs, NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_NUM, NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_DEN);
}

static int32_t dragscroll_consume(int32_t *buffer, dragscroll_axis_t axis) {
    int32_t divisor = dragscroll_axis_divisor(axis);
    int32_t step    = *buffer / divisor;

    if (step == 0) {
        return 0;
    }

    *buffer -= step * divisor;
    return step;
}

static int32_t dragscroll_clamp_hv(int32_t value) {
    if (value < MOUSE_REPORT_HV_MIN) {
        return MOUSE_REPORT_HV_MIN;
    }

    if (value > MOUSE_REPORT_HV_MAX) {
        return MOUSE_REPORT_HV_MAX;
    }

    return value;
}

static void dragscroll_decay_cross_axis(int32_t *buffer, dragscroll_axis_t axis) {
    if (*buffer == 0) {
        return;
    }

    *buffer /= NOAH_DRAGSCROLL_CROSS_AXIS_DECAY_DIVISOR;
    if (!dragscroll_axis_above_threshold(axis, dragscroll_abs32(*buffer))) {
        *buffer = 0;
    }
}

static dragscroll_axis_t dragscroll_choose_start_axis(int32_t abs_x, int32_t abs_y) {
    bool can_start_x = dragscroll_axis_meets_start(DRAGSCROLL_AXIS_X, abs_x, abs_y);
    bool can_start_y = dragscroll_axis_meets_start(DRAGSCROLL_AXIS_Y, abs_y, abs_x);

    if (can_start_x == can_start_y) {
        return DRAGSCROLL_AXIS_NONE;
    }

    if (can_start_x) {
        return DRAGSCROLL_AXIS_X;
    }

    return DRAGSCROLL_AXIS_Y;
}

static bool dragscroll_gesture_active(bool had_motion, uint32_t motion_age) {
    return had_motion || motion_age <= NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS;
}

static dragscroll_axis_t dragscroll_opposite_axis(dragscroll_axis_t axis) {
    return axis == DRAGSCROLL_AXIS_X ? DRAGSCROLL_AXIS_Y : DRAGSCROLL_AXIS_X;
}

static bool dragscroll_refresh_axis_lock(bool had_motion, uint32_t motion_age) {
    int32_t abs_x          = dragscroll_abs32(dragscroll_state.buffer_x);
    int32_t abs_y          = dragscroll_abs32(dragscroll_state.buffer_y);
    bool    gesture_active = dragscroll_gesture_active(had_motion, motion_age);

    if (dragscroll_state.locked_axis == DRAGSCROLL_AXIS_NONE) {
        if (!gesture_active) {
            return false;
        }

        dragscroll_state.locked_axis = dragscroll_choose_start_axis(abs_x, abs_y);
        return dragscroll_state.locked_axis != DRAGSCROLL_AXIS_NONE;
    }

    dragscroll_axis_t locked_axis   = dragscroll_state.locked_axis;
    dragscroll_axis_t opposite_axis = dragscroll_opposite_axis(locked_axis);
    int32_t           locked_abs    = locked_axis == DRAGSCROLL_AXIS_X ? abs_x : abs_y;
    int32_t           opposite_abs  = opposite_axis == DRAGSCROLL_AXIS_X ? abs_x : abs_y;

    if (!gesture_active) {
        dragscroll_state.locked_axis = DRAGSCROLL_AXIS_NONE;
        return false;
    }

    if (dragscroll_axis_meets_sustain(locked_axis, locked_abs, opposite_abs)) {
        return true;
    }

    if (!dragscroll_axis_above_threshold(DRAGSCROLL_AXIS_X, abs_x) && !dragscroll_axis_above_threshold(DRAGSCROLL_AXIS_Y, abs_y)) {
        dragscroll_state.locked_axis = DRAGSCROLL_AXIS_NONE;
        return false;
    }

    if (opposite_axis == DRAGSCROLL_AXIS_X) {
        if (dragscroll_axis_meets_start(DRAGSCROLL_AXIS_X, abs_x, abs_y)) {
            dragscroll_state.locked_axis = DRAGSCROLL_AXIS_X;
            return true;
        }
    } else {
        if (dragscroll_axis_meets_start(DRAGSCROLL_AXIS_Y, abs_y, abs_x)) {
            dragscroll_state.locked_axis = DRAGSCROLL_AXIS_Y;
            return true;
        }
    }

    // Keep the current lock through ambiguous wobble. Releasing here makes the
    // gesture stutter whenever the user briefly moves near-diagonal without
    // actually intending to switch axes.
    return true;
}

static bool dragscroll_emit_locked_axis(report_mouse_t *mouse_report) {
    dragscroll_axis_t locked_axis = dragscroll_state.locked_axis;
    int32_t          *buffer      = locked_axis == DRAGSCROLL_AXIS_X ? &dragscroll_state.buffer_x : &dragscroll_state.buffer_y;
    int32_t           step        = dragscroll_consume(buffer, locked_axis);

    if (step == 0) {
        return false;
    }

    if (locked_axis == DRAGSCROLL_AXIS_X) {
        mouse_report->h = dragscroll_clamp_hv((int32_t)mouse_report->h + step);
        dragscroll_decay_cross_axis(&dragscroll_state.buffer_y, DRAGSCROLL_AXIS_Y);
    } else if (locked_axis == DRAGSCROLL_AXIS_Y) {
        mouse_report->v = dragscroll_clamp_hv((int32_t)mouse_report->v + step);
        dragscroll_decay_cross_axis(&dragscroll_state.buffer_x, DRAGSCROLL_AXIS_X);
    } else {
        return false;
    }

    return true;
}

report_mouse_t handle_dragscroll_mode(report_mouse_t mouse_report) {
    bool     had_motion = mouse_report.x != 0 || mouse_report.y != 0;
    uint32_t now        = timer_read32();

    if (had_motion) {
#    ifdef CHARYBDIS_DRAGSCROLL_REVERSE_X
        dragscroll_state.buffer_x -= mouse_report.x;
#    else
        dragscroll_state.buffer_x += mouse_report.x;
#    endif

#    ifdef CHARYBDIS_DRAGSCROLL_REVERSE_Y
        dragscroll_state.buffer_y -= mouse_report.y;
#    else
        dragscroll_state.buffer_y += mouse_report.y;
#    endif

        dragscroll_state.last_motion_time = now;
    }

    mouse_report.x = 0;
    mouse_report.y = 0;

    if (dragscroll_state.buffer_x != 0 || dragscroll_state.buffer_y != 0) {
        if (timer_elapsed32(dragscroll_state.last_motion_time) > CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS) {
            dragscroll_state.locked_axis = DRAGSCROLL_AXIS_NONE;
            dragscroll_state.buffer_x    = 0;
            dragscroll_state.buffer_y    = 0;
            return mouse_report;
        }
    }

    if (timer_elapsed32(dragscroll_state.last_scroll_time) < CHARYBDIS_SCROLL_RATE_LIMIT_MS) {
        return mouse_report;
    }

    uint32_t motion_age = timer_elapsed32(dragscroll_state.last_motion_time);
    if (!dragscroll_refresh_axis_lock(had_motion, motion_age)) {
        return mouse_report;
    }

    if (dragscroll_emit_locked_axis(&mouse_report)) {
        dragscroll_state.last_scroll_time = now;
    }

    return mouse_report;
}

void reset_dragscroll_mode(void) {
    dragscroll_state = (dragscroll_state_t){0};
}

#else

report_mouse_t handle_dragscroll_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

void reset_dragscroll_mode(void) {}

#endif // POINTING_DEVICE_ENABLE
