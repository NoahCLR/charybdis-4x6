#include "../../../compat/qmk_gesture_timing.h"
#include "lib/profile/runtime/effective_settings_runtime.h"
// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Foundation
// ────────────────────────────────────────────────────────────────────────────

#include "runtime.h"
#include "../planning/effect_plan.h"
#include "ownership_state.h"
#include "state_query.h"
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

void key_runtime_hot_path_test_record_preview_slot_visit(void) {
    key_runtime_hot_path_test_counters.preview_slot_visit_count++;
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

// A press the runtime does not handle. Every dual-role key is handled and takes
// its hold time from its resolution (D-F27), so this is the tap/hold setting.
static uint16_t key_runtime_core_default_hold_term(void) {
    return noah_setting(NOAH_SETTING_TAP_HOLD_TERM, CUSTOM_TAP_HOLD_TERM);
}

static uint16_t key_runtime_core_default_long_hold_term(void) {
    return noah_setting(NOAH_SETTING_LONG_HOLD_TERM, CUSTOM_LONGER_HOLD_TERM);
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

// A series counts taps up to the deepest authored branch. A press inside the
// same quick run past that branch takes no tap count: the run becomes typing
// (D-F33), its taps so far are sent as the first tap once each, and this press
// and every further one in the run are single taps of their own, until the
// multi-tap window lapses. So a run's taps are never lost and never fire a
// branch the run went past.
static uint8_t key_runtime_core_tap_series_next_tap_count(const tap_series_t *series) {
    return series ? (uint8_t)(series->tap_count + 1u) : 1u;
}

static bool key_runtime_core_tap_series_in_run(const key_runtime_core_state_t *state, const tap_series_t *series, uint16_t keycode, uint16_t now) {
    return series && series->active && !series->resolved && series->keycode == keycode && series->authored_tap_depth > 1u && (key_runtime_core_elapsed(series->last_tap_at, now) <= series->tap_term_ms || key_runtime_core_tap_series_pending_gesture(state, series));
}

static bool key_runtime_core_tap_series_can_accept_press(const key_runtime_core_state_t *state, const tap_series_t *series, uint16_t keycode, uint16_t now) {
    return key_runtime_core_tap_series_in_run(state, series, keycode, now) && !series->typing && series->tap_count < series->authored_tap_depth;
}

static bool key_runtime_core_tap_series_overshoots(const key_runtime_core_state_t *state, const tap_series_t *series, uint16_t keycode, uint16_t now) {
    return key_runtime_core_tap_series_in_run(state, series, keycode, now) && !key_runtime_core_tap_series_can_accept_press(state, series, keycode, now);
}

// A press that overshot its run sends the taps counted so far as the first
// tap, once each, rather than the branch they reached.
static void key_runtime_core_tap_series_type_first_tap(tap_series_t *series) {
    series->tap_repeat_count             = 0u;
    series->tap_branch_has_authored_tap  = false;
    series->tap_branch_has_authored_step = false;
}

// ── The tap series' transitions ─────────────────────────────────────────────
// The series is written only here, plus its flush and clear (planning/
// tap_series_flush.c, key_runtime_core_tap_series_clear()), the scan's
// resolve (key_runtime_core_tap_series_resolve() in planning/tap_series.h) and
// typing its first tap (key_runtime_core_tap_series_type_first_tap()). A
// press joins it; the release decision for a key's first tap starts it, and
// for a later tap preserves or ends it; a hold's release ends its pending
// hold. What it carries of a press is filled from that press's token in one
// place.

static void key_runtime_core_tap_series_fill_from_token(key_runtime_core_state_t *state, tap_series_t *series, const press_token_t *token) {
    const key_runtime_slot_interaction_t *interaction = &token->interaction;

    if (!series->active) {
        state->tap_series_count++;
    }

    series->active                       = true;
    series->keycode                      = token->resolved_keycode;
    series->tap_count                    = interaction->selection.tap_count ? interaction->selection.tap_count : key_runtime_core_tap_series_next_tap_count(series);
    series->single_action                = series->single_action == KC_NO ? interaction->binding.tap_action : series->single_action;
    series->tap_action                   = interaction->binding.tap_action;
    series->tap_repeat_count             = interaction->binding.tap_repeat_count;
    series->tap_branch_has_authored_step = key_behavior_step_present(interaction->selection.step);
    series->tap_branch_has_authored_tap  = interaction->selection.step.tap.present;
    series->has_more_taps                = interaction->binding.has_more_taps;
    series->authored_tap_depth           = interaction->binding.authored_tap_depth;
    series->typing                       = token->typing;
    series->hold                         = interaction->binding.hold;
    series->long_hold                    = interaction->binding.long_hold;
    series->tap_hold_term_ms             = interaction->binding.tap_hold_term;
    series->last_action                  = interaction->binding.tap_action;
    series->tap_term_ms                  = interaction->binding.multi_tap_term;
    series->feedback_sequence            = key_runtime_core_state_next_feedback_sequence(state);
    key_runtime_core_tap_series_active_set(state, series, true);
}

// A handled press joins its key's series, which a hold at this tap count keeps
// pending until the press is decided. The series keeps its first tap and the
// modifiers that tap was released under.
static void key_runtime_core_tap_series_join(key_runtime_core_state_t *state, tap_series_t *series, const press_token_t *token) {
    if (!(state && series && token && token->active && token->handled_key)) {
        return;
    }

    key_runtime_core_tap_series_fill_from_token(state, series, token);
    series->pending_hold = token->interaction.binding.hold.present || token->interaction.binding.long_hold.present;
    series->last_tap_at  = token->pressed_at;
}

// A first tap the release decision buffers starts the series, under the
// modifiers the release was planned with: a pointing mode's mask has already
// taken out what the mode holds for itself. The tap a typing press leaves
// waits as a typing series too (`typing`), so the next press in the run is a
// single tap of its own as well.
static void key_runtime_core_tap_series_start(key_runtime_core_state_t *state, keypos_t key_pos, const press_token_t *token, keyboard_mod_state_t mods) {
    tap_series_t *series = key_runtime_core_tap_series_state(state, key_pos);

    if (!(series && token)) {
        return;
    }

    key_runtime_core_tap_series_clear(state, series);
    key_runtime_core_tap_series_fill_from_token(state, series, token);
    key_runtime_core_tap_series_drop_hold(series);
    series->last_tap_at     = token->released_at;
    series->saved_mod_state = mods;
}

// A hold's release ends the series' pending hold; the release decision settles
// the rest.
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

// A later tap's release that keeps the series waits a new multi-tap window
// from that release, with nothing held.
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
        key_runtime_core_tap_series_drop_hold(series);
    }
    series->last_tap_at       = key_runtime_core_press_token_state(state, key_pos)->released_at;
    series->feedback_sequence = key_runtime_core_state_next_feedback_sequence(state);
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
#if KEY_RUNTIME_CORE_TOKEN_ID_MAX < UINT16_MAX
    if (candidate > KEY_RUNTIME_CORE_TOKEN_ID_MAX) {
        candidate = 1u;
    }
#endif
    if (candidate == 0u) {
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

    if (!key_runtime_core_press_token_hold_eligible(state, token, now)) {
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

    if (series->pending_hold || series->resolved) {
        return;
    }

    if (key_runtime_core_elapsed(series->last_tap_at, now) <= series->tap_term_ms || key_runtime_core_tap_series_pending_gesture(state, series)) {
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
    uint16_t                       long_hold_term_ms;
    uint16_t                       token_id;
    uint8_t                        tap_count                   = 1u;
    bool                           handled                     = false;
    bool                           tap_outcome_available       = false;
    bool                           pd_mode_was_locked_on_press = false;
    bool                           typing                      = false;
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
        tap_count = key_runtime_core_tap_series_next_tap_count(series);
    } else {
        typing = key_runtime_core_tap_series_overshoots(state, series, event->keycode, now);
    }

    handled_key_lookup_row_tap_count_into(event->keycode, tap_count, !event->bypass_row, &resolution);
    handled = handled_key_resolution_is_handled(resolution);
    handled_key_materialized_default_into(&resolution, &materialized);
    hold_term_ms      = key_runtime_core_default_hold_term();
    long_hold_term_ms = key_runtime_core_default_long_hold_term();

    if (handled) {
        ctx = handled_key_resolution_ctx_make(event->key_pos, key_runtime_core_resolution_layers(state));
        handled_key_materialize_into(&resolution, &ctx, &materialized);
        key_runtime_slot_interaction_from_materialized_into(&materialized, &interaction);
        hold_term_ms                = handled_key_resolution_tap_hold_term(resolution);
        long_hold_term_ms           = handled_key_resolution_long_hold_term(resolution);
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
        .long_hold_term_ms           = long_hold_term_ms,
        .feedback_sequence           = key_runtime_core_state_next_feedback_sequence(state),
        .phase                       = PRESS_TOKEN_PHASE_PRESSED,
        .feedback_level              = KEY_RUNTIME_CORE_FEEDBACK_LEVEL_PRESS,
        .handled_key                 = handled,
        .tap_outcome_available       = tap_outcome_available,
        .pd_mode_was_locked_on_press = pd_mode_was_locked_on_press,
        .typing                      = typing,
        .interaction                 = interaction,
        .slot_phase                  = slot_phase,
    };
    key_runtime_core_press_token_active_set(state, token, true);
    state->press_token_count++;
    key_runtime_core_press_token_attach_press_leases(state, token);
}

static void key_runtime_core_press_token_end(key_runtime_core_state_t *state, const runtime_key_event_t *event, uint16_t now) {
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

    // A tap's release is the release decision's to settle (it starts,
    // preserves or ends the series); a hold's ends the series' pending hold.
    if (!(token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW && key_runtime_core_elapsed(token->pressed_at, now) < token->hold_term_ms)) {
        key_runtime_core_tap_series_note_hold_release(state, token);
    }

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

static __attribute__((noinline)) void key_runtime_core_observe_process_record_press(uint16_t keycode, const keyrecord_t *record, bool use_row, uint16_t now) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    runtime_key_event_t       event;

    if (!(state && record)) {
        return;
    }

    event = (runtime_key_event_t){
        .keycode = keycode,
        .key_pos = record->event.key,
        .bypass_row = !use_row,
    };
    key_runtime_core_refresh_for_time(state, now);
    key_runtime_core_press_token_begin(state, &event, noah_qmk_gesture_event_time(record));
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
    key_runtime_core_press_token_end(state, &event, noah_qmk_gesture_event_time(record));
    key_runtime_core_refresh_for_time(state, now);
}

void key_runtime_core_observe_process_record_event(uint16_t keycode, keyrecord_t *record) {
    key_runtime_core_observe_process_record_event_with_row(keycode, record, true);
}

void key_runtime_core_observe_process_record_event_with_row(uint16_t keycode, keyrecord_t *record, bool use_row) {
    uint16_t now;

    if (!record) {
        return;
    }

    now = timer_read();
    if (record->event.pressed) {
        key_runtime_core_observe_process_record_press(keycode, record, use_row, now);
    } else {
        key_runtime_core_observe_process_record_release(keycode, record, now);
    }
}

void key_runtime_core_offer_press_to_mode(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (state && key_runtime_core_keypos_valid(key_pos)) {
        key_origin_bitmap_add_keypos(state->pd_intercepted_bitmap, key_pos);
    }
}

bool key_runtime_core_press_offered_to_mode(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return state && key_runtime_core_keypos_valid(key_pos) && key_origin_bitmap_has_keypos(state->pd_intercepted_bitmap, key_pos);
}

void key_runtime_core_end_mode_offer(keypos_t key_pos, bool consumed) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;
    tap_series_t             *series;

    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }
    if (!consumed) {
        key_origin_bitmap_remove_keypos(state->pd_intercepted_bitmap, key_pos);
        return;
    }
    token = key_runtime_core_press_token_state(state, key_pos);
    if (!(token && token->active)) {
        return;
    }
    key_runtime_core_release_leases_for_token(state, token->token_id);
    // Reaching a mode takes another key's press, which already flushed any
    // series this key had pending, so the series here counts only this press.
    series = key_runtime_core_tap_series_state(state, key_pos);
    if (series && series->active) {
        key_runtime_core_tap_series_clear(state, series);
    }
    token->handled_key                 = false;
    token->tap_outcome_available       = false;
    token->pd_mode_was_locked_on_press = false;
    token->interaction                 = key_runtime_slot_interaction_default();
    token->slot_phase                  = KEY_RUNTIME_SLOT_PHASE_IDLE;
    key_runtime_core_shadow_projection_recompute(state);
}

