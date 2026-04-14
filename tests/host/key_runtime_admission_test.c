#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/interaction/handled_key.h"
#include "users/noah/lib/key/runtime/key_runtime_admission.h"
#include "users/noah/lib/key/runtime/key_runtime_index_internal.h"
#include "host_runtime_reset_fixture.h"

enum {
    TEST_MULTI_TAP_KEY = SAFE_RANGE + 0x60,
    TEST_ACTIVE_KEY    = SAFE_RANGE + 0x61,
};

layer_state_t layer_state;

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

static void test_track_slot(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos) {
    key_runtime_slot_track(slot, keycode, key_pos, key_runtime_slot_interaction_default(), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
}

static void test_reset_state(void) {
    host_runtime_fixture_reset_userspace_runtime();
}

uint16_t timer_read(void) {
    return 0;
}

uint16_t timer_elapsed(uint16_t last) {
    (void)last;
    return 0;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    (void)layer_num;
    (void)row;
    (void)column;
    return KC_TRNS;
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

void clear_mods(void) {}
void clear_weak_mods(void) {}
void clear_oneshot_mods(void) {}
void clear_oneshot_locked_mods(void) {}
void send_keyboard_report(void) {}

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

key_runtime_slot_hold_strategy_t handled_key_resolution_hold_strategy(handled_key_resolution_t key) {
    (void)key;
    return KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

uint16_t handled_key_resolution_tap_action(handled_key_resolution_t key) {
    return key.step.tap.present ? key.step.tap.action : KC_NO;
}

uint8_t handled_key_resolution_tap_repeat_count(handled_key_resolution_t key) {
    return handled_key_resolution_tap_action(key) == KC_NO ? 0 : 1;
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

pd_mode_mask_t handled_key_resolution_pd_mode(handled_key_resolution_t key) {
    (void)key;
    return 0;
}

handled_key_resolution_ctx_t handled_key_resolution_ctx_live(keypos_t key_pos) {
    return handled_key_resolution_ctx_make(key_pos, (layer_state_t)1u << 0);
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

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    (void)mt;
    return (delayed_action_mods_t){0};
}

static void test_find_slot_by_position_returns_matching_active_slot(void) {
    active_key_state_t *slot = test_slot(4, 5);
    keypos_t            pos  = test_keypos(4, 5);

    test_reset_state();
    test_track_slot(slot, TEST_ACTIVE_KEY, pos);

    CHECK(key_runtime_find_slot_by_position(pos) == slot);
    CHECK(key_runtime_first_active_slot() == slot);
    CHECK(active_key_matches(TEST_ACTIVE_KEY, pos));
}

static void test_find_slot_with_pending_multi_tap_returns_position_owner(void) {
    active_key_state_t *slot = test_slot(5, 2);
    keypos_t            pos  = test_keypos(5, 2);

    test_reset_state();
    key_runtime_slot_begin_pending_multi_tap(slot, TEST_MULTI_TAP_KEY, pos, KC_NO, 0, CUSTOM_TAP_HOLD_TERM, CUSTOM_MULTI_TAP_TERM, false);

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
    key_runtime_slot_begin_pending_multi_tap(slot, TEST_MULTI_TAP_KEY, target, KC_NO, 0, CUSTOM_TAP_HOLD_TERM, CUSTOM_MULTI_TAP_TERM, false);

    CHECK(key_runtime_select_slot_for_press(target) == slot);
}

static void test_select_slot_for_press_keeps_distinct_active_positions_independent(void) {
    active_key_state_t *slot_a = test_slot(0, 0);
    active_key_state_t *slot_b = test_slot(0, 1);
    active_key_state_t *slot_c = test_slot(0, 2);

    test_reset_state();
    test_track_slot(slot_a, TEST_ACTIVE_KEY, test_keypos(0, 0));
    test_track_slot(slot_b, TEST_ACTIVE_KEY, test_keypos(0, 1));

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
