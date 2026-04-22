#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "key_runtime_scenario_harness.h"
#include "users/noah/lib/key/runtime/core/runtime.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"
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
    TEST_MULTI_TAP_KEY_TWO = SAFE_RANGE + 0x7A,
    TEST_TAP_ACTION_TWO    = SAFE_RANGE + 0x7B,
    TEST_ALT_ACTION_TWO    = SAFE_RANGE + 0x7C,
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

static keypos_t test_keypos(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static key_behavior_view_t test_pressable_handled_key(uint16_t keycode) {
    key_behavior_view_t behavior = key_runtime_scenario_pressable_handled_key(keycode);

    behavior.tap_hold_term    = 150;
    behavior.longer_hold_term = 350;
    behavior.multi_tap_term   = 120;
    return behavior;
}

static void test_configure_multi_tap_key(void) {
    static const key_runtime_scenario_multi_tap_entry_t entries[] = {
        {
            .tap_count = 2,
            .step =
                {
                    .tap = TAP_SENDS(TEST_ALT_ACTION),
                },
        },
    };

    key_behavior_view_t behavior = test_pressable_handled_key(TEST_MULTI_TAP_KEY);

    behavior.has_multi_tap = true;
    behavior.single.tap    = (tap_behavior_t)TAP_SENDS(TEST_TAP_ACTION);
    key_runtime_scenario_add_pending_multi_tap_behavior(behavior, entries, ARRAY_SIZE(entries));
}

static void test_configure_multi_tap_key_two(void) {
    static const key_runtime_scenario_multi_tap_entry_t entries[] = {
        {
            .tap_count = 2,
            .step =
                {
                    .tap = TAP_SENDS(TEST_ALT_ACTION_TWO),
                },
        },
    };

    key_behavior_view_t behavior = test_pressable_handled_key(TEST_MULTI_TAP_KEY_TWO);

    behavior.has_multi_tap = true;
    behavior.single.tap    = (tap_behavior_t)TAP_SENDS(TEST_TAP_ACTION_TWO);
    key_runtime_scenario_add_pending_multi_tap_behavior(behavior, entries, ARRAY_SIZE(entries));
}

static void test_configure_multi_tap_lock_key(uint16_t layer_lock_action) {
    const key_runtime_scenario_multi_tap_entry_t entries[] = {
        {
            .tap_count = 2,
            .step =
                {
                    .tap       = TAP_SENDS(TEST_ALT_ACTION),
                    .long_hold = TAP_AT_HOLD_THRESHOLD(layer_lock_action),
                },
        },
    };

    key_behavior_view_t behavior = test_pressable_handled_key(TEST_MULTI_TAP_KEY);

    behavior.has_multi_tap = true;
    behavior.single.tap    = (tap_behavior_t)TAP_SENDS(LOCK_LAYER(TEST_OTHER_LAYER));
    behavior.single.hold   = (hold_behavior_t)PRESS_AND_HOLD_UNTIL_RELEASE(MO(TEST_OTHER_LAYER));
    key_runtime_scenario_add_pending_multi_tap_behavior(behavior, entries, ARRAY_SIZE(entries));
}

static void test_configure_multi_tap_hold_key(hold_behavior_t hold, hold_behavior_t long_hold) {
    const key_runtime_scenario_multi_tap_entry_t entries[] = {
        {
            .tap_count = 2,
            .step =
                {
                    .tap       = TAP_SENDS(TEST_ALT_ACTION),
                    .hold      = hold,
                    .long_hold = long_hold,
                },
        },
    };

    key_behavior_view_t behavior = test_pressable_handled_key(TEST_MULTI_TAP_KEY);

    behavior.has_multi_tap = true;
    behavior.single.tap    = (tap_behavior_t)TAP_SENDS(TEST_TAP_ACTION);
    key_runtime_scenario_add_pending_multi_tap_behavior(behavior, entries, ARRAY_SIZE(entries));
}

static void test_configure_fallback_tap_key(uint16_t keycode, uint16_t tap_action) {
    key_behavior_view_t behavior = test_pressable_handled_key(keycode);

    behavior.single.tap = (tap_behavior_t)TAP_SENDS(tap_action);
    key_runtime_scenario_add_behavior_view(behavior);
}

static void test_configure_immediate_hold_key(uint16_t keycode, uint16_t hold_action, hold_behavior_t long_hold) {
    key_behavior_view_t behavior = test_pressable_handled_key(keycode);

    behavior.single.hold = (hold_behavior_t){
        .present = true,
        .action  = hold_action,
        .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
    };
    behavior.single.long_hold = long_hold;
    key_runtime_scenario_add_behavior_view(behavior);
}

static void test_configure_threshold_then_long_hold_key(uint16_t keycode, uint16_t hold_action, uint16_t long_hold_action) {
    key_behavior_view_t behavior = test_pressable_handled_key(keycode);

    behavior.single.hold      = (hold_behavior_t)PRESS_AND_HOLD_UNTIL_RELEASE(hold_action);
    behavior.single.long_hold = (hold_behavior_t)TAP_AT_HOLD_THRESHOLD(long_hold_action);
    key_runtime_scenario_add_behavior_view(behavior);
}

static void test_configure_repeat_key(uint16_t keycode, uint16_t action, uint16_t repeat_hz) {
    key_behavior_view_t behavior = test_pressable_handled_key(keycode);

    behavior.single.hold = (hold_behavior_t)REPEAT_WHILE_HELD(action, repeat_hz);
    key_runtime_scenario_add_behavior_view(behavior);
}

static void test_configure_interruptible_layer_tap_key(uint16_t keycode, uint16_t tap_action) {
    key_behavior_view_t behavior = test_pressable_handled_key(keycode);

    behavior.is_momentary_layer = true;
    behavior.is_layer_tap       = true;
    behavior.single.tap         = (tap_behavior_t)TAP_SENDS(tap_action);
    behavior.single.hold        = (hold_behavior_t)PRESS_AND_HOLD_UNTIL_RELEASE(MO(TEST_OTHER_LAYER));
    key_runtime_scenario_add_behavior_view(behavior);
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
    CHECK(key_runtime_effect_delayed_action_key_pos(key_runtime_scenario_effect_at(0)).row == 1);
    CHECK(key_runtime_effect_delayed_action_key_pos(key_runtime_scenario_effect_at(0)).col == 1);
    CHECK(!key_runtime_scenario_slot_has_pending_multi_tap(test_keypos(1, 1)));
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

static void test_press_on_other_handled_position_keeps_foreign_pending_multi_tap_until_timeout(void) {
    static const key_runtime_scenario_step_t setup[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 1, 1),
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 1, 1),
    };
    static const key_runtime_scenario_step_t overlap_press[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY_TWO, 1, 3),
    };
    static const key_runtime_scenario_step_t timeout_and_scan[] = {
        KEY_RUNTIME_SCENARIO_ADVANCE(121),
        KEY_RUNTIME_SCENARIO_SCAN(),
    };

    key_runtime_scenario_reset();
    test_configure_multi_tap_key();
    test_configure_fallback_tap_key(TEST_HOLD_KEY_TWO, TEST_HOLD_ACTION_TWO);
    key_runtime_scenario_run(setup, ARRAY_SIZE(setup));
    key_runtime_scenario_clear_effects();
    key_runtime_scenario_run(overlap_press, ARRAY_SIZE(overlap_press));

    CHECK(key_runtime_scenario_effect_count() == 0);
    CHECK(key_runtime_scenario_slot_has_pending_multi_tap(test_keypos(1, 1)));
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(1, 3)) == TEST_HOLD_KEY_TWO);

    key_runtime_scenario_clear_effects();
    key_runtime_scenario_run(timeout_and_scan, ARRAY_SIZE(timeout_and_scan));

    CHECK(key_runtime_scenario_effect_count() == 1);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(key_runtime_scenario_effect_at(0)->data.delayed_action.action == TEST_TAP_ACTION);
    CHECK(!key_runtime_scenario_slot_has_pending_multi_tap(test_keypos(1, 1)));
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(1, 3)) == TEST_HOLD_KEY_TWO);
}

