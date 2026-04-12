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
#include "key_runtime_slot_result_internal.h"

#include "../action/action_lifecycle.h"
#include "../pointing/pd_modes.h"

static key_runtime_slot_hold_strategy_t key_runtime_slot_hold_strategy_for_handled_key(handled_key_view_t key) {
    if (handled_key_uses_implicit_hold(key)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    }

    if (handled_key_uses_fallback_hold(key)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK;
    }

    return KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

static key_runtime_slot_phase_t key_runtime_slot_initial_press_phase(hold_behavior_t hold) {
    return hold_registers_on_press(hold) ? KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW : KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW;
}

static key_runtime_slot_effect_request_t key_runtime_slot_step_begin_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_phase_t phase, key_runtime_slot_hold_strategy_t hold_strategy, bool pd_mode_was_locked_on_press) {
    key_runtime_slot_effect_request_t request = {0};

    if (!slot) {
        return request;
    }

    key_runtime_slot_track(slot, keycode, key_pos, tap_action, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, phase, hold_strategy);
    slot->lifecycle.pd_mode_was_locked_on_press = pd_mode_was_locked_on_press;

    if (hold_registers_on_press(hold)) {
        slot->lifecycle.held_action_keycode = hold.action;
        request.kind              = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
        request.action            = hold.action;
    }

    return request;
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
    key_behavior_view_t               behavior;
    hold_behavior_t                   hold;
    key_runtime_slot_hold_strategy_t  hold_strategy;
    pd_mode_mask_t                    pd_mode;
    bool                              matching_pending_multi_tap;
    bool                              flush_pending_multi_tap;
    bool                              needs_layer_press;
    bool                              reclaim_active_slot;
    bool                              active_held_action_survives_flush;
} key_runtime_slot_step_press_context_t;

static key_runtime_slot_step_press_context_t key_runtime_slot_step_press_context(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush) {
    key_behavior_view_t behavior = key.behavior;

    return (key_runtime_slot_step_press_context_t){
        .slot                              = slot,
        .keycode                           = keycode,
        .key_pos                           = key_pos,
        .key                               = key,
        .behavior                          = behavior,
        .hold                              = handled_key_single_hold(key),
        .hold_strategy                     = key_runtime_slot_hold_strategy_for_handled_key(key),
        .pd_mode                           = pd_mode_for_keycode(keycode),
        .matching_pending_multi_tap        = slot && behavior.has_multi_tap && key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos),
        .flush_pending_multi_tap           = slot && key_runtime_slot_has_pending_multi_tap(slot) && !key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos),
        .needs_layer_press                 = behavior.is_momentary_layer,
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
        key_runtime_slot_result_push_layer_press(&result, context->key_pos, behavior_get_layer(context->keycode));
    }

    if (key_runtime_slot_pending_multi_tap_pending_hold(context->slot) || context->needs_layer_press) {
        key_runtime_slot_effect_request_t begin_request = key_runtime_slot_step_begin_press(
            context->slot,
            context->keycode,
            context->key_pos,
            KC_NO,
            hold_behavior_none(),
            hold_behavior_none(),
            context->behavior.tap_hold_term,
            context->behavior.longer_hold_term,
            context->behavior.multi_tap_term,
            key_runtime_slot_pending_multi_tap_pending_hold(context->slot) ? KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW : KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
            KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT,
            false);
        key_runtime_slot_result_push_request_if_present(&result, context->key_pos, begin_request);
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
        key_runtime_slot_result_push_layer_press(&result, context->key_pos, behavior_get_layer(context->keycode));
    }

    if (context->reclaim_active_slot) {
        keypos_t                          reclaim_key_pos  = context->slot->owner.key_pos;
        key_runtime_slot_effect_request_t reclaim_request = key_runtime_slot_policy_take_flush(context->slot, context->active_held_action_survives_flush);
        key_runtime_slot_result_push_request_if_present(&result, reclaim_key_pos, reclaim_request);
    }

    key_runtime_slot_result_push_request_if_present(&result, context->key_pos, key_runtime_slot_step_begin_press(
                                                        context->slot,
                                                        context->keycode,
                                                        context->key_pos,
                                                        handled_key_tap_action(context->key),
                                                        context->hold,
                                                        context->behavior.single.long_hold,
                                                        context->behavior.tap_hold_term,
                                                        context->behavior.longer_hold_term,
                                                        context->behavior.multi_tap_term,
                                                        key_runtime_slot_initial_press_phase(context->hold),
                                                        context->hold_strategy,
                                                        context->pd_mode && pd_mode_locked(context->pd_mode)));
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

