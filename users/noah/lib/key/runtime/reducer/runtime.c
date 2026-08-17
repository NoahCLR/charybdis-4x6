// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Foundation
// ────────────────────────────────────────────────────────────────────────────

#include "runtime.h"
#include "../planning/effect_plan.h"
#include "ownership_state.h"
#include "../planning/release_internal.h"
#include "../planning/scan_planner.h"
#include "../planning/tap_series.h"

#include "../../../pointing/defs/pd_modes.h"
#include "../../behavior/handled_key_internal.h"
#include "../../behavior/handled_key_policy.h"
#include "../../behavior/key_behavior_lookup.h"
#include "../../ownership/held_action.h"
#include "../../../state/modifiers/keyboard_mod_policy.h"
#include "../feedback.h"
#include "../trace/core_trace.h"

#ifdef KEY_RUNTIME_HOT_PATH_TEST_INSTRUMENTATION
static key_runtime_hot_path_test_counters_t key_runtime_hot_path_test_counters;

void key_runtime_hot_path_test_counters_reset(void) {
    key_runtime_hot_path_test_counters = (key_runtime_hot_path_test_counters_t){0};
}

void key_runtime_hot_path_test_counters_snapshot(key_runtime_hot_path_test_counters_t *out) {
    if (out) {
        *out = key_runtime_hot_path_test_counters;
    }
}

bool key_runtime_hot_path_test_active_indexes_consistent(void) {
    key_runtime_core_state_t *state                                                      = key_runtime_core_state();
    uint32_t                  expected_press[KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT]  = {0};
    uint32_t                  expected_series[KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT] = {0};
    uint8_t                   press_count                                                = 0u;
    uint8_t                   series_count                                               = 0u;

    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        if (state->press_tokens[index].active) {
            expected_press[index / 32u] |= (uint32_t)1u << (index % 32u);
            press_count++;
        }
        if (state->tap_series[index].active) {
            expected_series[index / 32u] |= (uint32_t)1u << (index % 32u);
            series_count++;
        }
    }

    if (press_count != state->press_token_count || series_count != state->tap_series_count) {
        return false;
    }

    for (uint16_t word_index = 0; word_index < KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT; word_index++) {
        if (expected_press[word_index] != state->press_token_active_bitmap[word_index] || expected_series[word_index] != state->tap_series_active_bitmap[word_index]) {
            return false;
        }
    }

    return true;
}
#endif

__attribute__((weak)) const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

__attribute__((weak)) uint8_t pd_mode_buffered_tap_masked_real_mods(uint16_t keycode) {
    (void)keycode;
    return 0;
}

__attribute__((weak)) key_feedback_branch_confirm_mode_t key_feedback_branch_confirm_mode(void) {
    return KEY_FEEDBACK_BRANCH_CONFIRM_NON_BASE_TAPS;
}

__attribute__((weak)) key_feedback_tap_commit_mode_t key_feedback_tap_commit_mode(void) {
    return KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS;
}

static press_token_t *key_runtime_core_press_token_state(key_runtime_core_state_t *state, keypos_t key_pos);

bool key_runtime_core_keypos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static uint16_t key_runtime_core_keypos_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * MATRIX_COLS + key_pos.col);
}

static bool key_runtime_core_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

typedef enum {
    KEY_RUNTIME_CORE_FEEDBACK_LEVEL_PRESS = 0,
    KEY_RUNTIME_CORE_FEEDBACK_LEVEL_HOLD,
    KEY_RUNTIME_CORE_FEEDBACK_LEVEL_LONG_HOLD,
} key_runtime_core_feedback_level_t;