bool key_runtime_core_take_intercepted_release(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && key_runtime_core_keypos_valid(key_pos) && key_origin_bitmap_has_keypos(state->pd_intercepted_bitmap, key_pos))) {
        return false;
    }
    key_origin_bitmap_remove_keypos(state->pd_intercepted_bitmap, key_pos);
    return true;
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

static void key_runtime_core_apply_release_settlement(key_runtime_core_state_t *state, keypos_t key_pos, const key_runtime_core_release_effect_plan_t *release_plan, keyboard_mod_state_t keyboard_mod_state) {
    press_token_t *token;
    uint8_t        lease_count_before;

    if (!(state && release_plan && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    token = key_runtime_core_press_token_state(state, key_pos);
    switch (release_plan->settlement) {
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_CLEAR_ACTIVE_PRESERVE_PENDING_MULTI_TAP:
            key_runtime_core_tap_series_preserve_release(state, key_pos);
            break;
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_START_TAP_SERIES:
            key_runtime_core_tap_series_start(state, key_pos, token, keyboard_mod_state);
            break;
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_RESET:
            (void)key_runtime_core_reset_pending_multi_tap(key_pos);
            break;
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_NONE:
        default:
            return;
    }

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

// A down key whose hold is an output of its own: not a fallback hold, not a
// hold that changes other keys (those wait, D-F03), not a momentary layer key
// (MO, TT, OSL, LM: its layer is on from the press, and another key's press
// cancels its quick tap), and nothing a pointing mode owns. Another key's
// press decides it (D-F27).
static bool key_runtime_core_press_token_settles_on_other_press(const press_token_t *token, keypos_t token_key_pos) {
    return token->handled_key && token->interaction.pd_mode == 0 && !token->interaction.contract.suppress_tap_on_layer_interrupt && !key_runtime_slot_interaction_uses_fallback_hold(token->interaction) && !key_runtime_slot_interaction_uses_implicit_hold(token->interaction) && !key_runtime_core_press_token_hold_changes_other_keys(token) && !noah_action_desc_is_pd_mode_action(noah_action_describe(token->interaction.binding.tap_action)) && !key_runtime_core_press_offered_to_mode(token_key_pos);
}

// The release action a hold that waits for release would send now, if its
// hold time has passed.
static uint16_t key_runtime_core_press_token_release_hold_action(const press_token_t *token, uint16_t elapsed) {
    const key_runtime_slot_interaction_t *interaction = &token->interaction;

    if (interaction->contract.long_hold.release_action != KC_NO && elapsed >= interaction->binding.long_hold_term) {
        return interaction->contract.long_hold.release_action;
    }
    if (interaction->contract.hold.release_action != KC_NO && elapsed >= interaction->binding.tap_hold_term) {
        return interaction->contract.hold.release_action;
    }
    return KC_NO;
}

// Settles a key that settles on another key's press, before that press goes
// out: short of its hold time it is a tap, its pending repeated taps
// included; past it, a hold that waits for release sends that hold. A hold
// that fires at its threshold has already gone out. The key's own release
// then sends nothing.
static void key_runtime_core_settle_on_other_press(key_runtime_core_state_t *state, press_token_t *token, keypos_t token_key_pos, uint16_t other_pressed_at, key_runtime_core_effect_plan_t *plan) {
    tap_series_t         *series = key_runtime_core_tap_series_state(state, token_key_pos);
    uint16_t              elapsed = key_runtime_core_elapsed(token->pressed_at, other_pressed_at);
    uint16_t              first_hold_term;
    uint16_t              action;
    uint8_t               repeat_count;
    delayed_action_mods_t mods;
    bool                  tap_pending;

    if (!key_runtime_core_press_token_settles_on_other_press(token, token_key_pos)) {
        return;
    }

    if (!(series && series->active && series->keycode == token->resolved_keycode && !series->resolved)) {
        series = NULL;
    }
    first_hold_term = token->interaction.binding.hold.present ? token->interaction.binding.tap_hold_term : token->interaction.binding.long_hold_term;
    tap_pending     = (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE && series)) && !((token->interaction.binding.hold.present || token->interaction.binding.long_hold.present) && elapsed >= first_hold_term);

    if (tap_pending && series) {
        if (key_runtime_core_tap_series_take_flush(series, &action, &repeat_count, &mods)) {
            key_runtime_core_effect_plan_push_delayed_action(plan, token_key_pos, action, mods, repeat_count);
            if (key_runtime_core_tap_series_has_authored_tap_branch(series)) {
                key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, token_key_pos, action, series->tap_count);
            }
        }
        key_runtime_core_tap_series_clear(state, series);
    } else if (tap_pending) {
        key_runtime_core_effect_plan_push_dispatch_action(plan, token_key_pos, token->interaction.binding.tap_action);
        key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, token_key_pos, token->interaction.binding.tap_action, token->interaction.selection.tap_count);
    } else if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || token->slot_phase == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING) {
        action = key_runtime_core_press_token_release_hold_action(token, elapsed);
        if (action == KC_NO) {
            return;
        }
        key_runtime_core_effect_plan_push_dispatch_action(plan, token_key_pos, action);
        (void)key_runtime_core_reset_pending_multi_tap(token_key_pos);
    } else {
        return;
    }
    token->slot_phase = KEY_RUNTIME_SLOT_PHASE_IDLE;
}

