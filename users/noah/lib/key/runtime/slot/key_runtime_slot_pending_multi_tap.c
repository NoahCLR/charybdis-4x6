// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Pending Multi-Tap
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_pending_multi_tap.h"

#include "key_runtime_slot_policy.h"
#include "key_runtime_slot_release_resolver.h"
#include "key_runtime_slot_result_internal.h"

#include "../../../action/action_dispatch.h"
#include "../../../action/action_lifecycle.h"
#include "../key_runtime_index_internal.h"
#include "../key_runtime_trace.h"

static void key_runtime_slot_pending_multi_tap_clear_active_state(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    multi_tap_t pending_multi_tap = slot->pending_multi_tap;
    *slot                         = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
    slot->pending_multi_tap       = pending_multi_tap;
    key_runtime_index_sync_slot(slot);
}

typedef enum {
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
} key_runtime_slot_pending_multi_tap_release_outcome_t;

typedef struct {
    active_key_state_t            *slot;
    uint16_t                       keycode;
    uint16_t                       elapsed;
    keypos_t                       key_pos;
    bool                           is_momentary_layer;
    delayed_action_mods_t          mods;
    key_runtime_slot_interaction_t interaction;
    uint16_t                       tap_action;
    uint8_t                        tap_repeat_count;
    bool                           matched;
} key_runtime_slot_pending_multi_tap_release_context_t;

typedef struct {
    key_runtime_slot_pending_multi_tap_release_outcome_t outcome;
    uint16_t                                             action;
    uint8_t                                              repeat_count;
    delayed_action_mods_t                                mods;
} key_runtime_slot_pending_multi_tap_release_resolution_t;

static bool key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(const key_runtime_slot_pending_multi_tap_release_context_t *context, uint16_t action) {
    handled_key_hold_semantics_t semantics;

    if (!context) {
        return false;
    }

    semantics = context->interaction.contract.hold;

    if (semantics.threshold_action == KC_NO || context->tap_repeat_count != 1 || context->elapsed < context->interaction.binding.tap_hold_term) {
        return false;
    }

    if (semantics.threshold != HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD || action != semantics.threshold_action) {
        return false;
    }

    return semantics.uses_held_lifecycle;
}

static key_runtime_slot_pending_multi_tap_release_context_t key_runtime_slot_pending_multi_tap_release_context(active_key_state_t *slot, uint16_t keycode, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_release_context_t context = {
        .slot    = slot,
        .keycode = keycode,
        .elapsed = elapsed,
    };
    multi_tap_t *slot_multi_tap;

    if (!(slot && key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_pending_multi_tap_matches(slot, keycode, slot->owner.key_pos))) {
        return context;
    }

    slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
    if (!slot_multi_tap) {
        return context;
    }

    context.interaction        = key_runtime_slot_cached_interaction(slot);
    context.key_pos            = slot->owner.key_pos;
    context.is_momentary_layer = key_runtime_slot_interaction_is_momentary_layer(context.interaction);
    context.mods               = delayed_action_mods_from_multi_tap(slot_multi_tap);
    context.tap_action         = key_runtime_slot_resolve_pending_multi_tap_hold(slot, &context.tap_repeat_count);
    context.matched            = true;

    return context;
}

static bool key_runtime_slot_pending_multi_tap_release_preserves_chain(const key_runtime_slot_pending_multi_tap_release_context_t *context) {
    return context && context->tap_action == KC_NO && context->tap_repeat_count == 0 && key_runtime_slot_has_pending_multi_tap(context->slot);
}

static key_runtime_slot_release_semantics_t key_runtime_slot_pending_multi_tap_release_semantics(const key_runtime_slot_pending_multi_tap_release_context_t *context) {
    key_runtime_slot_release_semantics_t semantics = {
        .quick_tap_dispatches_tap        = true,
        .nonquick_release_dispatches_tap = true,
    };

    if (!context) {
        return semantics;
    }

    if (context->interaction.contract.hold.release_action != KC_NO ? context->elapsed >= context->interaction.binding.tap_hold_term : !context->interaction.binding.hold.present && context->interaction.contract.long_hold.release_action != KC_NO && context->elapsed >= context->interaction.binding.longer_hold_term) {
        semantics.hold_action_mode = KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION;
    }

    return semantics;
}

