// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Press Reducer
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_press_reduce.h"

#include "key_runtime_slot_effect.h"
#include "key_runtime_slot_pending_multi_tap.h"
#include "key_runtime_slot_policy.h"
#include "key_runtime_slot_result_internal.h"

#include "../key_runtime_index_internal.h"
#include "../key_runtime_trace.h"
#include "../../interaction/handled_key_internal.h"
#include "../../../pointing/defs/pd_modes.h"

static key_runtime_slot_phase_t key_runtime_slot_initial_press_phase(hold_behavior_t hold) {
    return hold_registers_on_press(hold) ? KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW : KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW;
}

static handled_key_materialized_t key_runtime_slot_press_materialized(handled_key_materialized_t materialized, uint16_t tap_action, uint8_t tap_repeat_count, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_hold_strategy_t hold_strategy) {
    materialized.tap_action                = tap_action;
    materialized.tap_repeat_count          = tap_repeat_count;
    materialized.hold                      = hold;
    materialized.long_hold                 = long_hold;
    materialized.authored.tap_hold_term    = tap_hold_term;
    materialized.authored.longer_hold_term = longer_hold_term;
    materialized.authored.multi_tap_term   = multi_tap_term;
    materialized.hold_strategy             = hold_strategy;
    return handled_key_materialized_refresh_contract(materialized);
}

static key_runtime_effect_builder_t key_runtime_slot_begin_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_materialized_t materialized, uint16_t tap_action, uint8_t tap_repeat_count, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_phase_t phase, key_runtime_slot_hold_strategy_t hold_strategy, bool pd_mode_was_locked_on_press) {
    key_runtime_effect_builder_t   builder = {0};
    key_runtime_slot_interaction_t interaction;

    if (!slot) {
        return builder;
    }

    interaction = key_runtime_slot_interaction_from_materialized(key_runtime_slot_press_materialized(materialized, tap_action, tap_repeat_count, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, hold_strategy));
    key_runtime_slot_track(slot, keycode, key_pos, interaction, phase);
    slot->lifecycle.pd_mode_was_locked_on_press = pd_mode_was_locked_on_press;

    if (hold_registers_on_press(hold)) {
        slot->lifecycle.held_action_keycode = hold.action;
        builder.kind                        = KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER;
        builder.action                      = hold.action;
        key_runtime_index_sync_slot(slot);
    }

    return builder;
}

typedef enum {
    KEY_RUNTIME_SLOT_PRESS_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_PRESS_OUTCOME_REUSE_PENDING_MULTI_TAP,
    KEY_RUNTIME_SLOT_PRESS_OUTCOME_BEGIN_FRESH,
} key_runtime_slot_press_outcome_t;

typedef struct {
    active_key_state_t        *slot;
    uint16_t                   keycode;
    keypos_t                   key_pos;
    handled_key_resolution_t   resolution;
    handled_key_materialized_t materialized;
    key_runtime_slot_binding_t binding;
    uint16_t                   tap_hold_term;
    uint16_t                   longer_hold_term;
    uint16_t                   multi_tap_term;
    bool                       matching_pending_multi_tap;
    bool                       flush_pending_multi_tap;
    bool                       needs_layer_press;
    bool                       reclaim_active_slot;
    bool                       active_held_action_survives_flush;
} key_runtime_slot_press_context_t;

static key_runtime_slot_press_context_t key_runtime_slot_press_context(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, bool active_held_action_survives_flush) {
    handled_key_materialized_t materialized = handled_key_materialize(resolution, handled_key_resolution_ctx_live(key_pos));

    return (key_runtime_slot_press_context_t){
        .slot                              = slot,
        .keycode                           = keycode,
        .key_pos                           = key_pos,
        .resolution                        = resolution,
        .materialized                      = materialized,
        .binding                           = key_runtime_slot_binding_from_materialized(materialized),
        .tap_hold_term                     = handled_key_resolution_tap_hold_term(resolution),
        .longer_hold_term                  = handled_key_resolution_longer_hold_term(resolution),
        .multi_tap_term                    = handled_key_resolution_multi_tap_term(resolution),
        .matching_pending_multi_tap        = slot && handled_key_resolution_has_multi_tap(resolution) && key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos),
        .flush_pending_multi_tap           = slot && key_runtime_slot_has_pending_multi_tap(slot) && !key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos),
        .needs_layer_press                 = (materialized.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0,
        .reclaim_active_slot               = slot && key_runtime_slot_active(slot) && !key_runtime_slot_matches(slot, keycode, key_pos),
        .active_held_action_survives_flush = active_held_action_survives_flush,
    };
}

static key_runtime_slot_press_outcome_t key_runtime_slot_press_outcome_for_context(const key_runtime_slot_press_context_t *context) {
    if (!(context && context->slot)) {
        return KEY_RUNTIME_SLOT_PRESS_OUTCOME_NONE;
    }

    if (context->matching_pending_multi_tap) {
        return KEY_RUNTIME_SLOT_PRESS_OUTCOME_REUSE_PENDING_MULTI_TAP;
    }

    return KEY_RUNTIME_SLOT_PRESS_OUTCOME_BEGIN_FRESH;
}

