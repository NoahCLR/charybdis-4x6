#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_scenario_harness.h"
#include "users/noah/lib/key/runtime/key_runtime_state.h"
#include "users/noah/noah_keymap_ids.h"

enum {
    TEST_FALLBACK_KEY      = 0x0004u,
    TEST_OTHER_KEY         = 0x0005u,
    TEST_MULTI_TAP_KEY     = SAFE_RANGE + 0x70,
    TEST_TAP_ACTION        = SAFE_RANGE + 0x71,
    TEST_ALT_ACTION        = SAFE_RANGE + 0x72,
    TEST_HOLD_KEY          = SAFE_RANGE + 0x73,
    TEST_HOLD_ACTION       = SAFE_RANGE + 0x74,
    TEST_HOLD_KEY_TWO      = SAFE_RANGE + 0x75,
    TEST_HOLD_KEY_THREE    = SAFE_RANGE + 0x76,
    TEST_HOLD_ACTION_TWO   = SAFE_RANGE + 0x77,
    TEST_HOLD_ACTION_THREE = SAFE_RANGE + 0x78,
    TEST_PD_MODE_KEY       = SAFE_RANGE + 0x79,
    TEST_NUM_LAYER         = 1,
    TEST_OTHER_LAYER       = 2,
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

    key_runtime_scenario_add_behavior_step(TEST_MULTI_TAP_KEY, 2,
                                           (key_behavior_step_t){
                                               .tap = TAP_SENDS(TEST_ALT_ACTION),
                                           });
}

static void test_configure_multi_tap_lock_key(uint16_t layer_lock_action) {
    key_runtime_scenario_add_behavior_view((key_behavior_view_t){
        .keycode          = TEST_MULTI_TAP_KEY,
        .handled          = true,
        .has_multi_tap    = true,
        .tap_hold_term    = 150,
        .longer_hold_term = 350,
        .multi_tap_term   = 120,
        .single =
            {
                .tap  = TAP_SENDS(LOCK_LAYER(TEST_OTHER_LAYER)),
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(TEST_OTHER_LAYER)),
            },
    });

    key_runtime_scenario_add_behavior_step(TEST_MULTI_TAP_KEY, 2,
                                           (key_behavior_step_t){
                                               .tap       = TAP_SENDS(TEST_ALT_ACTION),
                                               .long_hold = TAP_AT_HOLD_THRESHOLD(layer_lock_action),
                                           });
}

static void test_configure_multi_tap_hold_key(hold_behavior_t hold, hold_behavior_t long_hold) {
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

    key_runtime_scenario_add_behavior_step(TEST_MULTI_TAP_KEY, 2,
                                           (key_behavior_step_t){
                                               .tap       = TAP_SENDS(TEST_ALT_ACTION),
                                               .hold      = hold,
                                               .long_hold = long_hold,
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
                .hold =
                    {
                        .present = true,
                        .action  = hold_action,
                        .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                    },
                .long_hold = long_hold,
            },
    });
}

static void test_configure_threshold_then_long_hold_key(uint16_t keycode, uint16_t hold_action, uint16_t long_hold_action) {
    key_runtime_scenario_add_behavior_view((key_behavior_view_t){
        .keycode          = keycode,
        .handled          = true,
        .tap_hold_term    = 150,
        .longer_hold_term = 350,
        .multi_tap_term   = 120,
        .single =
            {
                .hold      = (hold_behavior_t)PRESS_AND_HOLD_UNTIL_RELEASE(hold_action),
                .long_hold = (hold_behavior_t)TAP_AT_HOLD_THRESHOLD(long_hold_action),
            },
    });
}

static void test_configure_repeat_key(uint16_t keycode, uint16_t action, uint16_t repeat_hz) {
    key_runtime_scenario_add_behavior_view((key_behavior_view_t){
        .keycode          = keycode,
        .handled          = true,
        .tap_hold_term    = 150,
        .longer_hold_term = 350,
        .multi_tap_term   = 120,
        .single =
            {
                .hold = (hold_behavior_t)REPEAT_WHILE_HELD(action, repeat_hz),
            },
    });
}

static void test_configure_interruptible_layer_tap_key(uint16_t keycode, uint16_t tap_action) {
    key_runtime_scenario_add_behavior_view((key_behavior_view_t){
        .keycode            = keycode,
        .handled            = true,
        .is_momentary_layer = true,
        .is_layer_tap       = true,
        .tap_hold_term      = 150,
        .longer_hold_term   = 350,
        .multi_tap_term     = 120,
        .single =
            {
                .tap  = TAP_SENDS(tap_action),
                .hold = (hold_behavior_t)PRESS_AND_HOLD_UNTIL_RELEASE(MO(TEST_OTHER_LAYER)),
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
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(key_runtime_scenario_effect_at(0)->data.delayed_action.action == TEST_TAP_ACTION);
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
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_LAYER_PRESS);
    CHECK(key_runtime_scenario_effect_at(0)->data.layer_press.layer == 3);
    CHECK(key_runtime_scenario_effect_at(0)->data.layer_press.key_pos.row == 2);
    CHECK(key_runtime_scenario_effect_at(0)->data.layer_press.key_pos.col == 4);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_LAYER_RELEASE);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 2);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 4);
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
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_HOLD_ACTION);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 0);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 3);
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
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_LAYER_PRESS);
    CHECK(key_runtime_scenario_effect_at(0)->data.layer_press.layer == 2);
    CHECK(key_runtime_scenario_effect_at(0)->data.layer_press.key_pos.row == 1);
    CHECK(key_runtime_scenario_effect_at(0)->data.layer_press.key_pos.col == 2);
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
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_FALLBACK_KEY);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.key_pos.row == 0);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.key_pos.col == 0);
    CHECK(test_slot_state(0, 0)->lifecycle.held_action_keycode == TEST_FALLBACK_KEY);
    CHECK(key_runtime_slot_hold_is_complete(test_slot_state(0, 0)));
}