typedef enum {
    KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_TAP,
    KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_ACTION,
    KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_PD_MODE_LOCK_TAP,
} key_runtime_slot_step_release_outcome_t;

typedef struct {
    bool                                    release_owned_state;
    key_runtime_slot_step_release_outcome_t outcome;
    uint16_t                                action;
    pd_mode_mask_t                          pd_mode_lock_tap;
} key_runtime_slot_step_release_resolution_t;

typedef struct {
    uint16_t            keycode;
    active_key_state_t  released_key;
    key_behavior_view_t behavior;
    uint16_t            elapsed;
    bool                quick_tap;
    bool                quick_immediate_hold;
    bool                buffered_base_tap;
    pd_mode_mask_t      lock_tap_mode;
} key_runtime_slot_step_release_context_t;

static bool key_runtime_slot_step_release_is_interrupted_layer_tap(const key_runtime_slot_step_release_context_t *context) {
    return context && context->behavior.is_momentary_layer && context->released_key.lifecycle.layer_interrupted;
}

static bool key_runtime_slot_step_release_is_buffered_base_tap(const key_runtime_slot_step_release_context_t *context) {
    return context && key_runtime_slot_uses_fallback_hold(&context->released_key) && context->released_key.binding.tap_action == KC_NO && context->released_key.lifecycle.held_action_keycode == KC_NO;
}

static bool key_runtime_slot_step_release_is_quick_tap(const key_runtime_slot_step_release_context_t *context) {
    return context && context->elapsed < context->released_key.timing.tap_hold_term && !key_runtime_slot_step_release_is_interrupted_layer_tap(context);
}

