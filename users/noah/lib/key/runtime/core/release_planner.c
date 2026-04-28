// Key Runtime Core Release Planner Effects

#include "release_internal.h"

#include "../../../pointing/defs/pd_modes.h"

static void key_runtime_core_release_effect_plan_push(key_runtime_core_release_effect_plan_t *plan, key_runtime_effect_t effect) {
    if (!plan) {
        return;
    }

    if (plan->count < ARRAY_SIZE(plan->items)) {
        plan->items[plan->count++] = effect;
        return;
    }

    plan->overflowed = true;
}

static void key_runtime_core_release_effect_plan_push_dispatch_action(key_runtime_core_release_effect_plan_t *plan, keypos_t key_pos, uint16_t action) {
    if (action == KC_NO) {
        return;
    }

    key_runtime_core_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                        .kind                 = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
                                                        .data.dispatch_action = {.action = action, .packed_key_pos = key_runtime_keypos_pack(key_pos)},
                                                    });
}

static void key_runtime_core_release_effect_plan_push_held_action(key_runtime_core_release_effect_plan_t *plan, key_runtime_effect_kind_t kind, keypos_t key_pos, uint16_t action) {
    if (!(plan && action != KC_NO)) {
        return;
    }

    key_runtime_core_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                        .kind = kind,
                                                        .data.held_action =
                                                            {
                                                                .key_pos = key_pos,
                                                                .action  = action,
                                                            },
                                                    });
}

static void key_runtime_core_release_effect_plan_push_release_owned_state(key_runtime_core_release_effect_plan_t *plan, keypos_t key_pos) {
    if (!plan) {
        return;
    }

    key_runtime_core_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                        .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                                        .data.key_pos = key_pos,
                                                    });
}

static void key_runtime_core_release_effect_plan_push_layer_release(key_runtime_core_release_effect_plan_t *plan, keypos_t key_pos) {
    if (!plan) {
        return;
    }

    key_runtime_core_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                        .kind         = KEY_RUNTIME_EFFECT_LAYER_RELEASE,
                                                        .data.key_pos = key_pos,
                                                    });
}

static void key_runtime_core_release_effect_plan_push_feedback_pulse(key_runtime_core_release_effect_plan_t *plan, keypos_t key_pos, key_feedback_pulse_kind_t kind) {
    if (!plan) {
        return;
    }

    key_runtime_core_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                        .kind = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
                                                        .data.feedback_pulse =
                                                            {
                                                                .key_pos = key_pos,
                                                                .kind    = kind,
                                                            },
                                                    });
}

static void key_runtime_core_release_effect_plan_push_tap_commit_feedback_pulse(key_runtime_core_release_effect_plan_t *plan, keypos_t key_pos, uint16_t action, uint8_t tap_count) {
    if (key_runtime_core_tap_commit_feedback_allowed(action, tap_count)) {
        key_runtime_core_release_effect_plan_push_feedback_pulse(plan, key_pos, KEY_FEEDBACK_PULSE_TAP_COMMITTED);
    }
}

static void key_runtime_core_release_effect_plan_push_pd_mode_lock_tap(key_runtime_core_release_effect_plan_t *plan, keypos_t key_pos, pd_mode_mask_t mode) {
    if (!(plan && mode != 0)) {
        return;
    }

    key_runtime_core_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                        .kind                  = KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP,
                                                        .data.pd_mode_lock_tap = {.pd_mode = mode, .key_pos = key_pos},
                                                    });
}

static void key_runtime_core_release_effect_plan_push_delayed_action(key_runtime_core_release_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
    if (!(plan && action != KC_NO && repeat_count != 0u)) {
        return;
    }

    key_runtime_core_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                        .kind = KEY_RUNTIME_EFFECT_DELAYED_ACTION,
                                                        .data.delayed_action =
                                                            {
                                                                .action         = action,
                                                                .packed_key_pos = key_runtime_keypos_pack(key_pos),
                                                                .mods           = mods,
                                                                .repeat_count   = repeat_count,
                                                            },
                                                    });
}

static void key_runtime_core_release_effect_plan_push_action_or_pd_mode_lock_tap(key_runtime_core_release_effect_plan_t *plan, keypos_t key_pos, uint16_t action) {
    const pd_mode_def_t *def = pd_mode_lock_action_lookup(action);

    if (def) {
        key_runtime_core_release_effect_plan_push_pd_mode_lock_tap(plan, key_pos, def->mode_flag);
        return;
    }

    key_runtime_core_release_effect_plan_push_dispatch_action(plan, key_pos, action);
}

