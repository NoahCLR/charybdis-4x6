// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Step
// ────────────────────────────────────────────────────────────────────────────
//
// Reducer implementation for handled-key slot events. The transition layer
// feeds one slot event at a time through this module, which owns the remaining
// press/release/scan/interrupt/flush state reducers.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_step.h"

#include "key_runtime_slot_effect.h"
#include "key_runtime_slot_result_internal.h"

#include "../action/action_dispatch.h"
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
    slot->pd_mode_was_locked_on_press = pd_mode_was_locked_on_press;

    if (hold_registers_on_press(hold)) {
        slot->held_action_keycode = hold.action;
        request.kind              = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
        request.action            = hold.action;
    }

    return request;
}

static key_runtime_slot_result_t key_runtime_slot_step_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush) {
    key_runtime_slot_result_t        result        = {0};
    key_behavior_view_t              behavior      = key.behavior;
    hold_behavior_t                  hold          = handled_key_single_hold(key);
    key_runtime_slot_hold_strategy_t hold_strategy = key_runtime_slot_hold_strategy_for_handled_key(key);
    pd_mode_mask_t                   mode          = pd_mode_for_keycode(keycode);

    if (!slot) {
        return result;
    }

    result.handled = true;

    if (behavior.has_multi_tap && key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos)) {
        key_runtime_slot_result_push_dispatch_action(&result, key_pos, key_runtime_slot_advance_pending_multi_tap(slot, keycode));

        if (behavior.is_momentary_layer) {
            key_runtime_slot_result_push_layer_press(&result, key_pos, behavior_get_layer(keycode));
        }

        if (key_runtime_slot_pending_multi_tap_pending_hold(slot) || behavior.is_momentary_layer) {
            key_runtime_slot_effect_request_t begin_request = key_runtime_slot_step_begin_press(
                slot,
                keycode,
                key_pos,
                KC_NO,
                hold_behavior_none(),
                hold_behavior_none(),
                behavior.tap_hold_term,
                behavior.longer_hold_term,
                behavior.multi_tap_term,
                key_runtime_slot_pending_multi_tap_pending_hold(slot) ? KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW : KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
                KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT,
                false);
            key_runtime_slot_result_push_request_if_present(&result, key_pos, begin_request);
        }

        return result;
    }

    if (key_runtime_slot_has_pending_multi_tap(slot) && !key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos)) {
        key_runtime_slot_pending_multi_tap_flush_t flush = key_runtime_slot_take_pending_multi_tap_flush(slot);
        key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    }

    if (behavior.is_momentary_layer) {
        key_runtime_slot_result_push_layer_press(&result, key_pos, behavior_get_layer(keycode));
    }

    if (key_runtime_slot_active(slot) && !key_runtime_slot_matches(slot, keycode, key_pos)) {
        keypos_t                          reclaim_key_pos  = slot->key_pos;
        key_runtime_slot_effect_request_t reclaim_request = key_runtime_slot_take_flush(slot, active_held_action_survives_flush);
        key_runtime_slot_result_push_request_if_present(&result, reclaim_key_pos, reclaim_request);
    }

    key_runtime_slot_effect_request_t begin_request = key_runtime_slot_step_begin_press(
        slot,
        keycode,
        key_pos,
        handled_key_tap_action(key),
        hold,
        behavior.single.long_hold,
        behavior.tap_hold_term,
        behavior.longer_hold_term,
        behavior.multi_tap_term,
        key_runtime_slot_initial_press_phase(hold),
        hold_strategy,
        mode && pd_mode_locked(mode));
    key_runtime_slot_result_push_request_if_present(&result, key_pos, begin_request);
    return result;
}

static void key_runtime_slot_clear_active_state(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    multi_tap_t pending_multi_tap = slot->pending_multi_tap;
    *slot                         = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
    slot->pending_multi_tap       = pending_multi_tap;
}

static uint16_t key_runtime_slot_select_release_hold_action(uint16_t elapsed, uint16_t hold_action, hold_behavior_t long_hold, uint16_t longer_hold_term) {
    if (hold_sends_on_release(long_hold) && elapsed >= longer_hold_term) {
        return long_hold.action;
    }

    return hold_action;
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
    return context && context->behavior.is_momentary_layer && context->released_key.layer_interrupted;
}

