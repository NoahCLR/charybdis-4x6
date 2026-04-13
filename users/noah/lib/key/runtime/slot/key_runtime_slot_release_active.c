// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Active Release
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_release_active.h"

#include "key_runtime_slot_pending_multi_tap.h"
#include "key_runtime_slot_policy.h"
#include "key_runtime_slot_result_internal.h"

#include "../key_runtime_trace.h"
#include "../../../action/action_lifecycle.h"

typedef enum {
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP,
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION,
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP,
} key_runtime_slot_release_outcome_t;

typedef struct {
    bool                               release_owned_state;
    key_runtime_slot_release_outcome_t outcome;
    uint16_t                           action;
    pd_mode_mask_t                     pd_mode_lock_tap;
} key_runtime_slot_release_resolution_t;

typedef struct {
    active_key_state_t                released_key;
    key_runtime_slot_interaction_t    interaction;
    key_runtime_slot_release_contract_t contract;
    uint16_t                          elapsed;
    bool                              quick_tap;
    bool                              quick_immediate_hold;
    bool                              buffered_base_tap;
    pd_mode_mask_t                    lock_tap_mode;
} key_runtime_slot_release_context_t;

typedef enum {
    KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_NONE = 0,
    KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_PRIMARY_ONLY,
    KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION,
} key_runtime_slot_release_hold_action_mode_t;

typedef struct {
    bool                                        buffered_base_tap_dispatches_tap;
    bool                                        quick_tap_dispatches_tap;
    bool                                        pd_mode_lock_tap_allowed;
    bool                                        quick_immediate_hold_dispatches_tap;
    bool                                        checks_fallback_hold_suppression;
    key_runtime_slot_release_hold_action_mode_t hold_action_mode;
    bool                                        long_hold_dispatches_action;
    bool                                        nonquick_release_dispatches_tap;
} key_runtime_slot_release_phase_contract_t;

typedef struct {
    key_runtime_slot_release_phase_contract_t tap_window;
    key_runtime_slot_release_phase_contract_t press_held_window;
    key_runtime_slot_release_phase_contract_t release_hold_pending;
    key_runtime_slot_release_phase_contract_t hold_tier_active;
    key_runtime_slot_release_phase_contract_t hold_complete;
} key_runtime_slot_release_phase_contracts_t;

static const key_runtime_slot_release_phase_contracts_t key_runtime_slot_release_phase_contracts = {
    .tap_window =
        {
            .buffered_base_tap_dispatches_tap = true,
            .quick_tap_dispatches_tap         = true,
            .pd_mode_lock_tap_allowed         = true,
            .checks_fallback_hold_suppression = true,
            .hold_action_mode                 = KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_PRIMARY_ONLY,
            .long_hold_dispatches_action      = true,
            .nonquick_release_dispatches_tap  = true,
        },
    .press_held_window =
        {
            .pd_mode_lock_tap_allowed            = true,
            .quick_immediate_hold_dispatches_tap = true,
            .long_hold_dispatches_action         = true,
        },
    .release_hold_pending =
        {
            .hold_action_mode = KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION,
        },
    .hold_tier_active =
        {
            .long_hold_dispatches_action = true,
        },
    .hold_complete =
        {
            .long_hold_dispatches_action = true,
        },
};

static bool key_runtime_slot_release_is_interrupted_layer_tap(const key_runtime_slot_release_context_t *context) {
    return context && context->contract.suppress_tap_on_layer_interrupt && context->released_key.lifecycle.layer_interrupted;
}

static bool key_runtime_slot_release_is_buffered_base_tap(const key_runtime_slot_release_context_t *context) {
    return context && context->contract.buffered_base_tap_dispatches_tap && context->released_key.lifecycle.held_action_keycode == KC_NO;
}

static bool key_runtime_slot_release_is_quick_tap(const key_runtime_slot_release_context_t *context) {
    return context && context->elapsed < context->interaction.binding.tap_hold_term && !key_runtime_slot_release_is_interrupted_layer_tap(context);
}

