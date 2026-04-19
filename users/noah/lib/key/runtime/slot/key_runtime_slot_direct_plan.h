// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Direct Plans
// ────────────────────────────────────────────────────────────────────────────
//
// Direct effect-plan surface for migrated slot reducers that should mutate
// slot state and return emitted effects without routing through
// `key_runtime_slot_result_t`.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../effects/key_runtime_effect_queue.h"
#include "key_runtime_slot_effect.h"

#define KEY_RUNTIME_SLOT_DIRECT_PLAN_CAPACITY 8u

typedef struct {
    bool handled;
    KEY_RUNTIME_EFFECT_QUEUE_FIELDS(KEY_RUNTIME_SLOT_DIRECT_PLAN_CAPACITY);
} key_runtime_slot_direct_plan_t;

static inline bool key_runtime_slot_direct_plan_builder_has_effect(key_runtime_effect_builder_t builder) {
    return builder.kind != KEY_RUNTIME_EFFECT_BUILDER_NONE || builder.release_owned_state || builder.feedback_pulse;
}

static inline void key_runtime_slot_direct_plan_push(key_runtime_slot_direct_plan_t *plan, key_runtime_effect_t effect) {
    if (!plan) {
        return;
    }

    if (plan->count < ARRAY_SIZE(plan->items)) {
        plan->items[plan->count++] = effect;
        return;
    }

    plan->overflowed = true;
}

static inline void key_runtime_slot_direct_plan_push_builder_if_present(key_runtime_slot_direct_plan_t *plan, keypos_t key_pos, key_runtime_effect_builder_t builder) {
    if (!key_runtime_slot_direct_plan_builder_has_effect(builder)) {
        return;
    }

    plan->handled = true;

    if (builder.release_owned_state) {
        key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                    .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                                    .data.key_pos = key_pos,
                                                });
    }

    switch (builder.kind) {
        case KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION:
            key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                        .kind        = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
                                                        .data.action = builder.action,
                                                    });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER:
            key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                        .kind = KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER,
                                                        .data.held_action =
                                                            {
                                                                .key_pos = key_pos,
                                                                .action  = builder.action,
                                                            },
                                                    });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_HELD_UNREGISTER:
            key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                        .kind = KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER,
                                                        .data.held_action =
                                                            {
                                                                .key_pos = key_pos,
                                                                .action  = builder.action,
                                                            },
                                                    });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_REPEAT_START:
            key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                        .kind = KEY_RUNTIME_EFFECT_REPEAT_START,
                                                        .data.repeat =
                                                            {
                                                                .key_pos   = key_pos,
                                                                .action    = builder.action,
                                                                .repeat_hz = builder.repeat_hz,
                                                            },
                                                    });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_NONE:
        default:
            break;
    }

    if (builder.feedback_pulse) {
        key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                    .kind                 = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
                                                    .data.long_hold_level = builder.feedback_long_hold_level,
                                                });
    }
}

static inline void key_runtime_slot_direct_plan_push_dispatch_action(key_runtime_slot_direct_plan_t *plan, keypos_t key_pos, uint16_t action) {
    if (!(plan && action != KC_NO)) {
        return;
    }

    (void)key_pos;
    plan->handled = true;
    key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                .kind        = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
                                                .data.action = action,
                                            });
}

static inline void key_runtime_slot_direct_plan_push_delayed_action(key_runtime_slot_direct_plan_t *plan, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
    if (!(plan && action != KC_NO && repeat_count != 0u)) {
        return;
    }

    plan->handled = true;
    key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                .kind = KEY_RUNTIME_EFFECT_DELAYED_ACTION,
                                                .data.delayed_action =
                                                    {
                                                        .action       = action,
                                                        .mods         = mods,
                                                        .repeat_count = repeat_count,
                                                    },
                                            });
}

static inline void key_runtime_slot_direct_plan_push_layer_press(key_runtime_slot_direct_plan_t *plan, keypos_t key_pos, uint8_t layer) {
    if (!plan) {
        return;
    }

    plan->handled = true;
    key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                .kind = KEY_RUNTIME_EFFECT_LAYER_PRESS,
                                                .data.layer_press =
                                                    {
                                                        .key_pos = key_pos,
                                                        .layer   = layer,
                                                    },
                                            });
}

static inline void key_runtime_slot_direct_plan_push_layer_release(key_runtime_slot_direct_plan_t *plan, keypos_t key_pos) {
    if (!plan) {
        return;
    }

    plan->handled = true;
    key_runtime_slot_direct_plan_push(plan, (key_runtime_effect_t){
                                                .kind         = KEY_RUNTIME_EFFECT_LAYER_RELEASE,
                                                .data.key_pos = key_pos,
                                            });
}
