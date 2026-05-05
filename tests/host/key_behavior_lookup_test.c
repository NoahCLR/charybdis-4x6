#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/key/behavior/handled_key.h"
#include "users/noah/lib/key/behavior/key_behavior_lookup.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"

enum {
    TEST_LAYER_TAP_KEY               = 0x04,
    TEST_AUTHORED_LAYER_TAP          = LT(2, TEST_LAYER_TAP_KEY),
    TEST_BARE_LAYER_TAP              = LT(3, TEST_LAYER_TAP_KEY),
    TEST_PD_MODE_KEY                 = SAFE_RANGE + 0x0Fu,
    TEST_TAP_ACTION                  = SAFE_RANGE + 0x10u,
    TEST_PD_MODE_LOCK_KEY            = SAFE_RANGE + 0x11u,
    TEST_TRANSPARENT_PD_KEY          = SAFE_RANGE + 0x12u,
    TEST_TRANSPARENT_KEY             = SAFE_RANGE + 0x13u,
    TEST_MULTI_TAP_KEY               = SAFE_RANGE + 0x14u,
    TEST_TRANSPARENT_HOLD_KEY        = SAFE_RANGE + 0x15u,
    TEST_TRANSPARENT_HOLD_OTHER_KEY  = SAFE_RANGE + 0x16u,
    TEST_TRANSPARENT_LONG_HOLD_KEY   = SAFE_RANGE + 0x17u,
    TEST_HOLD_ACTION                 = SAFE_RANGE + 0x18u,
    TEST_LONG_HOLD_ACTION            = SAFE_RANGE + 0x19u,
    TEST_HOLD_BEHAVIOR_KEY           = SAFE_RANGE + 0x1Au,
    TEST_LONG_HOLD_BEHAVIOR_KEY      = SAFE_RANGE + 0x1Bu,
    TEST_CHAIN_MULTI_TAP_KEY         = SAFE_RANGE + 0x1Cu,
    TEST_STACKED_PD_KEY              = SAFE_RANGE + 0x1Du,
    TEST_OTHER_PD_MODE_KEY           = SAFE_RANGE + 0x1Eu,
    TEST_BRANCH_CONFIRM_DISABLED_KEY = SAFE_RANGE + 0x1Fu,
    TEST_BRANCH_CONFIRM_OVERRIDE_KEY = SAFE_RANGE + 0x20u,
};

layer_state_t   layer_state;
static uint16_t test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
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

static keypos_t test_keypos(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static void test_reset_keymap(void) {
    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                test_keymap[layer][row][col] = KC_TRNS;
            }
        }
    }

    layer_state = 0;
    fake_time   = 1000u;
}

static void test_set_keymap_key(uint8_t layer, keypos_t key_pos, uint16_t keycode) {
    test_keymap[layer][key_pos.row][key_pos.col] = keycode;
}

static handled_key_materialized_t test_materialize(handled_key_resolution_t resolution, keypos_t key_pos) {
    return handled_key_materialize(resolution, handled_key_resolution_ctx_live(key_pos));
}

