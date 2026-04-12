// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Pending Multi-Tap
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_pending_multi_tap.h"

#include "key_runtime_slot_policy.h"
#include "key_runtime_slot_result_internal.h"

#include "../action/action_dispatch.h"
#include "../action/action_lifecycle.h"

static void key_runtime_slot_pending_multi_tap_clear_active_state(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    multi_tap_t pending_multi_tap = slot->pending_multi_tap;
    *slot                         = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
    slot->pending_multi_tap       = pending_multi_tap;
}

static bool key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(const active_key_state_t *slot, hold_behavior_t hold, uint16_t action, uint8_t repeat_count, uint16_t elapsed) {
    if (!slot || !hold.present || repeat_count != 1 || elapsed < slot->timing.tap_hold_term) {
        return false;
    }

    if (hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE || action != hold.action) {
        return false;
    }

    return noah_action_hold_kind(hold.action) != NOAH_ACTION_HOLD_KIND_PRESS_ONLY;
}

typedef enum {
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
} key_runtime_slot_pending_multi_tap_release_outcome_t;

typedef struct {
    active_key_state_t   *slot;
    uint16_t              keycode;
    handled_key_view_t    key;
    uint16_t              elapsed;
    keypos_t              key_pos;
    delayed_action_mods_t mods;
    hold_behavior_t       hold;
    hold_behavior_t       long_hold;
    uint16_t              action;
    uint8_t               repeat_count;
    bool                  matched;
} key_runtime_slot_pending_multi_tap_release_context_t;

typedef struct {
    key_runtime_slot_pending_multi_tap_release_outcome_t outcome;
    uint16_t                                             action;
    uint8_t                                              repeat_count;
    delayed_action_mods_t                                mods;
} key_runtime_slot_pending_multi_tap_release_resolution_t;

