#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_scenario_harness.h"
#include "users/noah/noah_keymap_ids.h"

enum {
    TEST_ACTIVE_KEY            = SAFE_RANGE + 0x90,
    TEST_MULTI_TAP_KEY         = SAFE_RANGE + 0x91,
    TEST_RELEASE_PRIMARY       = SAFE_RANGE + 0x92,
    TEST_RELEASE_LONG          = SAFE_RANGE + 0x93,
    TEST_HELD_ACTION           = SAFE_RANGE + 0x94,
    TEST_THRESHOLD_ACTION      = SAFE_RANGE + 0x95,
    TEST_SECOND_TAP_ACTION     = SAFE_RANGE + 0x96,
    TEST_CHAIN_CONTINUE_TAP    = SAFE_RANGE + 0x97,
    TEST_TAP_HOLD_TERM_MS      = 120,
    TEST_LONGER_HOLD_TERM_MS   = 240,
    TEST_MULTI_TAP_TERM_MS     = 150,
    TEST_TAP_SETUP_ELAPSED_MS  = TEST_TAP_HOLD_TERM_MS + 1,
    TEST_LONG_SETUP_ELAPSED_MS = TEST_LONGER_HOLD_TERM_MS + 1,
};

typedef enum {
    TEST_RELEASE_SUMMARY_NONE = 0,
    TEST_RELEASE_SUMMARY_ACTION,
    TEST_RELEASE_SUMMARY_HELD_LIFECYCLE,
    TEST_RELEASE_SUMMARY_PRESERVE_CHAIN,
} test_release_summary_kind_t;

typedef struct {
    test_release_summary_kind_t kind;
    uint16_t                    action;
    bool                        release_owned_state;
    bool                        pending_chain;
} test_release_summary_t;

typedef enum {
    TEST_ACTIVE_SETUP_NONE = 0,
    TEST_ACTIVE_SETUP_SCAN_AT_TAP,
    TEST_ACTIVE_SETUP_SCAN_AT_LONG,
} test_active_setup_t;

typedef enum {
    TEST_PENDING_SETUP_NONE = 0,
    TEST_PENDING_SETUP_SCAN_AT_TAP,
} test_pending_setup_t;

typedef struct {
    const char            *name;
    hold_behavior_t        hold;
    hold_behavior_t        long_hold;
    test_active_setup_t    active_setup;
    test_pending_setup_t   pending_setup;
    uint16_t               release_elapsed;
    test_release_summary_t expected;
} test_release_equivalence_case_t;

typedef struct {
    const char            *name;
    uint16_t               second_tap_action;
    hold_behavior_t        hold;
    hold_behavior_t        long_hold;
    bool                   has_more_taps;
    test_pending_setup_t   pending_setup;
    uint16_t               release_elapsed;
    test_release_summary_t expected;
} test_pending_release_case_t;

static void test_case_fail(const char *case_name, const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s: %s (%s:%d)\n", case_name, expr, file, line);
    exit(1);
}