const key_behavior_t key_behaviors[] = {
    {
        .keycode = TEST_AUTHORED_LAYER_TAP,
        .tap_counts =
            {
                [1] = {.tap = TAP_SENDS(TEST_TAP_ACTION)},
            },
    },
    {
        .keycode = TEST_TRANSPARENT_PD_KEY,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_TRNS)},
                [1] = {.tap = TAP_SENDS(KC_TRNS)},
            },
    },
    {
        .keycode = TEST_TRANSPARENT_KEY,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_TRNS)},
            },
    },
    {
        .keycode = TEST_MULTI_TAP_KEY,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_C)},
                [1] = {.tap = TAP_SENDS(TEST_TAP_ACTION)},
            },
    },
    {
        .keycode = TEST_CHAIN_MULTI_TAP_KEY,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_V)},
                [1] = {.tap = TAP_SENDS(TEST_TAP_ACTION)},
                [2] = {.tap = TAP_SENDS(TEST_HOLD_ACTION)},
            },
    },
    {
        .keycode = TEST_TRANSPARENT_HOLD_KEY,
        .tap_counts =
            {
                [0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_TRNS)},
                [1] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_TRNS)},
            },
    },
    {
        .keycode = TEST_TRANSPARENT_HOLD_OTHER_KEY,
        .tap_counts =
            {
                [0] = {.hold = TAP_AT_HOLD_THRESHOLD(KC_TRNS)},
            },
    },
    {
        .keycode = TEST_TRANSPARENT_LONG_HOLD_KEY,
        .tap_counts =
            {
                [0] = {.long_hold = TAP_AT_HOLD_THRESHOLD(KC_TRNS)},
            },
    },
    {
        .keycode = TEST_HOLD_BEHAVIOR_KEY,
        .tap_counts =
            {
                [0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(TEST_HOLD_ACTION)},
                [1] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(TEST_TAP_ACTION)},
            },
    },
    {
        .keycode = TEST_LONG_HOLD_BEHAVIOR_KEY,
        .tap_counts =
            {
                [0] = {.long_hold = TAP_AT_HOLD_THRESHOLD(TEST_LONG_HOLD_ACTION)},
            },
    },
    {
        .keycode = TEST_STACKED_PD_KEY,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_TRNS)},
                [1] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_OTHER_PD_MODE_KEY)},
            },
    },
    {
        .keycode                 = TEST_BRANCH_CONFIRM_DISABLED_KEY,
        .skip_rgb_branch_confirm = true,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_C)},
                [1] = {.tap = TAP_SENDS(KC_V)},
            },
    },
    {
        .keycode                 = TEST_BRANCH_CONFIRM_OVERRIDE_KEY,
        .rgb_branch_confirm_term = 42,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_C)},
                [1] = {.tap = TAP_SENDS(KC_V)},
            },
    },
};

const uint8_t key_behavior_count = ARRAY_SIZE(key_behaviors);

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    if (keycode == TEST_PD_MODE_KEY || keycode == TEST_TRANSPARENT_PD_KEY || keycode == TEST_STACKED_PD_KEY) {
        return 1u;
    }

    if (keycode == TEST_OTHER_PD_MODE_KEY) {
        return 2u;
    }

    return 0u;
}

bool is_pd_mode_lock_action(uint16_t action) {
    return action == TEST_PD_MODE_LOCK_KEY;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return test_keymap[layer_num][row][column];
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

bool layer_ownership_toggle_lock_state(uint8_t layer) {
    (void)layer;
    return true;
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    (void)key_pos;
    return true;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool pd_mode_toggle_lock_state_at(pd_mode_mask_t mode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_toggle_lock_state(mode);
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    (void)keycode;
}

bool noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    (void)keycode;
    (void)pressed;
    return false;
}

void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    (void)keycode;
    (void)pressed;
    (void)tap_count;
}

