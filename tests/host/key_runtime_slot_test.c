#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/key/key_runtime_state.h"

enum {
    TEST_MULTI_TAP_KEY = SAFE_RANGE + 0x40,
    TEST_ACTIVE_KEY    = SAFE_RANGE + 0x41,
    TEST_SINGLE_ACTION = SAFE_RANGE + 0x42,
    TEST_HOLD_ACTION   = SAFE_RANGE + 0x43,
};

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

static void test_reset_state(void) {
    fake_time                = 1000;
    noah_runtime_shared_state = (runtime_shared_state_t){0};
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
    if (keycode == TEST_MULTI_TAP_KEY && tap_count == 3) {
        return (key_behavior_step_t){
            .hold = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HOLD_ACTION),
        };
    }

    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    return keycode == TEST_MULTI_TAP_KEY && count < 3;
}

static void test_slot_pending_multi_tap_ownership_marks_slot_non_idle(void) {
    active_key_state_t *slot = key_runtime_slot_at(1);
    keypos_t            pos  = test_keypos(3, 4);

    test_reset_state();
    slot->pending_multi_tap = (multi_tap_t){
        .keycode = TEST_MULTI_TAP_KEY,
        .key_pos = pos,
        .count   = 1,
    };

    CHECK(!key_runtime_slot_idle(slot));
    CHECK(key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(key_runtime_slot_owns_key_position(slot, pos));
    CHECK(key_runtime_find_slot_with_pending_multi_tap(pos) == slot);
    CHECK(key_runtime_find_reclaimable_slot() == slot);
}

static void test_select_slot_for_press_prefers_slot_owning_pending_multi_tap(void) {
    active_key_state_t *primary   = key_runtime_primary_slot();
    active_key_state_t *secondary = key_runtime_slot_at(1);
    keypos_t            target    = test_keypos(6, 2);

    test_reset_state();
    primary->keycode = TEST_ACTIVE_KEY;
    primary->key_pos = test_keypos(1, 1);
    secondary->pending_multi_tap = (multi_tap_t){
        .keycode = TEST_MULTI_TAP_KEY,
        .key_pos = target,
        .count   = 1,
    };

    CHECK(key_runtime_select_slot_for_press(target) == secondary);
}

static void test_slot_pending_multi_tap_lifecycle_helpers(void) {
    active_key_state_t *slot = key_runtime_primary_slot();
    keypos_t            pos  = test_keypos(5, 0);

    test_reset_state();

    key_runtime_slot_begin_pending_multi_tap(slot, TEST_MULTI_TAP_KEY, pos, TEST_SINGLE_ACTION, 120, 150);
    CHECK(key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(slot->pending_multi_tap.count == 1);
    CHECK(!key_runtime_slot_pending_multi_tap_pending_hold(slot));
    CHECK(key_runtime_slot_pending_multi_tap_matches(slot, TEST_MULTI_TAP_KEY, pos));

    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(slot->pending_multi_tap.count == 2);
    CHECK(!key_runtime_slot_pending_multi_tap_pending_hold(slot));

    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(slot->pending_multi_tap.count == 3);
    CHECK(key_runtime_slot_pending_multi_tap_pending_hold(slot));
}

static void test_slot_track_preserves_pending_multi_tap(void) {
    active_key_state_t *slot = key_runtime_primary_slot();
    keypos_t            pos  = test_keypos(5, 1);

    test_reset_state();

    key_runtime_slot_begin_pending_multi_tap(slot, TEST_MULTI_TAP_KEY, pos, TEST_SINGLE_ACTION, 120, 150);
    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(key_runtime_slot_pending_multi_tap_pending_hold(slot));

    key_runtime_slot_track(slot, TEST_MULTI_TAP_KEY, pos, KC_NO, hold_behavior_none(), hold_behavior_none(), 120, 240, 150, false);

    CHECK(slot->keycode == TEST_MULTI_TAP_KEY);
    CHECK(slot->pending_multi_tap.count == 3);
    CHECK(slot->pending_multi_tap.pending_hold);
    CHECK(slot->pending_multi_tap.keycode == TEST_MULTI_TAP_KEY);
}

static void test_resolve_pending_multi_tap_hold_clears_slot_owned_state(void) {
    active_key_state_t *slot = key_runtime_primary_slot();
    keypos_t            pos  = test_keypos(5, 2);
    uint8_t             repeat_count = 0;

    test_reset_state();

    key_runtime_slot_begin_pending_multi_tap(slot, TEST_MULTI_TAP_KEY, pos, TEST_SINGLE_ACTION, 120, 150);
    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(key_runtime_slot_pending_multi_tap_pending_hold(slot));

    fake_time = (uint16_t)(fake_time + 130);

    CHECK(key_runtime_slot_resolve_pending_multi_tap_hold(slot, TEST_MULTI_TAP_KEY, &repeat_count) == TEST_HOLD_ACTION);
    CHECK(repeat_count == 1);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(key_runtime_slot_idle(slot));
}

int main(void) {
    test_slot_pending_multi_tap_ownership_marks_slot_non_idle();
    test_select_slot_for_press_prefers_slot_owning_pending_multi_tap();
    test_slot_pending_multi_tap_lifecycle_helpers();
    test_slot_track_preserves_pending_multi_tap();
    test_resolve_pending_multi_tap_hold_clears_slot_owned_state();

    puts("key_runtime_slot host tests passed");
    return 0;
}
