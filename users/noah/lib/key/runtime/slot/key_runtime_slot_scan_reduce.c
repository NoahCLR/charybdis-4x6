// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Scan Reducer
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_scan_reduce.h"

#include "key_runtime_slot_policy.h"

#include "../../../action/action_lifecycle.h"

static key_runtime_slot_result_t key_runtime_slot_result_from_direct_plan(key_runtime_slot_direct_plan_t plan) {
    key_runtime_slot_result_t result = {
        .handled    = plan.handled,
        .count      = plan.count,
        .overflowed = plan.overflowed,
    };

    for (uint8_t index = 0; index < plan.count; index++) {
        result.items[index] = plan.items[index];
    }

    return result;
}

static bool key_runtime_slot_active_scan_should_mark_release_hold_pending(active_key_state_t active_key_state, uint16_t elapsed) {
    key_runtime_slot_interaction_t interaction = key_runtime_slot_cached_interaction(&active_key_state);

    if (key_runtime_slot_phase(&active_key_state) != KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW) {
        return false;
    }

    if (interaction.contract.hold.release_action != KC_NO && elapsed >= interaction.binding.tap_hold_term) {
        return true;
    }

    return !interaction.binding.hold.present && interaction.contract.long_hold.release_action != KC_NO && elapsed >= interaction.binding.longer_hold_term;
}

static key_runtime_slot_direct_plan_t key_runtime_slot_step_active_scan_tap_window(active_key_state_t *slot, uint16_t elapsed) {
    key_runtime_slot_direct_plan_t plan = {0};
    key_runtime_slot_interaction_t interaction;

    if (!slot) {
        return plan;
    }

    interaction = key_runtime_slot_cached_interaction(slot);

    if (key_runtime_slot_uses_fallback_hold(slot) && slot->lifecycle.held_action_keycode == KC_NO && elapsed >= interaction.binding.tap_hold_term) {
        key_runtime_slot_direct_plan_push_builder_if_present(&plan, slot->owner.key_pos, key_runtime_slot_policy_activate_pending_fallback_hold(slot));
        return plan;
    }

    if (handled_key_hold_contract_fires_at_threshold(interaction.contract.long_hold) && elapsed >= interaction.binding.longer_hold_term) {
        key_runtime_slot_direct_plan_push_builder_if_present(&plan, slot->owner.key_pos, key_runtime_slot_policy_promote_to_long_hold(slot, interaction.binding.long_hold, interaction.contract.long_hold, false));
        return plan;
    }

    if (handled_key_hold_contract_fires_at_threshold(interaction.contract.hold) && elapsed >= interaction.binding.tap_hold_term) {
        key_runtime_slot_direct_plan_push_builder_if_present(&plan, slot->owner.key_pos, key_runtime_slot_policy_fire_hold_at_threshold(slot, interaction.binding.hold, interaction.contract.hold, !interaction.binding.long_hold.present, false));
        return plan;
    }

    if (key_runtime_slot_active_scan_should_mark_release_hold_pending(*slot, elapsed)) {
        key_runtime_slot_set_release_hold_pending(slot);
    }

    return plan;
}

static key_runtime_slot_direct_plan_t key_runtime_slot_step_active_scan_press_held_window(active_key_state_t *slot, uint16_t elapsed) {
    key_runtime_slot_direct_plan_t plan = {0};
    key_runtime_effect_builder_t   immediate_hold_builder = {0};
    key_runtime_effect_builder_t   effect_builder         = {0};
    key_runtime_slot_interaction_t interaction;

    if (!slot) {
        return plan;
    }

    interaction = key_runtime_slot_cached_interaction(slot);

    if (elapsed >= interaction.binding.tap_hold_term) {
        immediate_hold_builder = key_runtime_slot_policy_commit_immediate_hold(slot, !key_runtime_slot_uses_implicit_hold(slot), !interaction.binding.long_hold.present);
    }

    if (handled_key_hold_contract_fires_at_threshold(interaction.contract.long_hold) && elapsed >= interaction.binding.longer_hold_term) {
        effect_builder = key_runtime_slot_policy_promote_to_long_hold(slot, interaction.binding.long_hold, interaction.contract.long_hold, false);
    }

    key_runtime_slot_direct_plan_push_builder_if_present(&plan, slot->owner.key_pos, immediate_hold_builder);
    key_runtime_slot_direct_plan_push_builder_if_present(&plan, slot->owner.key_pos, effect_builder);
    return plan;
}

static key_runtime_slot_direct_plan_t key_runtime_slot_step_active_scan_hold_phase(active_key_state_t *slot, uint16_t elapsed) {
    key_runtime_slot_direct_plan_t plan = {0};
    key_runtime_slot_interaction_t interaction;

    if (!slot) {
        return plan;
    }

    interaction = key_runtime_slot_cached_interaction(slot);
    if (!(handled_key_hold_contract_fires_at_threshold(interaction.contract.long_hold) && elapsed >= interaction.binding.longer_hold_term)) {
        return plan;
    }

    key_runtime_slot_direct_plan_push_builder_if_present(&plan, slot->owner.key_pos, key_runtime_slot_policy_promote_to_long_hold(slot, interaction.binding.long_hold, interaction.contract.long_hold, false));
    return plan;
}

typedef key_runtime_slot_direct_plan_t (*key_runtime_slot_step_active_scan_phase_handler_t)(active_key_state_t *slot, uint16_t elapsed);

static const key_runtime_slot_step_active_scan_phase_handler_t key_runtime_slot_step_active_scan_phase_handlers[] = {
    [KEY_RUNTIME_SLOT_PHASE_IDLE] = NULL, [KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW] = key_runtime_slot_step_active_scan_tap_window, [KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW] = key_runtime_slot_step_active_scan_press_held_window, [KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING] = key_runtime_slot_step_active_scan_hold_phase, [KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE] = key_runtime_slot_step_active_scan_hold_phase, [KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE] = NULL,
};

key_runtime_slot_direct_plan_t key_runtime_slot_take_active_scan_plan(active_key_state_t *slot) {
    if (!(slot && key_runtime_slot_active(slot) && !key_runtime_slot_hold_is_complete(slot))) {
        return (key_runtime_slot_direct_plan_t){0};
    }

    // Pending multi-tap hold ownership lives in the dedicated pending-multi-tap
    // reducer. Once that reducer seeds long-hold metadata into the slot, the
    // generic active-scan reducer must not interpret it as an independent
    // active-slot threshold; otherwise a realistic multi-scan hold can fire the
    // same long-hold action twice.
    if (key_runtime_slot_pending_multi_tap_pending_hold(slot)) {
        return (key_runtime_slot_direct_plan_t){0};
    }

    uint16_t                                          elapsed = timer_elapsed(slot->timer);
    key_runtime_slot_phase_t                          phase   = key_runtime_slot_phase(slot);
    key_runtime_slot_step_active_scan_phase_handler_t handler = key_runtime_slot_step_active_scan_phase_handlers[phase];

    return handler ? handler(slot, elapsed) : (key_runtime_slot_direct_plan_t){0};
}

key_runtime_slot_result_t key_runtime_slot_reduce_active_scan(active_key_state_t *slot) {
    return key_runtime_slot_result_from_direct_plan(key_runtime_slot_take_active_scan_plan(slot));
}
