#include "scan_planner.h"

#include "effect_plan.h"
#include "../reducer/ownership_state.h"
#include "tap_series.h"

#include "../../../action/action_dispatch.h"

typedef enum {
    KEY_RUNTIME_CORE_SCAN_FEEDBACK_LEVEL_HOLD      = 1,
    KEY_RUNTIME_CORE_SCAN_FEEDBACK_LEVEL_LONG_HOLD = 2,
} key_runtime_core_scan_feedback_level_t;

static bool key_runtime_core_scan_keypos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static uint16_t key_runtime_core_scan_keypos_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * MATRIX_COLS + key_pos.col);
}

static keypos_t key_runtime_core_scan_invalid_keypos(void) {
    return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static keypos_t key_runtime_core_scan_keypos_from_index(uint16_t index) {
    if (index >= KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY) {
        return key_runtime_core_scan_invalid_keypos();
    }

    return (keypos_t){
        .row = (uint8_t)(index / MATRIX_COLS),
        .col = (uint8_t)(index % MATRIX_COLS),
    };
}

static press_token_t *key_runtime_core_scan_press_token_state(key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_scan_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->press_tokens[key_runtime_core_scan_keypos_index(key_pos)];
}

static bool key_runtime_core_scan_press_token_slot_index(const key_runtime_core_state_t *state, const press_token_t *token, uint16_t *out) {
    uintptr_t start;
    uintptr_t end;
    uintptr_t ptr;

    if (out) {
        *out = 0u;
    }

    if (!(state && token && out)) {
        return false;
    }

    start = (uintptr_t)&state->press_tokens[0];
    end   = (uintptr_t)&state->press_tokens[KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY];
    ptr   = (uintptr_t)token;
    if (!(ptr >= start && ptr < end && ((ptr - start) % sizeof(state->press_tokens[0])) == 0u)) {
        return false;
    }

    *out = (uint16_t)((ptr - start) / sizeof(state->press_tokens[0]));
    return true;
}

static keypos_t key_runtime_core_scan_press_token_key_pos(const key_runtime_core_state_t *state, const press_token_t *token) {
    uint16_t index;

    return key_runtime_core_scan_press_token_slot_index(state, token, &index) ? key_runtime_core_scan_keypos_from_index(index) : key_runtime_core_scan_invalid_keypos();
}

static uint16_t key_runtime_core_scan_elapsed(uint16_t start, uint16_t end) {
    return (uint16_t)(end - start);
}

static key_feedback_pulse_kind_t key_runtime_core_scan_hold_feedback_pulse_kind(bool long_hold_level) {
    return long_hold_level ? KEY_FEEDBACK_PULSE_LONG_HOLD : KEY_FEEDBACK_PULSE_HOLD;
}

static void key_runtime_core_scan_press_token_note_feedback_level(key_runtime_core_state_t *state, press_token_t *token, key_runtime_core_scan_feedback_level_t level) {
    if (!(state && token && token->active)) {
        return;
    }

    if (token->feedback_sequence != 0u && token->feedback_level >= (uint8_t)level) {
        return;
    }

    token->feedback_sequence = key_runtime_core_state_next_feedback_sequence(state);
    token->feedback_level    = (uint8_t)level;
}

static void key_runtime_core_scan_press_token_note_hold_feedback(key_runtime_core_state_t *state, press_token_t *token, bool long_hold_level) {
    key_runtime_core_scan_press_token_note_feedback_level(state, token, long_hold_level ? KEY_RUNTIME_CORE_SCAN_FEEDBACK_LEVEL_LONG_HOLD : KEY_RUNTIME_CORE_SCAN_FEEDBACK_LEVEL_HOLD);
}

void key_runtime_core_press_token_commit_hold_phase(key_runtime_core_state_t *state, press_token_t *token, bool completes_hold, bool long_hold_level) {
    if (!token) {
        return;
    }

    key_runtime_core_scan_press_token_note_hold_feedback(state, token, long_hold_level);
    token->slot_phase = completes_hold ? KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE : KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE;
}

void key_runtime_core_press_token_mark_release_hold_pending(key_runtime_core_state_t *state, press_token_t *token) {
    if (!token) {
        return;
    }

    key_runtime_core_scan_press_token_note_hold_feedback(state, token, false);
    token->slot_phase = KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING;
}

static bool key_runtime_core_press_token_active_scan_should_mark_release_hold_pending(const press_token_t *token, uint16_t elapsed) {
    if (!(token && token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW)) {
        return false;
    }

    if (token->interaction.contract.hold.release_action != KC_NO && elapsed >= token->interaction.binding.tap_hold_term) {
        return true;
    }

    return !token->interaction.binding.hold.present && token->interaction.contract.long_hold.release_action != KC_NO && elapsed >= token->interaction.binding.longer_hold_term;
}

static bool key_runtime_core_scan_press_token_has_pending_hold_series(const key_runtime_core_state_t *state, const press_token_t *token) {
    const tap_series_t *series;
    keypos_t            key_pos;

    if (!(state && token && token->active)) {
        return false;
    }

    key_pos = key_runtime_core_scan_press_token_key_pos(state, token);
    series  = key_runtime_core_tap_series_state((key_runtime_core_state_t *)state, key_pos);
    return series && series->active && (series->pending_hold || series->branch_confirming) && series->keycode == token->physical_keycode;
}

static bool key_runtime_core_scan_press_token_uses_implicit_hold(const press_token_t *token) {
    return token && token->handled_key && key_runtime_slot_interaction_uses_implicit_hold(token->interaction);
}

static bool key_runtime_core_scan_press_token_uses_fallback_hold(const press_token_t *token) {
    return token && token->handled_key && key_runtime_slot_interaction_uses_fallback_hold(token->interaction);
}

static bool key_runtime_core_scan_press_token_has_runtime_owned_state(const key_runtime_core_state_t *state, const press_token_t *token) {
    keypos_t token_key_pos = key_runtime_core_scan_press_token_key_pos(state, token);

    return token && key_runtime_core_held_action_keycode_at(token_key_pos) != KC_NO ? true : key_runtime_core_repeat_active_at(token ? token_key_pos : (keypos_t){0});
}

static bool key_runtime_core_scan_hold_activation_needs_pulse(hold_behavior_t hold, handled_key_hold_semantics_t semantics, bool pulse_momentary_layer_action) {
    noah_action_desc_t desc = noah_action_describe(hold.action);

    if (semantics.threshold == HANDLED_KEY_HOLD_THRESHOLD_DISPATCH) {
        return true;
    }

    if (noah_action_desc_is_momentary_layer_keycode(desc)) {
        return pulse_momentary_layer_action;
    }

    return false;
}

void key_runtime_core_press_token_refresh_slot_phase_for_scan(key_runtime_core_state_t *state, press_token_t *token, uint16_t now) {
    uint16_t elapsed;

    if (!(state && token && token->active && token->handled_key)) {
        return;
    }

    elapsed = key_runtime_core_scan_elapsed(token->pressed_at, now);

    if (key_runtime_core_scan_press_token_has_pending_hold_series(state, token)) {
        return;
    }

    switch (token->slot_phase) {
        case KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, true, true);
                return;
            }

            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) && elapsed >= token->interaction.binding.tap_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, !token->interaction.binding.long_hold.present, false);
                return;
            }

            if (key_runtime_core_press_token_active_scan_should_mark_release_hold_pending(token, elapsed)) {
                key_runtime_core_press_token_mark_release_hold_pending(state, token);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, true, true);
                return;
            }

            if (elapsed >= token->interaction.binding.tap_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, !token->interaction.binding.long_hold.present, false);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING:
        case KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, true, true);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE:
        case KEY_RUNTIME_SLOT_PHASE_IDLE:
        default:
            return;
    }
}