static pd_mode_mask_t key_runtime_slot_locked_pd_mode_tap_mode(const key_runtime_slot_release_context_t *context) {
    if (!context) {
        return 0;
    }

    pd_mode_mask_t mode = context->contract.quick_tap_pd_mode_lock;
    if (!mode) {
        return 0;
    }

    if (!context->released_key.lifecycle.pd_mode_was_locked_on_press || context->elapsed >= context->interaction.binding.tap_hold_term) {
        return 0;
    }

    if (key_runtime_slot_release_is_interrupted_layer_tap(context)) {
        return 0;
    }

    return mode;
}

static key_runtime_slot_release_resolution_t key_runtime_slot_release_resolution_base(const key_runtime_slot_release_context_t *context) {
    return (key_runtime_slot_release_resolution_t){
        .release_owned_state = context && (context->released_key.lifecycle.held_action_keycode != KC_NO || context->released_key.lifecycle.repeat_binding_active),
    };
}

static key_runtime_slot_release_resolution_t key_runtime_slot_release_resolution_tap(const key_runtime_slot_release_context_t *context) {
    key_runtime_slot_release_resolution_t resolution = key_runtime_slot_release_resolution_base(context);
    resolution.outcome                               = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP;
    return resolution;
}

static key_runtime_slot_release_resolution_t key_runtime_slot_release_resolution_action(const key_runtime_slot_release_context_t *context, uint16_t action) {
    key_runtime_slot_release_resolution_t resolution = key_runtime_slot_release_resolution_base(context);
    resolution.outcome                               = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION;
    resolution.action                                = action;
    return resolution;
}

static key_runtime_slot_release_resolution_t key_runtime_slot_release_resolution_pd_mode_lock_tap(const key_runtime_slot_release_context_t *context) {
    key_runtime_slot_release_resolution_t resolution = key_runtime_slot_release_resolution_base(context);
    resolution.outcome                               = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP;
    resolution.pd_mode_lock_tap                      = context ? context->lock_tap_mode : 0;
    return resolution;
}

static key_runtime_trace_release_outcome_t key_runtime_slot_release_trace_outcome(key_runtime_slot_release_outcome_t outcome) {
    switch (outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP:
            return KEY_RUNTIME_TRACE_RELEASE_OUTCOME_TAP;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION:
            return KEY_RUNTIME_TRACE_RELEASE_OUTCOME_ACTION;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            return KEY_RUNTIME_TRACE_RELEASE_OUTCOME_PD_MODE_LOCK_TAP;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE:
        default:
            return KEY_RUNTIME_TRACE_RELEASE_OUTCOME_NONE;
    }
}

static uint16_t key_runtime_slot_release_trace_flags(const key_runtime_slot_release_context_t *context, key_runtime_slot_release_phase_contract_t phase_contract,
                                                     key_runtime_slot_release_resolution_t resolution) {
    uint16_t flags = 0;

    if (!context) {
        return flags;
    }

    if (key_runtime_slot_release_is_interrupted_layer_tap(context)) {
        flags |= KEY_RUNTIME_TRACE_RELEASE_FLAG_INTERRUPTED_LAYER_TAP;
    }
    if (context->quick_tap) {
        flags |= KEY_RUNTIME_TRACE_RELEASE_FLAG_QUICK_TAP;
    }
    if (context->quick_immediate_hold) {
        flags |= KEY_RUNTIME_TRACE_RELEASE_FLAG_QUICK_IMMEDIATE_HOLD;
    }
    if (context->buffered_base_tap) {
        flags |= KEY_RUNTIME_TRACE_RELEASE_FLAG_BUFFERED_BASE_TAP;
    }
    if (context->lock_tap_mode) {
        flags |= KEY_RUNTIME_TRACE_RELEASE_FLAG_LOCK_TAP_AVAILABLE;
    }
    if (resolution.release_owned_state) {
        flags |= KEY_RUNTIME_TRACE_RELEASE_FLAG_RELEASE_OWNED_STATE;
    }
    if (phase_contract.checks_fallback_hold_suppression && context->contract.fallback_hold_suppresses_nonquick_release && !context->quick_tap && resolution.outcome == KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE) {
        flags |= KEY_RUNTIME_TRACE_RELEASE_FLAG_FALLBACK_SUPPRESSED;
    }

    return flags;
}

