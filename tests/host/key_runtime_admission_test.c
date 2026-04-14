#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/runtime/key_runtime_admission.h"
#include "users/noah/lib/state/runtime/runtime_shared_state.h"

enum {
    TEST_MULTI_TAP_KEY = SAFE_RANGE + 0x60,
    TEST_ACTIVE_KEY    = SAFE_RANGE + 0x61,
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

static active_key_state_t *test_slot(uint8_t row, uint8_t col) {
    return key_runtime_slot_for_position(test_keypos(row, col));
}

static void test_reset_state(void) {
    runtime_shared_state_reset(&noah_runtime_shared_state);
}

uint16_t timer_read(void) {
    return 0;
}

uint16_t timer_elapsed(uint16_t last) {
    (void)last;
    return 0;
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
    (void)keycode;
    (void)tap_count;
    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    (void)keycode;
    (void)count;
    return false;
}

handled_key_resolution_t handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count) {
    (void)keycode;
    (void)tap_count;
    return (handled_key_resolution_t){0};
}

bool handled_key_resolution_has_multi_tap(handled_key_resolution_t key) {
    (void)key;
    return false;
}

bool handled_key_resolution_is_momentary_layer(handled_key_resolution_t key) {
    (void)key;
    return false;
}

bool handled_key_resolution_is_layer_tap(handled_key_resolution_t key) {
    (void)key;
    return false;
}

hold_behavior_t handled_key_resolution_hold(handled_key_resolution_t key) {
    return key.step.hold;
}

hold_behavior_t handled_key_resolution_long_hold(handled_key_resolution_t key) {
    return key.step.long_hold;
}

hold_behavior_t handled_key_resolution_hold_at_position(handled_key_resolution_t key, keypos_t key_pos) {
    (void)key_pos;
    return handled_key_resolution_hold(key);
}

hold_behavior_t handled_key_resolution_long_hold_at_position(handled_key_resolution_t key, keypos_t key_pos) {
    (void)key_pos;
    return handled_key_resolution_long_hold(key);
}

key_runtime_slot_hold_strategy_t handled_key_resolution_hold_strategy(handled_key_resolution_t key) {
    (void)key;
    return KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

key_runtime_slot_hold_strategy_t handled_key_resolution_hold_strategy_at_position(handled_key_resolution_t key, keypos_t key_pos) {
    (void)key_pos;
    return handled_key_resolution_hold_strategy(key);
}

uint16_t handled_key_resolution_tap_action(handled_key_resolution_t key) {
    return key.step.tap.present ? key.step.tap.action : KC_NO;
}

uint16_t handled_key_resolution_tap_action_at_position(handled_key_resolution_t key, keypos_t key_pos) {
    (void)key_pos;
    return handled_key_resolution_tap_action(key);
}

uint8_t handled_key_resolution_tap_repeat_count(handled_key_resolution_t key) {
    return handled_key_resolution_tap_action(key) == KC_NO ? 0 : 1;
}

uint8_t handled_key_resolution_tap_repeat_count_at_position(handled_key_resolution_t key, keypos_t key_pos) {
    (void)key_pos;
    return handled_key_resolution_tap_repeat_count(key);
}

bool handled_key_resolution_tap_resolves_on_press(handled_key_resolution_t key) {
    (void)key;
    return false;
}

bool handled_key_resolution_uses_fallback_hold(handled_key_resolution_t key) {
    (void)key;
    return false;
}

bool handled_key_resolution_uses_implicit_hold(handled_key_resolution_t key) {
    (void)key;
    return false;
}

uint8_t handled_key_resolution_layer(handled_key_resolution_t key) {
    (void)key;
    return UINT8_MAX;
}

uint8_t handled_key_resolution_layer_at_position(handled_key_resolution_t key, keypos_t key_pos) {
    (void)key_pos;
    return handled_key_resolution_layer(key);
}

pd_mode_mask_t handled_key_resolution_pd_mode(handled_key_resolution_t key) {
    (void)key;
    return 0;
}

pd_mode_mask_t handled_key_resolution_pd_mode_at_position(handled_key_resolution_t key, keypos_t key_pos) {
    (void)key_pos;
    return handled_key_resolution_pd_mode(key);
}

uint16_t handled_key_resolution_flags_at_position(handled_key_resolution_t key, keypos_t key_pos) {
    (void)key_pos;
    return key.flags;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool is_layer_key(uint16_t keycode) {
    (void)keycode;
    return false;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    (void)mt;
    return (delayed_action_mods_t){0};
}

static void test_find_slot_by_position_returns_matching_active_slot(void) {
    active_key_state_t *slot = test_slot(4, 5);
    keypos_t            pos  = test_keypos(4, 5);

    test_reset_state();
    slot->owner.keycode = TEST_ACTIVE_KEY;
    slot->owner.key_pos = pos;

    CHECK(key_runtime_find_slot_by_position(pos) == slot);
    CHECK(key_runtime_first_active_slot() == slot);
    CHECK(active_key_matches(TEST_ACTIVE_KEY, pos));
}

static void test_find_slot_with_pending_multi_tap_returns_position_owner(void) {
    active_key_state_t *slot = test_slot(5, 2);
    keypos_t            pos  = test_keypos(5, 2);

    test_reset_state();
    slot->pending_multi_tap = (multi_tap_t){
        .keycode = TEST_MULTI_TAP_KEY,
        .key_pos = pos,
        .count   = 1,
    };

    CHECK(key_runtime_find_slot_with_pending_multi_tap(pos) == slot);
}

static void test_select_slot_for_press_returns_direct_position_slot(void) {
    keypos_t            target = test_keypos(6, 1);
    active_key_state_t *slot   = test_slot(6, 1);

    test_reset_state();

    CHECK(key_runtime_select_slot_for_press(target) == slot);
}

static void test_select_slot_for_press_reuses_position_with_pending_multi_tap(void) {
    keypos_t            target = test_keypos(6, 2);
    active_key_state_t *slot   = test_slot(6, 2);

    test_reset_state();
    slot->pending_multi_tap = (multi_tap_t){
        .keycode = TEST_MULTI_TAP_KEY,
        .key_pos = target,
        .count   = 1,
    };

    CHECK(key_runtime_select_slot_for_press(target) == slot);
}

static void test_select_slot_for_press_keeps_distinct_active_positions_independent(void) {
    active_key_state_t *slot_a = test_slot(0, 0);
    active_key_state_t *slot_b = test_slot(0, 1);
    active_key_state_t *slot_c = test_slot(0, 2);

    test_reset_state();
    slot_a->owner.keycode = TEST_ACTIVE_KEY;
    slot_a->owner.key_pos = test_keypos(0, 0);
    slot_b->owner.keycode = TEST_ACTIVE_KEY;
    slot_b->owner.key_pos = test_keypos(0, 1);

    CHECK(key_runtime_select_slot_for_press(test_keypos(0, 2)) == slot_c);
    CHECK(slot_a->owner.keycode == TEST_ACTIVE_KEY);
    CHECK(slot_b->owner.keycode == TEST_ACTIVE_KEY);
}

int main(void) {
    test_find_slot_by_position_returns_matching_active_slot();
    test_find_slot_with_pending_multi_tap_returns_position_owner();
    test_select_slot_for_press_returns_direct_position_slot();
    test_select_slot_for_press_reuses_position_with_pending_multi_tap();
    test_select_slot_for_press_keeps_distinct_active_positions_independent();

    puts("key_runtime_admission host tests passed");
    return 0;
}