static void test_interrupted_layer_tap_with_intermediate_scan_releases_without_tap(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(LT(TEST_OTHER_LAYER, TEST_FALLBACK_KEY), 5, 0), KEY_RUNTIME_SCENARIO_ADVANCE(60), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_PRESS(TEST_OTHER_KEY, 5, 1), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_RELEASE(LT(TEST_OTHER_LAYER, TEST_FALLBACK_KEY), 5, 0),
    };

    key_runtime_scenario_reset();
    test_configure_interruptible_layer_tap_key(LT(TEST_OTHER_LAYER, TEST_FALLBACK_KEY), TEST_TAP_ACTION);
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 2);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_LAYER_PRESS);
    CHECK(key_runtime_scenario_effect_at(0)->data.layer_press.layer == TEST_OTHER_LAYER);
    CHECK(key_runtime_scenario_effect_at(0)->data.layer_press.key_pos.row == 5);
    CHECK(key_runtime_scenario_effect_at(0)->data.layer_press.key_pos.col == 0);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_LAYER_RELEASE);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 5);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 0);
    CHECK(test_slot_state(5, 0)->owner.keycode == KC_NO);
}

static void test_immediate_hold_promotes_long_hold_after_registration(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY, 0, 3), KEY_RUNTIME_SCENARIO_ADVANCE(151), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(200), KEY_RUNTIME_SCENARIO_SCAN(),
    };

    key_runtime_scenario_reset();
    test_configure_immediate_hold_key(TEST_HOLD_KEY, TEST_HOLD_ACTION, (hold_behavior_t)TAP_AT_HOLD_THRESHOLD(TEST_ALT_ACTION));
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 5);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_HOLD_ACTION);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(!key_runtime_scenario_effect_at(1)->data.long_hold_level);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(2)->data.key_pos.row == 0);
    CHECK(key_runtime_scenario_effect_at(2)->data.key_pos.col == 3);
    CHECK(key_runtime_scenario_effect_at(3)->kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(key_runtime_scenario_effect_at(3)->data.action == TEST_ALT_ACTION);
    CHECK(key_runtime_scenario_effect_at(4)->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(key_runtime_scenario_effect_at(4)->data.long_hold_level);
    CHECK(key_runtime_slot_hold_is_complete(test_slot_state(0, 3)));
    CHECK(test_slot_state(0, 3)->lifecycle.held_action_keycode == KC_NO);
}

static void test_threshold_hold_then_long_hold_with_intermediate_scans_promotes_once(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY_TWO, 0, 4), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(60), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_RELEASE(TEST_HOLD_KEY_TWO, 0, 4),
    };

    key_runtime_scenario_reset();
    test_configure_threshold_then_long_hold_key(TEST_HOLD_KEY_TWO, TEST_HOLD_ACTION_TWO, TEST_ALT_ACTION);
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 4);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_HOLD_ACTION_TWO);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 0);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 4);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(key_runtime_scenario_effect_at(2)->data.action == TEST_ALT_ACTION);
    CHECK(key_runtime_scenario_effect_at(3)->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(key_runtime_scenario_effect_at(3)->data.long_hold_level);
    CHECK(test_slot_state(0, 4)->owner.keycode == KC_NO);
    CHECK(test_slot_state(0, 4)->lifecycle.held_action_keycode == KC_NO);
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
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_HOLD_ACTION);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(1)->data.held_action.action == TEST_HOLD_ACTION_TWO);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(2)->data.held_action.action == TEST_HOLD_ACTION_THREE);
    CHECK(test_slot_state(2, 0)->owner.keycode == TEST_HOLD_KEY);
    CHECK(test_slot_state(2, 1)->owner.keycode == TEST_HOLD_KEY_TWO);
    CHECK(test_slot_state(2, 2)->owner.keycode == TEST_HOLD_KEY_THREE);
}

