#include "feedback_projection.h"

__attribute__((weak)) void key_feedback_pulse_observe(keypos_t key_pos, key_feedback_pulse_kind_t kind, uint8_t tap_branch) {
    (void)key_pos;
    (void)kind;
    (void)tap_branch;
}

static void key_runtime_core_feedback_projection_set_pulse(key_runtime_core_state_t *state, keypos_t key_pos, key_feedback_pulse_kind_t kind, uint8_t tap_branch) {
    if (!state) {
        return;
    }

    state->feedback_pulse_timer           = timer_read();
    state->feedback_pulse_sequence        = key_runtime_core_state_next_feedback_sequence(state);
    state->feedback_pulse_active          = true;
    state->feedback_pulse_kind            = kind;
    state->feedback_pulse_key_pos         = key_pos;
    state->feedback_pulse_tap_branch      = tap_branch;
    state->feedback_pulse_queued          = false;
    state->feedback_pulse_queued_sequence = 0u;
}

static void key_runtime_core_feedback_projection_queue_pulse(key_runtime_core_state_t *state, keypos_t key_pos, key_feedback_pulse_kind_t kind, uint8_t tap_branch) {
    if (!state) {
        return;
    }

    if (state->feedback_pulse_active) {
        state->feedback_pulse_queued            = true;
        state->feedback_pulse_queued_sequence   = key_runtime_core_state_next_feedback_sequence(state);
        state->feedback_pulse_queued_kind       = kind;
        state->feedback_pulse_queued_key_pos    = key_pos;
        state->feedback_pulse_queued_tap_branch = tap_branch;
        return;
    }

    key_runtime_core_feedback_projection_set_pulse(state, key_pos, kind, tap_branch);
}

void key_runtime_core_feedback_projection_project_pulse(keypos_t key_pos, key_feedback_pulse_kind_t kind, uint8_t tap_branch) {
    key_runtime_core_feedback_projection_queue_pulse(key_runtime_core_state(), key_pos, kind, tap_branch);
    key_feedback_pulse_observe(key_pos, kind, tap_branch);
}
