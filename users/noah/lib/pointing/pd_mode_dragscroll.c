// ────────────────────────────────────────────────────────────────────────────
// PD Mode Dragscroll
// ────────────────────────────────────────────────────────────────────────────

#include "pd_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

#    include "pd_mode_handler_common.h"

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
#    ifndef NOAH_DRAGSCROLL_AXIS_LOCK_TIMEOUT_MS
#        define NOAH_DRAGSCROLL_AXIS_LOCK_TIMEOUT_MS 40
#    endif
#    ifndef NOAH_DRAGSCROLL_HORIZONTAL_STEP_DIVISOR
#        define NOAH_DRAGSCROLL_HORIZONTAL_STEP_DIVISOR CHARYBDIS_SCROLL_STEP_DIVISOR
#    endif
#    ifndef NOAH_DRAGSCROLL_VERTICAL_STEP_DIVISOR
#        define NOAH_DRAGSCROLL_VERTICAL_STEP_DIVISOR CHARYBDIS_SCROLL_STEP_DIVISOR
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

static bool dragscroll_buffer_ready(int32_t value) {
    return dragscroll_abs32(value) > CHARYBDIS_DRAGSCROLL_BUFFER_SIZE;
}

static int32_t dragscroll_step_divisor(bool horizontal) {
    int32_t divisor = horizontal ? NOAH_DRAGSCROLL_HORIZONTAL_STEP_DIVISOR : NOAH_DRAGSCROLL_VERTICAL_STEP_DIVISOR;
    return divisor > 0 ? divisor : 1;
}

static int32_t dragscroll_consume(int32_t *buffer, bool horizontal) {
    int32_t divisor = dragscroll_step_divisor(horizontal);
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

static void dragscroll_decay_cross_axis(int32_t *buffer) {
    if (*buffer == 0) {
        return;
    }

    *buffer /= NOAH_DRAGSCROLL_CROSS_AXIS_DECAY_DIVISOR;
    if (!dragscroll_buffer_ready(*buffer)) {
        *buffer = 0;
    }
}

static dragscroll_axis_t dragscroll_try_lock_axis(int32_t abs_x, int32_t abs_y) {
    if (abs_x == 0 && abs_y == 0) {
        return DRAGSCROLL_AXIS_NONE;
    }

    if (abs_x >= abs_y * CHARYBDIS_SCROLL_SNAP_RATIO) {
        return DRAGSCROLL_AXIS_X;
    }

    if (abs_y >= abs_x * CHARYBDIS_SCROLL_SNAP_RATIO) {
        return DRAGSCROLL_AXIS_Y;
    }

    return DRAGSCROLL_AXIS_NONE;
}

static void dragscroll_refresh_axis_lock(void) {
    int32_t abs_x = dragscroll_abs32(dragscroll_state.buffer_x);
    int32_t abs_y = dragscroll_abs32(dragscroll_state.buffer_y);

    if (dragscroll_state.locked_axis == DRAGSCROLL_AXIS_X) {
        if (dragscroll_buffer_ready(dragscroll_state.buffer_x)) {
            return;
        }

        if (abs_y >= abs_x * CHARYBDIS_SCROLL_SNAP_RATIO) {
            dragscroll_state.locked_axis = DRAGSCROLL_AXIS_Y;
        } else if (!dragscroll_buffer_ready(dragscroll_state.buffer_y)) {
            dragscroll_state.locked_axis = DRAGSCROLL_AXIS_NONE;
        }
    } else if (dragscroll_state.locked_axis == DRAGSCROLL_AXIS_Y) {
        if (dragscroll_buffer_ready(dragscroll_state.buffer_y)) {
            return;
        }

        if (abs_x >= abs_y * CHARYBDIS_SCROLL_SNAP_RATIO) {
            dragscroll_state.locked_axis = DRAGSCROLL_AXIS_X;
        } else if (!dragscroll_buffer_ready(dragscroll_state.buffer_x)) {
            dragscroll_state.locked_axis = DRAGSCROLL_AXIS_NONE;
        }
    }

    if (dragscroll_state.locked_axis == DRAGSCROLL_AXIS_NONE) {
        dragscroll_state.locked_axis = dragscroll_try_lock_axis(abs_x, abs_y);
    }
}

static bool dragscroll_emit_axis(report_mouse_t *mouse_report, bool horizontal) {
    int32_t *buffer = horizontal ? &dragscroll_state.buffer_x : &dragscroll_state.buffer_y;
    int32_t  step   = dragscroll_consume(buffer, horizontal);

    if (step == 0) {
        return false;
    }

    if (horizontal) {
        mouse_report->h = dragscroll_clamp_hv((int32_t)mouse_report->h + step);
    } else {
        mouse_report->v = dragscroll_clamp_hv((int32_t)mouse_report->v + step);
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
    } else if (dragscroll_state.locked_axis != DRAGSCROLL_AXIS_NONE && timer_elapsed32(dragscroll_state.last_motion_time) > NOAH_DRAGSCROLL_AXIS_LOCK_TIMEOUT_MS) {
        dragscroll_state.locked_axis = DRAGSCROLL_AXIS_NONE;
    }

    mouse_report.x = 0;
    mouse_report.y = 0;

    if (dragscroll_state.buffer_x != 0 || dragscroll_state.buffer_y != 0) {
        if (timer_elapsed32(dragscroll_state.last_motion_time) > CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS) {
            reset_dragscroll_mode();
            return pd_mode_freeze_mouse();
        }
    }

    if (timer_elapsed32(dragscroll_state.last_scroll_time) < CHARYBDIS_SCROLL_RATE_LIMIT_MS) {
        return mouse_report;
    }

    if (!dragscroll_buffer_ready(dragscroll_state.buffer_x) && !dragscroll_buffer_ready(dragscroll_state.buffer_y)) {
        return mouse_report;
    }

    dragscroll_refresh_axis_lock();

    bool emitted = false;
    if (dragscroll_state.locked_axis == DRAGSCROLL_AXIS_X) {
        emitted = dragscroll_emit_axis(&mouse_report, true);
        if (emitted) {
            dragscroll_decay_cross_axis(&dragscroll_state.buffer_y);
        }
    } else if (dragscroll_state.locked_axis == DRAGSCROLL_AXIS_Y) {
        emitted = dragscroll_emit_axis(&mouse_report, false);
        if (emitted) {
            dragscroll_decay_cross_axis(&dragscroll_state.buffer_x);
        }
    } else {
        if (dragscroll_buffer_ready(dragscroll_state.buffer_x)) {
            emitted = dragscroll_emit_axis(&mouse_report, true) || emitted;
        }
        if (dragscroll_buffer_ready(dragscroll_state.buffer_y)) {
            emitted = dragscroll_emit_axis(&mouse_report, false) || emitted;
        }
    }

    if (emitted) {
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