bool key_runtime_core_resolve_pending_multi_tap_scan(keypos_t key_pos, key_runtime_core_pending_multi_tap_scan_resolution_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;
    tap_series_t             *series;
    uint16_t                  elapsed;
    uint16_t                  flush_action;
    uint8_t                   flush_repeat_count;

    if (out) {
        *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){0};
    }

    if (!(state && out && key_runtime_core_scan_keypos_valid(key_pos))) {
        return false;
    }

    token  = key_runtime_core_scan_press_token_state(state, key_pos);
    series = key_runtime_core_tap_series_state(state, key_pos);
    if (!(series && series->active)) {
        return false;
    }

    if (!series->pending_hold && token && token->active && token->handled_key && series->keycode == token->resolved_keycode) {
        return true;
    }

    if (!series->pending_hold) {
        if (key_runtime_core_tap_series_pending_combo_output(state, series)) {
            return true;
        }

        if (key_runtime_core_scan_elapsed(series->last_tap_at, state->current_time) > series->tap_term_ms && key_runtime_core_pending_multi_tap_flush_resolution(series, &flush_action, &flush_repeat_count)) {
            uint8_t flush_tap_count = series->tap_count;

            *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){
                .outcome      = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_FLUSH,
                .action       = flush_action,
                .repeat_count = flush_repeat_count,
                .tap_count    = flush_tap_count,
            };
        }

        return true;
    }

    if (!(token && token->active && token->handled_key && series->keycode == token->resolved_keycode)) {
        return false;
    }

    elapsed = key_runtime_core_scan_elapsed(token->pressed_at, state->current_time);
    if (token->interaction.contract.hold.release_action != KC_NO && elapsed >= token->interaction.binding.tap_hold_term && token->slot_phase != KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING) {
        *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){
            .outcome   = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_RELEASE_HOLD_PENDING,
            .tap_count = series->tap_count,
        };
        return true;
    }

    if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
        *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){
            .outcome        = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD,
            .hold           = token->interaction.binding.long_hold,
            .semantics      = token->interaction.contract.long_hold,
            .completes_hold = true,
            .action         = token->interaction.binding.long_hold.action,
        };
        return true;
    }

    if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) && elapsed >= token->interaction.binding.tap_hold_term) {
        *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){
            .outcome        = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD,
            .hold           = token->interaction.binding.hold,
            .semantics      = token->interaction.contract.hold,
            .completes_hold = !token->interaction.binding.long_hold.present,
            .action         = token->interaction.binding.hold.action,
        };
        return true;
    }

    return true;
}

