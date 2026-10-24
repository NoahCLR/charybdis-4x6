// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Dragscroll
// ────────────────────────────────────────────────────────────────────────────

#include "pointing_mode_handlers.h"

#if defined(POINTING_DEVICE_ENABLE)

#    include "pointing_mode_handler_common.h"
#    include "pointing_mode_configured.h"

#    include <stdbool.h>
#    include <limits.h>

#    ifndef MOUSE_REPORT_HV_MIN
#        define MOUSE_REPORT_HV_MIN INT8_MIN
#    endif
#    ifndef MOUSE_REPORT_HV_MAX
#        define MOUSE_REPORT_HV_MAX INT8_MAX
#    endif

// Hard cap on stored residual motion, in raw sensor counts.
//
// Reachable residuals are tiny: dragscroll_consume() drains whole steps and
// leaves less than one divisor behind, the cross-axis decay zeroes the idle
// axis, and the slot's expiry discards everything after a pause.
// The cap only guards the degenerate case where motion never resolves into an
// axis lock, so nothing ever drains while reports keep arriving. It sits far
// above any residual real motion can build, so it cannot change scroll feel,
// and far enough below INT32_MAX that a capped buffer plus one full report
// delta still cannot overflow.
#    define NOAH_DRAGSCROLL_BUFFER_LIMIT ((int32_t)1 << 24)

_Static_assert(NOAH_DRAGSCROLL_BUFFER_LIMIT > UINT16_MAX, "dragscroll residual cap must exceed any slot's scroll step");
_Static_assert(NOAH_DRAGSCROLL_BUFFER_LIMIT <= INT32_MAX - ((int32_t)INT16_MAX + 1), "dragscroll residual cap must leave headroom for one full mouse report delta");

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
    if (value >= 0) {
        return value;
    }

    // INT32_MIN has no positive counterpart, so report the largest magnitude
    // instead of negating it. Saturated buffers never reach here, but the helper
    // stays defined for the whole int32_t domain.
    return value == INT32_MIN ? INT32_MAX : -value;
}

// Saturating counterpart to the raw `+=`/`-=` on the residual buffers.
static void dragscroll_accumulate(int32_t *buffer, int32_t delta) {
    if (delta > 0 && *buffer > NOAH_DRAGSCROLL_BUFFER_LIMIT - delta) {
        *buffer = NOAH_DRAGSCROLL_BUFFER_LIMIT;
        return;
    }

    if (delta < 0 && *buffer < -NOAH_DRAGSCROLL_BUFFER_LIMIT - delta) {
        *buffer = -NOAH_DRAGSCROLL_BUFFER_LIMIT;
        return;
    }

    *buffer += delta;
}

// The configured engine supplies an immutable, validated slot for one poll.
// Every tuning value is one of the slot's fields.
static const noah_pd_config_t *configured_scroll;
#    define scroll_u16(field) ((uint16_t)configured_scroll->field)
#    define scroll_u8(field) ((uint8_t)configured_scroll->field)
// Whether the slot scrolls this axis (both, horizontal only or vertical only).
static bool dragscroll_axis_scrolls(dragscroll_axis_t locked) {
    uint8_t axes = scroll_u8(axis);
    return axes == NOAH_PD_SCROLL_BOTH || (axes == NOAH_PD_SCROLL_HORIZONTAL) == (locked == DRAGSCROLL_AXIS_X);
}

static int32_t dragscroll_axis_threshold(dragscroll_axis_t axis) {
    return axis == DRAGSCROLL_AXIS_X ? scroll_u16(scroll.threshold_h) : scroll_u16(scroll.threshold_v);
}

static int32_t dragscroll_axis_divisor(dragscroll_axis_t axis) {
    int32_t divisor = axis == DRAGSCROLL_AXIS_X ? scroll_u16(scroll.divisor_h) : scroll_u16(scroll.divisor_v);
    return divisor > 0 ? divisor : 1;
}

static bool dragscroll_axis_above_threshold(dragscroll_axis_t axis, int32_t value_abs) {
    return value_abs >= dragscroll_axis_threshold(axis);
}

static bool dragscroll_axis_ratio_satisfied(int32_t axis_abs, int32_t other_abs, uint16_t ratio_num, uint16_t ratio_den) {
    return (int64_t)axis_abs * ratio_den >= (int64_t)other_abs * ratio_num;
}

static bool dragscroll_axis_meets_start(dragscroll_axis_t axis, int32_t axis_abs, int32_t other_abs) {
    return dragscroll_axis_above_threshold(axis, axis_abs) && dragscroll_axis_ratio_satisfied(axis_abs, other_abs, scroll_u8(scroll_policy.start_num), scroll_u8(scroll_policy.start_den));
}