static uint16_t key_runtime_slot_release_trace_detail(const key_runtime_slot_release_context_t *context, key_runtime_slot_release_resolution_t resolution) {
    if (!context) {
        return 0;
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP:
            return context->contract.tap.action;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION:
            return resolution.action;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            return resolution.pd_mode_lock_tap;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE:
        default:
            return 0;
    }
}

static bool key_runtime_slot_release_has_primary_hold_action(const key_runtime_slot_release_context_t *context) {
    return context && key_runtime_slot_release_hold_contract_has_primary_action(context->contract.hold);
}

static bool key_runtime_slot_release_can_select_hold_action(const key_runtime_slot_release_context_t *context) {
    return context && key_runtime_slot_release_hold_contract_has_any_action(context->contract.hold);
}

static key_runtime_slot_release_resolution_t key_runtime_slot_release_resolution_selected_hold_action(const key_runtime_slot_release_context_t *context) {
    if (!context) {
        return (key_runtime_slot_release_resolution_t){0};
    }

    return key_runtime_slot_release_resolution_action(context, key_runtime_slot_release_contract_select_hold_action(context->contract, context->elapsed, context->interaction.binding.longer_hold_term));
}

static bool key_runtime_slot_release_long_hold_ready(const key_runtime_slot_release_context_t *context) {
    return context && key_runtime_slot_release_hold_contract_long_ready(context->contract.hold, context->elapsed, context->interaction.binding.longer_hold_term);
}

static key_runtime_slot_release_phase_contract_t key_runtime_slot_release_phase_contract(key_runtime_slot_phase_t phase) {
    switch (phase) {
        case KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW:
            return key_runtime_slot_release_phase_contracts.tap_window;
        case KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW:
            return key_runtime_slot_release_phase_contracts.press_held_window;
        case KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING:
            return key_runtime_slot_release_phase_contracts.release_hold_pending;
        case KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE:
            return key_runtime_slot_release_phase_contracts.hold_tier_active;
        case KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE:
            return key_runtime_slot_release_phase_contracts.hold_complete;
        case KEY_RUNTIME_SLOT_PHASE_IDLE:
        default:
            return (key_runtime_slot_release_phase_contract_t){0};
    }
}

static key_runtime_slot_release_resolution_t key_runtime_slot_release_resolve_phase_contract(const key_runtime_slot_release_context_t *context,
                                                                                              key_runtime_slot_release_phase_contract_t phase_contract) {
    if (!context) {
        return (key_runtime_slot_release_resolution_t){0};
    }

    if (phase_contract.buffered_base_tap_dispatches_tap && context->buffered_base_tap) {
        return key_runtime_slot_release_resolution_tap(context);
    }

    if (phase_contract.quick_tap_dispatches_tap && context->quick_tap) {
        if (phase_contract.pd_mode_lock_tap_allowed && context->lock_tap_mode) {
            return key_runtime_slot_release_resolution_pd_mode_lock_tap(context);
        }

        return key_runtime_slot_release_resolution_tap(context);
    }

    if (phase_contract.pd_mode_lock_tap_allowed && context->lock_tap_mode) {
        return key_runtime_slot_release_resolution_pd_mode_lock_tap(context);
    }

    if (phase_contract.quick_immediate_hold_dispatches_tap && context->quick_immediate_hold) {
        return key_runtime_slot_release_resolution_tap(context);
    }

    if (phase_contract.checks_fallback_hold_suppression && context->contract.fallback_hold_suppresses_nonquick_release) {
        return key_runtime_slot_release_resolution_base(context);
    }

    switch (phase_contract.hold_action_mode) {
        case KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_PRIMARY_ONLY:
            if (key_runtime_slot_release_has_primary_hold_action(context)) {
                return key_runtime_slot_release_resolution_selected_hold_action(context);
            }
            break;
        case KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION:
            if (key_runtime_slot_release_can_select_hold_action(context)) {
                return key_runtime_slot_release_resolution_selected_hold_action(context);
            }
            break;
        case KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_NONE:
        default:
            break;
    }

    if (phase_contract.long_hold_dispatches_action && key_runtime_slot_release_long_hold_ready(context)) {
        return key_runtime_slot_release_resolution_action(context, context->contract.hold.long_action);
    }

    if (phase_contract.nonquick_release_dispatches_tap && context->contract.nonquick_release_dispatches_tap) {
        return key_runtime_slot_release_resolution_tap(context);
    }

    return key_runtime_slot_release_resolution_base(context);
}
static key_runtime_slot_release_resolution_t key_runtime_slot_resolve_release(active_key_state_t released_key, uint16_t elapsed) {
    key_runtime_slot_interaction_t interaction = key_runtime_slot_cached_interaction(&released_key);
    key_runtime_slot_release_context_t context = {
        .released_key = released_key,
        .interaction  = interaction,
        .contract     = key_runtime_slot_release_contract(interaction),
        .elapsed      = elapsed,
    };
    key_runtime_slot_phase_t phase = key_runtime_slot_phase(&released_key);
    key_runtime_slot_release_phase_contract_t phase_contract = key_runtime_slot_release_phase_contract(phase);

    context.quick_tap            = key_runtime_slot_release_is_quick_tap(&context);
    context.quick_immediate_hold = context.contract.quick_release_of_immediate_hold_dispatches_tap && key_runtime_slot_allows_tap_release(&released_key) && context.quick_tap;
    context.buffered_base_tap    = key_runtime_slot_release_is_buffered_base_tap(&context);
    context.lock_tap_mode        = key_runtime_slot_locked_pd_mode_tap_mode(&context);

    key_runtime_slot_release_resolution_t resolution = key_runtime_slot_release_resolve_phase_contract(&context, phase_contract);

    key_runtime_trace_release_resolution(phase, key_runtime_slot_release_trace_outcome(resolution.outcome), key_runtime_slot_release_trace_flags(&context, phase_contract, resolution),
                                         key_runtime_slot_release_trace_detail(&context, resolution));

    return resolution;
}