static keypos_t key_runtime_core_invalid_keypos(void) {
    return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static keypos_t key_runtime_core_keypos_from_slot_index(uint16_t index) {
    if (index >= KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY) {
        return key_runtime_core_invalid_keypos();
    }

    return (keypos_t){
        .row = (uint8_t)(index / MATRIX_COLS),
        .col = (uint8_t)(index % MATRIX_COLS),
    };
}

static bool key_runtime_core_press_token_slot_index(const key_runtime_core_state_t *state, const press_token_t *token, uint16_t *out) {
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

static keypos_t key_runtime_core_press_token_resolve_key_pos(const key_runtime_core_state_t *state, const press_token_t *token) {
    uint16_t index;

    return key_runtime_core_press_token_slot_index(state, token, &index) ? key_runtime_core_keypos_from_slot_index(index) : key_runtime_core_invalid_keypos();
}

static bool key_runtime_core_tap_series_slot_index(const key_runtime_core_state_t *state, const tap_series_t *series, uint16_t *out) {
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

static keypos_t key_runtime_core_tap_series_resolve_key_pos(const key_runtime_core_state_t *state, const tap_series_t *series) {
    uint16_t index;

    return key_runtime_core_tap_series_slot_index(state, series, &index) ? key_runtime_core_keypos_from_slot_index(index) : key_runtime_core_invalid_keypos();
}

static void key_runtime_core_active_bitmap_set(uint32_t *bitmap, uint16_t index) {
    if (bitmap && index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY) {
        bitmap[index / 32u] |= (uint32_t)1u << (index % 32u);
    }
}

static void key_runtime_core_active_bitmap_clear(uint32_t *bitmap, uint16_t index) {
    if (bitmap && index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY) {
        bitmap[index / 32u] &= ~((uint32_t)1u << (index % 32u));
    }
}

static void key_runtime_core_press_token_active_set(key_runtime_core_state_t *state, const press_token_t *token, bool active) {
    uint16_t index;

    if (key_runtime_core_press_token_slot_index(state, token, &index)) {
        if (active) {
            key_runtime_core_active_bitmap_set(state->press_token_active_bitmap, index);
        } else {
            key_runtime_core_active_bitmap_clear(state->press_token_active_bitmap, index);
        }
    }
}

static void key_runtime_core_tap_series_active_set(key_runtime_core_state_t *state, const tap_series_t *series, bool active) {
    uint16_t index;

    if (key_runtime_core_tap_series_slot_index(state, series, &index)) {
        if (active) {
            key_runtime_core_active_bitmap_set(state->tap_series_active_bitmap, index);
        } else {
            key_runtime_core_active_bitmap_clear(state->tap_series_active_bitmap, index);
        }
    }
}

static uint16_t key_runtime_core_default_hold_term(uint16_t keycode) {
    return (IS_QK_LAYER_TAP(keycode) || IS_QK_MOD_TAP(keycode)) ? TAPPING_TERM : CUSTOM_TAP_HOLD_TERM;
}

static uint16_t key_runtime_core_default_longer_hold_term(void) {
    return CUSTOM_LONGER_HOLD_TERM;
}

static uint16_t key_runtime_core_default_multi_tap_term(void) {
    return CUSTOM_MULTI_TAP_TERM;
}

static press_token_t *key_runtime_core_press_token_state(key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->press_tokens[key_runtime_core_keypos_index(key_pos)];
}

tap_series_t *key_runtime_core_tap_series_state(key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->tap_series[key_runtime_core_keypos_index(key_pos)];
}

void key_runtime_core_tap_series_clear(key_runtime_core_state_t *state, tap_series_t *series) {
    if (!(state && series && series->active)) {
        return;
    }

    key_runtime_core_tap_series_active_set(state, series, false);
    *series = (tap_series_t){0};
    if (state->tap_series_count != 0u) {
        state->tap_series_count--;
    }
}

static uint16_t key_runtime_core_elapsed(uint16_t start, uint16_t end) {
    return (uint16_t)(end - start);
}

static bool key_runtime_core_tap_series_can_accept_press(const key_runtime_core_state_t *state, const tap_series_t *series, uint16_t keycode, uint16_t now) {
    return series && series->active && !series->branch_confirmed && !series->branch_confirming && series->keycode == keycode && series->authored_has_more_taps && (key_runtime_core_elapsed(series->last_tap_at, now) <= series->tap_term_ms || key_runtime_core_tap_series_pending_combo_output(state, series));
}

static bool key_runtime_core_token_id_is_reserved(const key_runtime_core_state_t *state, uint16_t token_id) {
    if (!(state && token_id != 0u)) {
        return true;
    }

    if (state->press_token_count != 0u || state->pending_release_count != 0u) {
        for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
            const press_token_t *token = &state->press_tokens[index];

            if (token->token_id == token_id && (token->active || token->pending_release_emission || token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING)) {
                return true;
            }
        }
    }

    if (state->lease_count != 0u) {
        for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
            const lease_t *lease = &state->leases[index];

            if (lease->active && lease->owner_token_id == token_id) {
                return true;
            }
        }
    }

    if (state->pending_release_count != 0u) {
        for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
            const pending_release_slot_t *pending = &state->pending_releases[index];

            if ((pending->flags & KEY_RUNTIME_PENDING_RELEASE_FLAG_ACTIVE) != 0u && pending->owner_token_id == token_id) {
                return true;
            }
        }
    }

    return false;
}

static uint16_t key_runtime_core_next_token_id_candidate(uint16_t token_id) {
    return token_id >= KEY_RUNTIME_CORE_TOKEN_ID_MAX ? 1u : (uint16_t)(token_id + 1u);
}

static __attribute__((noinline)) bool key_runtime_core_allocate_token_id(key_runtime_core_state_t *state, uint16_t *out) {
    uint16_t candidate;
    uint16_t first_candidate;

    if (out) {
        *out = 0u;
    }
    if (!(state && out)) {
        return false;
    }

    candidate = state->next_token_id;
    if (candidate == 0u || candidate > KEY_RUNTIME_CORE_TOKEN_ID_MAX) {
        candidate = 1u;
    }
    first_candidate = candidate;

    do {
        uint16_t next_candidate = key_runtime_core_next_token_id_candidate(candidate);

        if (!key_runtime_core_token_id_is_reserved(state, candidate)) {
            *out                 = candidate;
            state->next_token_id = next_candidate;
            return true;
        }
        candidate = next_candidate;
    } while (candidate != first_candidate);

    state->next_token_id = first_candidate;
    return false;
}

static layer_state_t key_runtime_core_resolution_layers(const key_runtime_core_state_t *state) {
    return (layer_state | (state ? state->shadow_projection.layer_state : 0u) | ((layer_state_t)1u << 0));
}

static void key_runtime_core_press_token_cancel(key_runtime_core_state_t *state, press_token_t *token, uint16_t now) {
    if (!(state && token && token->active)) {
        return;
    }

    key_runtime_core_release_leases_for_token(state, token->token_id);
    key_runtime_core_shadow_projection_recompute(state);
    key_runtime_core_press_token_active_set(state, token, false);
    token->active      = false;
    token->released_at = now;
    token->phase       = PRESS_TOKEN_PHASE_CANCELLED;
    if (state->press_token_count != 0u) {
        state->press_token_count--;
    }
}