static bool dragscroll_axis_meets_sustain(dragscroll_axis_t axis, int32_t axis_abs, int32_t other_abs) {
    return dragscroll_axis_above_threshold(axis, axis_abs) && dragscroll_axis_ratio_satisfied(axis_abs, other_abs, scroll_u8(scroll_policy.sustain_num), scroll_u8(scroll_policy.sustain_den));
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

    *buffer /= scroll_u8(scroll_policy.decay);
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
    return had_motion || motion_age <= scroll_u16(scroll.axis_timeout_ms);
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

// A gesture locked to an axis the slot does not scroll runs as usual, consuming
// its steps and decaying the other axis, but sends nothing: a sideways swipe in
// a vertical-only mode is dropped, not leaked into the vertical drift.
static bool dragscroll_emit_locked_axis(report_mouse_t *mouse_report) {
    dragscroll_axis_t locked_axis = dragscroll_state.locked_axis;
    int32_t          *buffer      = locked_axis == DRAGSCROLL_AXIS_X ? &dragscroll_state.buffer_x : &dragscroll_state.buffer_y;
    int32_t           step        = dragscroll_consume(buffer, locked_axis);
    bool              scrolls     = dragscroll_axis_scrolls(locked_axis);

    if (step == 0) {
        return false;
    }

    if (locked_axis == DRAGSCROLL_AXIS_X) {
        if (scrolls) mouse_report->h = dragscroll_clamp_hv((int32_t)mouse_report->h + step);
        dragscroll_decay_cross_axis(&dragscroll_state.buffer_y, DRAGSCROLL_AXIS_Y);
    } else if (locked_axis == DRAGSCROLL_AXIS_Y) {
        if (scrolls) mouse_report->v = dragscroll_clamp_hv((int32_t)mouse_report->v + step);
        dragscroll_decay_cross_axis(&dragscroll_state.buffer_x, DRAGSCROLL_AXIS_X);
    } else {
        return false;
    }

    return scrolls;
}

static void dragscroll_expire_prior_state(uint32_t prior_motion_age) {
    bool has_prior_state = dragscroll_state.buffer_x != 0 || dragscroll_state.buffer_y != 0 || dragscroll_state.locked_axis != DRAGSCROLL_AXIS_NONE;

    if (!has_prior_state) {
        return;
    }

    if (prior_motion_age > scroll_u16(scroll.expiry_ms)) {
        dragscroll_state.buffer_x    = 0;
        dragscroll_state.buffer_y    = 0;
        dragscroll_state.locked_axis = DRAGSCROLL_AXIS_NONE;
        return;
    }

    if (prior_motion_age > scroll_u16(scroll.axis_timeout_ms)) {
        dragscroll_state.locked_axis = DRAGSCROLL_AXIS_NONE;
    }
}

static report_mouse_t dragscroll_step(report_mouse_t mouse_report) {
    bool     had_motion       = mouse_report.x != 0 || mouse_report.y != 0;
    uint32_t now              = timer_read32();
    uint32_t prior_motion_age = now - dragscroll_state.last_motion_time;

    // Expire the previous gesture before current motion can refresh its age.
    // Lock and residual deadlines are intentionally independent.
    dragscroll_expire_prior_state(prior_motion_age);

    if (had_motion) {
        uint8_t invert = scroll_u8(scroll_policy.invert);
        dragscroll_accumulate(&dragscroll_state.buffer_x, (invert & 1u) ? -(int32_t)mouse_report.x : (int32_t)mouse_report.x);
        dragscroll_accumulate(&dragscroll_state.buffer_y, (invert & 2u) ? -(int32_t)mouse_report.y : (int32_t)mouse_report.y);

        dragscroll_state.last_motion_time = now;
    }

    mouse_report.x = 0;
    mouse_report.y = 0;

    if (now - dragscroll_state.last_scroll_time < scroll_u16(scroll.interval_ms)) {
        return mouse_report;
    }

    uint32_t motion_age = had_motion ? 0u : prior_motion_age;
    if (!dragscroll_refresh_axis_lock(had_motion, motion_age)) {
        return mouse_report;
    }

    if (dragscroll_emit_locked_axis(&mouse_report)) {
        dragscroll_state.last_scroll_time = now;
    }

    return mouse_report;
}

report_mouse_t noah_pd_configured_scroll(const noah_pd_config_t *slot, report_mouse_t report) {
    configured_scroll = slot;
    report = dragscroll_step(report);
    configured_scroll = NULL;
    return report;
}

void reset_dragscroll_mode(void) {
    dragscroll_state = (dragscroll_state_t){0};
}

#else

void reset_dragscroll_mode(void) {}

#endif // POINTING_DEVICE_ENABLE
