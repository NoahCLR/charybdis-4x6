#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/key_behavior.h"
#include "users/noah/lib/key/held_action.h"

enum {
    TEST_SHARED_ACTION      = SAFE_RANGE + 0x40,
    TEST_SECOND_ACTION      = SAFE_RANGE + 0x41,
    TEST_PER_KEY_ACTION     = SAFE_RANGE + 0x42,
    TEST_PRESS_ONLY_ACTION  = SAFE_RANGE + 0x43,
};

typedef struct {
    keypos_t  key_pos;
    uint16_t  action;
} action_call_t;

typedef struct {
    uint16_t action;
} tap_call_t;

typedef struct {
    uint16_t action;
    bool     pressed;
} pointer_action_call_t;

static action_call_t press_calls[16];
static action_call_t release_calls[16];
static tap_call_t    tap_calls[32];
static pointer_action_call_t pointer_action_calls[16];
static uint8_t       press_call_count;
static uint8_t       release_call_count;
static uint8_t       tap_call_count;
static uint8_t       pointer_action_call_count;
static uint16_t      mod_register_calls[16];
static uint16_t      mod_unregister_calls[16];
static uint8_t       mod_register_count;
static uint8_t       mod_unregister_count;
static uint16_t      fake_time;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)            \
    do {                       \
        if (!(expr)) {         \
            test_fail(#expr, __FILE__, __LINE__); \
        }                      \
    } while (0)

static keypos_t test_keypos(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static void test_reset_stubs(void) {
    press_call_count    = 0;
    release_call_count  = 0;
    tap_call_count      = 0;
    pointer_action_call_count = 0;
    mod_register_count  = 0;
    mod_unregister_count = 0;
    fake_time           = 1000;
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    switch (action) {
        case TEST_PER_KEY_ACTION:
            return NOAH_ACTION_HOLD_KIND_PER_KEY;
        case TEST_PRESS_ONLY_ACTION:
            return NOAH_ACTION_HOLD_KIND_PRESS_ONLY;
        default:
            return NOAH_ACTION_HOLD_KIND_SHARED;
    }
}

void noah_action_press(keypos_t key_pos, uint16_t action) {
    CHECK(press_call_count < ARRAY_SIZE(press_calls));
    press_calls[press_call_count++] = (action_call_t){
        .key_pos = key_pos,
        .action  = action,
    };
}

void noah_action_release(keypos_t key_pos, uint16_t action) {
    CHECK(release_call_count < ARRAY_SIZE(release_calls));
    release_calls[release_call_count++] = (action_call_t){
        .key_pos = key_pos,
        .action  = action,
    };
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

void action_dispatch(uint16_t action) {
    CHECK(tap_call_count < ARRAY_SIZE(tap_calls));
    tap_calls[tap_call_count++] = (tap_call_t){
        .action = action,
    };
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    CHECK(pointer_action_call_count < ARRAY_SIZE(pointer_action_calls));
    pointer_action_calls[pointer_action_call_count++] = (pointer_action_call_t){
        .action  = action,
        .pressed = pressed,
    };
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    CHECK(mod_register_count < ARRAY_SIZE(mod_register_calls));
    mod_register_calls[mod_register_count++] = keycode;
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    CHECK(mod_unregister_count < ARRAY_SIZE(mod_unregister_calls));
    mod_unregister_calls[mod_unregister_count++] = keycode;
}

static void test_shared_action_refcounts_press_and_release(void) {
    keypos_t key_a = test_keypos(0, 0);
    keypos_t key_b = test_keypos(0, 1);

    test_reset_stubs();

    held_action_register(key_a, TEST_SHARED_ACTION);
    held_action_register(key_b, TEST_SHARED_ACTION);

    CHECK(press_call_count == 1);
    CHECK(press_calls[0].action == TEST_SHARED_ACTION);
    CHECK(held_action_survives_flush(key_a, TEST_SHARED_ACTION));
    CHECK(held_action_survives_flush(key_b, TEST_SHARED_ACTION));

    held_action_unregister(key_a, TEST_SHARED_ACTION);
    CHECK(release_call_count == 0);
    CHECK(!held_action_survives_flush(key_a, TEST_SHARED_ACTION));
    CHECK(held_action_survives_flush(key_b, TEST_SHARED_ACTION));

    held_action_unregister(key_b, TEST_SHARED_ACTION);
    CHECK(release_call_count == 1);
    CHECK(release_calls[0].action == TEST_SHARED_ACTION);
    CHECK(!held_action_survives_flush(key_b, TEST_SHARED_ACTION));
}

static void test_per_key_action_dispatches_for_each_owner(void) {
    keypos_t key_a = test_keypos(1, 0);
    keypos_t key_b = test_keypos(1, 1);

    test_reset_stubs();

    held_action_register(key_a, TEST_PER_KEY_ACTION);
    held_action_register(key_b, TEST_PER_KEY_ACTION);

    CHECK(press_call_count == 2);
    CHECK(press_calls[0].action == TEST_PER_KEY_ACTION);
    CHECK(press_calls[1].action == TEST_PER_KEY_ACTION);

    held_action_unregister(key_a, TEST_PER_KEY_ACTION);
    CHECK(release_call_count == 1);
    CHECK(release_calls[0].action == TEST_PER_KEY_ACTION);

    held_action_unregister(key_b, TEST_PER_KEY_ACTION);
    CHECK(release_call_count == 2);
    CHECK(release_calls[1].action == TEST_PER_KEY_ACTION);
}

static void test_modifier_ownership_refcounts_without_action_dispatch(void) {
    keypos_t key_a = test_keypos(2, 0);
    keypos_t key_b = test_keypos(2, 1);

    test_reset_stubs();

    held_action_register(key_a, KC_LEFT_SHIFT);
    held_action_register(key_b, KC_LEFT_SHIFT);

    CHECK(mod_register_count == 1);
    CHECK(mod_register_calls[0] == KC_LEFT_SHIFT);
    CHECK(press_call_count == 0);
    CHECK(held_action_survives_flush(key_a, KC_LEFT_SHIFT));

    CHECK(held_modifier_release_owned_by_key(key_a));
    CHECK(mod_unregister_count == 0);

    held_action_unregister(key_b, KC_LEFT_SHIFT);
    CHECK(mod_unregister_count == 1);
    CHECK(mod_unregister_calls[0] == KC_LEFT_SHIFT);
    CHECK(release_call_count == 0);
    CHECK(held_action_survives_flush(key_b, KC_LEFT_SHIFT));
}

static void test_rebinding_same_key_releases_old_action_before_pressing_new(void) {
    keypos_t key_pos = test_keypos(3, 3);

    test_reset_stubs();

    held_action_register(key_pos, TEST_SHARED_ACTION);
    held_action_register(key_pos, TEST_SECOND_ACTION);

    CHECK(press_call_count == 2);
    CHECK(press_calls[0].action == TEST_SHARED_ACTION);
    CHECK(press_calls[1].action == TEST_SECOND_ACTION);
    CHECK(release_call_count == 1);
    CHECK(release_calls[0].action == TEST_SHARED_ACTION);

    held_action_unregister(key_pos, TEST_SECOND_ACTION);
    CHECK(release_call_count == 2);
    CHECK(release_calls[1].action == TEST_SECOND_ACTION);
}

static void test_release_owned_by_key_reports_missing_bindings(void) {
    test_reset_stubs();

    CHECK(!held_action_release_owned_by_key(test_keypos(7, 7)));
    CHECK(!held_modifier_release_owned_by_key(test_keypos(7, 7)));
}

static void test_repeat_binding_taps_immediately_and_on_tick_until_release(void) {
    keypos_t key_pos = test_keypos(5, 5);

    test_reset_stubs();

    held_action_repeat_start(key_pos, TEST_SHARED_ACTION, 25);

    CHECK(tap_call_count == 1);
    CHECK(tap_calls[0].action == TEST_SHARED_ACTION);
    CHECK(pointer_action_call_count == 1);
    CHECK(pointer_action_calls[0].action == TEST_SHARED_ACTION);
    CHECK(pointer_action_calls[0].pressed);

    fake_time = (uint16_t)(fake_time + 39);
    held_action_repeat_tick();
    CHECK(tap_call_count == 1);

    fake_time = (uint16_t)(fake_time + 1);
    held_action_repeat_tick();
    CHECK(tap_call_count == 2);
    CHECK(tap_calls[1].action == TEST_SHARED_ACTION);

    CHECK(held_action_release_owned_by_key(key_pos));
    CHECK(pointer_action_call_count == 2);
    CHECK(pointer_action_calls[1].action == TEST_SHARED_ACTION);
    CHECK(!pointer_action_calls[1].pressed);

    fake_time = (uint16_t)(fake_time + 80);
    held_action_repeat_tick();
    CHECK(tap_call_count == 2);
}

static void test_repeat_binding_catches_up_after_scan_gap(void) {
    keypos_t key_pos = test_keypos(6, 6);

    test_reset_stubs();

    held_action_repeat_start(key_pos, TEST_SECOND_ACTION, 25);
    CHECK(tap_call_count == 1);

    fake_time = (uint16_t)(fake_time + 120);
    held_action_repeat_tick();

    CHECK(tap_call_count == 4);
    CHECK(tap_calls[1].action == TEST_SECOND_ACTION);
    CHECK(tap_calls[2].action == TEST_SECOND_ACTION);
    CHECK(tap_calls[3].action == TEST_SECOND_ACTION);
}

static void test_repeat_binding_rejects_rates_above_supported_range(void) {
    keypos_t key_pos = test_keypos(7, 0);

    test_reset_stubs();

    held_action_repeat_start(key_pos, TEST_SHARED_ACTION, (uint16_t)(KEY_BEHAVIOR_REPEAT_MAX_HZ + 1u));

    CHECK(tap_call_count == 0);
    CHECK(pointer_action_call_count == 0);
    CHECK(!held_action_release_owned_by_key(key_pos));
}

int main(void) {
    test_shared_action_refcounts_press_and_release();
    test_per_key_action_dispatches_for_each_owner();
    test_modifier_ownership_refcounts_without_action_dispatch();
    test_rebinding_same_key_releases_old_action_before_pressing_new();
    test_release_owned_by_key_reports_missing_bindings();
    test_repeat_binding_taps_immediately_and_on_tick_until_release();
    test_repeat_binding_catches_up_after_scan_gap();
    test_repeat_binding_rejects_rates_above_supported_range();

    puts("held_action host tests passed");
    return 0;
}
