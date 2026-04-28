// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Release Planner
// ────────────────────────────────────────────────────────────────────────────
//
// Reducer-owned semantic release-decision helper for active-slot release and
// pending-multi-tap release planning. Callers map returned decisions into
// path-specific effects.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../interaction.h"

typedef enum {
    KEY_RUNTIME_RELEASE_TAP_OUTCOME_NONE = 0,
    KEY_RUNTIME_RELEASE_TAP_OUTCOME_DISPATCH_ACTION,
    KEY_RUNTIME_RELEASE_TAP_OUTCOME_BUFFER_MULTI_TAP,
} key_runtime_release_tap_outcome_t;

typedef struct {
    key_runtime_release_tap_outcome_t outcome;
    uint16_t                          action;
    uint8_t                           repeat_count;
} key_runtime_release_tap_contract_t;

typedef struct {
    uint16_t primary_action;
    uint16_t long_action;
} key_runtime_release_hold_contract_t;

typedef struct {
    key_runtime_release_tap_contract_t  tap;
    key_runtime_release_hold_contract_t hold;
    pd_mode_mask_t                      quick_tap_pd_mode_lock;
    bool                                suppress_tap_on_layer_interrupt;
    bool                                buffered_base_tap_dispatches_tap;
    bool                                quick_release_of_immediate_hold_dispatches_tap;
    bool                                fallback_hold_suppresses_nonquick_release;
    bool                                nonquick_release_dispatches_tap;
} key_runtime_release_contract_t;

typedef enum {
    KEY_RUNTIME_RELEASE_DECISION_OUTCOME_NONE = 0,
    KEY_RUNTIME_RELEASE_DECISION_OUTCOME_TAP,
    KEY_RUNTIME_RELEASE_DECISION_OUTCOME_ACTION,
    KEY_RUNTIME_RELEASE_DECISION_OUTCOME_PD_MODE_LOCK_TAP,
} key_runtime_release_decision_outcome_t;

typedef enum {
    KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_NONE = 0,
    KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_PRIMARY_ONLY,
    KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION,
} key_runtime_release_hold_action_mode_t;

typedef struct {
    bool                                   buffered_base_tap_dispatches_tap;
    bool                                   quick_tap_dispatches_tap;
    bool                                   pd_mode_lock_tap_allowed;
    bool                                   quick_immediate_hold_dispatches_tap;
    bool                                   checks_fallback_hold_suppression;
    key_runtime_release_hold_action_mode_t hold_action_mode;
    bool                                   long_hold_dispatches_action;
    bool                                   nonquick_release_dispatches_tap;
} key_runtime_release_semantics_t;

typedef struct {
    key_runtime_slot_interaction_t interaction;
    key_runtime_release_semantics_t semantics;
    uint16_t                        elapsed;
    bool                            held_action_active;
    bool                            repeat_active;
    bool                            other_press_interrupted;
    bool                            momentary_layer_tap_interrupted;
    bool                            pd_mode_was_locked_on_press;
} key_runtime_release_query_t;

typedef struct {
    bool                                   release_owned_state;
    key_runtime_release_decision_outcome_t outcome;
    uint16_t                               action;
    pd_mode_mask_t                         pd_mode_lock_tap;
} key_runtime_release_decision_t;

