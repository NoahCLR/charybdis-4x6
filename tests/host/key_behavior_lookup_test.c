#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/key/interaction/handled_key.h"
#include "users/noah/lib/key/interaction/key_behavior_lookup.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"

enum {
    TEST_LAYER_TAP_KEY      = 0x04,
    TEST_AUTHORED_LAYER_TAP = LT(2, TEST_LAYER_TAP_KEY),
    TEST_BARE_LAYER_TAP     = LT(3, TEST_LAYER_TAP_KEY),
    TEST_PD_MODE_KEY        = SAFE_RANGE + 0x0Fu,
    TEST_TAP_ACTION         = SAFE_RANGE + 0x10u,
    TEST_PD_MODE_LOCK_KEY   = SAFE_RANGE + 0x11u,
    TEST_TRANSPARENT_PD_KEY = SAFE_RANGE + 0x12u,
    TEST_TRANSPARENT_KEY    = SAFE_RANGE + 0x13u,
    TEST_MULTI_TAP_KEY      = SAFE_RANGE + 0x14u,
};

layer_state_t   layer_state;
static uint16_t test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

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
}

static void test_set_keymap_key(uint8_t layer, keypos_t key_pos, uint16_t keycode) {
    test_keymap[layer][key_pos.row][key_pos.col] = keycode;
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
};

const uint8_t key_behavior_count = ARRAY_SIZE(key_behaviors);

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    return keycode == TEST_PD_MODE_KEY || keycode == TEST_TRANSPARENT_PD_KEY ? 1u : 0u;
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
    keypos_t                 key_pos     = test_keypos(0, 0);
    handled_key_resolution_t resolution;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, KC_V);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    resolution = handled_key_lookup(TEST_TRANSPARENT_PD_KEY);
    CHECK(handled_key_resolution_tap_action_at_position(resolution, key_pos) == KC_V);
    CHECK(handled_key_resolution_tap_repeat_count_at_position(resolution, key_pos) == 1);
}

static void test_transparent_tap_falls_back_to_base_layer_tap_action(void) {
    keypos_t                 key_pos     = test_keypos(0, 1);
    handled_key_resolution_t resolution;

    test_reset_keymap();
    test_set_keymap_key(0, key_pos, KC_C);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = (layer_state_t)1u << 2;

    resolution = handled_key_lookup(TEST_TRANSPARENT_PD_KEY);
    CHECK(handled_key_resolution_tap_action_at_position(resolution, key_pos) == KC_C);
}

static void test_transparent_tap_resolves_bare_lt_to_its_tap_keycode(void) {
    keypos_t                 key_pos     = test_keypos(0, 2);
    handled_key_resolution_t resolution;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_BARE_LAYER_TAP);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    resolution = handled_key_lookup(TEST_TRANSPARENT_PD_KEY);
    CHECK(handled_key_resolution_tap_action_at_position(resolution, key_pos) == TEST_LAYER_TAP_KEY);
}

static void test_transparent_tap_stops_at_plain_pd_mode_key_without_tap_output(void) {
    keypos_t                 key_pos     = test_keypos(0, 3);
    handled_key_resolution_t resolution;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_PD_MODE_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    resolution = handled_key_lookup(TEST_TRANSPARENT_PD_KEY);
    CHECK(handled_key_resolution_tap_action_at_position(resolution, key_pos) == KC_NO);
    CHECK(handled_key_resolution_tap_repeat_count_at_position(resolution, key_pos) == 0);
}

static void test_transparent_tap_chains_through_lower_authored_transparency(void) {
    keypos_t                 key_pos     = test_keypos(0, 4);
    handled_key_resolution_t resolution;

    test_reset_keymap();
    test_set_keymap_key(0, key_pos, KC_V);
    test_set_keymap_key(1, key_pos, TEST_TRANSPARENT_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    resolution = handled_key_lookup(TEST_TRANSPARENT_PD_KEY);
    CHECK(handled_key_resolution_tap_action_at_position(resolution, key_pos) == KC_V);
}

static void test_transparent_tap_uses_current_tap_count_for_lower_handled_key(void) {
    keypos_t                 key_pos     = test_keypos(0, 5);
    handled_key_resolution_t resolution;

    test_reset_keymap();
    test_set_keymap_key(1, key_pos, TEST_MULTI_TAP_KEY);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_PD_KEY);
    layer_state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2);

    resolution = handled_key_lookup_tap_count(TEST_TRANSPARENT_PD_KEY, 2);
    CHECK(handled_key_resolution_tap_action_at_position(resolution, key_pos) == TEST_TAP_ACTION);
    CHECK(handled_key_resolution_tap_repeat_count_at_position(resolution, key_pos) == 1);
}

int main(void) {
    test_bare_lt_falls_back_to_qmk();
    test_authored_lt_uses_custom_runtime();
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

    puts("key_behavior_lookup host tests passed");
    return 0;
}
