// ────────────────────────────────────────────────────────────────────────────
// PD Mode Handler Common Helpers
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../action/action_dispatch.h"

typedef void (*pd_mode_tap_fn_t)(uint16_t keycode);

typedef struct {
    int32_t accumulator;
    int8_t  last_dir;
} pd_mode_axis_state_t;

static inline report_mouse_t pd_mode_freeze_mouse(void) {
    return (report_mouse_t){0};
}

static inline void pd_mode_tap_code(uint16_t keycode) {
    noah_emit_synthetic_qmk_tap(keycode, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
}

static inline void pd_mode_axis_reset(pd_mode_axis_state_t *state) {
    state->accumulator = 0;
    state->last_dir    = 0;
}

static inline void pd_mode_axis_emit(pd_mode_axis_state_t *state, int16_t delta, uint16_t positive, uint16_t negative, uint16_t threshold, pd_mode_tap_fn_t dispatch) {
    if (delta != 0) {
        int8_t dir = (delta > 0) ? 1 : -1;
        if (state->last_dir != 0 && dir != state->last_dir) {
            state->accumulator = 0;
        }
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

static inline report_mouse_t pd_mode_handle_vertical_threshold(report_mouse_t mouse_report, pd_mode_axis_state_t *state, uint16_t positive, uint16_t negative, uint16_t threshold, pd_mode_tap_fn_t dispatch) {
    pd_mode_axis_emit(state, mouse_report.y, positive, negative, threshold, dispatch);
    return pd_mode_freeze_mouse();
}
