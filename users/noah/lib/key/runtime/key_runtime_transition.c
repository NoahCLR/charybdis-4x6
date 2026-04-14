// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "key_runtime_transition.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "key_runtime_feedback.h"
#include "key_runtime_admission.h"
#include "key_runtime_index.h"
#include "key_runtime_index_internal.h"
#include "slot/key_runtime_slot_step.h"
#include "key_runtime_internal.h"
#include "key_runtime_trace.h"
#include "../../action/action_dispatch.h"
#include "../../action/action_lifecycle.h"
#include "../../pointing/defs/pd_modes.h"
#include "../../state/ownership/layer_ownership.h"
#include "../../state/runtime/split_runtime_sync.h"
#include "../ownership/held_action.h"
#include "../ownership/held_repeat.h"

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan) {
    *plan = (key_runtime_transition_plan_t){0};
}

static void key_runtime_transition_log_plan_overflow(key_runtime_effect_kind_t kind, uint8_t capacity) {
#ifdef CONSOLE_ENABLE
    uprintf("Key runtime transition plan overflow dropping effect kind %u after %u queued effects\n", (unsigned int)kind, (unsigned int)capacity);
#else
    (void)kind;
    (void)capacity;
#endif
}

static void key_runtime_transition_fail_host_overflow(key_runtime_effect_kind_t kind, uint8_t capacity) {
#ifdef NOAH_HOST_TEST_ENV
    noah_host_test_fail_runtime_overflow("key runtime transition plan", (unsigned int)kind, (unsigned int)capacity);
#else
    (void)kind;
    (void)capacity;
#endif
}

static void key_runtime_transition_plan_push(key_runtime_transition_plan_t *plan, key_runtime_effect_t effect) {
    if (plan->count < ARRAY_SIZE(plan->items)) {
        plan->items[plan->count++] = effect;
        return;
    }

    if (!plan->overflowed) {
        plan->overflowed = true;
        key_runtime_transition_log_plan_overflow(effect.kind, ARRAY_SIZE(plan->items));
        key_runtime_transition_fail_host_overflow(effect.kind, ARRAY_SIZE(plan->items));
    }
}

static void key_runtime_transition_apply_slot_result(const key_runtime_slot_result_t *result, key_runtime_transition_plan_t *plan) {
    if (!result || !result->handled) {
        return;
    }

    for (uint8_t index = 0; index < result->count; index++) {
        key_runtime_transition_plan_push(plan, result->items[index]);
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
        const key_runtime_effect_t *effect = &plan->items[i];
        key_runtime_trace_effect_execute(i, effect);

        switch (effect->kind) {
            case KEY_RUNTIME_EFFECT_DISPATCH_ACTION:
                noah_emit_action_tap(effect->data.action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
                break;
            case KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER:
                held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
                break;
            case KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER:
                held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
                break;
            case KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY:
                held_action_release_owned_by_key(effect->data.key_pos);
                break;
            case KEY_RUNTIME_EFFECT_REPEAT_START:
                held_repeat_start(effect->data.repeat.key_pos, effect->data.repeat.action, effect->data.repeat.repeat_hz);
                break;
            case KEY_RUNTIME_EFFECT_LAYER_PRESS:
                layer_ownership_momentary_press(effect->data.layer_press.key_pos, effect->data.layer_press.layer);
                break;
            case KEY_RUNTIME_EFFECT_LAYER_RELEASE:
                layer_ownership_momentary_release(effect->data.key_pos);
                break;
            case KEY_RUNTIME_EFFECT_FEEDBACK_PULSE:
                key_feedback_pulse_arm(effect->data.long_hold_level);
                break;
            case KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP:
                if (pd_mode_toggle_lock_state(effect->data.pd_mode)) {
                    split_runtime_sync();
                }
                break;
            case KEY_RUNTIME_EFFECT_DELAYED_ACTION:
                for (uint8_t repeat = 0; repeat < effect->data.delayed_action.repeat_count; repeat++) {
                    dispatch_delayed_action(effect->data.delayed_action.action, effect->data.delayed_action.mods);
                }
                break;
            case KEY_RUNTIME_EFFECT_NONE:
            default:
                break;
        }
    }
}

bool key_runtime_transition_handled_key_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, bool active_held_action_survives_flush, key_runtime_transition_plan_t *plan) {
    return key_runtime_transition_apply_slot_step(slot,
                                                  (key_runtime_slot_event_t){
                                                      .kind = KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS,
                                                      .data.handled_press =
                                                          {
                                                              .keycode                           = keycode,
                                                              .key_pos                           = key_pos,
                                                              .key                               = resolution,
                                                              .active_held_action_survives_flush = active_held_action_survives_flush,
                                                          },
                                                  },
                                                  plan);
}

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan) {
    active_key_state_t *slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t             pending_count = key_runtime_index_snapshot_pending_multi_tap_slots(slots, ARRAY_SIZE(slots));

    for (uint8_t index = 0; index < pending_count; index++) {
        key_runtime_transition_apply_slot_step(slots[index], (key_runtime_slot_event_t){.kind = KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_FLUSH}, plan);
    }
}

