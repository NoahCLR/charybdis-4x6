// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Scan
// ────────────────────────────────────────────────────────────────────────────
//
// Scan-specific slot event helpers for the handled-key runtime.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_scan.h"

#include "key_runtime_slot_effect.h"

#include "../action/action_dispatch.h"
#include "../action/action_lifecycle.h"

typedef enum {
    KEY_RUNTIME_SLOT_SCAN_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD,
    KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD,
} key_runtime_slot_scan_outcome_t;

typedef struct {
    bool                            commit_immediate_hold;
    bool                            immediate_hold_needs_feedback;
    bool                            immediate_hold_completes_hold;
    bool                            set_release_hold_pending;
    bool                            activate_fallback_hold;
    key_runtime_slot_scan_outcome_t outcome;
    hold_behavior_t                 hold;
    hold_behavior_t                 long_hold;
} key_runtime_slot_scan_resolution_t;

typedef enum {
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD,
} key_runtime_slot_pending_multi_tap_scan_outcome_t;

typedef struct {
    key_runtime_slot_pending_multi_tap_scan_outcome_t outcome;
    bool                                              release_layer_before_action;
    hold_behavior_t                                   hold;
    hold_behavior_t                                   long_hold;
} key_runtime_slot_pending_multi_tap_scan_resolution_t;

typedef struct {
    bool                           release_layer_before_action;
    key_runtime_slot_effect_request_t effect_request;
} key_runtime_slot_pending_multi_tap_scan_apply_t;

typedef struct {
    key_runtime_slot_effect_request_t immediate_hold_request;
    key_runtime_slot_effect_request_t effect_request;
} key_runtime_slot_active_scan_apply_t;

static bool key_runtime_slot_scan_request_has_effect(key_runtime_slot_effect_request_t request) {
    return request.kind != KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE || request.release_owned_state || request.feedback_pulse;
}

static bool key_runtime_slot_should_mark_release_hold_pending(active_key_state_t active_key_state, uint16_t elapsed) {
    if (key_runtime_slot_phase(&active_key_state) != KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW) {
        return false;
    }

    if (hold_sends_on_release(active_key_state.hold) && elapsed >= active_key_state.tap_hold_term) {
        return true;
    }

    return !active_key_state.hold.present && hold_sends_on_release(active_key_state.long_hold) && elapsed >= active_key_state.longer_hold_term;
}

static key_runtime_slot_scan_resolution_t key_runtime_slot_resolve_scan(active_key_state_t active_key_state, uint16_t elapsed) {
    key_runtime_slot_scan_resolution_t resolution = {0};
    key_runtime_slot_phase_t           phase      = key_runtime_slot_phase(&active_key_state);

    if (phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW && elapsed >= active_key_state.tap_hold_term) {
        resolution.commit_immediate_hold         = true;
        resolution.immediate_hold_needs_feedback = !key_runtime_slot_uses_implicit_hold(&active_key_state);
        resolution.immediate_hold_completes_hold = !active_key_state.long_hold.present;
    }

    if (key_runtime_slot_uses_fallback_hold(&active_key_state) && phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW && active_key_state.held_action_keycode == KC_NO && elapsed >= active_key_state.tap_hold_term) {
        resolution.activate_fallback_hold = true;
        return resolution;
    }

    if ((phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW || phase == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING || phase == KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE) && hold_fires_at_threshold(active_key_state.long_hold) && elapsed >= active_key_state.longer_hold_term) {
        resolution.outcome   = KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD;
        resolution.long_hold = active_key_state.long_hold;
        return resolution;
    }

    if (phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW && hold_fires_at_threshold(active_key_state.hold) && elapsed >= active_key_state.tap_hold_term) {
        resolution.outcome   = KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD;
        resolution.hold      = active_key_state.hold;
        resolution.long_hold = active_key_state.long_hold;
        return resolution;
    }

    if (key_runtime_slot_should_mark_release_hold_pending(active_key_state, elapsed)) {
        resolution.set_release_hold_pending = true;
    }

    return resolution;
}

static key_runtime_slot_active_scan_apply_t key_runtime_slot_apply_scan_resolution(active_key_state_t *slot, key_runtime_slot_scan_resolution_t resolution) {
    key_runtime_slot_active_scan_apply_t apply = {0};

    if (!slot) {
        return apply;
    }

    if (resolution.set_release_hold_pending) {
        key_runtime_slot_set_release_hold_pending(slot);
    }

    if (resolution.commit_immediate_hold) {
        apply.immediate_hold_request = key_runtime_slot_commit_immediate_hold(slot, resolution.immediate_hold_needs_feedback, resolution.immediate_hold_completes_hold);
    }

    if (resolution.activate_fallback_hold) {
        apply.effect_request = key_runtime_slot_activate_pending_fallback_hold_request(slot);
        return apply;
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD:
            apply.effect_request = key_runtime_slot_fire_hold_at_threshold(slot, resolution.hold, resolution.long_hold, false);
            return apply;
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD:
            apply.effect_request = key_runtime_slot_promote_to_long_hold(slot, resolution.long_hold, false);
            return apply;
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_NONE:
        default:
            return apply;
    }
}

static bool key_runtime_slot_pending_multi_tap_hold_elapsed(const multi_tap_t *multi_tap_state, uint16_t elapsed) {
    return multi_tap_state->pending_hold && hold_fires_at_threshold(multi_tap_state->hold) && elapsed >= multi_tap_state->tap_hold_term;
}