void key_runtime_core_plan_fallback_hold_activation(key_runtime_core_state_t *state, press_token_t *token, key_runtime_core_effect_plan_t *plan) {
    keypos_t token_key_pos;

    if (!(state && token && token->active && token->handled_key && key_runtime_core_scan_press_token_uses_fallback_hold(token))) {
        return;
    }

    if (key_runtime_core_scan_press_token_has_runtime_owned_state(state, token) || token->resolved_keycode == KC_NO) {
        return;
    }

    token_key_pos = key_runtime_core_scan_press_token_key_pos(state, token);
    key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, token_key_pos, token->resolved_keycode);
}

static void key_runtime_core_scan_clear_confirmed_tap_series_at(key_runtime_core_state_t *state, keypos_t key_pos) {
    tap_series_t *series = key_runtime_core_tap_series_state(state, key_pos);

    if (series && series->active && series->branch_confirmed) {
        key_runtime_core_tap_series_clear(state, series);
    }
}

static void key_runtime_core_plan_threshold_hold_effects(key_runtime_core_state_t *state, press_token_t *token, hold_behavior_t hold, handled_key_hold_semantics_t semantics, bool completes_hold, bool long_hold_level, key_runtime_core_effect_plan_t *plan) {
    keypos_t                  token_key_pos;
    key_feedback_pulse_kind_t feedback_kind;

    if (!(state && token && token->active && token->handled_key && hold.action != KC_NO)) {
        return;
    }

    token_key_pos = key_runtime_core_scan_press_token_key_pos(state, token);
    feedback_kind = key_runtime_core_scan_hold_feedback_pulse_kind(long_hold_level);

    if (key_runtime_core_scan_press_token_has_runtime_owned_state(state, token)) {
        key_runtime_core_effect_plan_push_release_owned_state(plan, token_key_pos);
    }

    switch (semantics.threshold) {
        case HANDLED_KEY_HOLD_THRESHOLD_DISPATCH:
            key_runtime_core_effect_plan_push_dispatch_action(plan, token_key_pos, hold.action);
            key_runtime_core_effect_plan_push_feedback_pulse(plan, token_key_pos, feedback_kind);
            key_runtime_core_press_token_commit_hold_phase(state, token, completes_hold, long_hold_level);
            return;
        case HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD:
            key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, token_key_pos, hold.action);
            if (key_runtime_core_scan_hold_activation_needs_pulse(hold, semantics, false)) {
                key_runtime_core_effect_plan_push_feedback_pulse(plan, token_key_pos, feedback_kind);
            }
            key_runtime_core_press_token_commit_hold_phase(state, token, completes_hold, long_hold_level);
            return;
        case HANDLED_KEY_HOLD_THRESHOLD_REPEAT:
            key_runtime_core_effect_plan_push_repeat_start(plan, token_key_pos, hold.action, hold.repeat_hz);
            key_runtime_core_effect_plan_push_feedback_pulse(plan, token_key_pos, feedback_kind);
            key_runtime_core_press_token_commit_hold_phase(state, token, completes_hold, long_hold_level);
            return;
        case HANDLED_KEY_HOLD_THRESHOLD_NONE:
        default:
            return;
    }
}

