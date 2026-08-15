// ────────────────────────────────────────────────────────────────────────────
// PD Mode Handler Common Helpers
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../action/action_dispatch.h"
#include "pd_mode_handlers.h"

#ifndef NOAH_PD_MODE_MAX_TAPS_PER_TICK
#    define NOAH_PD_MODE_MAX_TAPS_PER_TICK 4
#endif
#ifndef NOAH_PD_MODE_MAX_BACKLOG_TAPS
#    define NOAH_PD_MODE_MAX_BACKLOG_TAPS 32
#endif

_Static_assert(NOAH_PD_MODE_MAX_TAPS_PER_TICK > 0, "discrete pointing tap budget must be nonzero");
_Static_assert(NOAH_PD_MODE_MAX_TAPS_PER_TICK <= UINT8_MAX, "discrete pointing tap budget must fit uint8_t");
_Static_assert(NOAH_PD_MODE_MAX_BACKLOG_TAPS > 0, "discrete pointing backlog must be nonzero");
_Static_assert(NOAH_PD_MODE_MAX_BACKLOG_TAPS <= UINT16_MAX, "discrete pointing backlog must fit uint16_t diagnostics");
_Static_assert(NOAH_PD_MODE_MAX_TAPS_PER_TICK <= NOAH_PD_MODE_MAX_BACKLOG_TAPS, "discrete pointing tap budget must not exceed its backlog cap");

#define PD_MODE_VALIDATE_AXIS_THRESHOLD(threshold_)                                  \
    _Static_assert((threshold_) > 0, "discrete pointing threshold must be nonzero"); \
    _Static_assert(((uint64_t)(threshold_) * ((uint64_t)NOAH_PD_MODE_MAX_BACKLOG_TAPS + 1u)) - 1u <= (uint32_t)INT32_MAX - ((uint32_t)INT16_MAX + 1u), "discrete pointing threshold and backlog leave no room for one full input delta")

typedef void (*pd_mode_tap_fn_t)(uint16_t keycode);

typedef struct {
    int32_t  accumulator;
    uint32_t saturated_report_count;
    uint32_t discarded_tap_count;
    uint16_t maximum_backlog_taps;
    uint8_t  maximum_taps_emitted;
    int8_t   last_dir;
} pd_mode_axis_state_t;

static inline report_mouse_t pd_mode_freeze_mouse(void) {
    return (report_mouse_t){0};
}

static inline void pd_mode_tap_code(uint16_t keycode) {
    noah_emit_synthetic_qmk_tap(keycode, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
}

static inline void pd_mode_axis_reset(pd_mode_axis_state_t *state) {
    *state = (pd_mode_axis_state_t){0};
}

static inline void pd_mode_axis_diagnostic_add(uint32_t *counter, uint32_t amount) {
    if (*counter > UINT32_MAX - amount) {
        *counter = UINT32_MAX;
        return;
    }

    *counter += amount;
}

static inline uint32_t pd_mode_axis_magnitude(int32_t value) {
    return value < 0 ? (uint32_t)(-(value + 1)) + 1u : (uint32_t)value;
}

static inline void pd_mode_axis_bound_accumulator(pd_mode_axis_state_t *state, int16_t delta, uint16_t threshold) {
    if (delta != 0) {
        int8_t dir = (delta > 0) ? 1 : -1;
        if (state->last_dir != 0 && dir != state->last_dir) {
            state->accumulator = 0;
        }
        state->last_dir = dir;
    }

    int32_t  proposed      = state->accumulator + (int32_t)delta;
    uint32_t magnitude     = pd_mode_axis_magnitude(proposed);
    uint32_t whole_taps    = magnitude / threshold;
    uint32_t residual      = magnitude % threshold;
    uint32_t retained_taps = whole_taps;

    if (retained_taps > NOAH_PD_MODE_MAX_BACKLOG_TAPS) {
        uint32_t discarded_taps = retained_taps - NOAH_PD_MODE_MAX_BACKLOG_TAPS;

        retained_taps = NOAH_PD_MODE_MAX_BACKLOG_TAPS;
        pd_mode_axis_diagnostic_add(&state->saturated_report_count, 1u);
        pd_mode_axis_diagnostic_add(&state->discarded_tap_count, discarded_taps);
    }

    if (retained_taps > state->maximum_backlog_taps) {
        state->maximum_backlog_taps = (uint16_t)retained_taps;
    }

    int32_t bounded    = (int32_t)(retained_taps * threshold + residual);
    state->accumulator = (int32_t)(proposed < 0 ? -bounded : bounded);
}

static inline uint8_t pd_mode_axis_emit(pd_mode_axis_state_t *state, int16_t delta, uint16_t positive, uint16_t negative, uint16_t threshold, uint8_t *remaining_budget, pd_mode_tap_fn_t dispatch) {
    uint8_t emitted = 0;

    pd_mode_axis_bound_accumulator(state, delta, threshold);

    while (*remaining_budget > 0 && state->accumulator >= threshold) {
        dispatch(positive);
        state->accumulator -= threshold;
        (*remaining_budget)--;
        emitted++;
    }
    while (*remaining_budget > 0 && state->accumulator <= -(int32_t)threshold) {
        dispatch(negative);
        state->accumulator += threshold;
        (*remaining_budget)--;
        emitted++;
    }

    if (emitted > state->maximum_taps_emitted) {
        state->maximum_taps_emitted = emitted;
    }

    return emitted;
}

static inline report_mouse_t pd_mode_handle_vertical_threshold(report_mouse_t mouse_report, pd_mode_axis_state_t *state, uint16_t positive, uint16_t negative, uint16_t threshold, pd_mode_tap_fn_t dispatch) {
    uint8_t remaining_budget = NOAH_PD_MODE_MAX_TAPS_PER_TICK;

    (void)pd_mode_axis_emit(state, mouse_report.y, positive, negative, threshold, &remaining_budget, dispatch);
    return pd_mode_freeze_mouse();
}

static inline void pd_mode_axis_debug_snapshot(const pd_mode_axis_state_t *state, uint16_t threshold, pd_mode_axis_debug_snapshot_t *out) {
    if (out == NULL) {
        return;
    }

    *out = (pd_mode_axis_debug_snapshot_t){
        .accumulated_motion     = state->accumulator,
        .saturated_report_count = state->saturated_report_count,
        .discarded_tap_count    = state->discarded_tap_count,
        .pending_tap_count      = (uint16_t)(pd_mode_axis_magnitude(state->accumulator) / threshold),
        .maximum_backlog_taps   = state->maximum_backlog_taps,
        .maximum_taps_emitted   = state->maximum_taps_emitted,
        .direction              = state->last_dir,
    };
}