static void key_runtime_core_press_token_refresh_phase(key_runtime_core_state_t *state, press_token_t *token, uint16_t now) {
    press_token_phase_t previous_phase;

    if (!(token && token->active)) {
        return;
    }

    previous_phase = token->phase;

    if (token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING || token->phase == PRESS_TOKEN_PHASE_RELEASED || token->phase == PRESS_TOKEN_PHASE_CANCELLED) {
        return;
    }

    if (key_runtime_core_elapsed(token->pressed_at, now) >= token->hold_term_ms) {
        token->phase = PRESS_TOKEN_PHASE_HELD;
    } else if (token->phase == PRESS_TOKEN_PHASE_PRESSED) {
        token->phase = PRESS_TOKEN_PHASE_HOLD_PENDING;
    }

    if (token->phase != previous_phase && token->phase == PRESS_TOKEN_PHASE_HELD) {
        key_runtime_core_press_token_attach_hold_leases(state, token);
    }
}

static void key_runtime_core_tap_series_release_if_expired(tap_series_t *series, uint16_t now, key_runtime_core_state_t *state) {
    uint16_t action;
    uint8_t  repeat_count;

    if (!(series && state && series->active)) {
        return;
    }

    if (series->pending_hold || series->branch_confirmed || series->branch_confirming) {
        return;
    }

    if (key_runtime_core_elapsed(series->last_tap_at, now) <= series->tap_term_ms) {
        return;
    }

    if (!key_runtime_core_pending_multi_tap_flush_resolution(series, &action, &repeat_count)) {
        return;
    }

    if (action == KC_NO && repeat_count == 0u) {
        key_runtime_core_tap_series_clear(state, series);
    }
}

static void key_runtime_core_refresh_for_time(key_runtime_core_state_t *state, uint16_t now) {
    if (!state) {
        return;
    }

    state->current_time = now;

    for (uint16_t word_index = 0; word_index < KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT; word_index++) {
        uint32_t active_bits = state->press_token_active_bitmap[word_index] | state->tap_series_active_bitmap[word_index];

        while (active_bits != 0u) {
            uint16_t index = (uint16_t)(word_index * 32u + (uint16_t)__builtin_ctz(active_bits));

            active_bits &= active_bits - 1u;
#ifdef KEY_RUNTIME_HOT_PATH_TEST_INSTRUMENTATION
            key_runtime_hot_path_test_counters.refresh_slot_visit_count++;
#endif
            key_runtime_core_press_token_refresh_phase(state, &state->press_tokens[index], now);
            key_runtime_core_tap_series_release_if_expired(&state->tap_series[index], now, state);
        }
    }
}

static void key_runtime_core_refresh_slot_phases_for_scan(key_runtime_core_state_t *state, uint16_t now) {
    if (!state) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        key_runtime_core_press_token_refresh_slot_phase_for_scan(state, &state->press_tokens[index], now);
    }
}

static void key_runtime_core_press_token_begin(key_runtime_core_state_t *state, const runtime_key_event_t *event, uint16_t now) {
    press_token_t                 *token;
    tap_series_t                  *series;
    handled_key_resolution_t       resolution;
    handled_key_materialized_t     materialized;
    key_runtime_slot_interaction_t interaction = key_runtime_slot_interaction_default();
    handled_key_resolution_ctx_t   ctx;
    uint16_t                       hold_term_ms;
    uint16_t                       longer_hold_term_ms;
    uint16_t                       token_id;
    uint8_t                        tap_count                   = 1u;
    bool                           handled                     = false;
    bool                           tap_outcome_available       = false;
    bool                           pd_mode_was_locked_on_press = false;
    key_runtime_slot_phase_t       slot_phase                  = KEY_RUNTIME_SLOT_PHASE_IDLE;

    if (!state) {
        return;
    }

    state->token_allocation_failed_packed_key_pos = KEY_RUNTIME_PACKED_KEYPOS_NONE;
    if (!event) {
        return;
    }

    token  = key_runtime_core_press_token_state(state, event->key_pos);
    series = key_runtime_core_tap_series_state(state, event->key_pos);
    if (!token) {
        return;
    }

    if (!key_runtime_core_allocate_token_id(state, &token_id)) {
        if (state->token_allocation_failure_count != UINT8_MAX) {
            state->token_allocation_failure_count++;
        }
        state->token_allocation_failed_packed_key_pos = key_runtime_keypos_pack(event->key_pos);
        return;
    }

    if (token->active) {
        uint16_t cancelled_token_id = token->token_id;

        state->cancelled_press_count++;
        key_runtime_core_press_token_cancel(state, token, now);
        key_runtime_core_adopt_runtime_owned_state_leases(state, cancelled_token_id, token_id);
    }

    if (key_runtime_core_tap_series_can_accept_press(state, series, event->keycode, now)) {
        tap_count = (uint8_t)(series->tap_count + 1u);
    }

    handled_key_lookup_tap_count_into(event->keycode, tap_count, &resolution);
    handled = handled_key_resolution_is_handled(resolution);
    handled_key_materialized_default_into(&resolution, &materialized);
    hold_term_ms        = key_runtime_core_default_hold_term(event->keycode);
    longer_hold_term_ms = key_runtime_core_default_longer_hold_term();

    if (handled) {
        ctx = handled_key_resolution_ctx_make(event->key_pos, key_runtime_core_resolution_layers(state));
        handled_key_materialize_into(&resolution, &ctx, &materialized);
        key_runtime_slot_interaction_from_materialized_into(&materialized, &interaction);
        hold_term_ms                = handled_key_resolution_tap_hold_term(resolution);
        longer_hold_term_ms         = handled_key_resolution_longer_hold_term(resolution);
        tap_outcome_available       = handled_key_resolution_has_multi_tap(resolution) || materialized.tap_action != KC_NO;
        pd_mode_was_locked_on_press = materialized.pd_mode != 0 && state->shadow_projection.pd_mode_local_locked == materialized.pd_mode;
        slot_phase                  = hold_registers_on_press(materialized.hold) ? KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW : KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *other = &state->press_tokens[index];
        keypos_t       other_key_pos;

        other_key_pos = key_runtime_core_press_token_resolve_key_pos(state, other);
        if (!(other->active && !key_runtime_core_keypos_equal(other_key_pos, event->key_pos))) {
            continue;
        }

        other->other_press_interrupted = true;
        if (other->interaction.contract.suppress_tap_on_layer_interrupt) {
            other->momentary_layer_tap_interrupted = true;
        }
    }

    *token = (press_token_t){
        .active                      = true,
        .token_id                    = token_id,
        .physical_keycode            = event->keycode,
        .resolved_keycode            = event->keycode,
        .observed_release_keycode    = KC_NO,
        .pressed_at                  = now,
        .hold_term_ms                = hold_term_ms,
        .longer_hold_term_ms         = longer_hold_term_ms,
        .feedback_sequence           = key_runtime_core_state_next_feedback_sequence(state),
        .phase                       = PRESS_TOKEN_PHASE_PRESSED,
        .feedback_level              = KEY_RUNTIME_CORE_FEEDBACK_LEVEL_PRESS,
        .handled_key                 = handled,
        .tap_outcome_available       = tap_outcome_available,
        .pd_mode_was_locked_on_press = pd_mode_was_locked_on_press,
        .interaction                 = interaction,
        .slot_phase                  = slot_phase,
    };
    key_runtime_core_press_token_active_set(state, token, true);
    state->press_token_count++;
    key_runtime_core_press_token_attach_press_leases(state, token);

    if (series && tap_count > 1u) {
        series->pending_hold = materialized.hold.present || materialized.long_hold.present;
    }
}