static uint16_t key_runtime_core_scan_token_branch_confirm_started_at(const press_token_t *token, bool long_hold_level) {
    uint16_t boundary;

    if (!token) {
        return 0u;
    }

    boundary = long_hold_level ? token->interaction.binding.longer_hold_term : token->interaction.binding.tap_hold_term;
    return (uint16_t)(token->pressed_at + boundary);
}

static bool key_runtime_core_tap_series_start_threshold_branch_confirm(key_runtime_core_state_t *state, tap_series_t *series, const press_token_t *token, key_runtime_tap_series_branch_confirm_kind_t kind, bool long_hold_level) {
    bool started;

    if (!(state && series && token && (token->interaction.flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0u)) {
        return false;
    }

    started = key_runtime_core_tap_series_start_branch_confirm(state, series, kind, token->interaction.selection.tap_count, key_runtime_core_scan_token_branch_confirm_started_at(token, long_hold_level), token->interaction.binding.branch_confirm_term);
    if (started) {
        series->branch_confirm_long_hold_level = long_hold_level;
    }
    return started;
}

static void key_runtime_core_plan_branch_confirm_complete(key_runtime_core_state_t *state, tap_series_t *series, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    press_token_t *token;
    uint16_t       elapsed;

    if (!(state && series && series->active && series->branch_confirming && plan)) {
        return;
    }

    if (key_runtime_core_tap_series_branch_confirm_window_active(series->branch_confirm_started_at, series->branch_confirm_duration_ms, state->current_time)) {
        return;
    }

    switch ((key_runtime_tap_series_branch_confirm_kind_t)series->branch_confirm_kind) {
        case KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION:
            key_runtime_core_plan_branch_confirm_delayed_action(state, series, key_pos, plan);
            return;
        case KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_RELEASE_HOLD_PENDING:
            token = key_runtime_core_scan_press_token_state(state, key_pos);
            if (!(token && token->active && token->handled_key)) {
                key_runtime_core_tap_series_clear(state, series);
                return;
            }

            key_runtime_core_press_token_mark_release_hold_pending(state, token);
            series->branch_confirming = false;
            series->branch_confirmed  = true;
            series->pending_hold      = false;
            series->hold              = hold_behavior_none();
            series->long_hold         = hold_behavior_none();

            elapsed = key_runtime_core_scan_elapsed(token->pressed_at, state->current_time);
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_tap_series_clear(state, series);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
            }
            return;
        case KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_THRESHOLD_HOLD:
            token = key_runtime_core_scan_press_token_state(state, key_pos);
            if (!(token && token->active && token->handled_key)) {
                key_runtime_core_tap_series_clear(state, series);
                return;
            }

            if (series->branch_confirm_long_hold_level) {
                key_runtime_core_tap_series_clear(state, series);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
                return;
            }

            key_runtime_core_tap_series_clear(state, series);
            key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.hold, token->interaction.contract.hold, !token->interaction.binding.long_hold.present, false, plan);
            return;
        case KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_NONE:
        default:
            key_runtime_core_tap_series_clear(state, series);
            return;
    }
}

