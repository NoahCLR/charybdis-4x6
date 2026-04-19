// ────────────────────────────────────────────────────────────────────────────
// Runtime V2 Foundation
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_v2.h"

#include <string.h>

#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/keyboard_mod_ownership.h"
#include "../state/ownership/layer_ownership.h"
#include "../state/runtime/runtime_debug.h"
#include "runtime_v2_trace.h"

static uint8_t runtime_v2_refcount_mask(const uint8_t *refcounts, uint8_t count) {
    uint8_t mask = 0;

    if (!refcounts) {
        return 0;
    }

    for (uint8_t index = 0; index < count && index < 8u; index++) {
        if (refcounts[index] != 0u) {
            mask |= (uint8_t)(1u << index);
        }
    }

    return mask;
}

static bool runtime_v2_keypos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static uint16_t runtime_v2_keypos_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * MATRIX_COLS + key_pos.col);
}

static uint16_t runtime_v2_default_hold_term(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode) ? TAPPING_TERM : CUSTOM_TAP_HOLD_TERM;
}

static uint16_t runtime_v2_default_multi_tap_term(void) {
    return CUSTOM_MULTI_TAP_TERM;
}

static press_token_t *runtime_v2_press_token_state(runtime_v2_state_t *state, keypos_t key_pos) {
    if (!(state && runtime_v2_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->press_tokens[runtime_v2_keypos_index(key_pos)];
}

static tap_series_t *runtime_v2_tap_series_state(runtime_v2_state_t *state, keypos_t key_pos) {
    if (!(state && runtime_v2_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->tap_series[runtime_v2_keypos_index(key_pos)];
}

static uint16_t runtime_v2_elapsed(uint16_t start, uint16_t end) {
    return (uint16_t)(end - start);
}

static void runtime_v2_press_token_refresh_phase(press_token_t *token, uint16_t now) {
    if (!(token && token->active)) {
        return;
    }

    if (token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING || token->phase == PRESS_TOKEN_PHASE_RELEASED || token->phase == PRESS_TOKEN_PHASE_CANCELLED) {
        return;
    }

    if (runtime_v2_elapsed(token->pressed_at, now) >= token->hold_term_ms) {
        token->phase = PRESS_TOKEN_PHASE_HELD;
        return;
    }

    if (token->phase == PRESS_TOKEN_PHASE_PRESSED) {
        token->phase = PRESS_TOKEN_PHASE_HOLD_PENDING;
    }
}

static void runtime_v2_tap_series_release_if_expired(tap_series_t *series, uint16_t now, runtime_v2_state_t *state) {
    if (!(series && state && series->active)) {
        return;
    }

    if (series->pending_hold) {
        return;
    }

    if (runtime_v2_elapsed(series->last_tap_at, now) <= series->tap_term_ms) {
        return;
    }

    *series = (tap_series_t){0};
    if (state->tap_series_count != 0u) {
        state->tap_series_count--;
    }
}

static void runtime_v2_refresh_for_time(runtime_v2_state_t *state, uint16_t now) {
    if (!state) {
        return;
    }

    state->current_time = now;

    for (uint16_t index = 0; index < RUNTIME_V2_PRESS_TOKEN_CAPACITY; index++) {
        runtime_v2_press_token_refresh_phase(&state->press_tokens[index], now);
        runtime_v2_tap_series_release_if_expired(&state->tap_series[index], now, state);
    }
}

static void runtime_v2_press_token_begin(runtime_v2_state_t *state, const runtime_key_event_t *event, uint16_t now) {
    press_token_t *token;
    tap_series_t  *series;
    uint16_t       hold_term_ms;

    if (!(state && event)) {
        return;
    }

    token = runtime_v2_press_token_state(state, event->key_pos);
    series = runtime_v2_tap_series_state(state, event->key_pos);
    if (!token) {
        return;
    }

    if (token->active) {
        state->cancelled_press_count++;
    } else {
        state->press_token_count++;
    }

    hold_term_ms = runtime_v2_default_hold_term(event->keycode);
    *token = (press_token_t){
        .active             = true,
        .token_id           = state->next_token_id++,
        .key_pos            = event->key_pos,
        .physical_keycode   = event->keycode,
        .resolved_keycode   = event->keycode,
        .observed_release_keycode = KC_NO,
        .pressed_at         = now,
        .hold_term_ms       = hold_term_ms,
        .phase              = PRESS_TOKEN_PHASE_PRESSED,
    };

    if (series && series->active && series->keycode == event->keycode && runtime_v2_elapsed(series->last_tap_at, now) <= series->tap_term_ms) {
        series->pending_hold = true;
    }
}

static void runtime_v2_tap_series_note_tap(runtime_v2_state_t *state, const press_token_t *token, uint16_t now) {
    tap_series_t *series;
    bool          reuse_existing;

    if (!(state && token && token->active)) {
        return;
    }

    series = runtime_v2_tap_series_state(state, token->key_pos);
    if (!series) {
        return;
    }

    reuse_existing = series->active && series->keycode == token->resolved_keycode && runtime_v2_elapsed(series->last_tap_at, now) <= series->tap_term_ms;

    if (!series->active) {
        state->tap_series_count++;
    }

    *series = (tap_series_t){
        .active       = true,
        .key_pos      = token->key_pos,
        .keycode      = token->resolved_keycode,
        .tap_count    = (uint8_t)(reuse_existing ? (uint8_t)(series->tap_count + 1u) : 1u),
        .pending_hold = false,
        .last_action  = token->resolved_keycode,
        .last_tap_at  = now,
        .tap_term_ms  = runtime_v2_default_multi_tap_term(),
    };
}

static void runtime_v2_tap_series_note_hold_release(runtime_v2_state_t *state, const press_token_t *token) {
    tap_series_t *series;

    if (!(state && token)) {
        return;
    }

    series = runtime_v2_tap_series_state(state, token->key_pos);
    if (!(series && series->active && series->pending_hold && series->keycode == token->resolved_keycode)) {
        return;
    }

    series->pending_hold = false;
}

static void runtime_v2_press_token_end(runtime_v2_state_t *state, const runtime_key_event_t *event, uint16_t now) {
    press_token_t released_token;
    press_token_t *token;

    if (!(state && event)) {
        return;
    }

    token = runtime_v2_press_token_state(state, event->key_pos);
    if (!(token && token->active)) {
        state->orphan_release_count++;
        return;
    }

    runtime_v2_press_token_refresh_phase(token, now);
    token->observed_release_keycode = event->keycode;
    token->released_at              = now;
    if (event->keycode != token->resolved_keycode) {
        token->release_keycode_mismatched = true;
        state->release_keycode_mismatch_count++;
    }

    released_token = *token;
    if (runtime_v2_elapsed(token->pressed_at, now) < token->hold_term_ms) {
        runtime_v2_tap_series_note_tap(state, token, now);
    } else {
        runtime_v2_tap_series_note_hold_release(state, token);
    }

    *token = released_token;
    token->active = false;
    token->phase  = PRESS_TOKEN_PHASE_RELEASED;
    if (state->press_token_count != 0u) {
        state->press_token_count--;
    }
}

void runtime_v2_apply_event(const runtime_event_t *event, uint16_t event_time) {
    runtime_v2_state_t *state = runtime_v2_state();

    if (!(state && event)) {
        return;
    }

    switch (event->kind) {
        case RUNTIME_EVENT_KIND_KEY_DOWN:
            runtime_v2_refresh_for_time(state, event_time);
            runtime_v2_press_token_begin(state, &event->data.key_event, event_time);
            return;
        case RUNTIME_EVENT_KIND_KEY_UP:
            runtime_v2_refresh_for_time(state, event_time);
            runtime_v2_press_token_end(state, &event->data.key_event, event_time);
            return;
        case RUNTIME_EVENT_KIND_TIMER_ADVANCE:
            runtime_v2_refresh_for_time(state, (uint16_t)(event_time + event->data.timer_advance.advance_ms));
            return;
        case RUNTIME_EVENT_KIND_SCAN:
        case RUNTIME_EVENT_KIND_POINTER_REPORT:
        case RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT:
            runtime_v2_refresh_for_time(state, event_time);
            return;
    }
}

const press_token_t *runtime_v2_press_token_at(keypos_t key_pos) {
    return runtime_v2_press_token_state(runtime_v2_state(), key_pos);
}

const tap_series_t *runtime_v2_tap_series_at(keypos_t key_pos) {
    return runtime_v2_tap_series_state(runtime_v2_state(), key_pos);
}

projection_snapshot_t runtime_v2_projection_snapshot_capture(void) {
    projection_snapshot_t                  snapshot = {0};
    layer_ownership_debug_snapshot_t       layer_snapshot;
    keyboard_mod_ownership_debug_snapshot_t mod_snapshot;
    pointer_layer_policy_debug_snapshot_t  pointer_snapshot;
    runtime_v2_state_t                    *state = runtime_v2_state();

    memset(&layer_snapshot, 0, sizeof(layer_snapshot));
    memset(&mod_snapshot, 0, sizeof(mod_snapshot));
    memset(&pointer_snapshot, 0, sizeof(pointer_snapshot));

    layer_ownership_debug_snapshot(&layer_snapshot);
    keyboard_mod_ownership_debug_snapshot(&mod_snapshot);
    pointer_layer_policy_debug_snapshot(layer_state, &pointer_snapshot);

    snapshot.layer_state                  = layer_state;
    snapshot.locked_layer_mask            = layer_snapshot.locked_mask;
    snapshot.keyboard_mod_state           = mod_snapshot.live_state;
    snapshot.keyboard_managed_mod_mask    = runtime_v2_refcount_mask(mod_snapshot.managed_refcounts, KEYBOARD_MOD_OWNERSHIP_MOD_COUNT);
    snapshot.keyboard_physical_mod_mask   = runtime_v2_refcount_mask(mod_snapshot.physical_refcounts, KEYBOARD_MOD_OWNERSHIP_MOD_COUNT);
    snapshot.pd_mode_local_active         = pd_mode_local_active_snapshot();
    snapshot.pd_mode_local_locked         = pd_mode_local_locked_snapshot();
    snapshot.pd_mode_display_active       = pd_mode_display_active_snapshot();
    snapshot.pd_mode_display_locked       = pd_mode_display_locked_snapshot();
    snapshot.pointer_anchor_active        = pointer_snapshot.auto_mouse_anchored;
    snapshot.pointer_pd_mode_anchor_active = pointer_snapshot.pd_mode_anchor_active;
    snapshot.pointer_prefers_typing_layer = pointer_snapshot.prefers_typing_layer;
    snapshot.pointer_toggle_enabled       = pointer_snapshot.auto_mouse_toggle_enabled;
    snapshot.pointer_sniping_layer_active = pointer_snapshot.sniping_layer_active;
    snapshot.pointer_key_tracker          = pointer_snapshot.auto_mouse_key_tracker;
    snapshot.pointer_layer                = pointer_snapshot.auto_mouse_layer;
    snapshot.active_slot_count            = noah_runtime_debug_active_slot_count();
    snapshot.pending_multi_tap_slot_count = noah_runtime_debug_pending_multi_tap_slot_count();
    snapshot.deferred_release_count       = noah_runtime_debug_deferred_release_count();

    if (state) {
        snapshot.v2_press_token_count     = state->press_token_count;
        snapshot.v2_tap_series_count      = state->tap_series_count;
        snapshot.v2_lease_count           = state->lease_count;
        snapshot.v2_persistent_intent_count = state->persistent_intent_count;
        snapshot.v2_release_keycode_mismatch_count = state->release_keycode_mismatch_count;
        snapshot.v2_orphan_release_count = state->orphan_release_count;
        snapshot.v2_cancelled_press_count = state->cancelled_press_count;
        state->last_projection            = snapshot;
    }

    return snapshot;
}

bool runtime_v2_projection_snapshot_equal(const projection_snapshot_t *lhs, const projection_snapshot_t *rhs) {
    if (!(lhs && rhs)) {
        return lhs == rhs;
    }

    return lhs->layer_state == rhs->layer_state &&
           lhs->locked_layer_mask == rhs->locked_layer_mask &&
           lhs->keyboard_mod_state.real == rhs->keyboard_mod_state.real &&
           lhs->keyboard_mod_state.weak == rhs->keyboard_mod_state.weak &&
           lhs->keyboard_mod_state.oneshot == rhs->keyboard_mod_state.oneshot &&
           lhs->keyboard_mod_state.oneshot_locked == rhs->keyboard_mod_state.oneshot_locked &&
           lhs->keyboard_managed_mod_mask == rhs->keyboard_managed_mod_mask &&
           lhs->keyboard_physical_mod_mask == rhs->keyboard_physical_mod_mask &&
           lhs->pd_mode_local_active == rhs->pd_mode_local_active &&
           lhs->pd_mode_local_locked == rhs->pd_mode_local_locked &&
           lhs->pd_mode_display_active == rhs->pd_mode_display_active &&
           lhs->pd_mode_display_locked == rhs->pd_mode_display_locked &&
           lhs->pointer_anchor_active == rhs->pointer_anchor_active &&
           lhs->pointer_pd_mode_anchor_active == rhs->pointer_pd_mode_anchor_active &&
           lhs->pointer_prefers_typing_layer == rhs->pointer_prefers_typing_layer &&
           lhs->pointer_toggle_enabled == rhs->pointer_toggle_enabled &&
           lhs->pointer_sniping_layer_active == rhs->pointer_sniping_layer_active &&
           lhs->pointer_key_tracker == rhs->pointer_key_tracker &&
           lhs->pointer_layer == rhs->pointer_layer &&
           lhs->active_slot_count == rhs->active_slot_count &&
           lhs->pending_multi_tap_slot_count == rhs->pending_multi_tap_slot_count &&
           lhs->deferred_release_count == rhs->deferred_release_count &&
           lhs->v2_press_token_count == rhs->v2_press_token_count &&
           lhs->v2_tap_series_count == rhs->v2_tap_series_count &&
           lhs->v2_lease_count == rhs->v2_lease_count &&
           lhs->v2_persistent_intent_count == rhs->v2_persistent_intent_count &&
           lhs->v2_release_keycode_mismatch_count == rhs->v2_release_keycode_mismatch_count &&
           lhs->v2_orphan_release_count == rhs->v2_orphan_release_count &&
           lhs->v2_cancelled_press_count == rhs->v2_cancelled_press_count;
}

#if defined(NOAH_RUNTIME_TRACE_ENABLE)

void runtime_v2_trace_capture_projection(void) {
    projection_snapshot_t snapshot = runtime_v2_projection_snapshot_capture();

    runtime_v2_trace_record_projection_snapshot(&snapshot);
}

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
