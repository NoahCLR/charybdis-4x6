#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/key_runtime_admission.h"

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

static void test_reset_state(void) {
    noah_runtime_shared_state = (runtime_shared_state_t){0};
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

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool pd_mode_locked(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool action_dispatch_is_layer_lock(uint16_t action) {
    (void)action;
    return false;
}

bool is_layer_key(uint16_t keycode) {
    (void)keycode;
    return false;
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    (void)action;
    return NOAH_ACTION_HOLD_KIND_SHARED;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    (void)mt;
    return (delayed_action_mods_t){0};
}

static void test_find_slot_by_position_returns_matching_active_slot(void) {
    active_key_state_t *secondary = key_runtime_slot_at(1);
    keypos_t            pos       = test_keypos(4, 5);

    test_reset_state();
    secondary->keycode = TEST_ACTIVE_KEY;
    secondary->key_pos = pos;

    CHECK(key_runtime_find_slot_by_position(pos) == secondary);
    CHECK(key_runtime_first_active_slot() == secondary);
    CHECK(active_key_matches(TEST_ACTIVE_KEY, pos));
}

static void test_find_reclaimable_slot_returns_pending_multi_tap_owner(void) {
    active_key_state_t *slot = key_runtime_slot_at(1);
    keypos_t            pos  = test_keypos(5, 2);

    test_reset_state();
    slot->pending_multi_tap = (multi_tap_t){
        .keycode = TEST_MULTI_TAP_KEY,
        .key_pos = pos,
        .count   = 1,
    };

    CHECK(key_runtime_find_slot_with_pending_multi_tap(pos) == slot);
    CHECK(key_runtime_find_reclaimable_slot() == slot);
}

static void test_select_slot_for_press_prefers_free_slot_before_reclaim(void) {
    active_key_state_t *primary   = key_runtime_primary_slot();
    active_key_state_t *secondary = key_runtime_slot_at(1);
    keypos_t            target    = test_keypos(6, 1);

    test_reset_state();
    primary->pending_multi_tap = (multi_tap_t){
        .keycode = TEST_MULTI_TAP_KEY,
        .key_pos = test_keypos(1, 1),
        .count   = 1,
    };

    CHECK(key_runtime_select_slot_for_press(target) == secondary);
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

static void test_select_slot_for_press_falls_back_to_primary_when_all_slots_busy(void) {
    active_key_state_t *primary   = key_runtime_primary_slot();
    active_key_state_t *secondary = key_runtime_slot_at(1);

    test_reset_state();
    primary->keycode = TEST_ACTIVE_KEY;
    primary->key_pos = test_keypos(0, 0);
    secondary->keycode = TEST_ACTIVE_KEY;
    secondary->key_pos = test_keypos(0, 1);

    CHECK(key_runtime_select_slot_for_press(test_keypos(0, 2)) == primary);
}

int main(void) {
    test_find_slot_by_position_returns_matching_active_slot();
    test_find_reclaimable_slot_returns_pending_multi_tap_owner();
    test_select_slot_for_press_prefers_free_slot_before_reclaim();
    test_select_slot_for_press_prefers_slot_owning_pending_multi_tap();
    test_select_slot_for_press_falls_back_to_primary_when_all_slots_busy();

    puts("key_runtime_admission host tests passed");
    return 0;
}
