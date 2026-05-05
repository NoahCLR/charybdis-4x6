#include "state_query.h"

#include "ownership_state.h"

#include "../../behavior/handled_key_policy.h"

static bool key_runtime_core_query_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static uint16_t key_runtime_core_query_keypos_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * MATRIX_COLS + key_pos.col);
}

static keypos_t key_runtime_core_query_invalid_keypos(void) {
    return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static keypos_t key_runtime_core_query_keypos_from_slot_index(uint16_t index) {
    if (index >= KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY) {
        return key_runtime_core_query_invalid_keypos();
    }

    return (keypos_t){
        .row = (uint8_t)(index / MATRIX_COLS),
        .col = (uint8_t)(index % MATRIX_COLS),
    };
}

static press_token_t *key_runtime_core_query_press_token_state(key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->press_tokens[key_runtime_core_query_keypos_index(key_pos)];
}

static tap_series_t *key_runtime_core_query_tap_series_state(key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->tap_series[key_runtime_core_query_keypos_index(key_pos)];
}

static bool key_runtime_core_query_press_token_slot_index(const key_runtime_core_state_t *state, const press_token_t *token, uint16_t *out) {
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

static keypos_t key_runtime_core_query_press_token_resolve_key_pos(const key_runtime_core_state_t *state, const press_token_t *token) {
    uint16_t index;

    return key_runtime_core_query_press_token_slot_index(state, token, &index) ? key_runtime_core_query_keypos_from_slot_index(index) : key_runtime_core_query_invalid_keypos();
}

static bool key_runtime_core_query_tap_series_slot_index(const key_runtime_core_state_t *state, const tap_series_t *series, uint16_t *out) {
    uintptr_t start;
    uintptr_t end;
    uintptr_t ptr;

    if (out) {
        *out = 0u;
    }

    if (!(state && series && out)) {
        return false;
    }

    start = (uintptr_t)&state->tap_series[0];
    end   = (uintptr_t)&state->tap_series[KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY];
    ptr   = (uintptr_t)series;
    if (!(ptr >= start && ptr < end && ((ptr - start) % sizeof(state->tap_series[0])) == 0u)) {
        return false;
    }

    *out = (uint16_t)((ptr - start) / sizeof(state->tap_series[0]));
    return true;
}

static keypos_t key_runtime_core_query_tap_series_resolve_key_pos(const key_runtime_core_state_t *state, const tap_series_t *series) {
    uint16_t index;

    return key_runtime_core_query_tap_series_slot_index(state, series, &index) ? key_runtime_core_query_keypos_from_slot_index(index) : key_runtime_core_query_invalid_keypos();
}

static uint16_t key_runtime_core_query_elapsed(uint16_t start, uint16_t end) {
    return (uint16_t)(end - start);
}

static bool key_runtime_core_query_press_token_has_pending_hold_series(const key_runtime_core_state_t *state, const press_token_t *token) {
    const tap_series_t *series;
    keypos_t            key_pos;

    if (!(state && token && token->active)) {
        return false;
    }

    key_pos = key_runtime_core_query_press_token_resolve_key_pos(state, token);
    series  = key_runtime_core_query_tap_series_state((key_runtime_core_state_t *)state, key_pos);
    return series && series->active && (series->pending_hold || series->branch_confirming) && series->keycode == token->physical_keycode;
}

static bool key_runtime_core_query_hold_semantics_owns_state_at_threshold(handled_key_hold_semantics_t semantics) {
    return handled_key_hold_semantics_registers_held(semantics) || handled_key_hold_semantics_repeats_while_held(semantics);
}

static bool key_runtime_core_query_press_token_quick_tap_suppressed(const press_token_t *token) {
    return token && token->interaction.contract.suppress_tap_on_layer_interrupt && token->momentary_layer_tap_interrupted;
}

static bool key_runtime_core_query_press_token_has_pd_mode_quick_lock_candidate(const press_token_t *token) {
    return token && token->interaction.contract.quick_tap_pd_mode_lock != 0 && token->pd_mode_was_locked_on_press && !token->pd_mode_lock_consumed_on_press && !key_runtime_core_query_press_token_quick_tap_suppressed(token);
}

static bool key_runtime_core_query_press_token_owned_state_active(const key_runtime_core_state_t *state, const press_token_t *token) {
    if (!(state && token && token->active)) {
        return false;
    }

    if (key_runtime_core_owner_has_runtime_owned_state_lease(state, token->token_id)) {
        return true;
    }

    if (token->interaction.contract.quick_release_of_immediate_hold_dispatches_tap && !handled_key_hold_semantics_fires_at_threshold(token->interaction.contract.hold)) {
        return true;
    }

    if (key_runtime_core_query_elapsed(token->pressed_at, state->current_time) < token->hold_term_ms) {
        return false;
    }

    return key_runtime_core_query_hold_semantics_owns_state_at_threshold(token->interaction.contract.hold);
}

static void key_runtime_core_query_press_token_deferred_release_profile(const key_runtime_core_state_t *state, const press_token_t *token, bool *before_tap_term, bool *after_tap_term) {
    bool before = false;
    bool after  = false;

    if (!(state && token && token->active && token->handled_key)) {
        goto done;
    }

    if (key_runtime_core_query_press_token_has_pending_hold_series(state, token) || key_runtime_core_query_press_token_owned_state_active(state, token)) {
        goto done;
    }

    if (token->interaction.contract.buffered_base_tap_dispatches_tap) {
        before = true;
        after  = true;
        goto done;
    }

    if (!key_runtime_core_query_press_token_quick_tap_suppressed(token)) {
        before = token->tap_outcome_available || key_runtime_core_query_press_token_has_pd_mode_quick_lock_candidate(token);
    }

    after = token->interaction.contract.nonquick_release_dispatches_tap;

done:
    if (before_tap_term) {
        *before_tap_term = before;
    }

    if (after_tap_term) {
        *after_tap_term = after;
    }
}

static bool key_runtime_core_query_press_token_deferred_release_blocker_tracks_tap_term(const key_runtime_core_state_t *state, const press_token_t *token) {
    bool before = false;
    bool after  = false;

    key_runtime_core_query_press_token_deferred_release_profile(state, token, &before, &after);
    return before != after;
}

static bool key_runtime_core_query_press_token_blocks_deferred_release(const key_runtime_core_state_t *state, const press_token_t *token) {
    bool before = false;
    bool after  = false;

    if (!(state && token && token->active)) {
        return false;
    }

    key_runtime_core_query_press_token_deferred_release_profile(state, token, &before, &after);
    if (before == after) {
        return before;
    }

    return key_runtime_core_query_elapsed(token->pressed_at, state->current_time) < token->hold_term_ms ? before : after;
}

static uint8_t key_runtime_core_query_effective_deferred_release_blocker_count(const key_runtime_core_state_t *state) {
    uint8_t count = 0;

    if (!state) {
        return 0u;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        if (key_runtime_core_query_press_token_blocks_deferred_release(state, &state->press_tokens[index])) {
            count++;
        }
    }

    return count;
}

static uint8_t key_runtime_core_query_timed_deferred_release_blocker_count(const key_runtime_core_state_t *state) {
    uint8_t count = 0;

    if (!state) {
        return 0u;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        if (key_runtime_core_query_press_token_deferred_release_blocker_tracks_tap_term(state, &state->press_tokens[index])) {
            count++;
        }
    }

    return count;
}

static bool key_runtime_core_query_has_foreign_effective_deferred_release_blocker_except(const key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];
        keypos_t             token_key_pos;

        token_key_pos = key_runtime_core_query_press_token_resolve_key_pos(state, token);
        if (!key_runtime_core_query_press_token_blocks_deferred_release(state, token) || key_runtime_core_query_keypos_equal(token_key_pos, key_pos)) {
            continue;
        }

        return true;
    }

    return false;
}