static void key_runtime_core_tap_series_note_tap(key_runtime_core_state_t *state, const press_token_t *token, uint16_t now) {
    tap_series_t        *series;
    keypos_t             token_key_pos;
    bool                 reuse_existing;
    bool                 needs_series;
    uint16_t             single_action;
    uint16_t             tap_action;
    uint8_t              tap_repeat_count;
    bool                 has_more_taps;
    bool                 authored_has_more_taps;
    hold_behavior_t      hold;
    hold_behavior_t      long_hold;
    uint16_t             tap_hold_term_ms;
    uint16_t             branch_confirm_term_ms;
    uint16_t             tap_term_ms;
    uint8_t              tap_count;
    bool                 pending_hold                 = false;
    bool                 tap_branch_has_authored_step = false;
    bool                 tap_branch_has_authored_tap  = false;
    keyboard_mod_state_t saved_mod_state;

    if (!(state && token && token->active)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
    series        = key_runtime_core_tap_series_state(state, token_key_pos);
    if (!series) {
        return;
    }

    reuse_existing         = key_runtime_core_tap_series_can_accept_press(state, series, token->resolved_keycode, now);
    single_action          = reuse_existing ? series->single_action : token->resolved_keycode;
    tap_action             = token->resolved_keycode;
    tap_repeat_count       = 0u;
    has_more_taps          = false;
    authored_has_more_taps = false;
    hold                   = hold_behavior_none();
    long_hold              = hold_behavior_none();
    tap_hold_term_ms       = key_runtime_core_default_hold_term(token->resolved_keycode);
    branch_confirm_term_ms = CUSTOM_RGB_BRANCH_CONFIRM_TERM;
    tap_term_ms            = key_runtime_core_default_multi_tap_term();
    tap_count              = (uint8_t)(reuse_existing ? (uint8_t)(series->tap_count + 1u) : 1u);
    saved_mod_state        = reuse_existing ? series->saved_mod_state : keyboard_mod_policy_current_state();
    if (!reuse_existing) {
        saved_mod_state = keyboard_mod_policy_without_real_mods(saved_mod_state, pd_mode_buffered_tap_masked_real_mods(token->resolved_keycode));
    }

    if (token->handled_key) {
        uint16_t handled_tap_action = token->interaction.binding.tap_action;

        single_action                = reuse_existing ? series->single_action : handled_tap_action;
        tap_action                   = handled_tap_action;
        tap_repeat_count             = token->interaction.binding.tap_repeat_count;
        has_more_taps                = token->interaction.binding.has_more_taps;
        authored_has_more_taps       = token->interaction.binding.authored_has_more_taps;
        hold                         = token->interaction.binding.hold;
        long_hold                    = token->interaction.binding.long_hold;
        pending_hold                 = reuse_existing && (hold.present || long_hold.present);
        tap_branch_has_authored_step = key_behavior_step_present(token->interaction.selection.step);
        tap_branch_has_authored_tap  = token->interaction.selection.step.tap.present;
        tap_hold_term_ms             = token->interaction.binding.tap_hold_term;
        branch_confirm_term_ms       = token->interaction.binding.branch_confirm_term;
        tap_term_ms                  = token->interaction.binding.multi_tap_term;
        if (token->interaction.selection.tap_count != 0u) {
            tap_count = token->interaction.selection.tap_count;
        }
    }

    needs_series = reuse_existing || (token->handled_key && has_more_taps);
    if (!needs_series) {
        return;
    }

    if (!series->active) {
        state->tap_series_count++;
    }

    *series = (tap_series_t){
        .active                       = true,
        .keycode                      = token->resolved_keycode,
        .tap_count                    = tap_count,
        .pending_hold                 = pending_hold,
        .single_action                = single_action,
        .tap_action                   = tap_action,
        .tap_repeat_count             = tap_repeat_count,
        .tap_branch_has_authored_step = tap_branch_has_authored_step,
        .tap_branch_has_authored_tap  = tap_branch_has_authored_tap,
        .has_more_taps                = has_more_taps,
        .authored_has_more_taps       = authored_has_more_taps,
        .hold                         = hold,
        .long_hold                    = long_hold,
        .tap_hold_term_ms             = tap_hold_term_ms,
        .branch_confirm_term_ms       = branch_confirm_term_ms,
        .last_action                  = tap_action,
        .last_tap_at                  = now,
        .last_counted_tap_at          = now,
        .tap_term_ms                  = tap_term_ms,
        .feedback_sequence            = key_runtime_core_state_next_feedback_sequence(state),
        .saved_mod_state              = saved_mod_state,
    };
    key_runtime_core_tap_series_active_set(state, series, true);
}

static void key_runtime_core_tap_series_note_hold_release(key_runtime_core_state_t *state, const press_token_t *token) {
    tap_series_t *series;
    keypos_t      token_key_pos;

    if (!(state && token)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
    series        = key_runtime_core_tap_series_state(state, token_key_pos);
    if (!(series && series->active && series->pending_hold && series->keycode == token->resolved_keycode)) {
        return;
    }

    series->pending_hold = false;
}

static void key_runtime_core_tap_series_preserve_release(key_runtime_core_state_t *state, keypos_t key_pos) {
    tap_series_t *series;

    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    series = key_runtime_core_tap_series_state(state, key_pos);
    if (!(series && series->active)) {
        return;
    }

    if (series->pending_hold) {
        series->pending_hold = false;
        series->hold         = hold_behavior_none();
        series->long_hold    = hold_behavior_none();
    }
    series->last_tap_at       = state->current_time;
    series->feedback_sequence = key_runtime_core_state_next_feedback_sequence(state);
}

static void key_runtime_core_press_token_end(key_runtime_core_state_t *state, const runtime_key_event_t *event, uint16_t now) {
    press_token_t  released_token;
    press_token_t *token;

    if (!(state && event)) {
        return;
    }

    token = key_runtime_core_press_token_state(state, event->key_pos);
    if (!(token && token->active)) {
        state->orphan_release_count++;
        return;
    }

    key_runtime_core_press_token_refresh_phase(state, token, now);
    token->observed_release_keycode = event->keycode;
    token->released_at              = now;
    if (event->keycode != token->resolved_keycode) {
        token->release_keycode_mismatched = true;
        state->release_keycode_mismatch_count++;
    }

    released_token = *token;
    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW && key_runtime_core_elapsed(token->pressed_at, now) < token->hold_term_ms) {
        key_runtime_core_tap_series_note_tap(state, token, now);
    } else {
        key_runtime_core_tap_series_note_hold_release(state, token);
    }

    *token = released_token;
    key_runtime_core_press_token_active_set(state, token, false);
    token->active = false;
    token->phase  = PRESS_TOKEN_PHASE_RELEASED;
    if (state->press_token_count != 0u) {
        state->press_token_count--;
    }
}

void key_runtime_core_apply_event(const runtime_event_t *event, uint16_t event_time) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && event)) {
        return;
    }

    switch (event->kind) {
        case RUNTIME_EVENT_KIND_KEY_DOWN:
            key_runtime_core_refresh_for_time(state, event_time);
            key_runtime_core_press_token_begin(state, &event->data.key_event, event_time);
            return;
        case RUNTIME_EVENT_KIND_KEY_UP:
            key_runtime_core_refresh_for_time(state, event_time);
            key_runtime_core_press_token_end(state, &event->data.key_event, event_time);
            return;
        case RUNTIME_EVENT_KIND_TIMER_ADVANCE:
            key_runtime_core_refresh_for_time(state, (uint16_t)(event_time + event->data.timer_advance.advance_ms));
            return;
        case RUNTIME_EVENT_KIND_SCAN:
            key_runtime_core_refresh_for_time(state, event_time);
            key_runtime_core_refresh_slot_phases_for_scan(state, event_time);
            return;
        case RUNTIME_EVENT_KIND_POINTER_REPORT:
        case RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT:
            key_runtime_core_refresh_for_time(state, event_time);
            return;
    }
}