static bool key_runtime_slot_step_release_is_buffered_base_tap(const key_runtime_slot_step_release_context_t *context) {
    return context && key_runtime_slot_uses_fallback_hold(&context->released_key) && context->released_key.tap_action == KC_NO && context->released_key.held_action_keycode == KC_NO;
}

static bool key_runtime_slot_step_release_is_quick_tap(const key_runtime_slot_step_release_context_t *context) {
    return context && context->elapsed < context->released_key.tap_hold_term && !key_runtime_slot_step_release_is_interrupted_layer_tap(context);
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

    if (!context->released_key.pd_mode_was_locked_on_press || context->elapsed >= context->released_key.tap_hold_term) {
        return 0;
    }

    if (key_runtime_slot_step_release_is_interrupted_layer_tap(context)) {
        return 0;
    }

    return mode;
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolution_base(const key_runtime_slot_step_release_context_t *context) {
    return (key_runtime_slot_step_release_resolution_t){
        .release_owned_state = context && (context->released_key.held_action_keycode != KC_NO || context->released_key.repeat_binding_active),
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

    if (hold_sends_on_release(context->released_key.hold)) {
        return key_runtime_slot_step_release_resolution_action(
            context,
            key_runtime_slot_select_release_hold_action(
                context->elapsed,
                context->released_key.hold.action,
                context->released_key.long_hold,
                context->released_key.longer_hold_term));
    }

    if (hold_sends_on_release(context->released_key.long_hold) && context->elapsed >= context->released_key.longer_hold_term) {
        return key_runtime_slot_step_release_resolution_action(context, context->released_key.long_hold.action);
    }

    if (!context->behavior.is_momentary_layer && context->released_key.tap_action != KC_NO) {
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

    if (hold_sends_on_release(context->released_key.long_hold) && context->elapsed >= context->released_key.longer_hold_term) {
        return key_runtime_slot_step_release_resolution_action(context, context->released_key.long_hold.action);
    }

    return key_runtime_slot_step_release_resolution_base(context);
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolve_release_hold_pending(const key_runtime_slot_step_release_context_t *context) {
    if (!context) {
        return (key_runtime_slot_step_release_resolution_t){0};
    }

    return key_runtime_slot_step_release_resolution_action(
        context,
        key_runtime_slot_select_release_hold_action(
            context->elapsed,
            context->released_key.hold.action,
            context->released_key.long_hold,
            context->released_key.longer_hold_term));
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_release_resolve_hold_phase(const key_runtime_slot_step_release_context_t *context) {
    if (!context) {
        return (key_runtime_slot_step_release_resolution_t){0};
    }

    if (hold_sends_on_release(context->released_key.long_hold) && context->elapsed >= context->released_key.longer_hold_term) {
        return key_runtime_slot_step_release_resolution_action(context, context->released_key.long_hold.action);
    }

    return key_runtime_slot_step_release_resolution_base(context);
}

static key_runtime_slot_step_release_resolution_t key_runtime_slot_step_resolve_release(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior, uint16_t elapsed) {
    key_runtime_slot_step_release_context_t context = {
        .keycode      = keycode,
        .released_key = released_key,
        .behavior     = behavior,
        .elapsed      = elapsed,
    };

    context.quick_tap            = key_runtime_slot_step_release_is_quick_tap(&context);
    context.quick_immediate_hold = hold_registers_on_press(released_key.hold) && key_runtime_slot_allows_tap_release(&released_key) && context.quick_tap;
    context.buffered_base_tap    = key_runtime_slot_step_release_is_buffered_base_tap(&context);
    context.lock_tap_mode        = key_runtime_slot_step_locked_pd_mode_tap_mode(&context);

    switch (key_runtime_slot_phase(&released_key)) {
        case KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW:
            return key_runtime_slot_step_release_resolve_tap_window(&context);
        case KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW:
            return key_runtime_slot_step_release_resolve_press_held_window(&context);
        case KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING:
            return key_runtime_slot_step_release_resolve_release_hold_pending(&context);
        case KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE:
        case KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE:
            return key_runtime_slot_step_release_resolve_hold_phase(&context);
        case KEY_RUNTIME_SLOT_PHASE_IDLE:
        default:
            return key_runtime_slot_step_release_resolution_base(&context);
    }
}

static key_runtime_slot_result_t key_runtime_slot_step_active_release(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior) {
    key_runtime_slot_result_t result = {0};

    if (!slot || slot->keycode == KC_NO) {
        return result;
    }

    active_key_state_t released_key = *slot;
    uint16_t           elapsed      = timer_elapsed(released_key.timer);

    result.handled = true;

    if (behavior.is_momentary_layer) {
        key_runtime_slot_result_push_layer_release(&result, released_key.key_pos);
    }

    key_runtime_slot_reset(slot);

    key_runtime_slot_step_release_resolution_t resolution = key_runtime_slot_step_resolve_release(keycode, released_key, behavior, elapsed);
    if (resolution.release_owned_state) {
        key_runtime_slot_result_push_request_if_present(&result, released_key.key_pos, (key_runtime_slot_effect_request_t){
                                                                                    .release_owned_state = true,
                                                                                });
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_TAP:
            if (behavior.has_multi_tap) {
                key_runtime_slot_begin_pending_multi_tap(slot, keycode, released_key.key_pos, released_key.tap_action, released_key.tap_hold_term, released_key.multi_tap_term);
            } else if (released_key.tap_action != KC_NO) {
                key_runtime_slot_result_push_dispatch_action(&result, released_key.key_pos, released_key.tap_action);
            }
            return result;
        case KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_ACTION:
            key_runtime_slot_result_push_dispatch_action(&result, released_key.key_pos, resolution.action);
            return result;
        case KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            key_runtime_slot_result_push_pd_mode_lock_tap(&result, resolution.pd_mode_lock_tap);
            return result;
        case KEY_RUNTIME_SLOT_STEP_RELEASE_OUTCOME_NONE:
        default:
            return result;
    }
}

static bool key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(const active_key_state_t *slot, hold_behavior_t hold, uint16_t action, uint8_t repeat_count, uint16_t elapsed) {
    if (!slot || !hold.present || repeat_count != 1 || elapsed < slot->tap_hold_term) {
        return false;
    }

    if (hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE || action != hold.action) {
        return false;
    }

    return noah_action_hold_kind(hold.action) != NOAH_ACTION_HOLD_KIND_PRESS_ONLY;
}

typedef enum {
    KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
    KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
    KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
} key_runtime_slot_step_pending_multi_tap_release_outcome_t;

typedef struct {
    active_key_state_t *slot;
    uint16_t            keycode;
    key_behavior_view_t behavior;
    uint16_t            elapsed;
    keypos_t            key_pos;
    delayed_action_mods_t mods;
    hold_behavior_t     hold;
    hold_behavior_t     long_hold;
    uint16_t            action;
    uint8_t             repeat_count;
    bool                matched;
} key_runtime_slot_step_pending_multi_tap_release_context_t;

typedef struct {
    key_runtime_slot_step_pending_multi_tap_release_outcome_t outcome;
    uint16_t                                                  action;
    uint8_t                                                   repeat_count;
    delayed_action_mods_t                                     mods;
} key_runtime_slot_step_pending_multi_tap_release_resolution_t;

static key_runtime_slot_step_pending_multi_tap_release_context_t key_runtime_slot_step_pending_multi_tap_release_context(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior, uint16_t elapsed) {
    key_runtime_slot_step_pending_multi_tap_release_context_t context = {
        .slot     = slot,
        .keycode  = keycode,
        .behavior = behavior,
        .elapsed  = elapsed,
    };
    multi_tap_t *slot_multi_tap;

    if (!(slot && key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_pending_multi_tap_matches(slot, keycode, slot->key_pos))) {
        return context;
    }

    slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
    if (!slot_multi_tap) {
        return context;
    }

    context.key_pos = slot->key_pos;
    context.mods    = delayed_action_mods_from_multi_tap(slot_multi_tap);
    context.hold    = slot_multi_tap->hold;
    context.long_hold = slot_multi_tap->long_hold;
    context.action  = key_runtime_slot_resolve_pending_multi_tap_hold(slot, keycode, &context.repeat_count);
    context.matched = true;

    if (!context.hold.present && hold_sends_on_release(context.long_hold) && elapsed >= slot->longer_hold_term) {
        context.action = context.long_hold.action;
    } else if (hold_sends_on_release(context.hold) && context.repeat_count == 1 && context.action == context.hold.action) {
        context.action = key_runtime_slot_select_release_hold_action(elapsed, context.hold.action, context.long_hold, slot->longer_hold_term);
    }

    return context;
}

static bool key_runtime_slot_step_pending_multi_tap_release_preserves_chain(const key_runtime_slot_step_pending_multi_tap_release_context_t *context) {
    return context && context->action == KC_NO && context->repeat_count == 0 && key_runtime_slot_has_pending_multi_tap(context->slot);
}

static key_runtime_slot_step_pending_multi_tap_release_resolution_t key_runtime_slot_step_pending_multi_tap_release_resolve(const key_runtime_slot_step_pending_multi_tap_release_context_t *context) {
    if (!(context && context->matched)) {
        return (key_runtime_slot_step_pending_multi_tap_release_resolution_t){0};
    }

    if (key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(context->slot, context->hold, context->action, context->repeat_count, context->elapsed)) {
        return (key_runtime_slot_step_pending_multi_tap_release_resolution_t){
            .outcome = KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
            .action  = context->action,
        };
    }

    if (key_runtime_slot_step_pending_multi_tap_release_preserves_chain(context)) {
        return (key_runtime_slot_step_pending_multi_tap_release_resolution_t){
            .outcome = KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
        };
    }

    return (key_runtime_slot_step_pending_multi_tap_release_resolution_t){
        .outcome      = KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
        .action       = context->action,
        .repeat_count = context->repeat_count,
        .mods         = context->mods,
    };
}

static key_runtime_slot_result_t key_runtime_slot_step_pending_multi_tap_hold_release(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior, uint16_t elapsed) {
    key_runtime_slot_step_pending_multi_tap_release_context_t    context    = key_runtime_slot_step_pending_multi_tap_release_context(slot, keycode, behavior, elapsed);
    key_runtime_slot_step_pending_multi_tap_release_resolution_t resolution = key_runtime_slot_step_pending_multi_tap_release_resolve(&context);
    key_runtime_slot_result_t                                    result     = {0};

    if (!context.matched) {
        return result;
    }

    result.handled = true;

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE:
            key_runtime_slot_result_push_request_if_present(&result, context.key_pos, (key_runtime_slot_effect_request_t){
                                                                                .kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER,
                                                                                .action = resolution.action,
                                                                            });
            key_runtime_slot_result_push_request_if_present(&result, context.key_pos, (key_runtime_slot_effect_request_t){
                                                                                .kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER,
                                                                                .action = resolution.action,
                                                                            });
            break;
        case KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION:
            key_runtime_slot_result_push_delayed_action(&result, resolution.action, resolution.mods, resolution.repeat_count);
            break;
        case KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN:
        case KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE:
        default:
            break;
    }

    if (behavior.is_momentary_layer) {
        key_runtime_slot_result_push_layer_release(&result, context.key_pos);
    }

    if (resolution.outcome == KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN) {
        // A quick release can intentionally keep the chain alive so a later
        // tap or timeout still resolves the current tap index.
        key_runtime_slot_clear_active_state(slot);
    } else {
        key_runtime_slot_reset(slot);
    }

    return result;
}

static key_runtime_slot_result_t key_runtime_slot_step_handled_release(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_behavior_view_t behavior) {
    key_runtime_slot_result_t result = {0};

    if (slot) {
        uint16_t elapsed = timer_elapsed(slot->timer);
        result           = key_runtime_slot_step_pending_multi_tap_hold_release(slot, keycode, behavior, elapsed);
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

    if (hold_sends_on_release(active_key_state.hold) && elapsed >= active_key_state.tap_hold_term) {
        return true;
    }

    return !active_key_state.hold.present && hold_sends_on_release(active_key_state.long_hold) && elapsed >= active_key_state.longer_hold_term;
}

static bool key_runtime_slot_pending_multi_tap_hold_elapsed(const multi_tap_t *multi_tap_state, uint16_t elapsed) {
    return multi_tap_state->pending_hold && hold_fires_at_threshold(multi_tap_state->hold) && elapsed >= multi_tap_state->tap_hold_term;
}

static key_runtime_slot_result_t key_runtime_slot_step_active_scan_tap_window(active_key_state_t *slot, uint16_t elapsed) {
    if (!slot) {
        return (key_runtime_slot_result_t){0};
    }

    if (key_runtime_slot_uses_fallback_hold(slot) && slot->held_action_keycode == KC_NO && elapsed >= slot->tap_hold_term) {
        return key_runtime_slot_result_from_effect_requests(slot->key_pos, (key_runtime_slot_effect_request_t){0}, key_runtime_slot_activate_pending_fallback_hold_request(slot));
    }

    if (hold_fires_at_threshold(slot->long_hold) && elapsed >= slot->longer_hold_term) {
        return key_runtime_slot_result_from_effect_requests(slot->key_pos, (key_runtime_slot_effect_request_t){0}, key_runtime_slot_promote_to_long_hold(slot, slot->long_hold, false));
    }

    if (hold_fires_at_threshold(slot->hold) && elapsed >= slot->tap_hold_term) {
        return key_runtime_slot_result_from_effect_requests(slot->key_pos, (key_runtime_slot_effect_request_t){0}, key_runtime_slot_fire_hold_at_threshold(slot, slot->hold, slot->long_hold, false));
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

    if (elapsed >= slot->tap_hold_term) {
        immediate_hold_request = key_runtime_slot_commit_immediate_hold(slot, !key_runtime_slot_uses_implicit_hold(slot), !slot->long_hold.present);
    }

    if (hold_fires_at_threshold(slot->long_hold) && elapsed >= slot->longer_hold_term) {
        effect_request = key_runtime_slot_promote_to_long_hold(slot, slot->long_hold, false);
    }

    return key_runtime_slot_result_from_effect_requests(slot->key_pos, immediate_hold_request, effect_request);
}

static key_runtime_slot_result_t key_runtime_slot_step_active_scan_hold_phase(active_key_state_t *slot, uint16_t elapsed) {
    if (!(slot && hold_fires_at_threshold(slot->long_hold) && elapsed >= slot->longer_hold_term)) {
        return (key_runtime_slot_result_t){0};
    }

    return key_runtime_slot_result_from_effect_requests(slot->key_pos, (key_runtime_slot_effect_request_t){0}, key_runtime_slot_promote_to_long_hold(slot, slot->long_hold, false));
}

static key_runtime_slot_result_t key_runtime_slot_step_active_scan(active_key_state_t *slot) {
    if (!(slot && key_runtime_slot_active(slot) && !key_runtime_slot_hold_is_complete(slot))) {
        return (key_runtime_slot_result_t){0};
    }

    uint16_t elapsed = timer_elapsed(slot->timer);

    switch (key_runtime_slot_phase(slot)) {
        case KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW:
            return key_runtime_slot_step_active_scan_tap_window(slot, elapsed);
        case KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW:
            return key_runtime_slot_step_active_scan_press_held_window(slot, elapsed);
        case KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING:
        case KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE:
            return key_runtime_slot_step_active_scan_hold_phase(slot, elapsed);
        case KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE:
        case KEY_RUNTIME_SLOT_PHASE_IDLE:
        default:
            return (key_runtime_slot_result_t){0};
    }
}

static bool key_runtime_slot_pending_multi_tap_scan_releases_layer_before_action(const active_key_state_t *slot, uint16_t action) {
    return slot && is_layer_key(slot->keycode) && action_dispatch_is_layer_lock(action);
}

typedef enum {
    KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD,
    KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD,
} key_runtime_slot_step_pending_multi_tap_scan_outcome_t;

typedef struct {
    key_runtime_slot_step_pending_multi_tap_scan_outcome_t outcome;
    bool                                                  release_layer_before_action;
    key_runtime_slot_effect_request_t                     effect_request;
} key_runtime_slot_step_pending_multi_tap_scan_resolution_t;

static key_runtime_slot_step_pending_multi_tap_scan_resolution_t key_runtime_slot_step_pending_multi_tap_scan_resolve(active_key_state_t *slot, multi_tap_t *slot_multi_tap, uint16_t elapsed) {
    key_runtime_slot_step_pending_multi_tap_scan_resolution_t resolution = {0};

    if (!(slot && slot_multi_tap)) {
        return resolution;
    }

    slot->long_hold = slot_multi_tap->long_hold;

    if (hold_fires_at_threshold(slot_multi_tap->long_hold) && elapsed >= slot->longer_hold_term) {
        resolution.outcome                    = KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD;
        resolution.release_layer_before_action = key_runtime_slot_pending_multi_tap_scan_releases_layer_before_action(slot, slot_multi_tap->long_hold.action);
        resolution.effect_request            = key_runtime_slot_promote_to_long_hold(slot, slot->long_hold, true);
        return resolution;
    }

    if (!key_runtime_slot_pending_multi_tap_hold_elapsed(slot_multi_tap, elapsed)) {
        return resolution;
    }

    resolution.outcome                    = KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD;
    resolution.release_layer_before_action = key_runtime_slot_pending_multi_tap_scan_releases_layer_before_action(slot, slot_multi_tap->hold.action);
    resolution.effect_request            = key_runtime_slot_fire_hold_at_threshold(slot, slot_multi_tap->hold, slot->long_hold, true);
    return resolution;
}

static key_runtime_slot_result_t key_runtime_slot_step_pending_multi_tap_scan_hold(active_key_state_t *slot, multi_tap_t *slot_multi_tap, keypos_t key_pos, uint16_t elapsed) {
    key_runtime_slot_step_pending_multi_tap_scan_resolution_t resolution = key_runtime_slot_step_pending_multi_tap_scan_resolve(slot, slot_multi_tap, elapsed);
    key_runtime_slot_result_t                                result      = {0};

    if (resolution.outcome == KEY_RUNTIME_SLOT_STEP_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE) {
        return result;
    }

    key_runtime_slot_reset_pending_multi_tap(slot);

    if (!(resolution.release_layer_before_action || key_runtime_slot_result_request_has_effect(resolution.effect_request))) {
        return result;
    }

    result.handled = true;
    if (resolution.release_layer_before_action) {
        key_runtime_slot_result_push_layer_release(&result, key_pos);
    }
    key_runtime_slot_result_push_request_if_present(&result, key_pos, resolution.effect_request);
    return result;
}

static key_runtime_slot_result_t key_runtime_slot_step_pending_multi_tap_scan(active_key_state_t *slot) {
    key_runtime_slot_result_t result = {0};
    keypos_t                  key_pos;

    if (!slot || !key_runtime_slot_has_pending_multi_tap(slot)) {
        return result;
    }

    key_pos = key_runtime_slot_active(slot) ? slot->key_pos : slot->pending_multi_tap.key_pos;

    if (key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_active(slot)) {
        multi_tap_t *slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
        if (!slot_multi_tap) {
            return result;
        }

        uint16_t elapsed = timer_elapsed(slot_multi_tap->timer);
        return key_runtime_slot_step_pending_multi_tap_scan_hold(slot, slot_multi_tap, key_pos, elapsed);
    }

    if (key_runtime_slot_pending_multi_tap_expired(slot)) {
        key_runtime_slot_pending_multi_tap_flush_t flush = key_runtime_slot_take_pending_multi_tap_flush(slot);
        result.handled = true;
        key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    }

    return result;
}

static key_runtime_slot_result_t key_runtime_slot_step_interrupt(active_key_state_t *slot, keypos_t other_key_pos) {
    key_runtime_slot_result_t         result  = {0};
    key_runtime_slot_effect_request_t request = key_runtime_slot_interrupt_on_other_press(slot, other_key_pos);

    if (!key_runtime_slot_result_request_has_effect(request)) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_request_if_present(&result, slot ? slot->key_pos : (keypos_t){0}, request);
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

key_runtime_slot_result_t key_runtime_slot_step(active_key_state_t *slot, key_runtime_slot_event_t event) {
    switch (event.kind) {
        case KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS:
            return key_runtime_slot_step_handled_press(
                slot,
                event.data.handled_press.keycode,
                event.data.handled_press.key_pos,
                event.data.handled_press.key,
                event.data.handled_press.active_held_action_survives_flush);
        case KEY_RUNTIME_SLOT_EVENT_HANDLED_RELEASE:
            return key_runtime_slot_step_handled_release(
                slot,
                event.data.handled_release.keycode,
                event.data.handled_release.key_pos,
                event.data.handled_release.behavior);
        case KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN:
            return key_runtime_slot_step_active_scan(slot);
        case KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN:
            return key_runtime_slot_step_pending_multi_tap_scan(slot);
        case KEY_RUNTIME_SLOT_EVENT_INTERRUPT:
            return key_runtime_slot_step_interrupt(slot, event.data.interrupt.other_key_pos);
        case KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_FLUSH:
            return key_runtime_slot_step_pending_multi_tap_flush(slot);
        case KEY_RUNTIME_SLOT_EVENT_NONE:
        default:
            return (key_runtime_slot_result_t){0};
    }
}