static bool key_runtime_core_query_press_token_allows_tap_release(const press_token_t *token) {
    if (!(token && token->active && token->handled_key)) {
        return false;
    }

    return token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || token->slot_phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW;
}

static bool key_runtime_core_query_press_token_uses_implicit_hold(const press_token_t *token) {
    return token && token->handled_key && key_runtime_slot_interaction_uses_implicit_hold(token->interaction);
}

static bool key_runtime_core_query_press_token_uses_fallback_hold(const press_token_t *token) {
    return token && token->handled_key && key_runtime_slot_interaction_uses_fallback_hold(token->interaction);
}

static uint8_t key_runtime_core_query_press_token_preview_layer_hint(const press_token_t *token) {
    return token ? token->interaction.contract.hold.preview_layer : UINT8_MAX;
}

bool key_runtime_core_blocker_queries_authoritative(void) {
    return key_runtime_core_state() != NULL;
}

bool key_runtime_core_has_any_deferred_release_blocker(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return key_runtime_core_blocker_queries_authoritative() && key_runtime_core_query_effective_deferred_release_blocker_count(state) != 0u;
}

bool key_runtime_core_has_foreign_deferred_release_blocker_except(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return key_runtime_core_blocker_queries_authoritative() && key_runtime_core_query_has_foreign_effective_deferred_release_blocker_except(state, key_pos);
}

const press_token_t *key_runtime_core_press_token_at(keypos_t key_pos) {
    return key_runtime_core_query_press_token_state(key_runtime_core_state(), key_pos);
}

const tap_series_t *key_runtime_core_tap_series_at(keypos_t key_pos) {
    return key_runtime_core_query_tap_series_state(key_runtime_core_state(), key_pos);
}

bool key_runtime_core_press_token_key_pos(const press_token_t *token, keypos_t *out) {
    key_runtime_core_state_t *state   = key_runtime_core_state();
    keypos_t                  key_pos = key_runtime_core_query_press_token_resolve_key_pos(state, token);

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out && token && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    *out = key_pos;
    return true;
}