static __attribute__((noinline)) void key_runtime_core_observe_process_record_press(uint16_t keycode, const keyrecord_t *record, uint16_t now) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    runtime_key_event_t       event;

    if (!(state && record)) {
        return;
    }

    event = (runtime_key_event_t){
        .keycode = keycode,
        .key_pos = record->event.key,
    };
    key_runtime_core_refresh_for_time(state, now);
    key_runtime_core_press_token_begin(state, &event, now);
}

static __attribute__((noinline)) void key_runtime_core_observe_process_record_release(uint16_t keycode, const keyrecord_t *record, uint16_t now) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    runtime_key_event_t       event;

    if (!(state && record)) {
        return;
    }

    event = (runtime_key_event_t){
        .keycode = keycode,
        .key_pos = record->event.key,
    };
    key_runtime_core_refresh_for_time(state, now);
    key_runtime_core_press_token_end(state, &event, now);
}

void key_runtime_core_observe_process_record_event(uint16_t keycode, keyrecord_t *record) {
    uint16_t now;

    if (!record) {
        return;
    }

    now = timer_read();
    if (record->event.pressed) {
        key_runtime_core_observe_process_record_press(keycode, record, now);
    } else {
        key_runtime_core_observe_process_record_release(keycode, record, now);
    }
}

