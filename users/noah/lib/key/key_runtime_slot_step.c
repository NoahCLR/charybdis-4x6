// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Step
// ────────────────────────────────────────────────────────────────────────────
//
// Reducer implementation for handled-key slot events. The transition layer
// feeds one slot event at a time through this module, which owns the remaining
// press/release/scan/interrupt/flush state reducers.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_step.h"

#include "key_runtime_slot_pending_multi_tap.h"
#include "key_runtime_slot_policy.h"
#include "key_runtime_slot_effect.h"
#include "key_runtime_slot_release_reduce.h"
#include "key_runtime_slot_scan_reduce.h"
#include "key_runtime_slot_result_internal.h"

#include "../action/action_lifecycle.h"
#include "../pointing/pd_modes.h"

static key_runtime_slot_phase_t key_runtime_slot_initial_press_phase(hold_behavior_t hold) {
    return hold_registers_on_press(hold) ? KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW : KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW;
}

static key_runtime_effect_builder_t key_runtime_slot_step_begin_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_phase_t phase, key_runtime_slot_hold_strategy_t hold_strategy, bool pd_mode_was_locked_on_press) {
    key_runtime_effect_builder_t builder = {0};

    if (!slot) {
        return builder;
    }

    key_runtime_slot_track(slot, keycode, key_pos, tap_action, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, phase, hold_strategy);
    key_runtime_slot_apply_handled_metadata(slot, key);
    slot->lifecycle.pd_mode_was_locked_on_press = pd_mode_was_locked_on_press;

    if (hold_registers_on_press(hold)) {
        slot->lifecycle.held_action_keycode = hold.action;
        builder.kind              = KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER;
        builder.action            = hold.action;
    }

    return builder;
}

typedef enum {
    KEY_RUNTIME_SLOT_STEP_PRESS_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_STEP_PRESS_OUTCOME_REUSE_PENDING_MULTI_TAP,
    KEY_RUNTIME_SLOT_STEP_PRESS_OUTCOME_BEGIN_FRESH,
} key_runtime_slot_step_press_outcome_t;

typedef struct {
    active_key_state_t               *slot;
    uint16_t                          keycode;
    keypos_t                          key_pos;
    handled_key_view_t                key;
    uint16_t                          tap_action;
    hold_behavior_t                   hold;
    hold_behavior_t                   long_hold;
    key_runtime_slot_hold_strategy_t  hold_strategy;
    uint16_t                          tap_hold_term;
    uint16_t                          longer_hold_term;
    uint16_t                          multi_tap_term;
    uint8_t                           layer;
    pd_mode_mask_t                    pd_mode;
    bool                              matching_pending_multi_tap;
    bool                              flush_pending_multi_tap;
    bool                              needs_layer_press;
    bool                              reclaim_active_slot;
    bool                              active_held_action_survives_flush;
} key_runtime_slot_step_press_context_t;

static key_runtime_slot_step_press_context_t key_runtime_slot_step_press_context(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush) {
    return (key_runtime_slot_step_press_context_t){
        .slot                              = slot,
        .keycode                           = keycode,
        .key_pos                           = key_pos,
        .key                               = key,
        .tap_action                        = handled_key_tap_action(key),
        .hold                              = handled_key_single_hold(key),
        .long_hold                         = handled_key_long_hold(key),
        .hold_strategy                     = handled_key_hold_strategy(key),
        .tap_hold_term                     = handled_key_tap_hold_term(key),
        .longer_hold_term                  = handled_key_longer_hold_term(key),
        .multi_tap_term                    = handled_key_multi_tap_term(key),
        .layer                             = handled_key_layer(key),
        .pd_mode                           = handled_key_pd_mode(key),
        .matching_pending_multi_tap        = slot && handled_key_has_multi_tap(key) && key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos),
        .flush_pending_multi_tap           = slot && key_runtime_slot_has_pending_multi_tap(slot) && !key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos),
        .needs_layer_press                 = handled_key_is_momentary_layer(key),
        .reclaim_active_slot               = slot && key_runtime_slot_active(slot) && !key_runtime_slot_matches(slot, keycode, key_pos),
        .active_held_action_survives_flush = active_held_action_survives_flush,
    };
}

