#include "projection.h"

#include "feedback_projection.h"
#include "pd_projection.h"
#include "pending_release_queue.h"
#include "trace.h"

#include <string.h>

#include "../../../action/action_dispatch.h"
#include "../../../pointing/policy/pointer_layer_policy.h"
#include "../../../state/ownership/keyboard_mod_ownership.h"
#include "../../../state/ownership/layer_ownership.h"
#include "../../../state/runtime/runtime_debug.h"
#include "../../ownership/held_action.h"
#include "../../ownership/held_repeat.h"
#include "../delayed_action.h"

void key_runtime_core_project_effect(const key_runtime_effect_t *effect) {
    if (!effect) {
        return;
    }

    switch (effect->kind) {
        case KEY_RUNTIME_EFFECT_DISPATCH_ACTION:
            noah_emit_action_tap_at(key_runtime_effect_dispatch_action_key_pos(effect), effect->data.dispatch_action.action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
            return;
        case KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER:
            key_runtime_core_pd_projection_preempt_held_action(effect->data.held_action.action);
            key_runtime_core_observe_held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
            held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
            return;
        case KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER:
            held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
            key_runtime_core_observe_held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
            return;
        case KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY:
            key_runtime_core_release_owned_state_by_key(effect->data.key_pos);
            held_action_release_owned_by_key(effect->data.key_pos);
            return;
        case KEY_RUNTIME_EFFECT_REPEAT_START:
            key_runtime_core_observe_repeat_start(effect->data.repeat.key_pos, effect->data.repeat.action, effect->data.repeat.repeat_hz);
            held_repeat_start(effect->data.repeat.key_pos, effect->data.repeat.action, effect->data.repeat.repeat_hz);
            return;
        case KEY_RUNTIME_EFFECT_LAYER_PRESS:
            layer_ownership_momentary_press(effect->data.layer_press.key_pos, effect->data.layer_press.layer);
            return;
        case KEY_RUNTIME_EFFECT_LAYER_RELEASE:
            layer_ownership_momentary_release(effect->data.key_pos);
            return;
        case KEY_RUNTIME_EFFECT_FEEDBACK_PULSE:
            key_runtime_core_feedback_projection_project_pulse(effect->data.feedback_pulse.key_pos, (key_feedback_pulse_kind_t)effect->data.feedback_pulse.kind, effect->data.feedback_pulse.tap_branch);
            return;
        case KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP:
            key_runtime_core_pd_projection_project_lock_tap(effect->data.pd_mode_lock_tap.pd_mode, effect->data.pd_mode_lock_tap.key_pos);
            return;
        case KEY_RUNTIME_EFFECT_DELAYED_ACTION:
            for (uint8_t repeat = 0, repeat_count = (uint8_t)(effect->data.delayed_action.repeat_count & KEY_RUNTIME_DELAYED_ACTION_REPEAT_COUNT_MASK); repeat < repeat_count; repeat++) {
                keypos_t             key_pos             = key_runtime_effect_delayed_action_key_pos(effect);
                bool                 defer_until_release = (effect->data.delayed_action.repeat_count & KEY_RUNTIME_DELAYED_ACTION_FLAG_DEFER_UNTIL_RELEASE) != 0u;
                bool                 tap_commit_feedback = (effect->data.delayed_action.repeat_count & KEY_RUNTIME_DELAYED_ACTION_FLAG_TAP_COMMIT_FEEDBACK) != 0u && repeat == (uint8_t)(repeat_count - 1u);
                const press_token_t *token               = defer_until_release ? key_runtime_core_press_token_at(key_pos) : NULL;
                uint16_t             owner               = token && token->active ? token->token_id : 0u;

                if (defer_until_release && key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, effect->data.delayed_action.action, effect->data.delayed_action.mods, tap_commit_feedback, owner)) {
                    continue;
                }

                dispatch_delayed_action_at(key_pos, effect->data.delayed_action.action, effect->data.delayed_action.mods);
                if (defer_until_release && tap_commit_feedback) {
                    key_runtime_core_project_effect(&(key_runtime_effect_t){
                        .kind = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
                        .data.feedback_pulse =
                            {
                                .key_pos = key_pos,
                                .kind    = KEY_FEEDBACK_PULSE_TAP_COMMITTED,
                            },
                    });
                }
            }
            return;
        case KEY_RUNTIME_EFFECT_NONE:
        default:
            return;
    }
}

void key_runtime_core_project_pending_release_dispatch(const pending_release_t *pending) {
    if (!pending) {
        return;
    }

    dispatch_delayed_action_at(pending->key_pos, pending->action, pending->mods);
    if (pending->tap_commit_feedback) {
        key_runtime_core_project_effect(&(key_runtime_effect_t){
            .kind = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
            .data.feedback_pulse =
                {
                    .key_pos = pending->key_pos,
                    .kind    = KEY_FEEDBACK_PULSE_TAP_COMMITTED,
                },
        });
    }
}

