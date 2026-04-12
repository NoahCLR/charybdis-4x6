// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Release Reducer
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_release_reduce.h"

#include "key_runtime_slot_pending_multi_tap.h"
#include "key_runtime_slot_policy.h"
#include "key_runtime_slot_result_internal.h"

#include "../action/action_lifecycle.h"
#include "../pointing/pd_modes.h"

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

key_runtime_slot_result_t key_runtime_slot_reduce_handled_release(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_behavior_view_t behavior) {
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