static void test_other_press_does_not_flush_active_same_key_multi_tap_chain(void) {
    static const key_runtime_scenario_step_t setup[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 1, 1),
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 1, 1),
        KEY_RUNTIME_SCENARIO_ADVANCE(40),
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 1, 1),
    };
    static const key_runtime_scenario_step_t overlap_press[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY_TWO, 1, 3),
    };

    key_runtime_scenario_reset();
    test_configure_multi_tap_hold_key((hold_behavior_t)PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HOLD_ACTION), hold_behavior_none());
    test_configure_fallback_tap_key(TEST_HOLD_KEY_TWO, TEST_HOLD_ACTION_TWO);
    key_runtime_scenario_run(setup, ARRAY_SIZE(setup));

    CHECK(noah_runtime_debug_active_slot_count() == 1);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1);
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(1, 1)) == TEST_MULTI_TAP_KEY);
    CHECK(key_runtime_scenario_slot_has_pending_multi_tap(test_keypos(1, 1)));

    key_runtime_scenario_clear_effects();
    key_runtime_scenario_run(overlap_press, ARRAY_SIZE(overlap_press));

    CHECK(key_runtime_scenario_effect_count() == 0);
    CHECK(noah_runtime_debug_active_slot_count() == 2);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1);
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(1, 1)) == TEST_MULTI_TAP_KEY);
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(1, 3)) == TEST_HOLD_KEY_TWO);
    CHECK(key_runtime_scenario_slot_has_pending_multi_tap(test_keypos(1, 1)));
}