bool owned_keycode_register(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool owned_keycode_unregister(uint16_t keycode) {
    (void)keycode;
    return false;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

void register_code16(uint16_t keycode) {
    (void)keycode;
}

void tap_code16(uint16_t keycode) {
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
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

static void test_bare_lt_falls_back_to_qmk(void) {
    key_behavior_view_t behavior = key_behavior_lookup(TEST_BARE_LAYER_TAP);

    CHECK(!behavior.handled);
    CHECK(!behavior.is_momentary_layer);
    CHECK(!behavior.is_layer_tap);
    CHECK(behavior.tap_hold_term == CUSTOM_TAP_HOLD_TERM);
}

static void test_authored_lt_uses_custom_runtime(void) {
    key_behavior_view_t behavior = key_behavior_lookup(TEST_AUTHORED_LAYER_TAP);

    CHECK(behavior.handled);
    CHECK(behavior.is_momentary_layer);
    CHECK(behavior.is_layer_tap);
    CHECK(behavior.tap_hold_term == TAPPING_TERM);
    CHECK(behavior.has_multi_tap);
}

static void test_branch_confirm_term_resolution(void) {
    key_behavior_view_t behavior = key_behavior_lookup(TEST_MULTI_TAP_KEY);

    CHECK(behavior.branch_confirm_term == CUSTOM_RGB_BRANCH_CONFIRM_TERM);

    behavior = key_behavior_lookup(TEST_BRANCH_CONFIRM_DISABLED_KEY);
    CHECK(behavior.branch_confirm_term == 0u);

    behavior = key_behavior_lookup(TEST_BRANCH_CONFIRM_OVERRIDE_KEY);
    CHECK(behavior.branch_confirm_term == 42u);
}

static void test_momentary_layer_stays_handled(void) {
    key_behavior_view_t behavior = key_behavior_lookup(MO(4));

    CHECK(behavior.handled);
    CHECK(behavior.is_momentary_layer);
    CHECK(!behavior.is_layer_tap);
}

static void test_plain_pd_mode_key_is_handled_without_authored_behavior(void) {
    key_behavior_view_t behavior = key_behavior_lookup(TEST_PD_MODE_KEY);

    CHECK(behavior.handled);
    CHECK(!behavior.is_momentary_layer);
    CHECK(!behavior.is_layer_tap);
    CHECK(!behavior.has_multi_tap);
    CHECK(behavior.tap_hold_term == CUSTOM_TAP_HOLD_TERM);
    CHECK(!behavior.single.tap.present);
    CHECK(!behavior.single.hold.present);
    CHECK(!behavior.single.long_hold.present);
}

static void test_pd_mode_lock_stays_out_of_handled_key_runtime(void) {
    key_behavior_view_t behavior = key_behavior_lookup(TEST_PD_MODE_LOCK_KEY);

    CHECK(!behavior.handled);
    CHECK(!behavior.is_momentary_layer);
    CHECK(!behavior.is_layer_tap);
}

static void test_repeat_rate_validation_helper_enforces_supported_range(void) {
    CHECK(!hold_repeat_rate_valid(0));
    CHECK(hold_repeat_rate_valid(1));
    CHECK(hold_repeat_rate_valid(KEY_BEHAVIOR_REPEAT_MAX_HZ));
    CHECK(!hold_repeat_rate_valid((uint16_t)(KEY_BEHAVIOR_REPEAT_MAX_HZ + 1u)));
}

static void test_transparent_tap_uses_lower_active_layer_tap_action(void) {
    keypos_t                   key_pos = test_keypos(0, 0);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, KC_V);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_PD_KEY), key_pos);
    CHECK(materialized.tap_action == KC_V);
    CHECK(materialized.tap_repeat_count == 1);
}

static void test_transparent_tap_falls_back_to_base_layer_tap_action(void) {
    keypos_t                   key_pos = test_keypos(0, 1);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(0, key_pos, KC_C);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = (layer_state_t)1u << 2;

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_PD_KEY), key_pos);
    CHECK(materialized.tap_action == KC_C);
}

static void test_transparent_tap_resolves_bare_lt_to_its_tap_keycode(void) {
    keypos_t                   key_pos = test_keypos(0, 2);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_BARE_LAYER_TAP);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_PD_KEY), key_pos);
    CHECK(materialized.tap_action == TEST_LAYER_TAP_KEY);
}

static void test_transparent_tap_stops_at_plain_pd_mode_key_without_tap_output(void) {
    keypos_t                   key_pos = test_keypos(0, 3);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_PD_MODE_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_PD_KEY), key_pos);
    CHECK(materialized.tap_action == KC_NO);
    CHECK(materialized.tap_repeat_count == 0);
}

static void test_transparent_tap_chains_through_lower_authored_transparency(void) {
    keypos_t                   key_pos = test_keypos(0, 4);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(0, key_pos, KC_V);
    test_set_keymap_key(1, key_pos, TEST_TRANSPARENT_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_PD_KEY), key_pos);
    CHECK(materialized.tap_action == KC_V);
}