void key_runtime_core_plan_pending_multi_tap_scan_for_key(key_runtime_core_state_t *state, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_pending_multi_tap_scan_resolution_t resolution;
    press_token_t                                       *token;
    tap_series_t                                        *series;
    delayed_action_mods_t                                mods;

    if (!(state && plan)) {
        return;
    }

    series = key_runtime_core_tap_series_state(state, key_pos);
    mods   = series ? series->saved_mod_state : (delayed_action_mods_t){0};
    if (series && series->active && series->branch_confirming) {
        key_runtime_core_plan_branch_confirm_complete(state, series, key_pos, plan);
        return;
    }

    if (!key_runtime_core_resolve_pending_multi_tap_scan(key_pos, &resolution)) {
        return;
    }

    token = key_runtime_core_scan_press_token_state(state, key_pos);
    switch (resolution.outcome) {
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_FLUSH:
            if (key_runtime_core_tap_series_start_delayed_action_branch_confirm(state, series, resolution.tap_count, (uint16_t)(series->last_tap_at + series->tap_term_ms), series->branch_confirm_term_ms, resolution.action, resolution.repeat_count, mods, true, false, KEY_FEEDBACK_PULSE_HOLD)) {
                return;
            }
            key_runtime_core_effect_plan_push_delayed_action(plan, key_pos, resolution.action, mods, resolution.repeat_count);
            key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, key_pos, resolution.action, resolution.tap_count);
            key_runtime_core_tap_series_clear(state, series);
            return;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD:
            if (key_runtime_core_tap_series_start_threshold_branch_confirm(state, series, token, KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_THRESHOLD_HOLD, false)) {
                return;
            }
            key_runtime_core_tap_series_clear(state, series);
            key_runtime_core_plan_threshold_hold_effects(state, token, resolution.hold, resolution.semantics, resolution.completes_hold, false, plan);
            return;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD:
            if (key_runtime_core_tap_series_start_threshold_branch_confirm(state, series, token, KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_THRESHOLD_HOLD, true)) {
                return;
            }
            key_runtime_core_tap_series_clear(state, series);
            key_runtime_core_plan_threshold_hold_effects(state, token, resolution.hold, resolution.semantics, resolution.completes_hold, true, plan);
            return;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_RELEASE_HOLD_PENDING:
            if (key_runtime_core_tap_series_start_threshold_branch_confirm(state, series, token, KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_RELEASE_HOLD_PENDING, false)) {
                return;
            }
            if (token) {
                key_runtime_core_press_token_mark_release_hold_pending(state, token);
                if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && key_runtime_core_scan_elapsed(token->pressed_at, state->current_time) >= token->interaction.binding.longer_hold_term) {
                    key_runtime_core_tap_series_clear(state, series);
                    key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
                    return;
                }
            }
            if (series) {
                series->branch_confirmed = true;
                series->pending_hold     = false;
                series->hold             = hold_behavior_none();
                series->long_hold        = hold_behavior_none();
            }
            return;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE:
        default:
            return;
    }
}

void key_runtime_core_plan_active_scan_for_token(key_runtime_core_state_t *state, press_token_t *token, key_runtime_core_effect_plan_t *plan) {
    uint16_t elapsed;
    keypos_t token_key_pos;

    if (!(state && token && token->active && token->handled_key && plan)) {
        return;
    }

    token_key_pos = key_runtime_core_scan_press_token_key_pos(state, token);

    if (key_runtime_core_scan_press_token_has_pending_hold_series(state, token)) {
        return;
    }

    elapsed = key_runtime_core_scan_elapsed(token->pressed_at, state->current_time);

    switch (token->slot_phase) {
        case KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW:
            if (key_runtime_core_scan_press_token_uses_fallback_hold(token) && key_runtime_core_held_action_keycode_at(token_key_pos) == KC_NO && elapsed >= token->interaction.binding.tap_hold_term) {
                key_runtime_core_plan_fallback_hold_activation(state, token, plan);
                return;
            }

            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_scan_clear_confirmed_tap_series_at(state, token_key_pos);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
                return;
            }

            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) && elapsed >= token->interaction.binding.tap_hold_term) {
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.hold, token->interaction.contract.hold, !token->interaction.binding.long_hold.present, false, plan);
                return;
            }

            if (key_runtime_core_press_token_active_scan_should_mark_release_hold_pending(token, elapsed)) {
                key_runtime_core_press_token_mark_release_hold_pending(state, token);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW:
            if (elapsed >= token->interaction.binding.tap_hold_term) {
                if (!key_runtime_core_scan_press_token_uses_implicit_hold(token)) {
                    key_runtime_core_effect_plan_push_feedback_pulse(plan, token_key_pos, KEY_FEEDBACK_PULSE_HOLD);
                }
                key_runtime_core_press_token_commit_hold_phase(state, token, !token->interaction.binding.long_hold.present, false);
            }
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_scan_clear_confirmed_tap_series_at(state, token_key_pos);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING:
        case KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_scan_clear_confirmed_tap_series_at(state, token_key_pos);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE:
        case KEY_RUNTIME_SLOT_PHASE_IDLE:
        default:
            return;
    }
}

bool key_runtime_core_settle_pending_fallback_hold(key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state       = key_runtime_core_state();
    bool                      settled_any = false;

    if (!(state && plan)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token  = &state->press_tokens[index];
        uint8_t        before = plan->count;

        key_runtime_core_plan_fallback_hold_activation(state, token, plan);
        if (plan->count != before) {
            settled_any = true;
        }
    }

    return settled_any;
}