static uint8_t key_runtime_core_projection_refcount_mask(const uint8_t *refcounts, uint8_t count) {
    uint8_t mask = 0;

    if (!refcounts) {
        return 0;
    }

    for (uint8_t index = 0; index < count && index < 8u; index++) {
        if (refcounts[index] != 0u) {
            mask |= (uint8_t)(1u << index);
        }
    }

    return mask;
}

projection_snapshot_t key_runtime_core_projection_snapshot_capture(void) {
    projection_snapshot_t                   snapshot = {0};
    layer_ownership_debug_snapshot_t        layer_snapshot;
    keyboard_mod_ownership_debug_snapshot_t mod_snapshot;
    pointer_layer_policy_debug_snapshot_t   pointer_snapshot;
    key_runtime_core_state_t               *state = key_runtime_core_state();

    memset(&layer_snapshot, 0, sizeof(layer_snapshot));
    memset(&mod_snapshot, 0, sizeof(mod_snapshot));
    memset(&pointer_snapshot, 0, sizeof(pointer_snapshot));

    layer_ownership_debug_snapshot(&layer_snapshot);
    keyboard_mod_ownership_debug_snapshot(&mod_snapshot);
    pointer_layer_policy_debug_snapshot(layer_state, &pointer_snapshot);

    snapshot.layer_state                          = layer_state;
    snapshot.locked_layer_mask                    = layer_snapshot.locked_mask;
    snapshot.keyboard_mod_state                   = mod_snapshot.live_state;
    snapshot.keyboard_managed_mod_mask            = key_runtime_core_projection_refcount_mask(mod_snapshot.managed_refcounts, KEYBOARD_MOD_OWNERSHIP_MOD_COUNT);
    snapshot.keyboard_physical_mod_mask           = key_runtime_core_projection_refcount_mask(mod_snapshot.physical_refcounts, KEYBOARD_MOD_OWNERSHIP_MOD_COUNT);
    snapshot.pd_mode_local_active                 = pd_mode_local_active_snapshot();
    snapshot.pd_mode_local_locked                 = pd_mode_local_locked_snapshot();
    snapshot.pd_mode_display_active               = pd_mode_display_active_snapshot();
    snapshot.pd_mode_display_locked               = pd_mode_display_locked_snapshot();
    snapshot.pointer_anchor_active                = pointer_snapshot.auto_mouse_anchored;
    snapshot.pointer_pd_mode_anchor_active        = pointer_snapshot.pd_mode_anchor_active;
    snapshot.pointer_prefers_typing_layer         = pointer_snapshot.prefers_typing_layer;
    snapshot.pointer_toggle_enabled               = pointer_snapshot.auto_mouse_toggle_enabled;
    snapshot.pointer_sniping_layer_active         = pointer_snapshot.sniping_layer_active;
    snapshot.pointer_key_tracker                  = pointer_snapshot.auto_mouse_key_tracker;
    snapshot.pointer_layer                        = pointer_snapshot.auto_mouse_layer;
    snapshot.active_slot_count                    = noah_runtime_debug_active_slot_count();
    snapshot.pending_multi_tap_slot_count         = noah_runtime_debug_pending_multi_tap_slot_count();
    snapshot.deferred_release_count               = noah_runtime_debug_deferred_release_count();
    snapshot.deferred_release_blocker_count       = noah_runtime_debug_deferred_release_blocker_count();
    snapshot.deferred_release_timed_blocker_count = noah_runtime_debug_deferred_release_timed_blocker_count();

    if (state) {
        snapshot.core_shadow_layer_state                   = state->shadow_projection.layer_state;
        snapshot.core_shadow_locked_layer_mask             = state->shadow_projection.locked_layer_mask;
        snapshot.core_shadow_keyboard_mod_state            = state->shadow_projection.keyboard_mod_state;
        snapshot.core_shadow_keyboard_managed_mod_mask     = state->shadow_projection.keyboard_managed_mod_mask;
        snapshot.core_shadow_keyboard_physical_mod_mask    = state->shadow_projection.keyboard_physical_mod_mask;
        snapshot.core_shadow_pd_mode_local_active          = state->shadow_projection.pd_mode_local_active;
        snapshot.core_shadow_pd_mode_local_locked          = state->shadow_projection.pd_mode_local_locked;
        snapshot.core_shadow_pointer_anchor_active         = state->shadow_projection.pointer_anchor_active;
        snapshot.core_shadow_pointer_pd_mode_anchor_active = state->shadow_projection.pointer_pd_mode_anchor_active;
        snapshot.core_shadow_pointer_prefers_typing_layer  = state->shadow_projection.pointer_prefers_typing_layer;
        snapshot.core_shadow_pointer_toggle_enabled        = state->shadow_projection.pointer_toggle_enabled;
        snapshot.core_press_token_count                    = state->press_token_count;
        snapshot.core_tap_series_count                     = state->tap_series_count;
        snapshot.core_lease_count                          = state->lease_count;
        snapshot.core_pending_release_count                = state->pending_release_count;
        snapshot.core_deferred_release_blocker_count       = key_runtime_core_deferred_release_blocker_count();
        snapshot.core_deferred_release_timed_blocker_count = key_runtime_core_deferred_release_timed_blocker_count();
        snapshot.core_persistent_intent_count              = state->persistent_intent_count;
        snapshot.core_release_keycode_mismatch_count       = state->release_keycode_mismatch_count;
        snapshot.core_orphan_release_count                 = state->orphan_release_count;
        snapshot.core_cancelled_press_count                = state->cancelled_press_count;
    }

    return snapshot;
}