bool key_runtime_core_tap_series_key_pos(const tap_series_t *series, keypos_t *out) {
    key_runtime_core_state_t *state   = key_runtime_core_state();
    keypos_t                  key_pos = key_runtime_core_query_tap_series_resolve_key_pos(state, series);

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out && series && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    *out = key_pos;
    return true;
}

uint8_t key_runtime_core_active_press_token_count(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    return state ? state->press_token_count : 0u;
}

bool key_runtime_core_active_press_token_key_pos(uint8_t order, keypos_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    uint8_t                   seen  = 0u;

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];

        if (!token->active) {
            continue;
        }

        if (seen++ == order) {
            *out = key_runtime_core_query_press_token_resolve_key_pos(state, token);
            return true;
        }
    }

    return false;
}

uint8_t key_runtime_core_pending_multi_tap_count(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    return state ? state->tap_series_count : 0u;
}

bool key_runtime_core_pending_multi_tap_key_pos(uint8_t order, keypos_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    uint8_t                   seen  = 0u;

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY; index++) {
        const tap_series_t *series = &state->tap_series[index];

        if (!series->active) {
            continue;
        }

        if (seen++ == order) {
            *out = key_runtime_core_query_tap_series_resolve_key_pos(state, series);
            return true;
        }
    }

    return false;
}

bool key_runtime_core_has_other_active_press_token(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];
        keypos_t             token_key_pos;

        token_key_pos = key_runtime_core_query_press_token_resolve_key_pos(state, token);
        if (token->active && !key_runtime_core_query_keypos_equal(token_key_pos, key_pos)) {
            return true;
        }
    }

    return false;
}

uint16_t key_runtime_core_owner_keycode_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return (token && token->active) ? token->resolved_keycode : KC_NO;
}

uint16_t key_runtime_core_tap_action_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return (token && token->active && token->handled_key) ? token->interaction.binding.tap_action : KC_NO;
}

key_runtime_slot_phase_t key_runtime_core_slot_phase_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return (token && token->active) ? token->slot_phase : KEY_RUNTIME_SLOT_PHASE_IDLE;
}

bool key_runtime_core_momentary_layer_tap_interrupted_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return token ? token->momentary_layer_tap_interrupted : false;
}

uint8_t key_runtime_core_pending_multi_tap_tap_count_at(keypos_t key_pos) {
    const tap_series_t *series = key_runtime_core_tap_series_at(key_pos);
    return (series && series->active) ? series->tap_count : 0u;
}

bool key_runtime_core_pending_multi_tap_holding_at(keypos_t key_pos) {
    const tap_series_t *series = key_runtime_core_tap_series_at(key_pos);
    return series ? series->pending_hold : false;
}

bool key_runtime_core_has_pending_multi_tap_at(keypos_t key_pos) {
    const tap_series_t *series = key_runtime_core_tap_series_at(key_pos);
    return series && series->active;
}

bool key_runtime_core_hold_is_complete_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return token && token->active && token->slot_phase == KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE;
}

uint8_t key_runtime_core_deferred_release_blocker_count(void) {
    return key_runtime_core_query_effective_deferred_release_blocker_count(key_runtime_core_state());
}

uint8_t key_runtime_core_deferred_release_timed_blocker_count(void) {
    return key_runtime_core_query_timed_deferred_release_blocker_count(key_runtime_core_state());
}

bool key_runtime_core_preview_owner_key_pos(keypos_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];
        keypos_t             token_key_pos;

        token_key_pos = key_runtime_core_query_press_token_resolve_key_pos(state, token);
        if (!(token->active && token->handled_key) || key_runtime_core_query_press_token_uses_implicit_hold(token) || key_runtime_core_query_press_token_uses_fallback_hold(token) || key_runtime_core_key_pos_held_action_keycode(state, token_key_pos) != KC_NO || key_runtime_core_key_pos_repeat_active(state, token_key_pos) || !key_runtime_core_query_press_token_allows_tap_release(token) || key_runtime_core_query_press_token_preview_layer_hint(token) == UINT8_MAX) {
            continue;
        }

        *out = token_key_pos;
        return true;
    }

    return false;
}

bool key_runtime_core_pending_fallback_key_pos(keypos_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];
        keypos_t             token_key_pos;

        if (!(token->active && token->handled_key) || !key_runtime_core_query_press_token_uses_fallback_hold(token) || key_runtime_core_query_press_token_owned_state_active(state, token) || token->resolved_keycode == KC_NO) {
            continue;
        }

        token_key_pos = key_runtime_core_query_press_token_resolve_key_pos(state, token);
        *out          = token_key_pos;
        return true;
    }

    return false;
}

uint8_t key_runtime_core_deferred_release_blocker_count_for_keypos(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token = key_runtime_core_query_press_token_state(state, key_pos);

    return key_runtime_core_query_press_token_blocks_deferred_release(state, token) ? 1u : 0u;
}