static key_runtime_slot_pending_multi_tap_release_resolution_t key_runtime_slot_pending_multi_tap_release_resolve(const key_runtime_slot_pending_multi_tap_release_context_t *context) {
    key_runtime_slot_release_decision_t decision;

    if (!(context && context->matched)) {
        return (key_runtime_slot_pending_multi_tap_release_resolution_t){0};
    }

    decision = key_runtime_slot_release_decide(&(key_runtime_slot_release_query_t){
        .interaction = context->interaction,
        .semantics   = key_runtime_slot_pending_multi_tap_release_semantics(context),
        .elapsed     = context->elapsed,
    });

    switch (decision.outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_ACTION:
            if (key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(context, decision.action)) {
                return (key_runtime_slot_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
                    .action  = decision.action,
                };
            }

            return (key_runtime_slot_pending_multi_tap_release_resolution_t){
                .outcome      = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
                .action       = decision.action,
                .repeat_count = decision.action == KC_NO ? 0 : 1,
                .mods         = context->mods,
            };
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_TAP:
            if (key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(context, context->tap_action)) {
                return (key_runtime_slot_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
                    .action  = context->tap_action,
                };
            }

            if (key_runtime_slot_pending_multi_tap_release_preserves_chain(context)) {
                return (key_runtime_slot_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
                };
            }

            return (key_runtime_slot_pending_multi_tap_release_resolution_t){
                .outcome      = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
                .action       = context->tap_action,
                .repeat_count = context->tap_repeat_count,
                .mods         = context->mods,
            };
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_PD_MODE_LOCK_TAP:
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_NONE:
        default:
            if (key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(context, context->tap_action)) {
                return (key_runtime_slot_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
                    .action  = context->tap_action,
                };
            }

            if (key_runtime_slot_pending_multi_tap_release_preserves_chain(context)) {
                return (key_runtime_slot_pending_multi_tap_release_resolution_t){
                    .outcome = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
                };
            }

            return (key_runtime_slot_pending_multi_tap_release_resolution_t){
                .outcome      = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
                .action       = context->tap_action,
                .repeat_count = context->tap_repeat_count,
                .mods         = context->mods,
            };
    }
}

key_runtime_slot_result_t key_runtime_slot_pending_multi_tap_handle_release(active_key_state_t *slot, uint16_t keycode, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_release_context_t    context    = key_runtime_slot_pending_multi_tap_release_context(slot, keycode, elapsed);
    key_runtime_slot_pending_multi_tap_release_resolution_t resolution = key_runtime_slot_pending_multi_tap_release_resolve(&context);
    key_runtime_slot_result_t                               result     = {0};

    if (!context.matched) {
        return result;
    }

    result.handled = true;

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION:
            key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_RELEASE_DELAYED_ACTION, resolution.repeat_count, resolution.action);
            break;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE:
            key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_RELEASE_HELD_LIFECYCLE, 0u, resolution.action);
            break;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN:
            key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_RELEASE_PRESERVE_CHAIN, context.slot ? context.slot->pending_multi_tap.count : 0u, 0u);
            break;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE:
        default:
            break;
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE:
            key_runtime_slot_result_push_builder_if_present(&result, context.key_pos,
                                                            (key_runtime_effect_builder_t){
                                                                .kind   = KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER,
                                                                .action = resolution.action,
                                                            });
            key_runtime_slot_result_push_builder_if_present(&result, context.key_pos,
                                                            (key_runtime_effect_builder_t){
                                                                .kind   = KEY_RUNTIME_EFFECT_BUILDER_HELD_UNREGISTER,
                                                                .action = resolution.action,
                                                            });
            break;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION:
            key_runtime_slot_result_push_delayed_action(&result, resolution.action, resolution.mods, resolution.repeat_count);
            break;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN:
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE:
        default:
            break;
    }

    if (context.is_momentary_layer) {
        key_runtime_slot_result_push_layer_release(&result, context.key_pos);
    }

    if (resolution.outcome == KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN) {
        key_runtime_slot_pending_multi_tap_clear_active_state(slot);
    } else {
        key_runtime_slot_reset(slot);
    }

    return result;
}