static void test_transparent_tap_uses_current_tap_count_for_lower_handled_key(void) {
    keypos_t                   key_pos = test_keypos(0, 5);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_MULTI_TAP_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup_tap_count(TEST_TRANSPARENT_PD_KEY, 2), key_pos);
    CHECK(materialized.tap_action == TEST_TAP_ACTION);
    CHECK(materialized.tap_repeat_count == 1);
}

static void test_transparent_tap_inherits_lower_release_resolve_contract_for_terminal_branch(void) {
    keypos_t                   key_pos = test_keypos(0, 6);
    handled_key_materialized_t second_tap;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_MULTI_TAP_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    second_tap = test_materialize(handled_key_lookup_tap_count(TEST_TRANSPARENT_PD_KEY, 2), key_pos);

    CHECK(!second_tap.tap_has_more_taps);
    CHECK(!second_tap.tap_resolves_on_press);
    CHECK(second_tap.tap_action == TEST_TAP_ACTION);
    CHECK(second_tap.tap_repeat_count == 1);
    CHECK(second_tap.hold.present == false);
    CHECK(second_tap.long_hold.present == false);
}

static void test_transparent_tap_inherits_lower_release_resolve_contract(void) {
    keypos_t                   key_pos = test_keypos(0, 7);
    handled_key_materialized_t second_tap;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_CHAIN_MULTI_TAP_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    second_tap = test_materialize(handled_key_lookup_tap_count(TEST_TRANSPARENT_PD_KEY, 2), key_pos);

    CHECK(second_tap.tap_action == TEST_TAP_ACTION);
    CHECK(second_tap.tap_repeat_count == 1);
    CHECK(second_tap.tap_has_more_taps);
    CHECK(!second_tap.tap_resolves_on_press);
    CHECK(second_tap.hold.present == false);
    CHECK(second_tap.long_hold.present == false);
}

static void test_transparent_hold_uses_lower_plain_key_normal_hold_behavior(void) {
    keypos_t                   key_pos = test_keypos(1, 0);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, KC_V);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_HOLD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY), key_pos);
    CHECK(!materialized.hold.present);
}

static void test_transparent_hold_uses_lower_pd_mode_behavior_and_metadata(void) {
    keypos_t                   key_pos = test_keypos(1, 1);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_PD_MODE_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_HOLD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY), key_pos);
    CHECK(materialized.hold.present);
    CHECK(materialized.hold.action == TEST_PD_MODE_KEY);
    CHECK(materialized.hold.mode == HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE);
    CHECK(materialized.hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT);
    CHECK(materialized.pd_mode == 1u);
}

static void test_stacked_pd_first_tap_defers_implicit_hold_until_threshold(void) {
    keypos_t                   key_pos = test_keypos(1, 7);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, KC_V);
    test_set_keymap_key(2, key_pos, TEST_STACKED_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_STACKED_PD_KEY), key_pos);
    CHECK(materialized.hold.present);
    CHECK(materialized.hold.action == TEST_STACKED_PD_KEY);
    CHECK(materialized.hold.mode == HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE);
    CHECK(materialized.hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT);
    CHECK((materialized.flags & HANDLED_KEY_FLAG_IMPLICIT_HOLD) == 0);
    CHECK(materialized.pd_mode == 1u);

    materialized = test_materialize(handled_key_lookup_tap_count(TEST_STACKED_PD_KEY, 2), key_pos);
    CHECK(materialized.hold.present);
    CHECK(materialized.hold.action == TEST_OTHER_PD_MODE_KEY);
    CHECK(materialized.hold.mode == HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE);
    CHECK(materialized.pd_mode == 1u);
}