#define CHECK_CASE(case_name, expr)                                 \
    do {                                                            \
        if (!(expr)) {                                              \
            test_case_fail((case_name), #expr, __FILE__, __LINE__); \
        }                                                           \
    } while (0)

#define HOLD_LIT(expr) ((hold_behavior_t)expr)
#define HOLD_NONE_LIT ((hold_behavior_t){0})

#define TEST_EXPECT_NONE()                 \
    (test_release_summary_t) {             \
        .kind = TEST_RELEASE_SUMMARY_NONE, \
    }

#define TEST_EXPECT_ACTION(action_, release_owned_)                                                        \
    (test_release_summary_t) {                                                                             \
        .kind = TEST_RELEASE_SUMMARY_ACTION, .action = (action_), .release_owned_state = (release_owned_), \
    }

#define TEST_EXPECT_HELD(action_, release_owned_)                                                                  \
    (test_release_summary_t) {                                                                                     \
        .kind = TEST_RELEASE_SUMMARY_HELD_LIFECYCLE, .action = (action_), .release_owned_state = (release_owned_), \
    }

#define TEST_EXPECT_PRESERVE_CHAIN()                                        \
    (test_release_summary_t) {                                              \
        .kind = TEST_RELEASE_SUMMARY_PRESERVE_CHAIN, .pending_chain = true, \
    }

static keypos_t test_keypos(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static key_behavior_view_t test_pressable_handled_key(uint16_t keycode) {
    key_behavior_view_t behavior = key_runtime_scenario_pressable_handled_key(keycode);

    behavior.tap_hold_term    = TEST_TAP_HOLD_TERM_MS;
    behavior.longer_hold_term = TEST_LONGER_HOLD_TERM_MS;
    behavior.multi_tap_term   = TEST_MULTI_TAP_TERM_MS;
    return behavior;
}

static void test_configure_active_release_key(hold_behavior_t hold, hold_behavior_t long_hold) {
    key_behavior_view_t behavior = test_pressable_handled_key(TEST_ACTIVE_KEY);

    behavior.single.tap       = tap_behavior_none();
    behavior.single.hold      = hold;
    behavior.single.long_hold = long_hold;
    key_runtime_scenario_add_behavior_view(behavior);
}

static void test_configure_pending_multi_tap_release_key(uint16_t second_tap_action, hold_behavior_t hold, hold_behavior_t long_hold, bool has_more_taps) {
    key_runtime_scenario_multi_tap_entry_t entries[2] = {
        [0] =
            {
                .tap_count = 2,
                .step =
                    {
                        .tap       = second_tap_action == KC_NO ? tap_behavior_none() : (tap_behavior_t)TAP_SENDS(second_tap_action),
                        .hold      = hold,
                        .long_hold = long_hold,
                    },
            },
    };
    uint8_t entry_count = 1;

    if (has_more_taps) {
        entries[entry_count++] = (key_runtime_scenario_multi_tap_entry_t){
            .tap_count = 3,
            .step =
                {
                    .tap = (tap_behavior_t)TAP_SENDS(TEST_CHAIN_CONTINUE_TAP),
                },
        };
    }

    key_behavior_view_t behavior = test_pressable_handled_key(TEST_MULTI_TAP_KEY);

    behavior.has_multi_tap = true;
    behavior.single.tap    = tap_behavior_none();
    key_runtime_scenario_add_pending_multi_tap_behavior(behavior, entries, entry_count);
}

static void test_run_steps(const key_runtime_scenario_step_t *steps, uint8_t step_count) {
    if (step_count == 0) {
        return;
    }

    key_runtime_scenario_run(steps, step_count);
}

static test_release_summary_t test_release_summary_from_effects(const char *case_name, keypos_t key_pos, uint16_t pre_release_held_action) {
    test_release_summary_t summary = {
        .pending_chain = key_runtime_scenario_slot_has_pending_multi_tap(key_pos),
    };
    uint16_t register_action   = KC_NO;
    uint16_t unregister_action = KC_NO;

    for (uint8_t index = 0; index < key_runtime_scenario_effect_count(); index++) {
        const key_runtime_scenario_effect_t *effect = key_runtime_scenario_effect_at(index);

        CHECK_CASE(case_name, effect != NULL);

        switch (effect->kind) {
            case KEY_RUNTIME_EFFECT_DISPATCH_ACTION:
                CHECK_CASE(case_name, summary.kind == TEST_RELEASE_SUMMARY_NONE);
                summary.kind   = TEST_RELEASE_SUMMARY_ACTION;
                summary.action = effect->data.action;
                break;
            case KEY_RUNTIME_EFFECT_DELAYED_ACTION:
                CHECK_CASE(case_name, summary.kind == TEST_RELEASE_SUMMARY_NONE);
                summary.kind   = TEST_RELEASE_SUMMARY_ACTION;
                summary.action = effect->data.delayed_action.action;
                break;
            case KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER:
                CHECK_CASE(case_name, register_action == KC_NO);
                register_action = effect->data.held_action.action;
                break;
            case KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER:
                CHECK_CASE(case_name, unregister_action == KC_NO);
                unregister_action = effect->data.held_action.action;
                break;
            case KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY:
                CHECK_CASE(case_name, !summary.release_owned_state);
                summary.release_owned_state = true;
                break;
            default:
                test_case_fail(case_name, "unexpected effect kind in release summary", __FILE__, __LINE__);
        }
    }

    if (register_action != KC_NO || unregister_action != KC_NO) {
        CHECK_CASE(case_name, register_action == unregister_action);
        CHECK_CASE(case_name, summary.kind == TEST_RELEASE_SUMMARY_NONE);
        summary.kind   = TEST_RELEASE_SUMMARY_HELD_LIFECYCLE;
        summary.action = register_action;
    }

    if (summary.kind == TEST_RELEASE_SUMMARY_NONE && summary.release_owned_state && pre_release_held_action != KC_NO) {
        summary.kind   = TEST_RELEASE_SUMMARY_HELD_LIFECYCLE;
        summary.action = pre_release_held_action;
    }

    if (summary.kind == TEST_RELEASE_SUMMARY_NONE && summary.pending_chain && key_runtime_scenario_effect_count() == 0) {
        summary.kind = TEST_RELEASE_SUMMARY_PRESERVE_CHAIN;
    }

    return summary;
}

static void test_expect_summary(const char *case_name, test_release_summary_t actual, test_release_summary_t expected) {
    CHECK_CASE(case_name, actual.kind == expected.kind);
    CHECK_CASE(case_name, actual.action == expected.action);
    CHECK_CASE(case_name, actual.release_owned_state == expected.release_owned_state);
    CHECK_CASE(case_name, actual.pending_chain == expected.pending_chain);
}

static uint16_t test_active_setup_elapsed(test_active_setup_t setup) {
    switch (setup) {
        case TEST_ACTIVE_SETUP_SCAN_AT_TAP:
            return TEST_TAP_SETUP_ELAPSED_MS;
        case TEST_ACTIVE_SETUP_SCAN_AT_LONG:
            return TEST_LONG_SETUP_ELAPSED_MS;
        case TEST_ACTIVE_SETUP_NONE:
        default:
            return 0;
    }
}

static uint16_t test_pending_setup_elapsed(test_pending_setup_t setup) {
    switch (setup) {
        case TEST_PENDING_SETUP_SCAN_AT_TAP:
            return TEST_TAP_SETUP_ELAPSED_MS;
        case TEST_PENDING_SETUP_NONE:
        default:
            return 0;
    }
}

static void test_run_active_release_setup(test_active_setup_t setup, keypos_t key_pos) {
    static const key_runtime_scenario_step_t press[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_ACTIVE_KEY, 1, 1),
    };
    static const key_runtime_scenario_step_t scan_at_tap[] = {
        KEY_RUNTIME_SCENARIO_ADVANCE(TEST_TAP_SETUP_ELAPSED_MS),
        KEY_RUNTIME_SCENARIO_SCAN(),
    };
    static const key_runtime_scenario_step_t scan_at_long[] = {
        KEY_RUNTIME_SCENARIO_ADVANCE(TEST_LONG_SETUP_ELAPSED_MS),
        KEY_RUNTIME_SCENARIO_SCAN(),
    };

    (void)key_pos;
    test_run_steps(press, ARRAY_SIZE(press));

    switch (setup) {
        case TEST_ACTIVE_SETUP_SCAN_AT_TAP:
            test_run_steps(scan_at_tap, ARRAY_SIZE(scan_at_tap));
            return;
        case TEST_ACTIVE_SETUP_SCAN_AT_LONG:
            test_run_steps(scan_at_long, ARRAY_SIZE(scan_at_long));
            return;
        case TEST_ACTIVE_SETUP_NONE:
        default:
            return;
    }
}

static test_release_summary_t test_run_active_release_case(const char *case_name, hold_behavior_t hold, hold_behavior_t long_hold, test_active_setup_t setup, uint16_t release_elapsed) {
    keypos_t key_pos = test_keypos(1, 1);
    uint16_t setup_elapsed;
    uint16_t pre_release_held_action;

    key_runtime_scenario_reset();
    test_configure_active_release_key(hold, long_hold);
    test_run_active_release_setup(setup, key_pos);

    setup_elapsed = test_active_setup_elapsed(setup);
    CHECK_CASE(case_name, release_elapsed >= setup_elapsed);
    pre_release_held_action = key_runtime_scenario_slot_held_action_keycode(key_pos);
    key_runtime_scenario_clear_effects();

    if (release_elapsed > setup_elapsed) {
        key_runtime_scenario_step_t advance[] = {
            KEY_RUNTIME_SCENARIO_ADVANCE((uint16_t)(release_elapsed - setup_elapsed)),
        };

        test_run_steps(advance, ARRAY_SIZE(advance));
    }

    key_runtime_scenario_step_t release[] = {
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_ACTIVE_KEY, 1, 1),
    };

    test_run_steps(release, ARRAY_SIZE(release));
    return test_release_summary_from_effects(case_name, key_pos, pre_release_held_action);
}

