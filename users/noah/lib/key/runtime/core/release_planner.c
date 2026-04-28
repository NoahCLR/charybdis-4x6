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

static uint16_t key_runtime_core_release_elapsed(uint16_t start, uint16_t end) {
    return (uint16_t)(end - start);
}

static key_feedback_pulse_kind_t key_runtime_core_release_hold_feedback_pulse_kind(bool long_hold_level) {
    return long_hold_level ? KEY_FEEDBACK_PULSE_LONG_HOLD : KEY_FEEDBACK_PULSE_HOLD;
}

static bool key_runtime_core_release_hold_action_feedback_kind(const press_token_t *token, uint16_t action, uint16_t elapsed, key_feedback_pulse_kind_t *out_kind) {
    key_runtime_release_contract_t contract;
    bool                           long_hold_level;
    uint16_t                       release_action;

    if (out_kind) {
        *out_kind = KEY_FEEDBACK_PULSE_HOLD;
    }

    if (!(token && token->handled_key && action != KC_NO)) {
        return false;
    }

    contract = key_runtime_release_contract_for_interaction(token->interaction);
    if (!key_runtime_release_hold_contract_has_any_action(contract.hold)) {
        return false;
    }

    release_action = key_runtime_release_contract_select_hold_action(contract, elapsed, token->interaction.binding.longer_hold_term);
    if (release_action != action) {
        return false;
    }

    long_hold_level = key_runtime_release_hold_contract_long_ready(contract.hold, elapsed, token->interaction.binding.longer_hold_term);
    if (out_kind) {
        *out_kind = key_runtime_core_release_hold_feedback_pulse_kind(long_hold_level);
    }
    return true;
}

static uint16_t key_runtime_core_pending_multi_tap_release_held_lifecycle_action(const press_token_t *token, uint16_t candidate_action, uint16_t elapsed) {
    handled_key_hold_semantics_t semantics;

    if (!(token && token->handled_key)) {
        return KC_NO;
    }

    semantics = token->interaction.contract.hold;
    if (semantics.threshold_action == KC_NO || elapsed < token->interaction.binding.tap_hold_term) {
        return KC_NO;
    }

    if (semantics.threshold != HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD || !semantics.uses_held_lifecycle) {
        return KC_NO;
    }

    if (candidate_action != KC_NO && candidate_action != semantics.threshold_action) {
        return KC_NO;
    }

    return semantics.threshold_action;
}

