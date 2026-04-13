#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/pointing/defs/pd_mode_flags.h"
#include "users/noah/lib/key/interaction/key_behavior_lookup.h"

enum {
    TEST_LAYER_TAP_KEY      = 0x04,
    TEST_AUTHORED_LAYER_TAP = LT(2, TEST_LAYER_TAP_KEY),
    TEST_BARE_LAYER_TAP     = LT(3, TEST_LAYER_TAP_KEY),
    TEST_PD_MODE_KEY        = SAFE_RANGE + 0x0Fu,
    TEST_TAP_ACTION         = SAFE_RANGE + 0x10,
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

const key_behavior_t key_behaviors[] = {
    {
        .keycode = TEST_AUTHORED_LAYER_TAP,
        .tap_counts =
            {
                [1] = {.tap = TAP_SENDS(TEST_TAP_ACTION)},
            },
    },
};

const uint8_t key_behavior_count = ARRAY_SIZE(key_behaviors);

bool action_dispatch_is_raw_qmk_layer_action(uint16_t action) {
    return IS_QK_MOMENTARY(action) || IS_QK_LAYER_TAP(action);
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    return keycode == TEST_PD_MODE_KEY ? 1u : 0u;
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

static void test_repeat_rate_validation_helper_enforces_supported_range(void) {
    CHECK(!hold_repeat_rate_valid(0));
    CHECK(hold_repeat_rate_valid(1));
    CHECK(hold_repeat_rate_valid(KEY_BEHAVIOR_REPEAT_MAX_HZ));
    CHECK(!hold_repeat_rate_valid((uint16_t)(KEY_BEHAVIOR_REPEAT_MAX_HZ + 1u)));
}

int main(void) {
    test_bare_lt_falls_back_to_qmk();
    test_authored_lt_uses_custom_runtime();
    test_momentary_layer_stays_handled();
    test_plain_pd_mode_key_is_handled_without_authored_behavior();
    test_repeat_rate_validation_helper_enforces_supported_range();

    puts("key_behavior_lookup host tests passed");
    return 0;
}