static void test_run_pending_release_setup(test_pending_setup_t setup) {
    static const key_runtime_scenario_step_t begin_chain[] = {
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 2, 2),
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 2, 2),
        KEY_RUNTIME_SCENARIO_PRESS(TEST_MULTI_TAP_KEY, 2, 2),
    };
    static const key_runtime_scenario_step_t scan_at_tap[] = {
        KEY_RUNTIME_SCENARIO_ADVANCE(TEST_TAP_SETUP_ELAPSED_MS),
        KEY_RUNTIME_SCENARIO_SCAN(),
    };

    test_run_steps(begin_chain, ARRAY_SIZE(begin_chain));

    if (setup == TEST_PENDING_SETUP_SCAN_AT_TAP) {
        test_run_steps(scan_at_tap, ARRAY_SIZE(scan_at_tap));
    }
}

static test_release_summary_t test_run_pending_release_case(const char *case_name, uint16_t second_tap_action, hold_behavior_t hold, hold_behavior_t long_hold, bool has_more_taps, test_pending_setup_t setup, uint16_t release_elapsed) {
    keypos_t key_pos = test_keypos(2, 2);
    uint16_t setup_elapsed;
    uint16_t pre_release_held_action;

    key_runtime_scenario_reset();
    test_configure_pending_multi_tap_release_key(second_tap_action, hold, long_hold, has_more_taps);
    test_run_pending_release_setup(setup);

    setup_elapsed = test_pending_setup_elapsed(setup);
    CHECK_CASE(case_name, release_elapsed >= setup_elapsed);
    pre_release_held_action = key_runtime_scenario_slot_held_action_keycode(key_pos);
    key_runtime_scenario_clear_effects();

    if (release_elapsed > setup_elapsed) {
        key_runtime_scenario_step_t advance[] = {
            KEY_RUNTIME_SCENARIO_ADVANCE((uint16_t)(release_elapsed - setup_elapsed)),
        };

        test_run_steps(advance, ARRAY_SIZE(advance));
    }

    key_runtime_scenario_step_t release[] = {
        KEY_RUNTIME_SCENARIO_RELEASE(TEST_MULTI_TAP_KEY, 2, 2),
    };

    test_run_steps(release, ARRAY_SIZE(release));
    return test_release_summary_from_effects(case_name, key_pos, pre_release_held_action);
}

