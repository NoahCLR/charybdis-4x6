// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_transition.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "key_runtime_feedback.h"
#include "key_runtime_admission.h"
#include "key_runtime_slot_effect.h"
#include "key_runtime_slot_step.h"
#include "key_runtime_state.h"
#include "key_runtime_trace.h"
#include "../action/action_dispatch.h"
#include "../action/action_lifecycle.h"
#include "../pointing/pd_modes.h"
#include "../state/layer_ownership.h"
#include "../state/split_runtime_sync.h"
#include "held_action.h"

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan) {
    *plan = (key_runtime_transition_plan_t){0};
}

static void key_runtime_transition_log_plan_overflow(key_runtime_transition_effect_kind_t kind, uint8_t capacity) {
#ifdef CONSOLE_ENABLE
    uprintf("Key runtime transition plan overflow dropping effect kind %u after %u queued effects\n", (unsigned int)kind, (unsigned int)capacity);
#else
    (void)kind;
    (void)capacity;
#endif
}

static void key_runtime_transition_plan_push(key_runtime_transition_plan_t *plan, key_runtime_transition_effect_t effect) {
    if (plan->count < ARRAY_SIZE(plan->effects)) {
        plan->effects[plan->count++] = effect;
        return;
    }

    if (!plan->overflowed) {
        plan->overflowed = true;
        key_runtime_transition_log_plan_overflow(effect.kind, ARRAY_SIZE(plan->effects));
    }
}

static void key_runtime_transition_plan_dispatch_action(key_runtime_transition_plan_t *plan, uint16_t action) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind        = KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION,
                                               .data.action = action,
                                           });
}

static void key_runtime_transition_plan_held_register(key_runtime_transition_plan_t *plan, keypos_t key_pos, uint16_t action) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind = KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_REGISTER,
                                               .data.held_action =
                                                   {
                                                       .key_pos = key_pos,
                                                       .action  = action,
                                                   },
                                           });
}

static void key_runtime_transition_plan_held_unregister(key_runtime_transition_plan_t *plan, keypos_t key_pos, uint16_t action) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind = KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_UNREGISTER,
                                               .data.held_action =
                                                   {
                                                       .key_pos = key_pos,
                                                       .action  = action,
                                                   },
                                           });
}

static void key_runtime_transition_plan_release_owned_state_by_key(key_runtime_transition_plan_t *plan, keypos_t key_pos) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind         = KEY_RUNTIME_TRANSITION_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                               .data.key_pos = key_pos,
                                           });
}

static void key_runtime_transition_plan_repeat_start(key_runtime_transition_plan_t *plan, keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind = KEY_RUNTIME_TRANSITION_EFFECT_REPEAT_START,
                                               .data.repeat =
                                                   {
                                                       .key_pos   = key_pos,
                                                       .action    = action,
                                                       .repeat_hz = repeat_hz,
                                                   },
                                           });
}

static void key_runtime_transition_plan_layer_press(key_runtime_transition_plan_t *plan, keypos_t key_pos, uint8_t layer) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind = KEY_RUNTIME_TRANSITION_EFFECT_LAYER_PRESS,
                                               .data.layer_press =
                                                   {
                                                       .key_pos = key_pos,
                                                       .layer   = layer,
                                                   },
                                           });
}

static void key_runtime_transition_plan_layer_release(key_runtime_transition_plan_t *plan, keypos_t key_pos) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind         = KEY_RUNTIME_TRANSITION_EFFECT_LAYER_RELEASE,
                                               .data.key_pos = key_pos,
                                           });
}

static void key_runtime_transition_plan_feedback_pulse(key_runtime_transition_plan_t *plan, bool long_hold_level) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind                 = KEY_RUNTIME_TRANSITION_EFFECT_FEEDBACK_PULSE,
                                               .data.long_hold_level = long_hold_level,
                                           });
}

static void key_runtime_transition_plan_pd_mode_lock_tap(key_runtime_transition_plan_t *plan, pd_mode_mask_t mode) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind         = KEY_RUNTIME_TRANSITION_EFFECT_PD_MODE_LOCK_TAP,
                                               .data.pd_mode = mode,
                                           });
}