static void test_independent_pending_multi_tap_chains_can_coexist_and_flush_independently(void) {
    static const key_runtime_scenario_step_t setup[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 1, 1), KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 1, 1), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY_TWO, 1, 3), KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY_TWO, 1, 3),
    };
    static const key_runtime_scenario_step_t timeout_and_scan[] = {
        KEY_RUNTIME_SCENARIO_ADVANCE(121),
        KEY_RUNTIME_SCENARIO_SCAN(),
    };

    key_runtime_scenario_reset();
    test_configure_multi_tap_key();
    test_configure_multi_tap_key_two();
    key_runtime_scenario_run(setup, ARRAY_SIZE(setup));

    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 2);
    CHECK(key_runtime_scenario_slot_has_pending_multi_tap(test_keypos(1, 1)));
    CHECK(key_runtime_scenario_slot_has_pending_multi_tap(test_keypos(1, 3)));

    key_runtime_scenario_clear_effects();
    key_runtime_scenario_run(timeout_and_scan, ARRAY_SIZE(timeout_and_scan));

    CHECK(key_runtime_scenario_effect_count() == 2);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(key_runtime_scenario_effect_at(0)->data.delayed_action.action == TEST_TAP_ACTION);
    CHECK(key_runtime_effect_delayed_action_key_pos(key_runtime_scenario_effect_at(0)).row == 1);
    CHECK(key_runtime_effect_delayed_action_key_pos(key_runtime_scenario_effect_at(0)).col == 1);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(key_runtime_scenario_effect_at(1)->data.delayed_action.action == TEST_TAP_ACTION_TWO);
    CHECK(key_runtime_effect_delayed_action_key_pos(key_runtime_scenario_effect_at(1)).row == 1);
    CHECK(key_runtime_effect_delayed_action_key_pos(key_runtime_scenario_effect_at(1)).col == 3);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
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
    CHECK(key_runtime_scenario_slot_held_action_keycode(test_keypos(0, 0)) == TEST_FALLBACK_KEY);
    CHECK(key_runtime_scenario_slot_hold_is_complete(test_keypos(0, 0)));
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
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(5, 0)) == KC_NO);
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
    CHECK(!key_runtime_scenario_effect_at(1)->data.feedback_pulse.long_hold_level);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(2)->data.key_pos.row == 0);
    CHECK(key_runtime_scenario_effect_at(2)->data.key_pos.col == 3);
    CHECK(key_runtime_scenario_effect_at(3)->kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(key_runtime_scenario_effect_at(3)->data.action == TEST_ALT_ACTION);
    CHECK(key_runtime_effect_dispatch_action_key_pos(key_runtime_scenario_effect_at(3)).row == 0);
    CHECK(key_runtime_effect_dispatch_action_key_pos(key_runtime_scenario_effect_at(3)).col == 3);
    CHECK(key_runtime_scenario_effect_at(4)->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(key_runtime_scenario_effect_at(4)->data.feedback_pulse.long_hold_level);
    CHECK(key_runtime_scenario_slot_hold_is_complete(test_keypos(0, 3)));
    CHECK(key_runtime_scenario_slot_held_action_keycode(test_keypos(0, 3)) == KC_NO);
}

static void test_threshold_hold_then_long_hold_with_intermediate_scans_promotes_once(void) {
    static const key_runtime_scenario_step_t press_and_scans[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_HOLD_KEY_TWO, 0, 4), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(60), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_SCAN(),
    };
    static const key_runtime_scenario_step_t release[] = {
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_HOLD_KEY_TWO, 0, 4),
    };
    key_runtime_scenario_reset();
    test_configure_threshold_then_long_hold_key(TEST_HOLD_KEY_TWO, TEST_HOLD_ACTION_TWO, TEST_ALT_ACTION);
    key_runtime_scenario_run(press_and_scans, ARRAY_SIZE(press_and_scans));

    key_runtime_scenario_run(release, ARRAY_SIZE(release));

    CHECK(key_runtime_scenario_effect_count() == 4);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_HOLD_ACTION_TWO);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 0);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 4);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(key_runtime_scenario_effect_at(2)->data.action == TEST_ALT_ACTION);
    CHECK(key_runtime_effect_dispatch_action_key_pos(key_runtime_scenario_effect_at(2)).row == 0);
    CHECK(key_runtime_effect_dispatch_action_key_pos(key_runtime_scenario_effect_at(2)).col == 4);
    CHECK(key_runtime_scenario_effect_at(3)->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(key_runtime_scenario_effect_at(3)->data.feedback_pulse.long_hold_level);
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(0, 4)) == KC_NO);
    CHECK(key_runtime_scenario_slot_held_action_keycode(test_keypos(0, 4)) == KC_NO);
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
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(2, 0)) == TEST_HOLD_KEY);
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(2, 1)) == TEST_HOLD_KEY_TWO);
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(2, 2)) == TEST_HOLD_KEY_THREE);
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
    CHECK(key_runtime_scenario_effect_at(1)->data.feedback_pulse.long_hold_level);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(key_runtime_scenario_effect_at(2)->data.action == LOCK_LAYER(TEST_NUM_LAYER));
    CHECK(key_runtime_scenario_effect_at(3)->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(key_runtime_scenario_effect_at(3)->data.feedback_pulse.long_hold_level);
    CHECK(!key_runtime_scenario_layer_locked(TEST_NUM_LAYER));
    CHECK(!key_runtime_scenario_slot_has_pending_multi_tap(test_keypos(4, 2)));
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(4, 2)) == KC_NO);
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
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(3, 1)) == KC_NO);
    CHECK(!key_runtime_scenario_slot_has_pending_multi_tap(test_keypos(3, 1)));
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
    CHECK(!key_runtime_scenario_effect_at(1)->data.feedback_pulse.long_hold_level);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(2)->data.key_pos.row == 6);
    CHECK(key_runtime_scenario_effect_at(2)->data.key_pos.col == 0);
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(6, 0)) == KC_NO);
}