static inline key_runtime_release_tap_contract_t key_runtime_release_tap_contract_for_interaction(key_runtime_slot_interaction_t interaction) {
    if ((interaction.flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0) {
        return (key_runtime_release_tap_contract_t){
            .outcome      = KEY_RUNTIME_RELEASE_TAP_OUTCOME_BUFFER_MULTI_TAP,
            .action       = interaction.binding.tap_action,
            .repeat_count = interaction.binding.tap_repeat_count,
        };
    }

    if (interaction.binding.tap_action != KC_NO) {
        return (key_runtime_release_tap_contract_t){
            .outcome      = KEY_RUNTIME_RELEASE_TAP_OUTCOME_DISPATCH_ACTION,
            .action       = interaction.binding.tap_action,
            .repeat_count = interaction.binding.tap_repeat_count,
        };
    }

    return (key_runtime_release_tap_contract_t){0};
}

static inline key_runtime_release_hold_contract_t key_runtime_release_hold_contract_for_interaction(key_runtime_slot_interaction_t interaction) {
    return (key_runtime_release_hold_contract_t){
        .primary_action = interaction.contract.hold.release_action,
        .long_action    = interaction.contract.long_hold.release_action,
    };
}

static inline key_runtime_release_contract_t key_runtime_release_contract_for_interaction(key_runtime_slot_interaction_t interaction) {
    return (key_runtime_release_contract_t){
        .tap                                            = key_runtime_release_tap_contract_for_interaction(interaction),
        .hold                                           = key_runtime_release_hold_contract_for_interaction(interaction),
        .quick_tap_pd_mode_lock                         = interaction.contract.quick_tap_pd_mode_lock,
        .suppress_tap_on_layer_interrupt                = interaction.contract.suppress_tap_on_layer_interrupt,
        .buffered_base_tap_dispatches_tap               = interaction.contract.buffered_base_tap_dispatches_tap,
        .quick_release_of_immediate_hold_dispatches_tap = interaction.contract.quick_release_of_immediate_hold_dispatches_tap,
        .fallback_hold_suppresses_nonquick_release      = interaction.contract.fallback_hold_suppresses_nonquick_release,
        .nonquick_release_dispatches_tap                = interaction.contract.nonquick_release_dispatches_tap,
    };
}

static inline bool key_runtime_release_hold_contract_has_primary_action(key_runtime_release_hold_contract_t contract) {
    return contract.primary_action != KC_NO;
}

static inline bool key_runtime_release_hold_contract_has_any_action(key_runtime_release_hold_contract_t contract) {
    return contract.primary_action != KC_NO || contract.long_action != KC_NO;
}

static inline bool key_runtime_release_hold_contract_long_ready(key_runtime_release_hold_contract_t contract, uint16_t elapsed, uint16_t longer_hold_term) {
    return contract.long_action != KC_NO && elapsed >= longer_hold_term;
}

static inline uint16_t key_runtime_release_hold_contract_select_action(key_runtime_release_hold_contract_t contract, uint16_t elapsed, uint16_t longer_hold_term) {
    if (key_runtime_release_hold_contract_long_ready(contract, elapsed, longer_hold_term)) {
        return contract.long_action;
    }

    return contract.primary_action;
}

static inline uint16_t key_runtime_release_contract_select_hold_action(key_runtime_release_contract_t contract, uint16_t elapsed, uint16_t longer_hold_term) {
    return key_runtime_release_hold_contract_select_action(contract.hold, elapsed, longer_hold_term);
}

static inline key_runtime_release_semantics_t key_runtime_release_semantics_for_phase(key_runtime_slot_phase_t phase) {
    switch (phase) {
        case KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW:
            return (key_runtime_release_semantics_t){
                .buffered_base_tap_dispatches_tap = true,
                .quick_tap_dispatches_tap         = true,
                .pd_mode_lock_tap_allowed         = true,
                .checks_fallback_hold_suppression = true,
                .hold_action_mode                 = KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_PRIMARY_ONLY,
                .long_hold_dispatches_action      = true,
                .nonquick_release_dispatches_tap  = true,
            };
        case KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW:
            return (key_runtime_release_semantics_t){
                .pd_mode_lock_tap_allowed            = true,
                .quick_immediate_hold_dispatches_tap = true,
                .long_hold_dispatches_action         = true,
            };
        case KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING:
            return (key_runtime_release_semantics_t){
                .hold_action_mode = KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION,
            };
        case KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE:
        case KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE:
            return (key_runtime_release_semantics_t){
                .long_hold_dispatches_action = true,
            };
        case KEY_RUNTIME_SLOT_PHASE_IDLE:
        default:
            return (key_runtime_release_semantics_t){0};
    }
}

static inline bool key_runtime_release_query_momentary_layer_tap_suppresses_quick_tap(const key_runtime_release_query_t *query) {
    key_runtime_release_contract_t contract = query ? key_runtime_release_contract_for_interaction(query->interaction) : (key_runtime_release_contract_t){0};
    return query && contract.suppress_tap_on_layer_interrupt && query->momentary_layer_tap_interrupted;
}

static inline bool key_runtime_release_query_quick_tap(const key_runtime_release_query_t *query) {
    return query && query->elapsed < query->interaction.binding.tap_hold_term && !key_runtime_release_query_momentary_layer_tap_suppresses_quick_tap(query);
}

static inline bool key_runtime_release_query_buffered_base_tap(const key_runtime_release_query_t *query) {
    key_runtime_release_contract_t contract = query ? key_runtime_release_contract_for_interaction(query->interaction) : (key_runtime_release_contract_t){0};
    return query && contract.buffered_base_tap_dispatches_tap && !query->held_action_active;
}

static inline bool key_runtime_release_query_quick_immediate_hold(const key_runtime_release_query_t *query) {
    key_runtime_release_contract_t contract = query ? key_runtime_release_contract_for_interaction(query->interaction) : (key_runtime_release_contract_t){0};
    return query && contract.quick_release_of_immediate_hold_dispatches_tap && !query->other_press_interrupted && key_runtime_release_query_quick_tap(query);
}

static inline pd_mode_mask_t key_runtime_release_query_lock_tap_mode(const key_runtime_release_query_t *query) {
    key_runtime_release_contract_t contract = query ? key_runtime_release_contract_for_interaction(query->interaction) : (key_runtime_release_contract_t){0};

    if (!query) {
        return 0;
    }

    if (!contract.quick_tap_pd_mode_lock) {
        return 0;
    }

    if (!query->pd_mode_was_locked_on_press || query->elapsed >= query->interaction.binding.tap_hold_term) {
        return 0;
    }

    if (key_runtime_release_query_momentary_layer_tap_suppresses_quick_tap(query)) {
        return 0;
    }

    return contract.quick_tap_pd_mode_lock;
}

static inline bool key_runtime_release_query_long_hold_ready(const key_runtime_release_query_t *query) {
    key_runtime_release_contract_t contract = query ? key_runtime_release_contract_for_interaction(query->interaction) : (key_runtime_release_contract_t){0};
    return query && key_runtime_release_hold_contract_long_ready(contract.hold, query->elapsed, query->interaction.binding.longer_hold_term);
}

static inline key_runtime_release_decision_t key_runtime_release_decision_base(const key_runtime_release_query_t *query) {
    return (key_runtime_release_decision_t){
        .release_owned_state = query && (query->held_action_active || query->repeat_active),
    };
}

static inline key_runtime_release_decision_t key_runtime_release_decision_tap(const key_runtime_release_query_t *query) {
    key_runtime_release_decision_t decision = key_runtime_release_decision_base(query);
    decision.outcome                       = KEY_RUNTIME_RELEASE_DECISION_OUTCOME_TAP;
    return decision;
}

static inline key_runtime_release_decision_t key_runtime_release_decision_action(const key_runtime_release_query_t *query, uint16_t action) {
    key_runtime_release_decision_t decision = key_runtime_release_decision_base(query);
    decision.outcome                       = KEY_RUNTIME_RELEASE_DECISION_OUTCOME_ACTION;
    decision.action                        = action;
    return decision;
}

static inline key_runtime_release_decision_t key_runtime_release_decision_pd_mode_lock_tap(const key_runtime_release_query_t *query, pd_mode_mask_t mode) {
    key_runtime_release_decision_t decision = key_runtime_release_decision_base(query);
    decision.outcome                       = KEY_RUNTIME_RELEASE_DECISION_OUTCOME_PD_MODE_LOCK_TAP;
    decision.pd_mode_lock_tap              = mode;
    return decision;
}

static inline key_runtime_release_decision_t key_runtime_release_decide(const key_runtime_release_query_t *query) {
    key_runtime_release_contract_t contract;
    bool                           quick_tap;
    bool                           buffered_base_tap;
    bool                           quick_immediate_hold;
    pd_mode_mask_t                 lock_tap_mode;

    if (!query) {
        return (key_runtime_release_decision_t){0};
    }

    contract             = key_runtime_release_contract_for_interaction(query->interaction);
    quick_tap            = key_runtime_release_query_quick_tap(query);
    buffered_base_tap    = key_runtime_release_query_buffered_base_tap(query);
    quick_immediate_hold = key_runtime_release_query_quick_immediate_hold(query);
    lock_tap_mode        = key_runtime_release_query_lock_tap_mode(query);

    if (query->semantics.buffered_base_tap_dispatches_tap && buffered_base_tap) {
        return key_runtime_release_decision_tap(query);
    }

    if (query->semantics.quick_tap_dispatches_tap && quick_tap) {
        if (query->semantics.pd_mode_lock_tap_allowed && lock_tap_mode) {
            return key_runtime_release_decision_pd_mode_lock_tap(query, lock_tap_mode);
        }

        return key_runtime_release_decision_tap(query);
    }

    if (query->semantics.pd_mode_lock_tap_allowed && lock_tap_mode) {
        return key_runtime_release_decision_pd_mode_lock_tap(query, lock_tap_mode);
    }

    if (query->semantics.quick_immediate_hold_dispatches_tap && quick_immediate_hold) {
        return key_runtime_release_decision_tap(query);
    }

    if (query->semantics.checks_fallback_hold_suppression && contract.fallback_hold_suppresses_nonquick_release) {
        return key_runtime_release_decision_base(query);
    }

    switch (query->semantics.hold_action_mode) {
        case KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_PRIMARY_ONLY:
            if (key_runtime_release_hold_contract_has_primary_action(contract.hold)) {
                return key_runtime_release_decision_action(query, key_runtime_release_contract_select_hold_action(contract, query->elapsed, query->interaction.binding.longer_hold_term));
            }
            break;
        case KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION:
            if (key_runtime_release_hold_contract_has_any_action(contract.hold)) {
                return key_runtime_release_decision_action(query, key_runtime_release_contract_select_hold_action(contract, query->elapsed, query->interaction.binding.longer_hold_term));
            }
            break;
        case KEY_RUNTIME_RELEASE_HOLD_ACTION_MODE_NONE:
        default:
            break;
    }

    if (query->semantics.long_hold_dispatches_action && key_runtime_release_query_long_hold_ready(query)) {
        return key_runtime_release_decision_action(query, contract.hold.long_action);
    }

    if (query->semantics.nonquick_release_dispatches_tap && contract.nonquick_release_dispatches_tap) {
        return key_runtime_release_decision_tap(query);
    }

    return key_runtime_release_decision_base(query);
}