static void key_runtime_transition_plan_delayed_action(key_runtime_transition_plan_t *plan, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
    if (action == KC_NO || repeat_count == 0) {
        return;
    }

    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind = KEY_RUNTIME_TRANSITION_EFFECT_DELAYED_ACTION,
                                               .data.delayed_action =
                                                   {
                                                       .action       = action,
                                                       .mods         = mods,
                                                       .repeat_count = repeat_count,
                                                   },
                                           });
}

static bool key_runtime_transition_slot_effect_request_has_effect(key_runtime_slot_effect_request_t request) {
    return request.kind != KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE || request.release_owned_state || request.feedback_pulse;
}

static void key_runtime_transition_plan_slot_effect_request(key_runtime_transition_plan_t *plan, keypos_t key_pos, key_runtime_slot_effect_request_t request) {
    if (request.release_owned_state) {
        key_runtime_transition_plan_release_owned_state_by_key(plan, key_pos);
    }

    switch (request.kind) {
        case KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION:
            key_runtime_transition_plan_dispatch_action(plan, request.action);
            break;
        case KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER:
            key_runtime_transition_plan_held_register(plan, key_pos, request.action);
            break;
        case KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER:
            key_runtime_transition_plan_held_unregister(plan, key_pos, request.action);
            break;
        case KEY_RUNTIME_SLOT_EFFECT_REQUEST_REPEAT_START:
            key_runtime_transition_plan_repeat_start(plan, key_pos, request.action, request.repeat_hz);
            break;
        case KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE:
        default:
            break;
    }

    if (request.feedback_pulse) {
        key_runtime_transition_plan_feedback_pulse(plan, request.feedback_long_hold_level);
    }
}

static void key_runtime_transition_plan_slot_effect_request_if_present(key_runtime_transition_plan_t *plan, keypos_t key_pos, key_runtime_slot_effect_request_t request) {
    if (!key_runtime_transition_slot_effect_request_has_effect(request)) {
        return;
    }

    key_runtime_transition_plan_slot_effect_request(plan, key_pos, request);
}

static void key_runtime_transition_apply_slot_result(const key_runtime_slot_result_t *result, key_runtime_transition_plan_t *plan) {
    if (!result || !result->handled) {
        return;
    }

    for (uint8_t index = 0; index < result->count; index++) {
        key_runtime_slot_result_effect_t effect = result->effects[index];

        switch (effect.kind) {
            case KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST:
                key_runtime_transition_plan_slot_effect_request_if_present(plan, effect.key_pos, effect.data.slot_effect_request);
                break;
            case KEY_RUNTIME_SLOT_RESULT_EFFECT_DELAYED_ACTION:
                key_runtime_transition_plan_delayed_action(plan, effect.data.delayed_action.action, effect.data.delayed_action.mods, effect.data.delayed_action.repeat_count);
                break;
            case KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_PRESS:
                key_runtime_transition_plan_layer_press(plan, effect.key_pos, effect.data.layer);
                break;
            case KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_RELEASE:
                key_runtime_transition_plan_layer_release(plan, effect.key_pos);
                break;
            case KEY_RUNTIME_SLOT_RESULT_EFFECT_PD_MODE_LOCK_TAP:
                key_runtime_transition_plan_pd_mode_lock_tap(plan, effect.data.pd_mode);
                break;
            case KEY_RUNTIME_SLOT_RESULT_EFFECT_NONE:
            default:
                break;
        }
    }
}

static bool key_runtime_transition_apply_slot_step(active_key_state_t *slot, key_runtime_slot_event_t event, key_runtime_transition_plan_t *plan) {
    key_runtime_slot_result_t result = key_runtime_slot_step(slot, event);

    if (!result.handled) {
        return false;
    }

    key_runtime_transition_apply_slot_result(&result, plan);
    return true;
}

