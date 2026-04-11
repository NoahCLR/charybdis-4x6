#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_scenario_harness.h"
#include "users/noah/lib/key/key_runtime_state.h"

enum {
    TEST_MULTI_TAP_KEY = SAFE_RANGE + 0x70,
    TEST_TAP_ACTION    = SAFE_RANGE + 0x71,
    TEST_ALT_ACTION    = SAFE_RANGE + 0x72,
    TEST_HOLD_KEY      = SAFE_RANGE + 0x73,
    TEST_HOLD_ACTION   = SAFE_RANGE + 0x74,
};

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

static void test_configure_multi_tap_key(void) {
    key_runtime_scenario_add_behavior_view((key_behavior_view_t){
        .keycode          = TEST_MULTI_TAP_KEY,
        .handled          = true,
        .has_multi_tap    = true,
        .tap_hold_term    = 150,
        .longer_hold_term = 350,
        .multi_tap_term   = 120,
        .single =
            {
                .tap = TAP_SENDS(TEST_TAP_ACTION),
            },
    });

    key_runtime_scenario_add_behavior_step(TEST_MULTI_TAP_KEY, 2, (key_behavior_step_t){
        .tap = TAP_SENDS(TEST_ALT_ACTION),
    });
}

static void test_single_tap_waits_for_multi_tap_timeout_before_dispatching(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 1, 1),
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 1, 1),
        KEY_RUNTIME_SCENARIO_ADVANCE(121),
        KEY_RUNTIME_SCENARIO_SCAN(),
    };

    key_runtime_scenario_reset();
    test_configure_multi_tap_key();
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 1);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_SCENARIO_EFFECT_DELAYED_ACTION);
    CHECK(key_runtime_scenario_effect_at(0)->action == TEST_TAP_ACTION);
    CHECK(!key_runtime_slot_has_pending_multi_tap(&key_runtime_scenario_state()->key.active_slots[0]));
}

static void test_momentary_layer_key_tracks_press_and_release_events(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(MO(3), 2, 4),
        KEY_RUNTIME_SCENARIO_RELEASE(MO(3), 2, 4),
    };

    key_runtime_scenario_reset();
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 2);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_SCENARIO_EFFECT_LAYER_PRESS);
    CHECK(key_runtime_scenario_effect_at(0)->layer == 3);
    CHECK(key_runtime_scenario_effect_at(0)->key_pos.row == 2);
    CHECK(key_runtime_scenario_effect_at(0)->key_pos.col == 4);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_SCENARIO_EFFECT_LAYER_RELEASE);
    CHECK(key_runtime_scenario_effect_at(1)->key_pos.row == 2);
    CHECK(key_runtime_scenario_effect_at(1)->key_pos.col == 4);
}

static void test_threshold_hold_registers_and_releases_owned_state(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY, 0, 3),
        KEY_RUNTIME_SCENARIO_ADVANCE(151),
        KEY_RUNTIME_SCENARIO_SCAN(),
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_HOLD_KEY, 0, 3),
    };

    key_runtime_scenario_reset();
    key_runtime_scenario_add_behavior_view((key_behavior_view_t){
        .keycode          = TEST_HOLD_KEY,
        .handled          = true,
        .tap_hold_term    = 150,
        .longer_hold_term = 350,
        .multi_tap_term   = 120,
        .single =
            {
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HOLD_ACTION),
            },
    });

    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 2);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_SCENARIO_EFFECT_HELD_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->action == TEST_HOLD_ACTION);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_SCENARIO_EFFECT_RELEASE_OWNED_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->key_pos.row == 0);
    CHECK(key_runtime_scenario_effect_at(1)->key_pos.col == 3);
}

int main(void) {
    test_single_tap_waits_for_multi_tap_timeout_before_dispatching();
    test_momentary_layer_key_tracks_press_and_release_events();
    test_threshold_hold_registers_and_releases_owned_state();

    puts("key_runtime_scenario host tests passed");
    return 0;
}