void key_runtime_core_observe_scan_cycle(uint16_t now) {
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, now);
}

bool key_runtime_core_reset_pending_multi_tap(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    tap_series_t             *series;

    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    series = key_runtime_core_tap_series_state(state, key_pos);
    if (!(series && series->active)) {
        return false;
    }

    key_runtime_core_tap_series_clear(state, series);
    return true;
}

static bool key_runtime_core_press_token_allows_tap_release(const press_token_t *token) {
    if (!(token && token->active && token->handled_key)) {
        return false;
    }

    return token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || token->slot_phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW;
}

static bool key_runtime_core_press_token_should_unlock_pd_lock_on_press(const press_token_t *token) {
    return token && token->handled_key && token->pd_mode_was_locked_on_press && token->interaction.pd_mode != 0;
}

static void key_runtime_core_apply_release_settlement(key_runtime_core_state_t *state, keypos_t key_pos, const key_runtime_core_release_effect_plan_t *release_plan) {
    press_token_t *token;
    uint8_t        lease_count_before;

    if (!(state && release_plan && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    switch (release_plan->settlement) {
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_CLEAR_ACTIVE_PRESERVE_PENDING_MULTI_TAP:
            key_runtime_core_tap_series_preserve_release(state, key_pos);
            break;
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_RESET:
            (void)key_runtime_core_reset_pending_multi_tap(key_pos);
            break;
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_NONE:
        default:
            return;
    }

    token = key_runtime_core_press_token_state(state, key_pos);
    if (token) {
        lease_count_before = state->lease_count;
        key_runtime_core_release_leases_for_token(state, token->token_id);
        if (state->lease_count != lease_count_before) {
            key_runtime_core_shadow_projection_recompute(state);
        }
    }
    if (token && !token->active && !token->pending_release_emission) {
        *token = (press_token_t){0};
    }
}

static void key_runtime_core_tap_series_seed(key_runtime_core_state_t *state, const key_runtime_core_pending_multi_tap_seed_t *seed, keyboard_mod_state_t mods) {
    tap_series_t *series;

    if (!(state && seed && seed->active)) {
        return;
    }

    series = key_runtime_core_tap_series_state(state, seed->key_pos);
    if (!series) {
        return;
    }

    if (!series->active) {
        state->tap_series_count++;
    }

    *series = (tap_series_t){
        .active                       = true,
        .keycode                      = seed->keycode,
        .tap_count                    = seed->tap_count == 0u ? 1u : seed->tap_count,
        .pending_hold                 = false,
        .single_action                = seed->tap_action,
        .tap_action                   = seed->tap_action,
        .tap_repeat_count             = seed->tap_repeat_count,
        .tap_branch_has_authored_step = seed->tap_branch_has_authored_step,
        .tap_branch_has_authored_tap  = seed->tap_branch_has_authored_tap,
        .has_more_taps                = seed->has_more_taps,
        .authored_has_more_taps       = seed->authored_has_more_taps,
        .hold                         = hold_behavior_none(),
        .long_hold                    = hold_behavior_none(),
        .tap_hold_term_ms             = seed->tap_hold_term,
        .branch_confirm_term_ms       = seed->branch_confirm_term,
        .last_action                  = seed->tap_action,
        .last_tap_at                  = state->current_time,
        .last_counted_tap_at          = state->current_time,
        .tap_term_ms                  = seed->multi_tap_term,
        .feedback_sequence            = key_runtime_core_state_next_feedback_sequence(state),
        .saved_mod_state              = mods,
    };
    key_runtime_core_tap_series_active_set(state, series, true);
}

static void key_runtime_core_tap_series_update_for_press(key_runtime_core_state_t *state, tap_series_t *series, const press_token_t *token) {
    keypos_t token_key_pos;

    if (!(state && series && token && token->active && token->handled_key)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);

    if (!series->active) {
        state->tap_series_count++;
    }

    series->active                       = true;
    series->keycode                      = token->resolved_keycode;
    series->tap_count                    = token->interaction.selection.tap_count ? token->interaction.selection.tap_count : (uint8_t)(series->tap_count + 1u);
    series->pending_hold                 = token->interaction.binding.hold.present || token->interaction.binding.long_hold.present;
    series->single_action                = series->single_action == KC_NO ? token->interaction.binding.tap_action : series->single_action;
    series->tap_action                   = token->interaction.binding.tap_action;
    series->tap_repeat_count             = token->interaction.binding.tap_repeat_count;
    series->tap_branch_has_authored_step = key_behavior_step_present(token->interaction.selection.step);
    series->tap_branch_has_authored_tap  = token->interaction.selection.step.tap.present;
    series->has_more_taps                = token->interaction.binding.has_more_taps;
    series->authored_has_more_taps       = token->interaction.binding.authored_has_more_taps;
    series->hold                         = token->interaction.binding.hold;
    series->long_hold                    = token->interaction.binding.long_hold;
    series->tap_hold_term_ms             = token->interaction.binding.tap_hold_term;
    series->branch_confirm_term_ms       = token->interaction.binding.branch_confirm_term;
    series->last_action                  = token->interaction.binding.tap_action;
    series->last_tap_at                  = state->current_time;
    series->last_counted_tap_at          = state->current_time;
    series->tap_term_ms                  = token->interaction.binding.multi_tap_term;
    series->feedback_sequence            = key_runtime_core_state_next_feedback_sequence(state);
    key_runtime_core_tap_series_active_set(state, series, true);
    (void)token_key_pos;
}

void key_runtime_core_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan)) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token = &state->press_tokens[index];
        keypos_t       token_key_pos;

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        if (!(token->active && !key_runtime_core_keypos_equal(token_key_pos, key_pos))) {
            continue;
        }

        key_runtime_core_plan_fallback_hold_activation(state, token, plan);
        token->other_press_interrupted = true;
        if (token->interaction.contract.suppress_tap_on_layer_interrupt) {
            token->momentary_layer_tap_interrupted = true;
        }
    }
}