void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan) {
    for (uint8_t i = 0; i < plan->count; i++) {
        const key_runtime_transition_effect_t *effect = &plan->effects[i];
        key_runtime_trace_effect_execute(i, effect);

        switch (effect->kind) {
            case KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION:
                action_dispatch(effect->data.action);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_REGISTER:
                held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_UNREGISTER:
                held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_RELEASE_OWNED_STATE_BY_KEY:
                held_action_release_owned_by_key(effect->data.key_pos);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_REPEAT_START:
                held_action_repeat_start(effect->data.repeat.key_pos, effect->data.repeat.action, effect->data.repeat.repeat_hz);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_LAYER_PRESS:
                layer_ownership_momentary_press(effect->data.layer_press.key_pos, effect->data.layer_press.layer);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_LAYER_RELEASE:
                layer_ownership_momentary_release(effect->data.key_pos);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_FEEDBACK_PULSE:
                key_feedback_pulse_arm(effect->data.long_hold_level);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_PD_MODE_LOCK_TAP:
                if (pd_mode_toggle_lock_state(effect->data.pd_mode)) {
                    split_runtime_sync();
                }
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_DELAYED_ACTION:
                for (uint8_t repeat = 0; repeat < effect->data.delayed_action.repeat_count; repeat++) {
                    dispatch_delayed_action(effect->data.delayed_action.action, effect->data.delayed_action.mods);
                }
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_NONE:
            default:
                break;
        }
    }
}

bool key_runtime_transition_handled_key_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush, key_runtime_transition_plan_t *plan) {
    return key_runtime_transition_apply_slot_step(slot,
                                                  (key_runtime_slot_event_t){
                                                      .kind              = KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS,
                                                      .data.handled_press = {
                                                          .keycode                          = keycode,
                                                          .key_pos                          = key_pos,
                                                          .key                              = key,
                                                          .active_held_action_survives_flush = active_held_action_survives_flush,
                                                      },
                                                  },
                                                  plan);
}

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan) {
    for (uint8_t index = 0; index < KEY_RUNTIME_SLOT_TABLE_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (!key_runtime_slot_has_pending_multi_tap(slot)) {
            continue;
        }

        key_runtime_transition_apply_slot_step(slot, (key_runtime_slot_event_t){.kind = KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_FLUSH}, plan);
    }
}

void key_runtime_transition_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    for (uint8_t index = 0; index < KEY_RUNTIME_SLOT_TABLE_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);
        if (!slot) {
            continue;
        }

        key_runtime_transition_apply_slot_step(slot,
                                               (key_runtime_slot_event_t){
                                                   .kind           = KEY_RUNTIME_SLOT_EVENT_INTERRUPT,
                                                   .data.interrupt = {
                                                       .other_key_pos = key_pos,
                                                   },
                                               },
                                               plan);
    }
}

void key_runtime_transition_interrupt_active_key_on_other_press(key_runtime_transition_plan_t *plan) {
    key_runtime_transition_interrupt_active_keys_on_other_press((keypos_t){0xFF, 0xFF}, plan);
}

static bool key_runtime_transition_process_active_key_release(uint16_t keycode, keyrecord_t *record, key_behavior_view_t behavior, key_runtime_transition_plan_t *plan) {
    key_runtime_transition_apply_slot_step(key_runtime_find_slot_by_position(record->event.key),
                                           (key_runtime_slot_event_t){
                                               .kind                = KEY_RUNTIME_SLOT_EVENT_HANDLED_RELEASE,
                                               .data.handled_release = {
                                                   .keycode  = keycode,
                                                   .key_pos  = record->event.key,
                                                   .behavior = behavior,
                                               },
                                           },
                                           plan);
    return true;
}

bool key_runtime_transition_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_view_t key, key_runtime_transition_plan_t *plan) {
    return key_runtime_transition_process_active_key_release(keycode, record, key.behavior, plan);
}

void key_runtime_transition_scan(key_runtime_transition_plan_t *plan) {
    for (uint8_t index = 0; index < KEY_RUNTIME_SLOT_TABLE_CAPACITY; index++) {
        key_runtime_transition_apply_slot_step(key_runtime_slot_at(index), (key_runtime_slot_event_t){.kind = KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN}, plan);
    }

    for (uint8_t index = 0; index < KEY_RUNTIME_SLOT_TABLE_CAPACITY; index++) {
        key_runtime_transition_apply_slot_step(key_runtime_slot_at(index), (key_runtime_slot_event_t){.kind = KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN}, plan);
    }
}