static key_runtime_slot_result_t key_runtime_slot_reduce_press_reuse_pending_multi_tap(const key_runtime_slot_press_context_t *context) {
    key_runtime_slot_result_t result = {0};
    uint16_t                  action;

    if (!(context && context->slot)) {
        return result;
    }

    result.handled = true;
    action         = key_runtime_slot_advance_pending_multi_tap(context->slot, context->keycode);
    key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_PRESS_REUSE_CHAIN, context->slot->pending_multi_tap.count, action);
    key_runtime_slot_result_push_dispatch_action(&result, context->key_pos, action);

    if (context->needs_layer_press) {
        key_runtime_slot_result_push_layer_press(&result, context->key_pos, context->materialized.layer);
    }

    if (key_runtime_slot_pending_multi_tap_pending_hold(context->slot) || context->needs_layer_press) {
        handled_key_resolution_t     current_tap          = key_runtime_slot_pending_multi_tap_pending_hold(context->slot) ? handled_key_lookup_tap_count(context->keycode, context->slot->pending_multi_tap.count) : context->resolution;
        handled_key_materialized_t   current_materialized = key_runtime_slot_pending_multi_tap_pending_hold(context->slot) ? handled_key_materialize(current_tap, handled_key_resolution_ctx_live(context->key_pos)) : context->materialized;
        bool                         pending_hold         = key_runtime_slot_pending_multi_tap_pending_hold(context->slot);
        bool                         pending_layer_press  = (current_materialized.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0;
        key_runtime_effect_builder_t begin_builder;

        if (pending_layer_press && !context->needs_layer_press) {
            key_runtime_slot_result_push_layer_press(&result, context->key_pos, current_materialized.layer);
        }

        begin_builder = key_runtime_slot_begin_press(context->slot, context->keycode, context->key_pos, current_materialized, KC_NO, 0, pending_hold ? current_materialized.hold : hold_behavior_none(), pending_hold ? current_materialized.long_hold : hold_behavior_none(), pending_hold ? current_tap.tap_hold_term : context->tap_hold_term, pending_hold ? current_tap.longer_hold_term : context->longer_hold_term, pending_hold ? current_tap.multi_tap_term : context->multi_tap_term, pending_hold ? KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW : KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE, pending_hold ? current_materialized.hold_strategy : KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT, false);
        key_runtime_slot_result_push_builder_if_present(&result, context->key_pos, begin_builder);
    }

    return result;
}

static key_runtime_slot_result_t key_runtime_slot_reduce_press_begin_fresh(const key_runtime_slot_press_context_t *context) {
    key_runtime_slot_result_t result = {0};

    if (!(context && context->slot)) {
        return result;
    }

    result.handled = true;

    if (context->flush_pending_multi_tap) {
        key_runtime_slot_pending_multi_tap_flush_t flush = key_runtime_slot_take_pending_multi_tap_flush(context->slot);
        key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_PRESS_FLUSH_CHAIN, flush.repeat_count, flush.action);
        key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    }

    if (context->needs_layer_press) {
        key_runtime_slot_result_push_layer_press(&result, context->key_pos, context->materialized.layer);
    }

    if (context->reclaim_active_slot) {
        keypos_t                     reclaim_key_pos = context->slot->owner.key_pos;
        key_runtime_effect_builder_t reclaim_builder = key_runtime_slot_policy_take_flush(context->slot, context->active_held_action_survives_flush);
        key_runtime_slot_result_push_builder_if_present(&result, reclaim_key_pos, reclaim_builder);
    }

    key_runtime_slot_result_push_builder_if_present(&result, context->key_pos, key_runtime_slot_begin_press(context->slot, context->keycode, context->key_pos, context->materialized, context->binding.tap_action, context->binding.tap_repeat_count, context->binding.hold, context->binding.long_hold, context->tap_hold_term, context->longer_hold_term, context->multi_tap_term, key_runtime_slot_initial_press_phase(context->binding.hold), context->materialized.hold_strategy, context->materialized.pd_mode && pd_mode_local_locked(context->materialized.pd_mode)));
    return result;
}

key_runtime_slot_result_t key_runtime_slot_reduce_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, bool active_held_action_survives_flush) {
    key_runtime_slot_press_context_t context = key_runtime_slot_press_context(slot, keycode, key_pos, resolution, active_held_action_survives_flush);

    switch (key_runtime_slot_press_outcome_for_context(&context)) {
        case KEY_RUNTIME_SLOT_PRESS_OUTCOME_REUSE_PENDING_MULTI_TAP:
            return key_runtime_slot_reduce_press_reuse_pending_multi_tap(&context);
        case KEY_RUNTIME_SLOT_PRESS_OUTCOME_BEGIN_FRESH:
            return key_runtime_slot_reduce_press_begin_fresh(&context);
        case KEY_RUNTIME_SLOT_PRESS_OUTCOME_NONE:
        default:
            return (key_runtime_slot_result_t){0};
    }
}