void key_runtime_core_flush_active_keys_except(keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan)) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token = &state->press_tokens[index];
        uint16_t       held_action;
        keypos_t       token_key_pos;

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        if (!(token->active && !key_runtime_core_keypos_equal(token_key_pos, key_pos))) {
            continue;
        }

        held_action = key_runtime_core_key_pos_held_action_keycode(state, token_key_pos);
        if (key_runtime_core_press_token_allows_tap_release(token) && held_action == KC_NO && !key_runtime_core_key_pos_repeat_active(state, token_key_pos) && !is_layer_key(token->resolved_keycode) && token->interaction.binding.tap_action != KC_NO) {
            key_runtime_core_effect_plan_push_dispatch_action(plan, token_key_pos, token->interaction.binding.tap_action);
            key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, token_key_pos, token->interaction.binding.tap_action, token->interaction.selection.tap_count);
        } else if (held_action != KC_NO && !held_action_survives_flush(token_key_pos, held_action)) {
            key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER, token_key_pos, held_action);
        } else if (key_runtime_core_key_pos_repeat_active(state, token_key_pos)) {
            key_runtime_core_effect_plan_push_release_owned_state(plan, token_key_pos);
        }

        key_runtime_core_press_token_cancel(state, token, state->current_time);
        key_runtime_core_shadow_projection_recompute(state);
    }
}

bool key_runtime_core_handle_handled_key_press(uint16_t keycode, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;
    tap_series_t             *series;
    uint16_t                  action;
    uint8_t                   repeat_count;
    delayed_action_mods_t     mods;
    keypos_t                  token_key_pos;
    keypos_t                  series_key_pos;

    if (!(state && plan && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    if (state->token_allocation_failed_packed_key_pos == key_runtime_keypos_pack(key_pos)) {
        state->token_allocation_failed_packed_key_pos = KEY_RUNTIME_PACKED_KEYPOS_NONE;
        return true;
    }

    token  = key_runtime_core_press_token_state(state, key_pos);
    series = key_runtime_core_tap_series_state(state, key_pos);
    if (!(token && token->active && token->handled_key)) {
        return false;
    }

    token_key_pos  = key_runtime_core_press_token_resolve_key_pos(state, token);
    series_key_pos = key_runtime_core_tap_series_resolve_key_pos(state, series);

    if (series && series->active && !key_runtime_core_tap_series_can_accept_press(state, series, keycode, state->current_time)) {
        if (series->branch_confirming && series->branch_confirm_kind == KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION) {
            key_runtime_core_plan_same_key_branch_confirm_interruption(state, series, series_key_pos, plan);
        } else if (key_runtime_core_tap_series_take_flush(series, &action, &repeat_count, &mods)) {
            bool tap_commit_feedback = key_runtime_core_tap_series_has_authored_tap_branch(series) && key_runtime_core_tap_commit_feedback_allowed(action, series->tap_count);

            key_runtime_core_effect_plan_push_deferred_delayed_action(plan, series_key_pos, action, mods, repeat_count, tap_commit_feedback);
            key_runtime_core_tap_series_clear(state, series);
        } else {
            key_runtime_core_tap_series_clear(state, series);
        }
    }

    if (key_runtime_core_press_token_should_unlock_pd_lock_on_press(token)) {
        key_runtime_core_effect_plan_push_pd_mode_lock_state(plan, token_key_pos, token->interaction.pd_mode, false);
        token->pd_mode_lock_consumed_on_press = true;
    }

    if (key_runtime_core_tap_series_can_accept_press(state, series, keycode, state->current_time)) {
        key_runtime_core_tap_series_update_for_press(state, series, token);
        if (token->interaction.binding.tap_resolves_on_press && !series->pending_hold) {
            key_runtime_core_effect_plan_push_dispatch_action(plan, token_key_pos, token->interaction.binding.tap_action);
            key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, token_key_pos, token->interaction.binding.tap_action, token->interaction.selection.tap_count);
            key_runtime_core_tap_series_clear(state, series);
        }
        if (key_runtime_slot_interaction_is_momentary_layer(token->interaction)) {
            key_runtime_core_effect_plan_push_layer_press(plan, token_key_pos, token->interaction.layer);
        }
        if (series && series->active && series->pending_hold && hold_registers_on_press(token->interaction.binding.hold)) {
            key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, token_key_pos, token->interaction.binding.hold.action);
        }
        token->slot_phase = (series && series->active && series->pending_hold) ? KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW : KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE;
        return true;
    }

    if (key_runtime_slot_interaction_is_momentary_layer(token->interaction)) {
        key_runtime_core_effect_plan_push_layer_press(plan, token_key_pos, token->interaction.layer);
    }

    if (hold_registers_on_press(token->interaction.binding.hold)) {
        key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, token_key_pos, token->interaction.binding.hold.action);
    }

    return true;
}