static key_runtime_slot_step_press_outcome_t key_runtime_slot_step_press_outcome_for_context(const key_runtime_slot_step_press_context_t *context) {
    if (!(context && context->slot)) {
        return KEY_RUNTIME_SLOT_STEP_PRESS_OUTCOME_NONE;
    }

    if (context->matching_pending_multi_tap) {
        return KEY_RUNTIME_SLOT_STEP_PRESS_OUTCOME_REUSE_PENDING_MULTI_TAP;
    }

    return KEY_RUNTIME_SLOT_STEP_PRESS_OUTCOME_BEGIN_FRESH;
}

static key_runtime_slot_result_t key_runtime_slot_step_handled_press_reuse_pending_multi_tap(const key_runtime_slot_step_press_context_t *context) {
    key_runtime_slot_result_t result = {0};

    if (!(context && context->slot)) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_dispatch_action(&result, context->key_pos, key_runtime_slot_advance_pending_multi_tap(context->slot, context->keycode));

    if (context->needs_layer_press) {
        key_runtime_slot_result_push_layer_press(&result, context->key_pos, context->layer);
    }

    if (key_runtime_slot_pending_multi_tap_pending_hold(context->slot) || context->needs_layer_press) {
        key_runtime_effect_builder_t begin_builder = key_runtime_slot_step_begin_press(
            context->slot,
            context->keycode,
            context->key_pos,
            context->key,
            KC_NO,
            hold_behavior_none(),
            hold_behavior_none(),
            context->tap_hold_term,
            context->longer_hold_term,
            context->multi_tap_term,
            key_runtime_slot_pending_multi_tap_pending_hold(context->slot) ? KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW : KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
            KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT,
            false);
        key_runtime_slot_result_push_builder_if_present(&result, context->key_pos, begin_builder);
    }

    return result;
}

static key_runtime_slot_result_t key_runtime_slot_step_handled_press_begin_fresh(const key_runtime_slot_step_press_context_t *context) {
    key_runtime_slot_result_t result = {0};

    if (!(context && context->slot)) {
        return result;
    }

    result.handled = true;

    if (context->flush_pending_multi_tap) {
        key_runtime_slot_pending_multi_tap_flush_t flush = key_runtime_slot_take_pending_multi_tap_flush(context->slot);
        key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    }

    if (context->needs_layer_press) {
        key_runtime_slot_result_push_layer_press(&result, context->key_pos, context->layer);
    }

    if (context->reclaim_active_slot) {
        keypos_t                          reclaim_key_pos  = context->slot->owner.key_pos;
        key_runtime_effect_builder_t reclaim_builder = key_runtime_slot_policy_take_flush(context->slot, context->active_held_action_survives_flush);
        key_runtime_slot_result_push_builder_if_present(&result, reclaim_key_pos, reclaim_builder);
    }

    key_runtime_slot_result_push_builder_if_present(&result, context->key_pos, key_runtime_slot_step_begin_press(
                                                        context->slot,
                                                        context->keycode,
                                                        context->key_pos,
                                                        context->key,
                                                        context->tap_action,
                                                        context->hold,
                                                        context->long_hold,
                                                        context->tap_hold_term,
                                                        context->longer_hold_term,
                                                        context->multi_tap_term,
                                                        key_runtime_slot_initial_press_phase(context->hold),
                                                        context->hold_strategy,
                                                        context->pd_mode && pd_mode_local_locked(context->pd_mode)));
    return result;
}

