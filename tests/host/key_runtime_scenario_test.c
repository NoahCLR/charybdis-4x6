#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_scenario_harness.h"
#include "users/noah/lib/key/key_runtime_state.h"

enum {
    TEST_FALLBACK_KEY = 0x0004u,
    TEST_OTHER_KEY    = 0x0005u,
    TEST_MULTI_TAP_KEY = SAFE_RANGE + 0x70,
    TEST_TAP_ACTION    = SAFE_RANGE + 0x71,
    TEST_ALT_ACTION    = SAFE_RANGE + 0x72,
    TEST_HOLD_KEY      = SAFE_RANGE + 0x73,
    TEST_HOLD_ACTION   = SAFE_RANGE + 0x74,
    TEST_HOLD_KEY_TWO  = SAFE_RANGE + 0x75,
    TEST_HOLD_KEY_THREE = SAFE_RANGE + 0x76,
    TEST_HOLD_ACTION_TWO = SAFE_RANGE + 0x77,
    TEST_HOLD_ACTION_THREE = SAFE_RANGE + 0x78,
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

static const active_key_state_t *test_slot_state(uint8_t row, uint8_t col) {
    return key_runtime_slot_for_position((keypos_t){.row = row, .col = col});
}

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

static void test_configure_fallback_tap_key(uint16_t keycode, uint16_t tap_action) {
    key_runtime_scenario_add_behavior_view((key_behavior_view_t){
        .keycode          = keycode,
        .handled          = true,
        .tap_hold_term    = 150,
        .longer_hold_term = 350,
        .multi_tap_term   = 120,
        .single =
            {
                .tap = TAP_SENDS(tap_action),
            },
    });
}

static void test_configure_immediate_hold_key(uint16_t keycode, uint16_t hold_action, hold_behavior_t long_hold) {
    key_runtime_scenario_add_behavior_view((key_behavior_view_t){
        .keycode          = keycode,
        .handled          = true,
        .tap_hold_term    = 150,
        .longer_hold_term = 350,
        .multi_tap_term   = 120,
        .single =
            {
                .hold = {
                    .present = true,
                    .action  = hold_action,
                    .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                },
                .long_hold = long_hold,
            },
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
    CHECK(!key_runtime_slot_has_pending_multi_tap(test_slot_state(1, 1)));
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

static void test_press_on_other_position_preserves_pending_multi_tap_before_new_layer_press(void) {
    static const key_runtime_scenario_step_t setup[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 1, 1),
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 1, 1),
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY_TWO, 1, 3),
    };
    static const key_runtime_scenario_step_t reclaim[] = {
        KEY_RUNTIME_SCENARIO_PRESS(MO(2), 1, 2),
    };

    key_runtime_scenario_reset();
    test_configure_multi_tap_key();
    test_configure_immediate_hold_key(TEST_HOLD_KEY_TWO, TEST_HOLD_ACTION_TWO, hold_behavior_none());
    key_runtime_scenario_run(setup, ARRAY_SIZE(setup));
    key_runtime_scenario_clear_effects();
    key_runtime_scenario_run(reclaim, ARRAY_SIZE(reclaim));

    CHECK(key_runtime_scenario_effect_count() == 1);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_SCENARIO_EFFECT_LAYER_PRESS);
    CHECK(key_runtime_scenario_effect_at(0)->layer == 2);
    CHECK(key_runtime_scenario_effect_at(0)->key_pos.row == 1);
    CHECK(key_runtime_scenario_effect_at(0)->key_pos.col == 2);
    CHECK(key_runtime_slot_has_pending_multi_tap(test_slot_state(1, 1)));
    CHECK(test_slot_state(1, 2)->owner.keycode == MO(2));
    CHECK(test_slot_state(1, 3)->owner.keycode == TEST_HOLD_KEY_TWO);
}