static pd_mode_mask_t key_runtime_slot_step_locked_pd_mode_tap_mode(const key_runtime_slot_step_release_context_t *context) {
    pd_mode_mask_t mode;

    if (!context) {
        return 0;
    }

    mode = pd_mode_for_keycode(context->keycode);
    if (!mode) {
        return 0;
    }

    if (!context->released_key.lifecycle.pd_mode_was_locked_on_press || context->elapsed >= context->released_key.timing.tap_hold_term) {
        return 0;
    }

    if (key_runtime_slot_step_release_is_interrupted_layer_tap(context)) {
        return 0;
    }

    return mode;
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolution_base(const key_runtime_slot_step_release_context_t *context) {
    return (key_runtime_slot_step_release_resolution_t){
        .release_owned_state = context && (context->released_key.lifecycle.held_action_keycode != KC_NO || context->released_key.lifecycle.repeat_binding_active),
    };
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolution_tap(const key_runtime_slot_step_release_context_t *context) {
    key_runtime_slot_step_release_resolution_t resolution = key_runtime_slot_step_release_resolution_base(context);
    resolution.outcome                                    = KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_TAP;
    return resolution;
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolution_action(const key_runtime_slot_step_release_context_t *context, uint16_t action) {
    key_runtime_slot_step_release_resolution_t resolution = key_runtime_slot_step_release_resolution_base(context);
    resolution.outcome                                    = KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_ACTION;
    resolution.action                                     = action;
    return resolution;
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolution_pd_mode_lock_tap(const key_runtime_slot_step_release_context_t *context) {
    key_runtime_slot_step_release_resolution_t resolution = key_runtime_slot_step_release_resolution_base(context);
    resolution.outcome                                    = KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_PD_MODE_LOCK_TAP;
    resolution.pd_mode_lock_tap                           = context ? context->lock_tap_mode : 0;
    return resolution;
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolve_tap_window(const key_runtime_slot_step_release_context_t *context) {
    if (!context) {
        return (key_runtime_slot_step_release_resolution_t){0};
    }

    if (context->buffered_base_tap) {
        return key_runtime_slot_step_release_resolution_tap(context);
    }

    if (context->quick_tap) {
        if (context->lock_tap_mode) {
            return key_runtime_slot_step_release_resolution_pd_mode_lock_tap(context);
        }

        return key_runtime_slot_step_release_resolution_tap(context);
    }

    if (key_runtime_slot_uses_fallback_hold(&context->released_key)) {
        return key_runtime_slot_step_release_resolution_base(context);
    }

    if (hold_sends_on_release(context->released_key.binding.hold)) {
        return key_runtime_slot_step_release_resolution_action(
            context,
            key_runtime_slot_policy_select_release_hold_action(
                context->elapsed,
                context->released_key.binding.hold.action,
                context->released_key.binding.long_hold,
                context->released_key.timing.longer_hold_term));
    }

    if (hold_sends_on_release(context->released_key.binding.long_hold) && context->elapsed >= context->released_key.timing.longer_hold_term) {
        return key_runtime_slot_step_release_resolution_action(context, context->released_key.binding.long_hold.action);
    }

    if (!context->behavior.is_momentary_layer && context->released_key.binding.tap_action != KC_NO) {
        return key_runtime_slot_step_release_resolution_tap(context);
    }

    return key_runtime_slot_step_release_resolution_base(context);
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolve_press_held_window(const key_runtime_slot_step_release_context_t *context) {
    if (!context) {
        return (key_runtime_slot_step_release_resolution_t){0};
    }

    if (context->lock_tap_mode) {
        return key_runtime_slot_step_release_resolution_pd_mode_lock_tap(context);
    }

    if (context->quick_immediate_hold) {
        return key_runtime_slot_step_release_resolution_tap(context);
    }

    if (hold_sends_on_release(context->released_key.binding.long_hold) && context->elapsed >= context->released_key.timing.longer_hold_term) {
        return key_runtime_slot_step_release_resolution_action(context, context->released_key.binding.long_hold.action);
    }

    return key_runtime_slot_step_release_resolution_base(context);
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolve_release_hold_pending(const key_runtime_slot_step_release_context_t *context) {
    if (!context) {
        return (key_runtime_slot_step_release_resolution_t){0};
    }

    return key_runtime_slot_step_release_resolution_action(
        context,
        key_runtime_slot_policy_select_release_hold_action(
            context->elapsed,
            context->released_key.binding.hold.action,
            context->released_key.binding.long_hold,
            context->released_key.timing.longer_hold_term));
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolve_hold_phase(const key_runtime_slot_step_release_context_t *context) {
    if (!context) {
        return (key_runtime_slot_step_release_resolution_t){0};
    }

    if (hold_sends_on_release(context->released_key.binding.long_hold) && context->elapsed >= context->released_key.timing.longer_hold_term) {
        return key_runtime_slot_step_release_resolution_action(context, context->released_key.binding.long_hold.action);
    }

    return key_runtime_slot_step_release_resolution_base(context);
}

typedef key_runtime_slot_step_release_resolution_t (*key_runtime_slot_step_release_phase_resolver_t)(const key_runtime_slot_step_release_context_t *context);

static const key_runtime_slot_step_release_phase_resolver_t key_runtime_slot_step_release_phase_resolvers[] = {
    [KEY_RUNTIME_SLOT_PHASE_IDLE]                 = key_runtime_slot_step_release_resolution_base,
    [KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW]           = key_runtime_slot_step_release_resolve_tap_window,
    [KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW]    = key_runtime_slot_step_release_resolve_press_held_window,
    [KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING] = key_runtime_slot_step_release_resolve_release_hold_pending,
    [KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE]     = key_runtime_slot_step_release_resolve_hold_phase,
    [KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE]        = key_runtime_slot_step_release_resolve_hold_phase,
};

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_resolve_release(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior, uint16_t elapsed) {
    key_runtime_slot_step_release_context_t context = {
        .keycode      = keycode,
        .released_key = released_key,
        .behavior     = behavior,
        .elapsed      = elapsed,
    };
    key_runtime_slot_phase_t                       phase    = key_runtime_slot_phase(&released_key);
    key_runtime_slot_step_release_phase_resolver_t resolver = key_runtime_slot_step_release_phase_resolvers[phase];

    context.quick_tap            = key_runtime_slot_step_release_is_quick_tap(&context);
    context.quick_immediate_hold = hold_registers_on_press(released_key.binding.hold) && key_runtime_slot_allows_tap_release(&released_key) && context.quick_tap;
    context.buffered_base_tap    = key_runtime_slot_step_release_is_buffered_base_tap(&context);
    context.lock_tap_mode        = key_runtime_slot_step_locked_pd_mode_tap_mode(&context);

    return resolver ? resolver(&context) : key_runtime_slot_step_release_resolution_base(&context);
}

static key_runtime_slot_result_t key_runtime_slot_step_active_release(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior) {
    key_runtime_slot_result_t result = {0};

    if (!slot || slot->owner.keycode == KC_NO) {
        return result;
    }

    active_key_state_t released_key = *slot;
    uint16_t           elapsed      = timer_elapsed(released_key.timer);

    result.handled = true;

    if (behavior.is_momentary_layer) {
        key_runtime_slot_result_push_layer_release(&result, released_key.owner.key_pos);
    }

    key_runtime_slot_reset(slot);

    key_runtime_slot_step_release_resolution_t resolution = key_runtime_slot_step_resolve_release(keycode, released_key, behavior, elapsed);
    if (resolution.release_owned_state) {
        key_runtime_slot_result_push_request_if_present(&result, released_key.owner.key_pos, (key_runtime_slot_effect_request_t){
                                                                                    .release_owned_state = true,
                                                                                });
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_TAP:
            if (behavior.has_multi_tap) {
                key_runtime_slot_begin_pending_multi_tap(slot, keycode, released_key.owner.key_pos, released_key.binding.tap_action, released_key.timing.tap_hold_term, released_key.timing.multi_tap_term);
            } else if (released_key.binding.tap_action != KC_NO) {
                key_runtime_slot_result_push_dispatch_action(&result, released_key.owner.key_pos, released_key.binding.tap_action);
            }
            return result;
        case KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_ACTION:
            key_runtime_slot_result_push_dispatch_action(&result, released_key.owner.key_pos, resolution.action);
            return result;
        case KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            key_runtime_slot_result_push_pd_mode_lock_tap(&result, resolution.pd_mode_lock_tap);
            return result;
        case KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_NONE:
        default:
            return result;
    }
}

static key_runtime_slot_result_t key_runtime_slot_step_handled_release(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_behavior_view_t behavior) {
    key_runtime_slot_result_t result = {0};

    if (slot) {
        uint16_t elapsed = timer_elapsed(slot->timer);
        result           = key_runtime_slot_pending_multi_tap_handle_release(slot, keycode, behavior, elapsed);
        if (result.handled) {
            return result;
        }
    }

    if (key_runtime_slot_matches(slot, keycode, key_pos)) {
        return key_runtime_slot_step_active_release(slot, keycode, behavior);
    }

    result.handled = true;
    if (behavior.is_momentary_layer) {
        key_runtime_slot_result_push_layer_release(&result, key_pos);
    }
    key_runtime_slot_result_push_request_if_present(&result, key_pos, (key_runtime_slot_effect_request_t){
                                                                       .release_owned_state = true,
                                                                   });
    return result;
}

static key_runtime_slot_result_t key_runtime_slot_result_from_effect_requests(keypos_t key_pos, key_runtime_slot_effect_request_t first_request, key_runtime_slot_effect_request_t second_request) {
    key_runtime_slot_result_t result = {0};

    if (!(key_runtime_slot_result_request_has_effect(first_request) || key_runtime_slot_result_request_has_effect(second_request))) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_request_if_present(&result, key_pos, first_request);
    key_runtime_slot_result_push_request_if_present(&result, key_pos, second_request);
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
        return key_runtime_slot_result_from_effect_requests(slot->owner.key_pos, (key_runtime_slot_effect_request_t){0}, key_runtime_slot_policy_activate_pending_fallback_hold(slot));
    }

    if (hold_fires_at_threshold(slot->binding.long_hold) && elapsed >= slot->timing.longer_hold_term) {
        return key_runtime_slot_result_from_effect_requests(slot->owner.key_pos, (key_runtime_slot_effect_request_t){0}, key_runtime_slot_policy_promote_to_long_hold(slot, slot->binding.long_hold, false));
    }

    if (hold_fires_at_threshold(slot->binding.hold) && elapsed >= slot->timing.tap_hold_term) {
        return key_runtime_slot_result_from_effect_requests(slot->owner.key_pos, (key_runtime_slot_effect_request_t){0}, key_runtime_slot_policy_fire_hold_at_threshold(slot, slot->binding.hold, slot->binding.long_hold, false));
    }

    if (key_runtime_slot_active_scan_should_mark_release_hold_pending(*slot, elapsed)) {
        key_runtime_slot_set_release_hold_pending(slot);
    }

    return (key_runtime_slot_result_t){0};
}

static key_runtime_slot_result_t key_runtime_slot_step_active_scan_press_held_window(active_key_state_t *slot, uint16_t elapsed) {
    key_runtime_slot_effect_request_t immediate_hold_request = {0};
    key_runtime_slot_effect_request_t effect_request         = {0};

    if (!slot) {
        return (key_runtime_slot_result_t){0};
    }

    if (elapsed >= slot->timing.tap_hold_term) {
        immediate_hold_request = key_runtime_slot_policy_commit_immediate_hold(slot, !key_runtime_slot_uses_implicit_hold(slot), !slot->binding.long_hold.present);
    }

    if (hold_fires_at_threshold(slot->binding.long_hold) && elapsed >= slot->timing.longer_hold_term) {
        effect_request = key_runtime_slot_policy_promote_to_long_hold(slot, slot->binding.long_hold, false);
    }

    return key_runtime_slot_result_from_effect_requests(slot->owner.key_pos, immediate_hold_request, effect_request);
}

static key_runtime_slot_result_t key_runtime_slot_step_active_scan_hold_phase(active_key_state_t *slot, uint16_t elapsed) {
    if (!(slot && hold_fires_at_threshold(slot->binding.long_hold) && elapsed >= slot->timing.longer_hold_term)) {
        return (key_runtime_slot_result_t){0};
    }

    return key_runtime_slot_result_from_effect_requests(slot->owner.key_pos, (key_runtime_slot_effect_request_t){0}, key_runtime_slot_policy_promote_to_long_hold(slot, slot->binding.long_hold, false));
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

static key_runtime_slot_result_t key_runtime_slot_step_active_scan(active_key_state_t *slot) {
    if (!(slot && key_runtime_slot_active(slot) && !key_runtime_slot_hold_is_complete(slot))) {
        return (key_runtime_slot_result_t){0};
    }

    uint16_t                                     elapsed = timer_elapsed(slot->timer);
    key_runtime_slot_phase_t                     phase   = key_runtime_slot_phase(slot);
    key_runtime_slot_step_active_scan_phase_handler_t handler = key_runtime_slot_step_active_scan_phase_handlers[phase];

    return handler ? handler(slot, elapsed) : (key_runtime_slot_result_t){0};
}

static key_runtime_slot_result_t key_runtime_slot_step_interrupt(active_key_state_t *slot, keypos_t other_key_pos) {
    key_runtime_slot_result_t         result  = {0};
    key_runtime_slot_effect_request_t request = key_runtime_slot_policy_interrupt_on_other_press(slot, other_key_pos);

    if (!key_runtime_slot_result_request_has_effect(request)) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_request_if_present(&result, slot ? slot->owner.key_pos : (keypos_t){0}, request);
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
    return key_runtime_slot_step_handled_release(
        slot,
        event->data.handled_release.keycode,
        event->data.handled_release.key_pos,
        event->data.handled_release.behavior);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_active_scan(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    (void)event;
    return key_runtime_slot_step_active_scan(slot);
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