static __attribute__((noinline)) bool key_runtime_core_try_pending_multi_tap_release(key_runtime_core_state_t *state, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    press_token_t                                          *token;
    tap_series_t                                           *series;
    key_runtime_core_release_effect_plan_t                  release_plan;
    key_runtime_core_pending_multi_tap_release_resolution_t pending_resolution;
    delayed_action_mods_t                                   series_mods;

    token       = key_runtime_core_press_token_state(state, key_pos);
    series      = key_runtime_core_tap_series_state(state, key_pos);
    series_mods = series ? series->saved_mod_state : (delayed_action_mods_t){0};
    if (!(series && series->active && token && token->handled_key && token->interaction.selection.tap_count > 1u && !token->active && token->observed_release_keycode != KC_NO && series->keycode == token->resolved_keycode && key_runtime_core_resolve_pending_multi_tap_release(key_pos, series->tap_action, series->tap_repeat_count, true, &pending_resolution) && key_runtime_core_plan_pending_multi_tap_release_effects(key_pos, key_runtime_slot_interaction_is_momentary_layer(token->interaction), &pending_resolution, series_mods, &release_plan))) {
        return false;
    }

    key_runtime_core_apply_release_settlement(state, key_pos, &release_plan);
    key_runtime_core_effect_plan_append_release_plan(plan, &release_plan);
    return true;
}

static __attribute__((noinline)) bool key_runtime_core_try_active_release(key_runtime_core_state_t *state, uint16_t keycode, keypos_t key_pos, keyboard_mod_state_t keyboard_mod_state, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_release_effect_plan_t       release_plan;
    key_runtime_core_active_release_resolution_t active_resolution;

    if (!(key_runtime_core_resolve_active_release(key_pos, &active_resolution) && key_runtime_core_plan_active_release_effects(key_pos, keycode, &active_resolution, &release_plan))) {
        return false;
    }

    key_runtime_core_apply_release_settlement(state, key_pos, &release_plan);
    key_runtime_core_effect_plan_append_release_plan(plan, &release_plan);
    if (release_plan.pending_multi_tap_seed.active) {
        key_runtime_core_tap_series_seed(state, &release_plan.pending_multi_tap_seed, keyboard_mod_state);
    }
    return true;
}

static __attribute__((noinline)) void key_runtime_core_plan_unmatched_release(key_runtime_core_state_t *state, keypos_t key_pos, const handled_key_resolution_t *resolution, key_runtime_core_effect_plan_t *plan) {
    handled_key_resolution_ctx_t ctx = handled_key_resolution_ctx_make(key_pos, key_runtime_core_resolution_layers(state));

    if (handled_key_resolution_materializes_momentary_layer(resolution, &ctx)) {
        key_runtime_core_effect_plan_push_layer_release(plan, key_pos);
    }
    key_runtime_core_effect_plan_push_release_owned_state(plan, key_pos);
}

bool key_runtime_core_handle_handled_key_release(uint16_t keycode, keypos_t key_pos, const handled_key_resolution_t *resolution, keyboard_mod_state_t keyboard_mod_state, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    if (key_runtime_core_try_pending_multi_tap_release(state, key_pos, plan)) {
        return true;
    }

    if (key_runtime_core_try_active_release(state, keycode, key_pos, keyboard_mod_state, plan)) {
        return true;
    }

    if (!(resolution && handled_key_resolution_is_handled(*resolution))) {
        return false;
    }

    key_runtime_core_plan_unmatched_release(state, key_pos, resolution, plan);
    return true;
}

void key_runtime_core_scan(key_runtime_core_effect_plan_t *plan, uint16_t now) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan)) {
        return;
    }

    key_runtime_core_refresh_for_time(state, now);

    for (uint16_t word_index = 0; word_index < KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT; word_index++) {
        uint32_t active_bits = state->press_token_active_bitmap[word_index];

        while (active_bits != 0u) {
            uint16_t index = (uint16_t)(word_index * 32u + (uint16_t)__builtin_ctz(active_bits));

            active_bits &= active_bits - 1u;
#ifdef KEY_RUNTIME_HOT_PATH_TEST_INSTRUMENTATION
            key_runtime_hot_path_test_counters.scan_press_slot_visit_count++;
#endif
            key_runtime_core_plan_active_scan_for_token(state, &state->press_tokens[index], plan);
        }
    }

    for (uint16_t word_index = 0; word_index < KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT; word_index++) {
        uint32_t active_bits = state->tap_series_active_bitmap[word_index];

        while (active_bits != 0u) {
            uint16_t index = (uint16_t)(word_index * 32u + (uint16_t)__builtin_ctz(active_bits));

            active_bits &= active_bits - 1u;
#ifdef KEY_RUNTIME_HOT_PATH_TEST_INSTRUMENTATION
            key_runtime_hot_path_test_counters.scan_tap_series_slot_visit_count++;
#endif
            tap_series_t *series = &state->tap_series[index];
            keypos_t      series_key_pos;

            if (!series->active) {
                continue;
            }

            series_key_pos = key_runtime_core_tap_series_resolve_key_pos(state, series);
            key_runtime_core_plan_pending_multi_tap_scan_for_key(state, series_key_pos, plan);
        }
    }
}