static void test_interrupt_other_press_activates_fallback_hold(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_FALLBACK_KEY, 0, 0),
        KEY_RUNTIME_SCENARIO_PRESS(TEST_OTHER_KEY, 0, 1),
    };

    key_runtime_scenario_reset();
    test_configure_fallback_tap_key(TEST_FALLBACK_KEY, TEST_TAP_ACTION);
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 1);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_SCENARIO_EFFECT_HELD_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->action == TEST_FALLBACK_KEY);
    CHECK(key_runtime_scenario_effect_at(0)->key_pos.row == 0);
    CHECK(key_runtime_scenario_effect_at(0)->key_pos.col == 0);
    CHECK(test_slot_state(0, 0)->lifecycle.held_action_keycode == TEST_FALLBACK_KEY);
    CHECK(key_runtime_slot_hold_is_complete(test_slot_state(0, 0)));
}

static void test_immediate_hold_promotes_long_hold_after_registration(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY, 0, 3),
        KEY_RUNTIME_SCENARIO_ADVANCE(151),
        KEY_RUNTIME_SCENARIO_SCAN(),
        KEY_RUNTIME_SCENARIO_ADVANCE(200),
        KEY_RUNTIME_SCENARIO_SCAN(),
    };

    key_runtime_scenario_reset();
    test_configure_immediate_hold_key(TEST_HOLD_KEY, TEST_HOLD_ACTION, (hold_behavior_t)TAP_AT_HOLD_THRESHOLD(TEST_ALT_ACTION));
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 5);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_SCENARIO_EFFECT_HELD_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->action == TEST_HOLD_ACTION);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_SCENARIO_EFFECT_FEEDBACK_PULSE);
    CHECK(!key_runtime_scenario_effect_at(1)->long_hold_level);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_SCENARIO_EFFECT_RELEASE_OWNED_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(2)->key_pos.row == 0);
    CHECK(key_runtime_scenario_effect_at(2)->key_pos.col == 3);
    CHECK(key_runtime_scenario_effect_at(3)->kind == KEY_RUNTIME_SCENARIO_EFFECT_DISPATCH_ACTION);
    CHECK(key_runtime_scenario_effect_at(3)->action == TEST_ALT_ACTION);
    CHECK(key_runtime_scenario_effect_at(4)->kind == KEY_RUNTIME_SCENARIO_EFFECT_FEEDBACK_PULSE);
    CHECK(key_runtime_scenario_effect_at(4)->long_hold_level);
    CHECK(key_runtime_slot_hold_is_complete(test_slot_state(0, 3)));
    CHECK(test_slot_state(0, 3)->lifecycle.held_action_keycode == KC_NO);
}

static void test_third_press_preserves_existing_positions_and_uses_its_own_slot(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY, 2, 0),
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY_TWO, 2, 1),
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY_THREE, 2, 2),
    };

    key_runtime_scenario_reset();
    test_configure_immediate_hold_key(TEST_HOLD_KEY, TEST_HOLD_ACTION, hold_behavior_none());
    test_configure_immediate_hold_key(TEST_HOLD_KEY_TWO, TEST_HOLD_ACTION_TWO, hold_behavior_none());
    test_configure_immediate_hold_key(TEST_HOLD_KEY_THREE, TEST_HOLD_ACTION_THREE, hold_behavior_none());
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 3);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_SCENARIO_EFFECT_HELD_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->action == TEST_HOLD_ACTION);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_SCENARIO_EFFECT_HELD_REGISTER);
    CHECK(key_runtime_scenario_effect_at(1)->action == TEST_HOLD_ACTION_TWO);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_SCENARIO_EFFECT_HELD_REGISTER);
    CHECK(key_runtime_scenario_effect_at(2)->action == TEST_HOLD_ACTION_THREE);
    CHECK(test_slot_state(2, 0)->owner.keycode == TEST_HOLD_KEY);
    CHECK(test_slot_state(2, 1)->owner.keycode == TEST_HOLD_KEY_TWO);
    CHECK(test_slot_state(2, 2)->owner.keycode == TEST_HOLD_KEY_THREE);
}

int main(void) {
    test_single_tap_waits_for_multi_tap_timeout_before_dispatching();
    test_momentary_layer_key_tracks_press_and_release_events();
    test_threshold_hold_registers_and_releases_owned_state();
    test_press_on_other_position_preserves_pending_multi_tap_before_new_layer_press();
    test_interrupt_other_press_activates_fallback_hold();
    test_immediate_hold_promotes_long_hold_after_registration();
    test_third_press_preserves_existing_positions_and_uses_its_own_slot();

    puts("key_runtime_scenario host tests passed");
    return 0;
}