void key_runtime_core_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    const press_token_t      *other;
    uint16_t                  other_pressed_at;

    if (!(state && plan)) {
        return;
    }

    // The press's own time, which a delivery held back behind another key
    // keeps; the runtime has already begun its token.
    other            = key_runtime_core_press_token_state(state, key_pos);
    other_pressed_at = other && other->active ? other->pressed_at : state->current_time;

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token = &state->press_tokens[index];
        keypos_t       token_key_pos;

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        if (!(token->active && !key_runtime_core_keypos_equal(token_key_pos, key_pos))) {
            continue;
        }

        key_runtime_core_plan_missed_scans_for_token(state, token, other_pressed_at, plan);
        key_runtime_core_plan_fallback_hold_activation(state, token, plan);
        key_runtime_core_settle_on_other_press(state, token, token_key_pos, other_pressed_at, plan);
        token->other_press_interrupted = true;
        if (token->interaction.contract.suppress_tap_on_layer_interrupt) {
            token->momentary_layer_tap_interrupted = true;
        }
    }
}

void key_runtime_core_settle_active_keys_on_other_release(keypos_t key_pos, uint16_t released_at, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan)) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token = &state->press_tokens[index];
        keypos_t       token_key_pos;

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        if (token->active && !key_runtime_core_keypos_equal(token_key_pos, key_pos)) {
            key_runtime_core_plan_missed_scans_for_token(state, token, released_at, plan);
            key_runtime_core_settle_on_other_press(state, token, token_key_pos, released_at, plan);
        }
    }
}

