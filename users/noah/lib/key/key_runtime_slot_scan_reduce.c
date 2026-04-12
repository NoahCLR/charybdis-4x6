// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Scan Reducer
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_scan_reduce.h"

#include "key_runtime_slot_policy.h"
#include "key_runtime_slot_result_internal.h"

#include "../action/action_lifecycle.h"

static key_runtime_slot_result_t key_runtime_slot_result_from_effect_builders(keypos_t key_pos, key_runtime_effect_builder_t first_builder, key_runtime_effect_builder_t second_builder) {
    key_runtime_slot_result_t result = {0};

    if (!(key_runtime_slot_result_builder_has_effect(first_builder) || key_runtime_slot_result_builder_has_effect(second_builder))) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_builder_if_present(&result, key_pos, first_builder);
    key_runtime_slot_result_push_builder_if_present(&result, key_pos, second_builder);
    return result;
}

static bool key_runtime_slot_active_scan_should_mark_release_hold_pending(active_key_state_t active_key_state, uint16_t elapsed) {
    if (key_runtime_slot_phase(&active_key_state) != KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW) {
        return false;
    }

    if (hold_sends_on_release(active_key_state.binding.hold) && elapsed >= active_key_state.timing.tap_hold_term) {
        return true;
    }

    return !active_key_state.binding.hold.present && hold_sends_on_release(active_key_state.binding.long_hold) && elapsed >= active_key_state.timing.longer_hold_term;
}

static key_runtime_slot_result_t key_runtime_slot_step_active_scan_tap_window(active_key_state_t *slot, uint16_t elapsed) {
    if (!slot) {
        return (key_runtime_slot_result_t){0};
    }

    if (key_runtime_slot_uses_fallback_hold(slot) && slot->lifecycle.held_action_keycode == KC_NO && elapsed >= slot->timing.tap_hold_term) {
        return key_runtime_slot_result_from_effect_builders(slot->owner.key_pos, (key_runtime_effect_builder_t){0}, key_runtime_slot_policy_activate_pending_fallback_hold(slot));
    }

    if (hold_fires_at_threshold(slot->binding.long_hold) && elapsed >= slot->timing.longer_hold_term) {
        return key_runtime_slot_result_from_effect_builders(slot->owner.key_pos, (key_runtime_effect_builder_t){0}, key_runtime_slot_policy_promote_to_long_hold(slot, slot->binding.long_hold, false));
    }

    if (hold_fires_at_threshold(slot->binding.hold) && elapsed >= slot->timing.tap_hold_term) {
        return key_runtime_slot_result_from_effect_builders(slot->owner.key_pos, (key_runtime_effect_builder_t){0}, key_runtime_slot_policy_fire_hold_at_threshold(slot, slot->binding.hold, slot->binding.long_hold, false));
    }

    if (key_runtime_slot_active_scan_should_mark_release_hold_pending(*slot, elapsed)) {
        key_runtime_slot_set_release_hold_pending(slot);
    }

    return (key_runtime_slot_result_t){0};
}

static key_runtime_slot_result_t key_runtime_slot_step_active_scan_press_held_window(active_key_state_t *slot, uint16_t elapsed) {
    key_runtime_effect_builder_t immediate_hold_builder = {0};
    key_runtime_effect_builder_t effect_builder         = {0};

    if (!slot) {
        return (key_runtime_slot_result_t){0};
    }

    if (elapsed >= slot->timing.tap_hold_term) {
        immediate_hold_builder = key_runtime_slot_policy_commit_immediate_hold(slot, !key_runtime_slot_uses_implicit_hold(slot), !slot->binding.long_hold.present);
    }

    if (hold_fires_at_threshold(slot->binding.long_hold) && elapsed >= slot->timing.longer_hold_term) {
        effect_builder = key_runtime_slot_policy_promote_to_long_hold(slot, slot->binding.long_hold, false);
    }

    return key_runtime_slot_result_from_effect_builders(slot->owner.key_pos, immediate_hold_builder, effect_builder);
}

static key_runtime_slot_result_t key_runtime_slot_step_active_scan_hold_phase(active_key_state_t *slot, uint16_t elapsed) {
    if (!(slot && hold_fires_at_threshold(slot->binding.long_hold) && elapsed >= slot->timing.longer_hold_term)) {
        return (key_runtime_slot_result_t){0};
    }

    return key_runtime_slot_result_from_effect_builders(slot->owner.key_pos, (key_runtime_effect_builder_t){0}, key_runtime_slot_policy_promote_to_long_hold(slot, slot->binding.long_hold, false));
}

typedef key_runtime_slot_result_t (*key_runtime_slot_step_active_scan_phase_handler_t)(active_key_state_t *slot, uint16_t elapsed);

static const key_runtime_slot_step_active_scan_phase_handler_t key_runtime_slot_step_active_scan_phase_handlers[] = {
    [KEY_RUNTIME_SLOT_PHASE_IDLE]                 = NULL,
    [KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW]           = key_runtime_slot_step_active_scan_tap_window,
    [KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW]    = key_runtime_slot_step_active_scan_press_held_window,
    [KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING] = key_runtime_slot_step_active_scan_hold_phase,
    [KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE]     = key_runtime_slot_step_active_scan_hold_phase,
    [KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE]        = NULL,
};

key_runtime_slot_result_t key_runtime_slot_reduce_active_scan(active_key_state_t *slot) {
    if (!(slot && key_runtime_slot_active(slot) && !key_runtime_slot_hold_is_complete(slot))) {
        return (key_runtime_slot_result_t){0};
    }

    // Pending multi-tap hold ownership lives in the dedicated pending-multi-tap
    // reducer. Once that reducer seeds long-hold metadata into the slot, the
    // generic active-scan reducer must not interpret it as an independent
    // active-slot threshold; otherwise a realistic multi-scan hold can fire the
    // same long-hold action twice.
    if (key_runtime_slot_pending_multi_tap_pending_hold(slot)) {
        return (key_runtime_slot_result_t){0};
    }

    uint16_t                                     elapsed = timer_elapsed(slot->timer);
    key_runtime_slot_phase_t                     phase   = key_runtime_slot_phase(slot);
    key_runtime_slot_step_active_scan_phase_handler_t handler = key_runtime_slot_step_active_scan_phase_handlers[phase];

    return handler ? handler(slot, elapsed) : (key_runtime_slot_result_t){0};
}