bool key_runtime_core_projection_snapshot_equal(const projection_snapshot_t *lhs, const projection_snapshot_t *rhs) {
    if (!(lhs && rhs)) {
        return lhs == rhs;
    }

    return lhs->layer_state == rhs->layer_state && lhs->locked_layer_mask == rhs->locked_layer_mask && lhs->keyboard_mod_state.real == rhs->keyboard_mod_state.real && lhs->keyboard_mod_state.weak == rhs->keyboard_mod_state.weak && lhs->keyboard_mod_state.oneshot == rhs->keyboard_mod_state.oneshot && lhs->keyboard_mod_state.oneshot_locked == rhs->keyboard_mod_state.oneshot_locked && lhs->keyboard_managed_mod_mask == rhs->keyboard_managed_mod_mask && lhs->keyboard_physical_mod_mask == rhs->keyboard_physical_mod_mask && lhs->pd_mode_local_active == rhs->pd_mode_local_active && lhs->pd_mode_local_locked == rhs->pd_mode_local_locked && lhs->pd_mode_display_active == rhs->pd_mode_display_active && lhs->pd_mode_display_locked == rhs->pd_mode_display_locked && lhs->pointer_anchor_active == rhs->pointer_anchor_active && lhs->pointer_pd_mode_anchor_active == rhs->pointer_pd_mode_anchor_active && lhs->pointer_prefers_typing_layer == rhs->pointer_prefers_typing_layer &&
           lhs->pointer_toggle_enabled == rhs->pointer_toggle_enabled && lhs->pointer_sniping_layer_active == rhs->pointer_sniping_layer_active && lhs->pointer_key_tracker == rhs->pointer_key_tracker && lhs->pointer_layer == rhs->pointer_layer && lhs->active_slot_count == rhs->active_slot_count && lhs->pending_multi_tap_slot_count == rhs->pending_multi_tap_slot_count && lhs->deferred_release_count == rhs->deferred_release_count && lhs->deferred_release_blocker_count == rhs->deferred_release_blocker_count && lhs->deferred_release_timed_blocker_count == rhs->deferred_release_timed_blocker_count && lhs->core_shadow_layer_state == rhs->core_shadow_layer_state && lhs->core_shadow_locked_layer_mask == rhs->core_shadow_locked_layer_mask && lhs->core_shadow_keyboard_mod_state.real == rhs->core_shadow_keyboard_mod_state.real && lhs->core_shadow_keyboard_mod_state.weak == rhs->core_shadow_keyboard_mod_state.weak &&
           lhs->core_shadow_keyboard_mod_state.oneshot == rhs->core_shadow_keyboard_mod_state.oneshot && lhs->core_shadow_keyboard_mod_state.oneshot_locked == rhs->core_shadow_keyboard_mod_state.oneshot_locked && lhs->core_shadow_keyboard_managed_mod_mask == rhs->core_shadow_keyboard_managed_mod_mask && lhs->core_shadow_keyboard_physical_mod_mask == rhs->core_shadow_keyboard_physical_mod_mask && lhs->core_shadow_pd_mode_local_active == rhs->core_shadow_pd_mode_local_active && lhs->core_shadow_pd_mode_local_locked == rhs->core_shadow_pd_mode_local_locked && lhs->core_shadow_pointer_anchor_active == rhs->core_shadow_pointer_anchor_active && lhs->core_shadow_pointer_pd_mode_anchor_active == rhs->core_shadow_pointer_pd_mode_anchor_active && lhs->core_shadow_pointer_prefers_typing_layer == rhs->core_shadow_pointer_prefers_typing_layer && lhs->core_shadow_pointer_toggle_enabled == rhs->core_shadow_pointer_toggle_enabled && lhs->core_press_token_count == rhs->core_press_token_count &&
           lhs->core_tap_series_count == rhs->core_tap_series_count && lhs->core_lease_count == rhs->core_lease_count && lhs->core_pending_release_count == rhs->core_pending_release_count && lhs->core_deferred_release_blocker_count == rhs->core_deferred_release_blocker_count && lhs->core_deferred_release_timed_blocker_count == rhs->core_deferred_release_timed_blocker_count && lhs->core_persistent_intent_count == rhs->core_persistent_intent_count && lhs->core_release_keycode_mismatch_count == rhs->core_release_keycode_mismatch_count && lhs->core_orphan_release_count == rhs->core_orphan_release_count && lhs->core_cancelled_press_count == rhs->core_cancelled_press_count;
}

#if defined(NOAH_RUNTIME_TRACE_ENABLE)

void key_runtime_core_trace_capture_projection(void) {
    projection_snapshot_t snapshot = key_runtime_core_projection_snapshot_capture();

    key_runtime_core_trace_record_projection_snapshot(&snapshot);
}

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