void key_runtime_transition_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    active_key_state_t *slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t             active_count = key_runtime_index_snapshot_active_slots(slots, ARRAY_SIZE(slots));

    for (uint8_t index = 0; index < active_count; index++) {
        key_runtime_transition_apply_slot_step(slots[index],
                                               (key_runtime_slot_event_t){
                                                   .kind = KEY_RUNTIME_SLOT_EVENT_INTERRUPT,
                                                   .data.interrupt =
                                                       {
                                                           .other_key_pos = key_pos,
                                                       },
                                               },
                                               plan);
    }
}

void key_runtime_transition_interrupt_active_key_on_other_press(key_runtime_transition_plan_t *plan) {
    key_runtime_transition_interrupt_active_keys_on_other_press((keypos_t){0xFF, 0xFF}, plan);
}

static bool key_runtime_transition_process_active_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    active_key_state_t      *slot            = key_runtime_find_slot_by_position(record->event.key);
    uint16_t                 release_keycode = keycode;
    handled_key_resolution_t release_key     = resolution;

    if (key_runtime_slot_active(slot) && slot->owner.keycode != keycode) {
        release_keycode = slot->owner.keycode;
        release_key     = handled_key_lookup(release_keycode);
    }

    key_runtime_transition_apply_slot_step(slot,
                                           (key_runtime_slot_event_t){
                                               .kind = KEY_RUNTIME_SLOT_EVENT_HANDLED_RELEASE,
                                               .data.handled_release =
                                                   {
                                                       .keycode = release_keycode,
                                                       .key_pos = record->event.key,
                                                       .key     = release_key,
                                                   },
                                           },
                                           plan);
    return true;
}

bool key_runtime_transition_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    return key_runtime_transition_process_active_key_release(keycode, record, resolution, plan);
}

void key_runtime_transition_scan(key_runtime_transition_plan_t *plan) {
    active_key_state_t *active_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    active_key_state_t *pending_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t             active_count  = key_runtime_index_snapshot_active_slots(active_slots, ARRAY_SIZE(active_slots));
    uint8_t             pending_count = key_runtime_index_snapshot_pending_multi_tap_slots(pending_slots, ARRAY_SIZE(pending_slots));

    for (uint8_t index = 0; index < active_count; index++) {
        key_runtime_transition_apply_slot_step(active_slots[index], (key_runtime_slot_event_t){.kind = KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN}, plan);
    }

    for (uint8_t index = 0; index < pending_count; index++) {
        key_runtime_transition_apply_slot_step(pending_slots[index], (key_runtime_slot_event_t){.kind = KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN}, plan);
    }
}