static void test_double_tap_hold_can_toggle_same_layer_lock_twice(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 4, 2), KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 4, 2), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 4, 2), KEY_RUNTIME_SCENARIO_ADVANCE(120), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(240), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 4, 2), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 4, 2), KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 4, 2), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 4, 2), KEY_RUNTIME_SCENARIO_ADVANCE(120), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(240), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 4, 2),
    };

    key_runtime_scenario_reset();
    test_configure_multi_tap_lock_key(LOCK_LAYER(TEST_NUM_LAYER));
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 4);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(key_runtime_scenario_effect_at(0)->data.action == LOCK_LAYER(TEST_NUM_LAYER));
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(key_runtime_scenario_effect_at(1)->data.long_hold_level);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(key_runtime_scenario_effect_at(2)->data.action == LOCK_LAYER(TEST_NUM_LAYER));
    CHECK(key_runtime_scenario_effect_at(3)->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(key_runtime_scenario_effect_at(3)->data.long_hold_level);
    CHECK(!key_runtime_scenario_layer_locked(TEST_NUM_LAYER));
    CHECK(!key_runtime_slot_has_pending_multi_tap(test_slot_state(4, 2)));
    CHECK(test_slot_state(4, 2)->owner.keycode == KC_NO);
}

static void test_double_tap_threshold_hold_with_intermediate_scan_registers_once(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 3, 1), KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 3, 1), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 3, 1), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(60), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 3, 1),
    };

    key_runtime_scenario_reset();
    test_configure_multi_tap_hold_key((hold_behavior_t)PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HOLD_ACTION), hold_behavior_none());
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 2);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_HOLD_ACTION);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 3);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 1);
    CHECK(test_slot_state(3, 1)->owner.keycode == KC_NO);
    CHECK(!key_runtime_slot_has_pending_multi_tap(test_slot_state(3, 1)));
}

static void test_repeat_hold_with_intermediate_scans_starts_once_and_releases_once(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY_THREE, 6, 0), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(60), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_RELEASE(TEST_HOLD_KEY_THREE, 6, 0),
    };

    key_runtime_scenario_reset();
    test_configure_repeat_key(TEST_HOLD_KEY_THREE, TEST_HOLD_ACTION_THREE, 25);
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 3);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_REPEAT_START);
    CHECK(key_runtime_scenario_effect_at(0)->data.repeat.action == TEST_HOLD_ACTION_THREE);
    CHECK(key_runtime_scenario_effect_at(0)->data.repeat.key_pos.row == 6);
    CHECK(key_runtime_scenario_effect_at(0)->data.repeat.key_pos.col == 0);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(!key_runtime_scenario_effect_at(1)->data.long_hold_level);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(2)->data.key_pos.row == 6);
    CHECK(key_runtime_scenario_effect_at(2)->data.key_pos.col == 0);
    CHECK(test_slot_state(6, 0)->owner.keycode == KC_NO);
}

static void test_pd_mode_quick_tap_with_intermediate_scan_toggles_once(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_PD_MODE_KEY, 6, 1),
        KEY_RUNTIME_SCENARIO_ADVANCE(100),
        KEY_RUNTIME_SCENARIO_SCAN(),
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_PD_MODE_KEY, 6, 1),
    };

    key_runtime_scenario_reset();
    key_runtime_scenario_add_pd_mode(TEST_PD_MODE_KEY, PD_MODE_VOLUME);
    key_runtime_scenario_set_pd_locked_modes(PD_MODE_VOLUME);
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 3);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_PD_MODE_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 6);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 1);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP);
    CHECK(key_runtime_scenario_effect_at(2)->data.pd_mode == PD_MODE_VOLUME);
    CHECK(test_slot_state(6, 1)->owner.keycode == KC_NO);
}

static void test_pd_mode_hold_with_intermediate_scan_does_not_toggle_lock(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_PD_MODE_KEY, 6, 2), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(60), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_RELEASE(TEST_PD_MODE_KEY, 6, 2),
    };

    key_runtime_scenario_reset();
    key_runtime_scenario_add_pd_mode(TEST_PD_MODE_KEY, PD_MODE_VOLUME);
    key_runtime_scenario_set_pd_locked_modes(PD_MODE_VOLUME);
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 2);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_PD_MODE_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 6);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 2);
    CHECK(test_slot_state(6, 2)->owner.keycode == KC_NO);
}

int main(void) {
    test_single_tap_waits_for_multi_tap_timeout_before_dispatching();
    test_momentary_layer_key_tracks_press_and_release_events();
    test_threshold_hold_registers_and_releases_owned_state();
    test_press_on_other_position_preserves_pending_multi_tap_before_new_layer_press();
    test_interrupt_other_press_activates_fallback_hold();
    test_interrupted_layer_tap_with_intermediate_scan_releases_without_tap();
    test_immediate_hold_promotes_long_hold_after_registration();
    test_threshold_hold_then_long_hold_with_intermediate_scans_promotes_once();
    test_third_press_preserves_existing_positions_and_uses_its_own_slot();
    test_double_tap_hold_can_toggle_same_layer_lock_twice();
    test_double_tap_threshold_hold_with_intermediate_scan_registers_once();
    test_repeat_hold_with_intermediate_scans_starts_once_and_releases_once();
    test_pd_mode_quick_tap_with_intermediate_scan_toggles_once();
    test_pd_mode_hold_with_intermediate_scan_does_not_toggle_lock();

    puts("key_runtime_scenario host tests passed");
    return 0;
}