bool key_runtime_core_resolve_active_release(keypos_t key_pos, key_runtime_core_active_release_resolution_t *out) {
    key_runtime_core_state_t        *state = key_runtime_core_state();
    const press_token_t             *token;
    key_runtime_release_semantics_t semantics;
    key_runtime_release_query_t     query;
    uint16_t                        elapsed;
    bool                            held_action_active;
    bool                            repeat_active;

    if (out) {
        *out = (key_runtime_core_active_release_resolution_t){0};
    }

    if (!(state && out && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    token = key_runtime_core_press_token_at(key_pos);
    if (!(token && token->handled_key && !token->active && token->observed_release_keycode != KC_NO)) {
        return false;
    }

    elapsed            = key_runtime_core_release_elapsed(token->pressed_at, token->released_at);
    semantics          = key_runtime_release_semantics_for_phase(token->slot_phase);
    held_action_active = key_runtime_core_owner_has_lease_kind(state, token->token_id, LEASE_KIND_HELD_ACTION);
    repeat_active      = key_runtime_core_owner_has_lease_kind(state, token->token_id, LEASE_KIND_REPEAT);
    query              = (key_runtime_release_query_t){
        .interaction                     = token->interaction,
        .semantics                       = semantics,
        .elapsed                         = elapsed,
        .held_action_active              = held_action_active,
        .repeat_active                   = repeat_active,
        .other_press_interrupted         = token->other_press_interrupted,
        .momentary_layer_tap_interrupted = token->momentary_layer_tap_interrupted,
        .pd_mode_was_locked_on_press     = token->pd_mode_was_locked_on_press,
    };

    *out = (key_runtime_core_active_release_resolution_t){
        .interaction                     = token->interaction,
        .phase                           = token->slot_phase,
        .elapsed                         = elapsed,
        .held_action_active              = query.held_action_active,
        .repeat_active                   = query.repeat_active,
        .momentary_layer_tap_interrupted = token->momentary_layer_tap_interrupted,
        .quick_tap                       = key_runtime_release_query_quick_tap(&query),
        .quick_immediate_hold            = key_runtime_release_query_quick_immediate_hold(&query),
        .buffered_base_tap               = key_runtime_release_query_buffered_base_tap(&query),
        .lock_tap_mode                   = key_runtime_release_query_lock_tap_mode(&query),
        .decision                        = key_runtime_release_decide(&query),
    };
    return true;
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

bool key_runtime_core_resolve_pending_multi_tap_release(keypos_t key_pos, uint16_t tap_action, uint8_t tap_repeat_count, bool preserve_chain_available, key_runtime_core_pending_multi_tap_release_resolution_t *out) {
    key_runtime_core_state_t        *state = key_runtime_core_state();
    const press_token_t             *token;
    tap_series_t                    *series;
    key_runtime_release_semantics_t semantics = {
        .quick_tap_dispatches_tap        = true,
        .nonquick_release_dispatches_tap = true,
    };
    key_runtime_release_decision_t decision;
    uint16_t                       elapsed;
    uint8_t                        series_tap_count;
    bool                           preserve_chain;
    bool                           terminal_tap_only_feedback_window;
    bool                           tap_branch_feedback_on_release;

    if (out) {
        *out = (key_runtime_core_pending_multi_tap_release_resolution_t){0};
    }

    if (!(state && out && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    token  = key_runtime_core_press_token_at(key_pos);
    series = key_runtime_core_tap_series_state(state, key_pos);
    if (!(token && token->handled_key && !token->active && token->observed_release_keycode != KC_NO)) {
        return false;
    }

    elapsed          = key_runtime_core_release_elapsed(token->pressed_at, token->released_at);
    series_tap_count = series ? series->tap_count : 0u;

    if (series && series->active && series->branch_confirming && series->branch_confirm_kind != KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION) {
        key_feedback_pulse_kind_t action_feedback_kind = KEY_FEEDBACK_PULSE_HOLD;

        semantics.hold_action_mode = KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION;
        decision                   = key_runtime_release_decide(&(key_runtime_release_query_t){
            .interaction = token->interaction,
            .semantics   = semantics,
            .elapsed     = elapsed,
        });

        if (decision.outcome == KEY_RUNTIME_RELEASE_DECISION_OUTCOME_ACTION) {
            bool action_feedback = series_tap_count > 1u && key_runtime_core_release_hold_action_feedback_kind(token, decision.action, elapsed, &action_feedback_kind);

            series->branch_confirm_kind                 = KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION;
            series->branch_confirm_action               = decision.action;
            series->branch_confirm_repeat_count         = decision.action == KC_NO ? 0u : 1u;
            series->branch_confirm_mods                 = series->saved_mod_state;
            series->branch_confirm_tap_commit_feedback  = false;
            series->branch_confirm_action_feedback      = action_feedback;
            series->branch_confirm_action_feedback_kind = (uint8_t)action_feedback_kind;
        }

        *out = (key_runtime_core_pending_multi_tap_release_resolution_t){
            .outcome = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
        };
        return true;
    }

    terminal_tap_only_feedback_window = !token->interaction.binding.has_more_taps && !token->interaction.binding.hold.present && !token->interaction.binding.long_hold.present && tap_action != KC_NO;
    preserve_chain                    = preserve_chain_available && elapsed < token->interaction.binding.tap_hold_term && (token->interaction.binding.has_more_taps || terminal_tap_only_feedback_window);
    tap_branch_feedback_on_release    = series_tap_count > 1u && token->slot_phase != KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING;

    if (token->interaction.contract.hold.release_action != KC_NO ? elapsed >= token->interaction.binding.tap_hold_term : !token->interaction.binding.hold.present && token->interaction.contract.long_hold.release_action != KC_NO && elapsed >= token->interaction.binding.longer_hold_term) {
        semantics.hold_action_mode = KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION;
    }

    decision = key_runtime_release_decide(&(key_runtime_release_query_t){
        .interaction = token->interaction,
        .semantics   = semantics,
        .elapsed     = elapsed,
    });

    switch (decision.outcome) {
        case KEY_RUNTIME_RELEASE_DECISION_OUTCOME_ACTION: {
            uint16_t                  held_lifecycle_action = key_runtime_core_pending_multi_tap_release_held_lifecycle_action(token, decision.action, elapsed);
            key_feedback_pulse_kind_t action_feedback_kind;
            bool                      action_feedback = series_tap_count > 1u && key_runtime_core_release_hold_action_feedback_kind(token, decision.action, elapsed, &action_feedback_kind);

            if (held_lifecycle_action != KC_NO) {
                key_runtime_core_tap_series_clear(state, series);
                *out = (key_runtime_core_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
                    .action  = held_lifecycle_action,
                };
                return true;
            }

            *out = (key_runtime_core_pending_multi_tap_release_resolution_t){
                .outcome             = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
                .action              = decision.action,
                .repeat_count        = decision.action == KC_NO ? 0u : 1u,
                .tap_branch_feedback = tap_branch_feedback_on_release,
                .tap_commit_feedback = false,
                .action_feedback      = action_feedback,
                .action_feedback_kind = action_feedback_kind,
                .tap_count           = series_tap_count,
            };
            return true;
        }
        case KEY_RUNTIME_RELEASE_DECISION_OUTCOME_TAP: {
            uint16_t held_lifecycle_action = key_runtime_core_pending_multi_tap_release_held_lifecycle_action(token, tap_action, elapsed);

            if (held_lifecycle_action != KC_NO) {
                key_runtime_core_tap_series_clear(state, series);
                *out = (key_runtime_core_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
                    .action  = held_lifecycle_action,
                };
                return true;
            }

            if (preserve_chain) {
                *out = (key_runtime_core_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
                };
                return true;
            }

            *out = (key_runtime_core_pending_multi_tap_release_resolution_t){
                .outcome             = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
                .action              = tap_action,
                .repeat_count        = tap_repeat_count,
                .tap_branch_feedback = key_runtime_core_tap_count_uses_branch_confirm(series_tap_count),
                .tap_commit_feedback = true,
                .tap_count           = series_tap_count,
            };
            return true;
        }
        case KEY_RUNTIME_RELEASE_DECISION_OUTCOME_PD_MODE_LOCK_TAP:
        case KEY_RUNTIME_RELEASE_DECISION_OUTCOME_NONE:
        default: {
            uint16_t held_lifecycle_action = key_runtime_core_pending_multi_tap_release_held_lifecycle_action(token, tap_action, elapsed);

            if (held_lifecycle_action != KC_NO) {
                key_runtime_core_tap_series_clear(state, series);
                *out = (key_runtime_core_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
                    .action  = held_lifecycle_action,
                };
                return true;
            }

            if (preserve_chain) {
                *out = (key_runtime_core_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
                };
                return true;
            }

            *out = (key_runtime_core_pending_multi_tap_release_resolution_t){
                .outcome             = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
                .action              = tap_action,
                .repeat_count        = tap_repeat_count,
                .tap_branch_feedback = key_runtime_core_tap_count_uses_branch_confirm(series_tap_count),
                .tap_commit_feedback = true,
                .tap_count           = series_tap_count,
            };
            return true;
        }
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