static void test_active_and_pending_release_semantics_stay_equivalent(void) {
    static const test_release_equivalence_case_t cases[] = {
        {
            .name            = "release primary dispatches after tap",
            .hold            = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_PRIMARY)),
            .long_hold       = HOLD_NONE_LIT,
            .active_setup    = TEST_ACTIVE_SETUP_SCAN_AT_TAP,
            .pending_setup   = TEST_PENDING_SETUP_NONE,
            .release_elapsed = TEST_TAP_SETUP_ELAPSED_MS,
            .expected        = TEST_EXPECT_ACTION(TEST_RELEASE_PRIMARY, false),
        },
        {
            .name            = "release long dispatches without primary hold",
            .hold            = HOLD_NONE_LIT,
            .long_hold       = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_LONG)),
            .active_setup    = TEST_ACTIVE_SETUP_SCAN_AT_LONG,
            .pending_setup   = TEST_PENDING_SETUP_NONE,
            .release_elapsed = TEST_LONG_SETUP_ELAPSED_MS,
            .expected        = TEST_EXPECT_ACTION(TEST_RELEASE_LONG, false),
        },
        {
            .name            = "release long wins over primary after longer term",
            .hold            = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_PRIMARY)),
            .long_hold       = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_LONG)),
            .active_setup    = TEST_ACTIVE_SETUP_SCAN_AT_TAP,
            .pending_setup   = TEST_PENDING_SETUP_NONE,
            .release_elapsed = TEST_LONG_SETUP_ELAPSED_MS,
            .expected        = TEST_EXPECT_ACTION(TEST_RELEASE_LONG, false),
        },
        {
            .name            = "release primary still wins before longer term",
            .hold            = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_PRIMARY)),
            .long_hold       = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_LONG)),
            .active_setup    = TEST_ACTIVE_SETUP_SCAN_AT_TAP,
            .pending_setup   = TEST_PENDING_SETUP_NONE,
            .release_elapsed = TEST_TAP_SETUP_ELAPSED_MS,
            .expected        = TEST_EXPECT_ACTION(TEST_RELEASE_PRIMARY, false),
        },
        {
            .name            = "threshold hold does not redispatch on release before long",
            .hold            = HOLD_LIT(TAP_AT_HOLD_THRESHOLD(TEST_THRESHOLD_ACTION)),
            .long_hold       = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_LONG)),
            .active_setup    = TEST_ACTIVE_SETUP_SCAN_AT_TAP,
            .pending_setup   = TEST_PENDING_SETUP_SCAN_AT_TAP,
            .release_elapsed = TEST_TAP_SETUP_ELAPSED_MS,
            .expected        = TEST_EXPECT_NONE(),
        },
        {
            .name            = "threshold hold releases long action after longer term",
            .hold            = HOLD_LIT(TAP_AT_HOLD_THRESHOLD(TEST_THRESHOLD_ACTION)),
            .long_hold       = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_LONG)),
            .active_setup    = TEST_ACTIVE_SETUP_SCAN_AT_TAP,
            .pending_setup   = TEST_PENDING_SETUP_SCAN_AT_TAP,
            .release_elapsed = TEST_LONG_SETUP_ELAPSED_MS,
            .expected        = TEST_EXPECT_ACTION(TEST_RELEASE_LONG, false),
        },
        {
            .name            = "held lifecycle survives release before long",
            .hold            = HOLD_LIT(PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HELD_ACTION)),
            .long_hold       = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_LONG)),
            .active_setup    = TEST_ACTIVE_SETUP_SCAN_AT_TAP,
            .pending_setup   = TEST_PENDING_SETUP_SCAN_AT_TAP,
            .release_elapsed = TEST_TAP_SETUP_ELAPSED_MS,
            .expected        = TEST_EXPECT_HELD(TEST_HELD_ACTION, true),
        },
        {
            .name            = "held lifecycle releases owned state and long action after longer term",
            .hold            = HOLD_LIT(PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HELD_ACTION)),
            .long_hold       = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_LONG)),
            .active_setup    = TEST_ACTIVE_SETUP_SCAN_AT_TAP,
            .pending_setup   = TEST_PENDING_SETUP_SCAN_AT_TAP,
            .release_elapsed = TEST_LONG_SETUP_ELAPSED_MS,
            .expected        = TEST_EXPECT_ACTION(TEST_RELEASE_LONG, true),
        },
    };

    for (uint8_t index = 0; index < ARRAY_SIZE(cases); index++) {
        test_release_summary_t active_summary  = test_run_active_release_case(cases[index].name, cases[index].hold, cases[index].long_hold, cases[index].active_setup, cases[index].release_elapsed);
        test_release_summary_t pending_summary = test_run_pending_release_case(cases[index].name, KC_NO, cases[index].hold, cases[index].long_hold, false, cases[index].pending_setup, cases[index].release_elapsed);

        test_expect_summary(cases[index].name, active_summary, cases[index].expected);
        test_expect_summary(cases[index].name, pending_summary, cases[index].expected);
    }
}