static void key_runtime_slot_release_apply_tap(key_runtime_slot_result_t *result, active_key_state_t *slot, uint16_t keycode, keypos_t key_pos,
                                               key_runtime_slot_interaction_t interaction, key_runtime_slot_release_contract_t contract) {
    if (!(result && slot)) {
        return;
    }

    switch (contract.tap.outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_BUFFER_MULTI_TAP:
            key_runtime_slot_begin_pending_multi_tap(slot, keycode, key_pos, contract.tap.action, contract.tap.repeat_count, interaction.binding.tap_hold_term,
                                                     interaction.binding.multi_tap_term, interaction.binding.has_more_taps);
            return;
        case KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_DISPATCH_ACTION:
            key_runtime_slot_result_push_dispatch_action(result, key_pos, contract.tap.action);
            return;
        case KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_NONE:
        default:
            return;
    }
}

key_runtime_slot_result_t key_runtime_slot_reduce_active_release(active_key_state_t *slot, uint16_t keycode) {
    key_runtime_slot_result_t result = {0};
    key_runtime_slot_interaction_t interaction;
    key_runtime_slot_release_contract_t contract;

    if (!slot || slot->owner.keycode == KC_NO) {
        return result;
    }

    active_key_state_t released_key = *slot;
    uint16_t           elapsed      = timer_elapsed(released_key.timer);
    interaction                    = key_runtime_slot_cached_interaction(&released_key);
    contract                       = key_runtime_slot_release_contract(interaction);

    result.handled = true;

    if (key_runtime_slot_interaction_is_momentary_layer(interaction)) {
        key_runtime_slot_result_push_layer_release(&result, released_key.owner.key_pos);
    }

    key_runtime_slot_reset(slot);

    key_runtime_slot_release_resolution_t resolution = key_runtime_slot_resolve_release(released_key, elapsed);
    if (resolution.release_owned_state) {
        key_runtime_slot_result_push_builder_if_present(&result, released_key.owner.key_pos,
                                                        (key_runtime_effect_builder_t){
                                                            .release_owned_state = true,
                                                        });
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP:
            key_runtime_slot_release_apply_tap(&result, slot, keycode, released_key.owner.key_pos, interaction, contract);
            return result;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION:
            key_runtime_slot_result_push_dispatch_action(&result, released_key.owner.key_pos, resolution.action);
            return result;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            key_runtime_slot_result_push_pd_mode_lock_tap(&result, resolution.pd_mode_lock_tap);
            return result;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE:
        default:
            return result;
    }
}