static void test_transparent_hold_uses_lower_layer_tap_metadata(void) {
    keypos_t                   key_pos = test_keypos(1, 2);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_BARE_LAYER_TAP);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_HOLD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY), key_pos);
    CHECK(!materialized.hold.present);
    CHECK(materialized.hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT);
    CHECK(materialized.layer == 3);
    CHECK((materialized.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0);
    CHECK((materialized.flags & HANDLED_KEY_FLAG_LAYER_TAP) != 0);
}

static void test_transparent_hold_other_uses_lower_layer_tap_metadata(void) {
    keypos_t                   key_pos = test_keypos(1, 3);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_BARE_LAYER_TAP);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_HOLD_OTHER_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_HOLD_OTHER_KEY), key_pos);
    CHECK(!materialized.hold.present);
    CHECK(materialized.layer == 3);
    CHECK((materialized.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0);
}

static void test_transparent_hold_chains_through_lower_authored_transparency(void) {
    keypos_t                   key_pos = test_keypos(1, 4);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(0, key_pos, TEST_HOLD_BEHAVIOR_KEY);
    test_set_keymap_key(1, key_pos, TEST_TRANSPARENT_HOLD_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_HOLD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY), key_pos);
    CHECK(materialized.hold.present);
    CHECK(materialized.hold.action == TEST_HOLD_ACTION);
    CHECK(materialized.hold.mode == HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD);
}

static void test_transparent_hold_uses_current_tap_count_for_lower_handled_key(void) {
    keypos_t                   key_pos = test_keypos(1, 5);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_HOLD_BEHAVIOR_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_HOLD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup_tap_count(TEST_TRANSPARENT_HOLD_KEY, 2), key_pos);
    CHECK(materialized.hold.present);
    CHECK(materialized.hold.action == TEST_TAP_ACTION);
    CHECK(materialized.hold.mode == HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD);
}

static void test_transparent_long_hold_uses_lower_explicit_long_hold_action(void) {
    keypos_t                   key_pos = test_keypos(1, 6);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_LONG_HOLD_BEHAVIOR_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_LONG_HOLD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_LONG_HOLD_KEY), key_pos);
    CHECK(materialized.long_hold.present);
    CHECK(materialized.long_hold.action == TEST_LONG_HOLD_ACTION);
    CHECK(materialized.long_hold.mode == HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD);
}

int main(void) {
    test_bare_lt_falls_back_to_qmk();
    test_authored_lt_uses_custom_runtime();
    test_branch_confirm_term_resolution();
    test_momentary_layer_stays_handled();
    test_plain_pd_mode_key_is_handled_without_authored_behavior();
    test_pd_mode_lock_stays_out_of_handled_key_runtime();
    test_repeat_rate_validation_helper_enforces_supported_range();
    test_transparent_tap_uses_lower_active_layer_tap_action();
    test_transparent_tap_falls_back_to_base_layer_tap_action();
    test_transparent_tap_resolves_bare_lt_to_its_tap_keycode();
    test_transparent_tap_stops_at_plain_pd_mode_key_without_tap_output();
    test_transparent_tap_chains_through_lower_authored_transparency();
    test_transparent_tap_uses_current_tap_count_for_lower_handled_key();
    test_transparent_tap_inherits_lower_release_resolve_contract_for_terminal_branch();
    test_transparent_tap_inherits_lower_release_resolve_contract();
    test_transparent_hold_uses_lower_plain_key_normal_hold_behavior();
    test_transparent_hold_uses_lower_pd_mode_behavior_and_metadata();
    test_stacked_pd_first_tap_defers_implicit_hold_until_threshold();
    test_transparent_hold_uses_lower_layer_tap_metadata();
    test_transparent_hold_other_uses_lower_layer_tap_metadata();
    test_transparent_hold_chains_through_lower_authored_transparency();
    test_transparent_hold_uses_current_tap_count_for_lower_handled_key();
    test_transparent_long_hold_uses_lower_explicit_long_hold_action();

    puts("key_behavior_lookup host tests passed");
    return 0;
}
