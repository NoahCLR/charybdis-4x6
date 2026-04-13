// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Results
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot-result builders used by the slot-step reducer implementation.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "key_runtime_slot_result_internal.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

static void key_runtime_slot_result_log_overflow(key_runtime_effect_kind_t kind, uint8_t capacity) {
#ifdef CONSOLE_ENABLE
    uprintf("Key runtime slot result overflow dropping effect kind %u after %u queued effects\n", (unsigned int)kind, (unsigned int)capacity);
#else
    (void)kind;
    (void)capacity;
#endif
}

static void key_runtime_slot_result_fail_host_overflow(key_runtime_effect_kind_t kind, uint8_t capacity) {
#ifdef NOAH_HOST_TEST_ENV
    noah_host_test_fail_runtime_overflow("key runtime slot result", (unsigned int)kind, (unsigned int)capacity);
#else
    (void)kind;
    (void)capacity;
#endif
}

void key_runtime_slot_result_push(key_runtime_slot_result_t *result, key_runtime_effect_t effect) {
    if (!result) {
        return;
    }

    if (result->count < ARRAY_SIZE(result->items)) {
        result->items[result->count++] = effect;
        return;
    }

    if (!result->overflowed) {
        result->overflowed = true;
        key_runtime_slot_result_log_overflow(effect.kind, ARRAY_SIZE(result->items));
        key_runtime_slot_result_fail_host_overflow(effect.kind, ARRAY_SIZE(result->items));
    }
}

bool key_runtime_slot_result_builder_has_effect(key_runtime_effect_builder_t builder) {
    return builder.kind != KEY_RUNTIME_EFFECT_BUILDER_NONE || builder.release_owned_state || builder.feedback_pulse;
}

void key_runtime_slot_result_push_builder_if_present(key_runtime_slot_result_t *result, keypos_t key_pos, key_runtime_effect_builder_t builder) {
    if (!key_runtime_slot_result_builder_has_effect(builder)) {
        return;
    }

    if (builder.release_owned_state) {
        key_runtime_slot_result_push(result, (key_runtime_effect_t){
                                                 .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                                 .data.key_pos = key_pos,
                                             });
    }

    switch (builder.kind) {
        case KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION:
            key_runtime_slot_result_push(result, (key_runtime_effect_t){
                                                     .kind        = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
                                                     .data.action = builder.action,
                                                 });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER:
            key_runtime_slot_result_push(result, (key_runtime_effect_t){
                                                     .kind = KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER,
                                                     .data.held_action =
                                                         {
                                                             .key_pos = key_pos,
                                                             .action  = builder.action,
                                                         },
                                                 });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_HELD_UNREGISTER:
            key_runtime_slot_result_push(result, (key_runtime_effect_t){
                                                     .kind = KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER,
                                                     .data.held_action =
                                                         {
                                                             .key_pos = key_pos,
                                                             .action  = builder.action,
                                                         },
                                                 });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_REPEAT_START:
            key_runtime_slot_result_push(result, (key_runtime_effect_t){
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
        key_runtime_slot_result_push(result, (key_runtime_effect_t){
                                                 .kind                 = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
                                                 .data.long_hold_level = builder.feedback_long_hold_level,
                                             });
    }
}

void key_runtime_slot_result_push_dispatch_action(key_runtime_slot_result_t *result, keypos_t key_pos, uint16_t action) {
    if (action == KC_NO) {
        return;
    }

    key_runtime_slot_result_push_builder_if_present(result, key_pos,
                                                    (key_runtime_effect_builder_t){
                                                        .kind   = KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION,
                                                        .action = action,
                                                    });
}

void key_runtime_slot_result_push_delayed_action(key_runtime_slot_result_t *result, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
    if (action == KC_NO || repeat_count == 0) {
        return;
    }

    key_runtime_slot_result_push(result, (key_runtime_effect_t){
                                             .kind = KEY_RUNTIME_EFFECT_DELAYED_ACTION,
                                             .data.delayed_action =
                                                 {
                                                     .action       = action,
                                                     .mods         = mods,
                                                     .repeat_count = repeat_count,
                                                 },
                                         });
}

void key_runtime_slot_result_push_layer_press(key_runtime_slot_result_t *result, keypos_t key_pos, uint8_t layer) {
    key_runtime_slot_result_push(result, (key_runtime_effect_t){
                                             .kind = KEY_RUNTIME_EFFECT_LAYER_PRESS,
                                             .data.layer_press =
                                                 {
                                                     .key_pos = key_pos,
                                                     .layer   = layer,
                                                 },
                                         });
}

void key_runtime_slot_result_push_layer_release(key_runtime_slot_result_t *result, keypos_t key_pos) {
    key_runtime_slot_result_push(result, (key_runtime_effect_t){
                                             .kind         = KEY_RUNTIME_EFFECT_LAYER_RELEASE,
                                             .data.key_pos = key_pos,
                                         });
}

void key_runtime_slot_result_push_pd_mode_lock_tap(key_runtime_slot_result_t *result, pd_mode_mask_t mode) {
    if (!mode) {
        return;
    }

    key_runtime_slot_result_push(result, (key_runtime_effect_t){
                                             .kind         = KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP,
                                             .data.pd_mode = mode,
                                         });
}