static void test_pd_mode_quick_tap_with_intermediate_scan_toggles_once(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_PD_MODE_KEY, 6, 1),
        KEY_RUNTIME_SCENARIO_ADVANCE(100),
        KEY_RUNTIME_SCENARIO_SCAN(),
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_PD_MODE_KEY, 6, 1),
    };

    key_runtime_scenario_reset();
    key_runtime_scenario_define_pd_mode_key(TEST_PD_MODE_KEY, PD_MODE_VOLUME, true);
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 3);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_PD_MODE_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 6);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 1);
    CHECK(key_runtime_scenario_effect_at(2)->kind == KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP);
    CHECK(key_runtime_scenario_effect_at(2)->data.pd_mode_lock_tap.pd_mode == PD_MODE_VOLUME);
    CHECK(key_runtime_scenario_effect_at(2)->data.pd_mode_lock_tap.key_pos.row == 6);
    CHECK(key_runtime_scenario_effect_at(2)->data.pd_mode_lock_tap.key_pos.col == 1);
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(6, 1)) == KC_NO);
}

static void test_pd_mode_hold_with_intermediate_scan_does_not_toggle_lock(void) {
    static const key_runtime_scenario_step_t scenario[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_PD_MODE_KEY, 6, 2), KEY_RUNTIME_SCENARIO_ADVANCE(100), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(60), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_RELEASE(TEST_PD_MODE_KEY, 6, 2),
    };

    key_runtime_scenario_reset();
    key_runtime_scenario_define_pd_mode_key(TEST_PD_MODE_KEY, PD_MODE_VOLUME, true);
    key_runtime_scenario_run(scenario, ARRAY_SIZE(scenario));

    CHECK(key_runtime_scenario_effect_count() == 2);
    CHECK(key_runtime_scenario_effect_at(0)->kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(key_runtime_scenario_effect_at(0)->data.held_action.action == TEST_PD_MODE_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.row == 6);
    CHECK(key_runtime_scenario_effect_at(1)->data.key_pos.col == 2);
    CHECK(key_runtime_scenario_slot_owner_keycode(test_keypos(6, 2)) == KC_NO);
}

int main(void) {
    test_single_tap_waits_for_multi_tap_timeout_before_dispatching();
    test_momentary_layer_key_tracks_press_and_release_events();
    test_threshold_hold_registers_and_releases_owned_state();
    test_press_on_other_handled_position_keeps_foreign_pending_multi_tap_until_timeout();
    test_other_press_does_not_flush_active_same_key_multi_tap_chain();
    test_independent_pending_multi_tap_chains_can_coexist_and_flush_independently();
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