static key_runtime_slot_result_t key_runtime_slot_step_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush) {
    key_runtime_slot_step_press_context_t context = key_runtime_slot_step_press_context(slot, keycode, key_pos, key, active_held_action_survives_flush);

    switch (key_runtime_slot_step_press_outcome_for_context(&context)) {
        case KEY_RUNTIME_SLOT_STEP_PRESS_OUTCOME_REUSE_PENDING_MULTI_TAP:
            return key_runtime_slot_step_handled_press_reuse_pending_multi_tap(&context);
        case KEY_RUNTIME_SLOT_STEP_PRESS_OUTCOME_BEGIN_FRESH:
            return key_runtime_slot_step_handled_press_begin_fresh(&context);
        case KEY_RUNTIME_SLOT_STEP_PRESS_OUTCOME_NONE:
        default:
            return (key_runtime_slot_result_t){0};
    }
}

static key_runtime_slot_result_t key_runtime_slot_step_interrupt(active_key_state_t *slot, keypos_t other_key_pos) {
    key_runtime_slot_result_t        result  = {0};
    key_runtime_effect_builder_t builder = key_runtime_slot_policy_interrupt_on_other_press(slot, other_key_pos);

    if (!key_runtime_slot_result_builder_has_effect(builder)) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_builder_if_present(&result, slot ? slot->owner.key_pos : (keypos_t){0}, builder);
    return result;
}

static key_runtime_slot_result_t key_runtime_slot_step_pending_multi_tap_flush(active_key_state_t *slot) {
    key_runtime_slot_result_t                  result = {0};
    key_runtime_slot_pending_multi_tap_flush_t flush  = key_runtime_slot_take_pending_multi_tap_flush(slot);

    if (!flush.handled) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    return result;
}

typedef key_runtime_slot_result_t (*key_runtime_slot_step_event_handler_t)(active_key_state_t *slot, const key_runtime_slot_event_t *event);

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_handled_press(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    return key_runtime_slot_step_handled_press(
        slot,
        event->data.handled_press.keycode,
        event->data.handled_press.key_pos,
        event->data.handled_press.key,
        event->data.handled_press.active_held_action_survives_flush);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_handled_release(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    return key_runtime_slot_reduce_handled_release(
        slot,
        event->data.handled_release.keycode,
        event->data.handled_release.key_pos,
        event->data.handled_release.key);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_active_scan(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    (void)event;
    return key_runtime_slot_reduce_active_scan(slot);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_pending_multi_tap_scan(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    (void)event;
    return key_runtime_slot_pending_multi_tap_handle_scan(slot);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_interrupt(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    return key_runtime_slot_step_interrupt(slot, event->data.interrupt.other_key_pos);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_pending_multi_tap_flush(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    (void)event;
    return key_runtime_slot_step_pending_multi_tap_flush(slot);
}

static const key_runtime_slot_step_event_handler_t key_runtime_slot_step_event_handlers[] = {
    [KEY_RUNTIME_SLOT_EVENT_NONE]                   = NULL,
    [KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS]          = key_runtime_slot_step_handle_event_handled_press,
    [KEY_RUNTIME_SLOT_EVENT_HANDLED_RELEASE]        = key_runtime_slot_step_handle_event_handled_release,
    [KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN]            = key_runtime_slot_step_handle_event_active_scan,
    [KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN] = key_runtime_slot_step_handle_event_pending_multi_tap_scan,
    [KEY_RUNTIME_SLOT_EVENT_INTERRUPT]              = key_runtime_slot_step_handle_event_interrupt,
    [KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_FLUSH] = key_runtime_slot_step_handle_event_pending_multi_tap_flush,
};

key_runtime_slot_result_t key_runtime_slot_step(active_key_state_t *slot, key_runtime_slot_event_t event) {
    key_runtime_slot_step_event_handler_t handler = event.kind < ARRAY_SIZE(key_runtime_slot_step_event_handlers) ? key_runtime_slot_step_event_handlers[event.kind] : NULL;

    return handler ? handler(slot, &event) : (key_runtime_slot_result_t){0};
}