static void test_pending_release_edge_cases(void) {
    static const test_pending_release_case_t cases[] = {
        {
            .name              = "quick release preserves pending chain when more taps remain",
            .second_tap_action = KC_NO,
            .hold              = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_PRIMARY)),
            .long_hold         = HOLD_NONE_LIT,
            .has_more_taps     = true,
            .pending_setup     = TEST_PENDING_SETUP_NONE,
            .release_elapsed   = 60,
            .expected          = TEST_EXPECT_PRESERVE_CHAIN(),
        },
        {
            .name              = "quick release falls back to second tap action when chain ends",
            .second_tap_action = TEST_SECOND_TAP_ACTION,
            .hold              = HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_RELEASE_PRIMARY)),
            .long_hold         = HOLD_NONE_LIT,
            .has_more_taps     = false,
            .pending_setup     = TEST_PENDING_SETUP_NONE,
            .release_elapsed   = 60,
            .expected          = TEST_EXPECT_ACTION(TEST_SECOND_TAP_ACTION, false),
        },
        {
            .name              = "pending release synthesizes held lifecycle without scan",
            .second_tap_action = KC_NO,
            .hold              = HOLD_LIT(PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HELD_ACTION)),
            .long_hold         = HOLD_NONE_LIT,
            .has_more_taps     = false,
            .pending_setup     = TEST_PENDING_SETUP_NONE,
            .release_elapsed   = TEST_TAP_SETUP_ELAPSED_MS,
            .expected          = TEST_EXPECT_HELD(TEST_HELD_ACTION, false),
        },
    };

    for (uint8_t index = 0; index < ARRAY_SIZE(cases); index++) {
        test_release_summary_t summary = test_run_pending_release_case(cases[index].name, cases[index].second_tap_action, cases[index].hold, cases[index].long_hold, cases[index].has_more_taps, cases[index].pending_setup, cases[index].release_elapsed);

        test_expect_summary(cases[index].name, summary, cases[index].expected);
    }
}

int main(void) {
    test_active_and_pending_release_semantics_stay_equivalent();
    test_pending_release_edge_cases();
    return 0;
}
