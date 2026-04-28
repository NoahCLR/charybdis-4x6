#include "tap_series.h"

#include "effect_plan.h"

static bool key_runtime_core_tap_series_flush_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static keypos_t key_runtime_core_tap_series_flush_keypos_from_index(uint16_t index) {
    if (index >= KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY) {
        return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
    }

    return (keypos_t){
        .row = (uint8_t)(index / MATRIX_COLS),
        .col = (uint8_t)(index % MATRIX_COLS),
    };
}

bool key_runtime_core_pending_multi_tap_flush_resolution(const tap_series_t *series, uint16_t *action, uint8_t *repeat_count) {
    uint16_t resolved_action;
    uint8_t  resolved_repeat_count;

    if (!(series && series->active && !series->branch_confirmed && !series->branch_confirming)) {
        return false;
    }

    resolved_action       = series->tap_repeat_count > 0u ? series->tap_action : series->single_action;
    resolved_repeat_count = series->tap_repeat_count > 0u ? series->tap_repeat_count : series->tap_count;
    if (action) {
        *action = resolved_action;
    }
    if (repeat_count) {
        *repeat_count = resolved_repeat_count;
    }
    return true;
}

bool key_runtime_core_tap_series_take_flush(tap_series_t *series, uint16_t *action, uint8_t *repeat_count, delayed_action_mods_t *mods) {
    if (action) {
        *action = KC_NO;
    }
    if (repeat_count) {
        *repeat_count = 0u;
    }
    if (mods) {
        *mods = (delayed_action_mods_t){0};
    }

    if (!key_runtime_core_pending_multi_tap_flush_resolution(series, action, repeat_count)) {
        return false;
    }

    if (mods) {
        *mods = series->saved_mod_state;
    }
    return true;
}

void key_runtime_core_plan_branch_confirm_delayed_action(key_runtime_core_state_t *state, tap_series_t *series, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_feedback_pulse_kind_t action_feedback_kind;

    if (!(state && series && series->active && series->branch_confirming && plan)) {
        return;
    }

    key_runtime_core_effect_plan_push_delayed_action(plan, key_pos, series->branch_confirm_action, series->branch_confirm_mods, series->branch_confirm_repeat_count);
    if (series->branch_confirm_tap_commit_feedback) {
        key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, key_pos, series->branch_confirm_action, series->branch_confirm_tap_count);
    }
    if (series->branch_confirm_action_feedback) {
        action_feedback_kind = (key_feedback_pulse_kind_t)series->branch_confirm_action_feedback_kind;
        key_runtime_core_effect_plan_push_feedback_pulse(plan, key_pos, action_feedback_kind);
    }

    key_runtime_core_tap_series_clear(state, series);
}

static void key_runtime_core_tap_series_flush_plan_series(key_runtime_core_state_t *state, tap_series_t *series, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    uint16_t              action;
    uint8_t               repeat_count;
    delayed_action_mods_t mods;

    if (!key_runtime_core_tap_series_take_flush(series, &action, &repeat_count, &mods)) {
        return;
    }

    key_runtime_core_effect_plan_push_delayed_action(plan, key_pos, action, mods, repeat_count);
    key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, key_pos, action, series->tap_count);
    key_runtime_core_tap_series_clear(state, series);
}

void key_runtime_core_flush_foreign_multi_tap(uint16_t keycode, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan)) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY; index++) {
        tap_series_t        *series = &state->tap_series[index];
        const press_token_t *owner;
        keypos_t             series_key_pos;

        series_key_pos = key_runtime_core_tap_series_flush_keypos_from_index(index);
        if (!(series->active && !(key_runtime_core_tap_series_flush_keypos_equal(series_key_pos, key_pos) && series->keycode == keycode))) {
            continue;
        }

        owner = key_runtime_core_press_token_at(series_key_pos);
        if (owner && owner->active) {
            continue;
        }

        if (series->branch_confirming) {
            if (series->branch_confirm_kind == KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION) {
                key_runtime_core_plan_branch_confirm_delayed_action(state, series, series_key_pos, plan);
            }
            continue;
        }

        key_runtime_core_tap_series_flush_plan_series(state, series, series_key_pos, plan);
    }
}

void key_runtime_core_flush_multi_tap(key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan)) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY; index++) {
        tap_series_t *series = &state->tap_series[index];
        keypos_t      series_key_pos;

        series_key_pos = key_runtime_core_tap_series_flush_keypos_from_index(index);
        if (series->active && series->branch_confirming) {
            if (series->branch_confirm_kind == KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION) {
                key_runtime_core_plan_branch_confirm_delayed_action(state, series, series_key_pos, plan);
            }
            continue;
        }

        key_runtime_core_tap_series_flush_plan_series(state, series, series_key_pos, plan);
    }
}