bool key_runtime_core_plan_active_release_effects(keypos_t key_pos, uint16_t keycode, const key_runtime_core_active_release_resolution_t *resolution, key_runtime_core_release_effect_plan_t *out) {
    key_runtime_release_contract_t contract;

    if (out) {
        *out = (key_runtime_core_release_effect_plan_t){
            .settlement = KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_RESET,
        };
    }

    if (!(resolution && out && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    contract = key_runtime_release_contract_for_interaction(resolution->interaction);

    if (key_runtime_slot_interaction_is_momentary_layer(resolution->interaction)) {
        key_runtime_core_release_effect_plan_push_layer_release(out, key_pos);
    }
    if (resolution->decision.release_owned_state) {
        key_runtime_core_release_effect_plan_push_release_owned_state(out, key_pos);
    }

    switch (resolution->decision.outcome) {
        case KEY_RUNTIME_RELEASE_DECISION_OUTCOME_TAP:
            switch (contract.tap.outcome) {
                case KEY_RUNTIME_RELEASE_TAP_OUTCOME_BUFFER_MULTI_TAP:
                    out->pending_multi_tap_seed = (key_runtime_core_pending_multi_tap_seed_t){
                        .active              = true,
                        .keycode             = keycode,
                        .key_pos             = key_pos,
                        .tap_count           = resolution->interaction.selection.tap_count,
                        .tap_action          = contract.tap.action,
                        .tap_repeat_count    = contract.tap.repeat_count,
                        .tap_hold_term       = resolution->interaction.binding.tap_hold_term,
                        .multi_tap_term      = resolution->interaction.binding.multi_tap_term,
                        .branch_confirm_term = resolution->interaction.binding.branch_confirm_term,
                        .has_more_taps       = resolution->interaction.binding.has_more_taps,
                    };
                    return true;
                case KEY_RUNTIME_RELEASE_TAP_OUTCOME_DISPATCH_ACTION:
                    key_runtime_core_release_effect_plan_push_action_or_pd_mode_lock_tap(out, key_pos, contract.tap.action);
                    key_runtime_core_release_effect_plan_push_tap_commit_feedback_pulse(out, key_pos, contract.tap.action, resolution->interaction.selection.tap_count);
                    return true;
                case KEY_RUNTIME_RELEASE_TAP_OUTCOME_NONE:
                default:
                    return true;
            }
        case KEY_RUNTIME_RELEASE_DECISION_OUTCOME_ACTION:
            key_runtime_core_release_effect_plan_push_action_or_pd_mode_lock_tap(out, key_pos, resolution->decision.action);
            return true;
        case KEY_RUNTIME_RELEASE_DECISION_OUTCOME_PD_MODE_LOCK_TAP:
            key_runtime_core_release_effect_plan_push_pd_mode_lock_tap(out, key_pos, resolution->decision.pd_mode_lock_tap);
            return true;
        case KEY_RUNTIME_RELEASE_DECISION_OUTCOME_NONE:
        default:
            return true;
    }
}

bool key_runtime_core_plan_pending_multi_tap_release_effects(keypos_t key_pos, bool is_momentary_layer, const key_runtime_core_pending_multi_tap_release_resolution_t *resolution, delayed_action_mods_t mods, key_runtime_core_release_effect_plan_t *out) {
    key_runtime_core_state_t *state;
    tap_series_t             *series;

    if (out) {
        *out = (key_runtime_core_release_effect_plan_t){
            .settlement = KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_RESET,
        };
    }

    if (!(resolution && out && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    if (is_momentary_layer) {
        key_runtime_core_release_effect_plan_push_layer_release(out, key_pos);
    }

    switch (resolution->outcome) {
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE:
            key_runtime_core_release_effect_plan_push_held_action(out, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, key_pos, resolution->action);
            key_runtime_core_release_effect_plan_push_held_action(out, KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER, key_pos, resolution->action);
            (void)key_runtime_core_reset_pending_multi_tap(key_pos);
            return true;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION:
            state  = key_runtime_core_state();
            series = state ? key_runtime_core_tap_series_state(state, key_pos) : NULL;
            if (resolution->tap_branch_feedback && key_runtime_core_tap_series_start_delayed_action_branch_confirm(state, series, resolution->tap_count, state ? state->current_time : timer_read(), series ? series->branch_confirm_term_ms : 0u, resolution->action, resolution->repeat_count, mods, resolution->tap_commit_feedback, resolution->action_feedback, resolution->action_feedback_kind)) {
                out->settlement = KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_CLEAR_ACTIVE_PRESERVE_PENDING_MULTI_TAP;
                return true;
            }
            key_runtime_core_release_effect_plan_push_delayed_action(out, key_pos, resolution->action, mods, resolution->repeat_count);
            if (resolution->tap_commit_feedback) {
                key_runtime_core_release_effect_plan_push_tap_commit_feedback_pulse(out, key_pos, resolution->action, resolution->tap_count);
            }
            if (resolution->action_feedback) {
                key_runtime_core_release_effect_plan_push_feedback_pulse(out, key_pos, resolution->action_feedback_kind);
            }
            (void)key_runtime_core_reset_pending_multi_tap(key_pos);
            return true;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN:
            out->settlement = KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_CLEAR_ACTIVE_PRESERVE_PENDING_MULTI_TAP;
            return true;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE:
        default:
            return true;
    }
}