static key_runtime_slot_pending_multi_tap_scan_resolution_t key_runtime_slot_resolve_pending_multi_tap_scan(active_key_state_t active_key_state, multi_tap_t multi_tap_state, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = {0};

    if (!multi_tap_state.pending_hold || active_key_state.keycode == KC_NO) {
        return resolution;
    }

    if (hold_fires_at_threshold(multi_tap_state.long_hold) && elapsed >= active_key_state.longer_hold_term) {
        resolution.outcome                     = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD;
        resolution.long_hold                   = multi_tap_state.long_hold;
        resolution.release_layer_before_action = is_layer_key(active_key_state.keycode) && action_dispatch_is_layer_lock(multi_tap_state.long_hold.action);
        return resolution;
    }

    if (key_runtime_slot_pending_multi_tap_hold_elapsed(&multi_tap_state, elapsed)) {
        resolution.outcome                     = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD;
        resolution.hold                        = multi_tap_state.hold;
        resolution.long_hold                   = multi_tap_state.long_hold;
        resolution.release_layer_before_action = is_layer_key(active_key_state.keycode) && action_dispatch_is_layer_lock(multi_tap_state.hold.action);
    }

    return resolution;
}

static key_runtime_slot_pending_multi_tap_scan_apply_t key_runtime_slot_apply_pending_multi_tap_scan_resolution(active_key_state_t *slot, key_runtime_slot_pending_multi_tap_scan_resolution_t resolution) {
    key_runtime_slot_pending_multi_tap_scan_apply_t apply = {0};

    if (!slot) {
        return apply;
    }

    apply.release_layer_before_action = resolution.release_layer_before_action;

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD:
            slot->long_hold      = resolution.long_hold;
            apply.effect_request = key_runtime_slot_promote_to_long_hold(slot, slot->long_hold, true);
            key_runtime_slot_reset_pending_multi_tap(slot);
            return apply;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD:
            slot->long_hold      = resolution.long_hold;
            apply.effect_request = key_runtime_slot_fire_hold_at_threshold(slot, resolution.hold, slot->long_hold, true);
            key_runtime_slot_reset_pending_multi_tap(slot);
            return apply;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE:
        default:
            return apply;
    }
}

key_runtime_slot_scan_event_t key_runtime_slot_take_active_scan_event(active_key_state_t *slot) {
    key_runtime_slot_scan_event_t event = {0};

    if (!(slot && key_runtime_slot_active(slot) && !key_runtime_slot_hold_is_complete(slot))) {
        return event;
    }

    uint16_t                           elapsed    = timer_elapsed(slot->timer);
    key_runtime_slot_scan_resolution_t resolution = key_runtime_slot_resolve_scan(*slot, elapsed);
    key_runtime_slot_active_scan_apply_t apply    = key_runtime_slot_apply_scan_resolution(slot, resolution);

    if (!(key_runtime_slot_scan_request_has_effect(apply.immediate_hold_request) || key_runtime_slot_scan_request_has_effect(apply.effect_request))) {
        return event;
    }

    event.handled                                 = true;
    event.kind                                    = KEY_RUNTIME_SLOT_SCAN_EVENT_ACTIVE_EFFECTS;
    event.key_pos                                 = slot->key_pos;
    event.data.active_effects.immediate_hold_request = apply.immediate_hold_request;
    event.data.active_effects.effect_request      = apply.effect_request;
    return event;
}

key_runtime_slot_scan_event_t key_runtime_slot_take_pending_multi_tap_scan_event(active_key_state_t *slot) {
    key_runtime_slot_scan_event_t event = {0};

    if (!slot || !key_runtime_slot_has_pending_multi_tap(slot)) {
        return event;
    }

    event.key_pos = key_runtime_slot_active(slot) ? slot->key_pos : slot->pending_multi_tap.key_pos;

    if (key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_active(slot)) {
        multi_tap_t *slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
        if (!slot_multi_tap) {
            return event;
        }

        uint16_t elapsed = timer_elapsed(slot_multi_tap->timer);
        key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = key_runtime_slot_resolve_pending_multi_tap_scan(*slot, *slot_multi_tap, elapsed);
        key_runtime_slot_pending_multi_tap_scan_apply_t      apply      = key_runtime_slot_apply_pending_multi_tap_scan_resolution(slot, resolution);

        if (apply.release_layer_before_action || key_runtime_slot_scan_request_has_effect(apply.effect_request)) {
            event.handled                                                = true;
            event.kind                                                   = KEY_RUNTIME_SLOT_SCAN_EVENT_PENDING_MULTI_TAP_EFFECTS;
            event.data.pending_multi_tap_effects.release_layer_before_action = apply.release_layer_before_action;
            event.data.pending_multi_tap_effects.effect_request          = apply.effect_request;
        }
        return event;
    }

    if (key_runtime_slot_pending_multi_tap_expired(slot)) {
        event.handled                          = true;
        event.kind                             = KEY_RUNTIME_SLOT_SCAN_EVENT_PENDING_MULTI_TAP_FLUSH;
        event.data.pending_multi_tap_flush     = key_runtime_slot_take_pending_multi_tap_flush(slot);
    }

    return event;
}
