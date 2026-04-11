// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Scan
// ────────────────────────────────────────────────────────────────────────────
//
// Scan-specific slot transition helpers for the handled-key runtime.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_scan.h"

#include "../action/action_dispatch.h"
#include "../action/action_lifecycle.h"

key_runtime_slot_scan_resolution_t key_runtime_slot_resolve_scan(active_key_state_t active_key_state, uint16_t elapsed) {
    key_runtime_slot_scan_resolution_t resolution = {0};

    if (hold_registers_on_press(active_key_state.hold) && !active_key_state.hold_one_shot_fired && elapsed >= active_key_state.tap_hold_term) {
        resolution.commit_immediate_hold         = true;
        resolution.immediate_hold_needs_feedback = !active_key_state.implicit_hold;
        resolution.immediate_hold_completes_hold = !active_key_state.long_hold.present;
    }

    if (active_key_state.fallback_hold_pending && active_key_state.held_action_keycode == KC_NO && elapsed >= active_key_state.tap_hold_term) {
        resolution.activate_fallback_hold = true;
        return resolution;
    }

    if (hold_fires_at_threshold(active_key_state.long_hold) && elapsed >= active_key_state.longer_hold_term) {
        resolution.outcome   = KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD;
        resolution.long_hold = active_key_state.long_hold;
        return resolution;
    }

    if (hold_fires_at_threshold(active_key_state.hold) && elapsed >= active_key_state.tap_hold_term) {
        resolution.outcome   = KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD;
        resolution.hold      = active_key_state.hold;
        resolution.long_hold = active_key_state.long_hold;
    }

    return resolution;
}

key_runtime_slot_scan_apply_t key_runtime_slot_apply_scan_resolution(active_key_state_t *slot, key_runtime_slot_scan_resolution_t resolution) {
    key_runtime_slot_scan_apply_t apply = {0};

    if (!slot) {
        return apply;
    }

    if (resolution.commit_immediate_hold) {
        apply.immediate_hold_request = key_runtime_slot_commit_immediate_hold(slot, resolution.immediate_hold_needs_feedback, resolution.immediate_hold_completes_hold);
    }

    if (resolution.activate_fallback_hold) {
        apply.outcome_request = key_runtime_slot_activate_pending_fallback_hold_request(slot);
        return apply;
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD:
            apply.outcome_request = key_runtime_slot_fire_hold_at_threshold(slot, resolution.hold, resolution.long_hold, false);
            return apply;
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD:
            apply.outcome_request = key_runtime_slot_promote_to_long_hold(slot, resolution.long_hold, false);
            return apply;
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_NONE:
        default:
            return apply;
    }
}

static bool key_runtime_slot_pending_multi_tap_hold_elapsed(const multi_tap_t *multi_tap_state, uint16_t elapsed) {
    return multi_tap_state->pending_hold && hold_fires_at_threshold(multi_tap_state->hold) && elapsed >= multi_tap_state->tap_hold_term;
}

key_runtime_slot_pending_multi_tap_scan_resolution_t key_runtime_slot_resolve_pending_multi_tap_scan(active_key_state_t active_key_state, multi_tap_t multi_tap_state, uint16_t elapsed) {
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

key_runtime_slot_pending_multi_tap_scan_apply_t key_runtime_slot_apply_pending_multi_tap_scan_resolution(active_key_state_t *slot, key_runtime_slot_pending_multi_tap_scan_resolution_t resolution) {
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

key_runtime_slot_pending_multi_tap_plan_t key_runtime_slot_take_pending_multi_tap_plan(active_key_state_t *slot) {
    key_runtime_slot_pending_multi_tap_plan_t plan = {0};

    if (!slot || !key_runtime_slot_has_pending_multi_tap(slot)) {
        return plan;
    }

    plan.key_pos = key_runtime_slot_active(slot) ? slot->key_pos : slot->pending_multi_tap.key_pos;

    if (key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_active(slot)) {
        multi_tap_t *slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
        if (!slot_multi_tap) {
            return plan;
        }

        uint16_t elapsed = timer_elapsed(slot_multi_tap->timer);
        key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = key_runtime_slot_resolve_pending_multi_tap_scan(*slot, *slot_multi_tap, elapsed);
        key_runtime_slot_pending_multi_tap_scan_apply_t      apply      = key_runtime_slot_apply_pending_multi_tap_scan_resolution(slot, resolution);

        if (apply.release_layer_before_action || apply.effect_request.kind != KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE || apply.effect_request.release_owned_state || apply.effect_request.feedback_pulse) {
            plan.handled                     = true;
            plan.kind                        = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_PLAN_EFFECT_REQUEST;
            plan.release_layer_before_action = apply.release_layer_before_action;
            plan.effect_request              = apply.effect_request;
        }
        return plan;
    }

    if (key_runtime_slot_pending_multi_tap_expired(slot)) {
        plan.handled = true;
        plan.kind    = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_PLAN_FLUSH;
        plan.flush   = key_runtime_slot_take_pending_multi_tap_flush(slot);
    }

    return plan;
}
