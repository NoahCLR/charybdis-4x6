#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/key/key_runtime_feedback.h"
#include "users/noah/lib/key/key_runtime_state.h"

static uint16_t fake_time;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

static void test_reset_state(void) {
    noah_runtime_shared_state = (runtime_shared_state_t){0};
    fake_time                 = 0;
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

uint8_t get_mods(void) {
    return 0;
}

uint8_t get_weak_mods(void) {
    return 0;
}

uint8_t get_oneshot_mods(void) {
    return 0;
}

uint8_t get_oneshot_locked_mods(void) {
    return 0;
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    (void)keycode;
    (void)tap_count;
    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    (void)keycode;
    (void)count;
    return false;
}

bool action_dispatch_is_layer_action(uint16_t action) {
    (void)action;
    return false;
}

bool action_dispatch_is_layer_lock(uint16_t action) {
    (void)action;
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static void test_non_passthrough_held_action_flashes(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .keycode             = KC_RIGHT_ALT,
        .held_action_keycode = SAFE_RANGE + 1,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_repeat_hold_flashes_while_active(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .keycode               = KC_RIGHT_ALT,
        .repeat_binding_active = true,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_fallback_hold_has_no_hold_feedback(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .keycode               = KC_RIGHT_ALT,
        .held_action_keycode   = KC_RIGHT_ALT,
        .fallback_hold_pending = true,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(flags == 0);
}

static void test_momentary_hold_preview_layer_is_exposed_before_threshold(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .keycode = KC_RIGHT_ALT,
        .hold =
            {
                .present = true,
                .action  = MO(3),
                .mode    = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE,
            },
    };

    CHECK(key_feedback_preview_layer() == 3);
}

static void test_momentary_hold_preview_layer_clears_once_layer_is_active(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .keycode             = KC_RIGHT_ALT,
        .hold_fired          = true,
        .held_action_keycode = MO(4),
    };

    CHECK(key_feedback_preview_layer() == UINT8_MAX);
}

static void test_non_layer_held_action_has_no_preview_layer(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .keycode             = KC_RIGHT_ALT,
        .hold_fired          = true,
        .held_action_keycode = SAFE_RANGE + 1,
    };

    CHECK(key_feedback_preview_layer() == UINT8_MAX);
}

static void test_feedback_falls_back_to_secondary_active_slot(void) {
    test_reset_state();

    noah_runtime_shared_state.key.active_slots[1] = (active_key_state_t){
        .keycode             = KC_RIGHT_ALT,
        .held_action_keycode = SAFE_RANGE + 1,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_multi_tap_pending_flag_uses_secondary_slot(void) {
    test_reset_state();

    noah_runtime_shared_state.key.active_slots[1].pending_multi_tap = (multi_tap_t){
        .keycode = KC_RIGHT_ALT,
        .key_pos = (keypos_t){.row = 4, .col = 2},
        .count   = 1,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_multi_tap_pending(flags));
}

int main(void) {
    test_non_passthrough_held_action_flashes();
    test_repeat_hold_flashes_while_active();
    test_fallback_hold_has_no_hold_feedback();
    test_momentary_hold_preview_layer_is_exposed_before_threshold();
    test_momentary_hold_preview_layer_clears_once_layer_is_active();
    test_non_layer_held_action_has_no_preview_layer();
    test_feedback_falls_back_to_secondary_active_slot();
    test_multi_tap_pending_flag_uses_secondary_slot();

    puts("key_runtime_feedback host tests passed");
    return 0;
}