void key_runtime_core_plan_missed_scans_before_release(keypos_t key_pos, uint16_t released_at, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;

    if (!(state && plan && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }
    token = key_runtime_core_press_token_state(state, key_pos);
    if (token && token->active) {
        key_runtime_core_plan_missed_scans_for_token(state, token, released_at, plan);
    }
}

void key_runtime_core_settle_before_press(uint16_t keycode, keypos_t key_pos, bool press_is_handled, key_runtime_core_effect_plan_t *plan) {
    if (key_runtime_core_has_other_active_press_token(key_pos)) {
        key_runtime_core_interrupt_active_keys_on_other_press(key_pos, plan);
        key_runtime_core_effect_plan_end_step(plan);
    }
    if (press_is_handled) {
        // Handled keys keep independent series alive, except a tap/hold key's:
        // its pending tap is typed before the key that followed it.
        key_runtime_core_flush_foreign_dual_role_multi_tap(keycode, key_pos, plan);
    } else {
        key_runtime_core_flush_foreign_multi_tap(keycode, key_pos, plan);
    }
}

void key_runtime_core_settle_before_release(uint16_t keycode, const keyrecord_t *record, uint16_t released_at, key_runtime_core_effect_plan_t *plan) {
    if (!record || record->event.pressed) {
        return;
    }
    // A combo output's times count from its dispatch, so it has no missed
    // hold to catch up.
    if (record->event.type == KEY_EVENT) {
        key_runtime_core_plan_missed_scans_before_release(record->event.key, released_at, plan);
        key_runtime_core_effect_plan_end_step(plan);
    }
    // Whether the key holds a layer or modifiers is asked of what that hold
    // projected.
    if (!key_runtime_core_has_other_active_press_token(record->event.key) || !key_runtime_core_release_is_mods_or_layer(keycode, record)) {
        return;
    }
    key_runtime_core_settle_active_keys_on_other_release(record->event.key, released_at, plan);
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

    if (series && series->active && !key_runtime_core_tap_series_can_accept_press(state, series, keycode, token->pressed_at)) {
        if (token->typing) {
            key_runtime_core_tap_series_type_first_tap(series);
        }
        if (key_runtime_core_tap_series_take_flush(series, &action, &repeat_count, &mods)) {
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

    if (key_runtime_core_tap_series_can_accept_press(state, series, keycode, token->pressed_at)) {
        key_runtime_core_tap_series_join(state, series, token);
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

    key_runtime_core_apply_release_settlement(state, key_pos, &release_plan, series_mods);
    key_runtime_core_effect_plan_append_release_plan(plan, &release_plan);
    return true;
}

static __attribute__((noinline)) bool key_runtime_core_try_active_release(key_runtime_core_state_t *state, keypos_t key_pos, keyboard_mod_state_t keyboard_mod_state, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_release_effect_plan_t       release_plan;
    key_runtime_core_active_release_resolution_t active_resolution;

    if (!(key_runtime_core_resolve_active_release(key_pos, &active_resolution) && key_runtime_core_plan_active_release_effects(key_pos, &active_resolution, &release_plan))) {
        return false;
    }

    key_runtime_core_apply_release_settlement(state, key_pos, &release_plan, keyboard_mod_state);
    key_runtime_core_effect_plan_append_release_plan(plan, &release_plan);
    return true;
}

static __attribute__((noinline)) void key_runtime_core_plan_unmatched_release(key_runtime_core_state_t *state, keypos_t key_pos, const handled_key_resolution_t *resolution, key_runtime_core_effect_plan_t *plan) {
    handled_key_resolution_ctx_t ctx = handled_key_resolution_ctx_make(key_pos, key_runtime_core_resolution_layers(state));

    if (handled_key_resolution_materializes_momentary_layer(resolution, &ctx)) {
        key_runtime_core_effect_plan_push_layer_release(plan, key_pos);
    }
    key_runtime_core_effect_plan_push_release_owned_state(plan, key_pos);
}

bool key_runtime_core_handle_handled_key_release(keypos_t key_pos, const handled_key_resolution_t *resolution, keyboard_mod_state_t keyboard_mod_state, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    if (key_runtime_core_try_pending_multi_tap_release(state, key_pos, plan)) {
        return true;
    }

    if (key_runtime_core_try_active_release(state, key_pos, keyboard_mod_state, plan)) {
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