static key_runtime_slot_pending_multi_tap_release_context_t key_runtime_slot_pending_multi_tap_release_context(active_key_state_t *slot, uint16_t keycode, handled_key_view_t key, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_release_context_t context = {
        .slot    = slot,
        .keycode = keycode,
        .key     = key,
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

    context.key_pos   = slot->owner.key_pos;
    context.mods      = delayed_action_mods_from_multi_tap(slot_multi_tap);
    context.hold      = slot_multi_tap->hold;
    context.long_hold = slot_multi_tap->long_hold;
    context.action    = key_runtime_slot_resolve_pending_multi_tap_hold(slot, keycode, &context.repeat_count);
    context.matched   = true;

    if (!context.hold.present && hold_sends_on_release(context.long_hold) && elapsed >= slot->timing.longer_hold_term) {
        context.action = context.long_hold.action;
    } else if (hold_sends_on_release(context.hold) && context.repeat_count == 1 && context.action == context.hold.action) {
        context.action = key_runtime_slot_policy_select_release_hold_action(elapsed, context.hold.action, context.long_hold, slot->timing.longer_hold_term);
    }

    return context;
}

static bool key_runtime_slot_pending_multi_tap_release_preserves_chain(const key_runtime_slot_pending_multi_tap_release_context_t *context) {
    return context && context->action == KC_NO && context->repeat_count == 0 && key_runtime_slot_has_pending_multi_tap(context->slot);
}

static key_runtime_slot_pending_multi_tap_release_resolution_t key_runtime_slot_pending_multi_tap_release_resolve(const key_runtime_slot_pending_multi_tap_release_context_t *context) {
    if (!(context && context->matched)) {
        return (key_runtime_slot_pending_multi_tap_release_resolution_t){0};
    }

    if (key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(context->slot, context->hold, context->action, context->repeat_count, context->elapsed)) {
        return (key_runtime_slot_pending_multi_tap_release_resolution_t){
            .outcome = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
            .action  = context->action,
        };
    }

    if (key_runtime_slot_pending_multi_tap_release_preserves_chain(context)) {
        return (key_runtime_slot_pending_multi_tap_release_resolution_t){
            .outcome = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
        };
    }

    return (key_runtime_slot_pending_multi_tap_release_resolution_t){
        .outcome      = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
        .action       = context->action,
        .repeat_count = context->repeat_count,
        .mods         = context->mods,
    };
}

key_runtime_slot_result_t key_runtime_slot_pending_multi_tap_handle_release(active_key_state_t *slot, uint16_t keycode, handled_key_view_t key, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_release_context_t    context    = key_runtime_slot_pending_multi_tap_release_context(slot, keycode, key, elapsed);
    key_runtime_slot_pending_multi_tap_release_resolution_t resolution = key_runtime_slot_pending_multi_tap_release_resolve(&context);
    key_runtime_slot_result_t                               result     = {0};

    if (!context.matched) {
        return result;
    }

    result.handled = true;

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

    if (handled_key_is_momentary_layer(context.key)) {
        key_runtime_slot_result_push_layer_release(&result, context.key_pos);
    }

    if (resolution.outcome == KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN) {
        key_runtime_slot_pending_multi_tap_clear_active_state(slot);
    } else {
        key_runtime_slot_reset(slot);
    }

    return result;
}

static bool key_runtime_slot_pending_multi_tap_hold_elapsed(const multi_tap_t *multi_tap_state, uint16_t elapsed) {
    return multi_tap_state->pending_hold && hold_fires_at_threshold(multi_tap_state->hold) && elapsed >= multi_tap_state->tap_hold_term;
}

static bool key_runtime_slot_pending_multi_tap_scan_releases_layer_before_action(const active_key_state_t *slot, uint16_t action) {
    return slot && is_layer_key(slot->owner.keycode) && action_dispatch_is_layer_lock(action);
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

static key_runtime_slot_pending_multi_tap_scan_resolution_t key_runtime_slot_pending_multi_tap_scan_resolve(active_key_state_t *slot, multi_tap_t *slot_multi_tap, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = {0};

    if (!(slot && slot_multi_tap)) {
        return resolution;
    }

    slot->binding.long_hold = slot_multi_tap->long_hold;

    if (hold_fires_at_threshold(slot_multi_tap->long_hold) && elapsed >= slot->timing.longer_hold_term) {
        resolution.outcome                     = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD;
        resolution.release_layer_before_action = key_runtime_slot_pending_multi_tap_scan_releases_layer_before_action(slot, slot_multi_tap->long_hold.action);
        resolution.effect_builder              = key_runtime_slot_policy_promote_to_long_hold(slot, slot->binding.long_hold, true);
        return resolution;
    }

    if (!key_runtime_slot_pending_multi_tap_hold_elapsed(slot_multi_tap, elapsed)) {
        return resolution;
    }

    resolution.outcome                     = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD;
    resolution.release_layer_before_action = key_runtime_slot_pending_multi_tap_scan_releases_layer_before_action(slot, slot_multi_tap->hold.action);
    resolution.effect_builder              = key_runtime_slot_policy_fire_hold_at_threshold(slot, slot_multi_tap->hold, slot->binding.long_hold, true);
    return resolution;
}

static key_runtime_slot_result_t key_runtime_slot_pending_multi_tap_scan_hold(active_key_state_t *slot, multi_tap_t *slot_multi_tap, keypos_t key_pos, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = key_runtime_slot_pending_multi_tap_scan_resolve(slot, slot_multi_tap, elapsed);
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
        multi_tap_t *slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
        if (!slot_multi_tap) {
            return result;
        }

        return key_runtime_slot_pending_multi_tap_scan_hold(slot, slot_multi_tap, key_pos, timer_elapsed(slot_multi_tap->timer));
    }

    if (key_runtime_slot_pending_multi_tap_expired(slot)) {
        key_runtime_slot_pending_multi_tap_flush_t flush = key_runtime_slot_take_pending_multi_tap_flush(slot);
        result.handled                                   = true;
        key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    }

    return result;
}