static bool key_runtime_slot_pending_multi_tap_hold_elapsed(const active_key_state_t *slot, uint16_t elapsed) {
    key_runtime_slot_interaction_t interaction = key_runtime_slot_cached_interaction(slot);

    return slot && key_runtime_slot_pending_multi_tap_pending_hold(slot) && handled_key_hold_contract_fires_at_threshold(interaction.contract.hold) && elapsed >= interaction.binding.tap_hold_term;
}

static bool key_runtime_slot_pending_multi_tap_scan_releases_layer_before_action(const active_key_state_t *slot, uint16_t action) {
    return slot && is_layer_key(slot->owner.keycode) && noah_action_desc_releases_momentary_layer_before_action(noah_action_describe(action));
}

typedef enum {
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD,
} key_runtime_slot_pending_multi_tap_scan_outcome_t;

typedef struct {
    key_runtime_slot_pending_multi_tap_scan_outcome_t outcome;
    bool                                              release_layer_before_action;
    key_runtime_effect_builder_t                      effect_builder;
} key_runtime_slot_pending_multi_tap_scan_resolution_t;

static key_runtime_slot_pending_multi_tap_scan_resolution_t key_runtime_slot_pending_multi_tap_scan_resolve(active_key_state_t *slot, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = {0};
    key_runtime_slot_interaction_t                       interaction;

    if (!slot) {
        return resolution;
    }

    interaction = key_runtime_slot_cached_interaction(slot);

    if (handled_key_hold_contract_fires_at_threshold(interaction.contract.long_hold) && elapsed >= interaction.binding.longer_hold_term) {
        resolution.outcome                     = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD;
        resolution.release_layer_before_action = key_runtime_slot_pending_multi_tap_scan_releases_layer_before_action(slot, interaction.binding.long_hold.action);
        // Momentary-layer actions use preview/real layer color as feedback, so
        // pending multi-tap promotion should not opt them into a pulse here.
        resolution.effect_builder = key_runtime_slot_policy_promote_to_long_hold(slot, interaction.binding.long_hold, interaction.contract.long_hold, false);
        return resolution;
    }

    if (!key_runtime_slot_pending_multi_tap_hold_elapsed(slot, elapsed)) {
        return resolution;
    }

    resolution.outcome                     = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD;
    resolution.release_layer_before_action = key_runtime_slot_pending_multi_tap_scan_releases_layer_before_action(slot, interaction.binding.hold.action);
    resolution.effect_builder              = key_runtime_slot_policy_fire_hold_at_threshold(slot, interaction.binding.hold, interaction.contract.hold, !interaction.binding.long_hold.present, false);
    return resolution;
}

static key_runtime_slot_result_t key_runtime_slot_pending_multi_tap_scan_hold(active_key_state_t *slot, keypos_t key_pos, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = key_runtime_slot_pending_multi_tap_scan_resolve(slot, elapsed);
    key_runtime_slot_result_t                            result     = {0};

    if (resolution.outcome == KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE) {
        return result;
    }

    key_runtime_slot_reset_pending_multi_tap(slot);

    if (!(resolution.release_layer_before_action || key_runtime_slot_result_builder_has_effect(resolution.effect_builder))) {
        return result;
    }

    result.handled = true;
    if (resolution.release_layer_before_action) {
        key_runtime_slot_result_push_layer_release(&result, key_pos);
    }
    key_runtime_slot_result_push_builder_if_present(&result, key_pos, resolution.effect_builder);
    return result;
}

key_runtime_slot_result_t key_runtime_slot_pending_multi_tap_handle_scan(active_key_state_t *slot) {
    key_runtime_slot_result_t result = {0};
    keypos_t                  key_pos;

    if (!slot || !key_runtime_slot_has_pending_multi_tap(slot)) {
        return result;
    }

    key_pos = key_runtime_slot_active(slot) ? slot->owner.key_pos : slot->pending_multi_tap.key_pos;

    if (key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_active(slot)) {
        return key_runtime_slot_pending_multi_tap_scan_hold(slot, key_pos, timer_elapsed(slot->pending_multi_tap.timer));
    }

    if (key_runtime_slot_pending_multi_tap_expired(slot)) {
        key_runtime_slot_pending_multi_tap_flush_t flush = key_runtime_slot_take_pending_multi_tap_flush(slot);
        result.handled                                   = true;
        key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_SCAN_EXPIRED_FLUSH, flush.repeat_count, flush.action);
        key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    }

    return result;
}
