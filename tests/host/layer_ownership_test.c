#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/state/ownership/layer_ownership.h"

layer_state_t layer_state;

static uint8_t layer_on_calls[LAYER_COUNT];
static uint8_t layer_off_calls[LAYER_COUNT];

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

static layer_state_t test_layer_mask(uint8_t layer) {
    return (layer_state_t)1u << layer;
}

static void test_reset_stubs(void) {
    layer_state = 0;
    memset(layer_on_calls, 0, sizeof(layer_on_calls));
    memset(layer_off_calls, 0, sizeof(layer_off_calls));
    layer_ownership_reset_for_test();
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & test_layer_mask(layer)) != 0;
}

void layer_on(uint8_t layer) {
    CHECK(layer < LAYER_COUNT);
    layer_state |= test_layer_mask(layer);
    layer_on_calls[layer]++;
}

void layer_off(uint8_t layer) {
    CHECK(layer < LAYER_COUNT);
    layer_state &= (layer_state_t)~test_layer_mask(layer);
    layer_off_calls[layer]++;
}

static void test_single_momentary_press_and_release_toggles_layer(void) {
    keypos_t key_pos = test_keypos(1, 2);

    test_reset_stubs();

    layer_ownership_momentary_press(key_pos, 3);
    CHECK(layer_state == test_layer_mask(3));
    CHECK(layer_on_calls[3] == 1);
    CHECK(layer_off_calls[3] == 0);

    CHECK(layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == 0);
    CHECK(layer_on_calls[3] == 1);
    CHECK(layer_off_calls[3] == 1);

    CHECK(!layer_ownership_momentary_release(key_pos));
    CHECK(layer_off_calls[3] == 1);
}

static void test_multiple_keys_share_momentary_layer_reference(void) {
    keypos_t key_a = test_keypos(0, 0);
    keypos_t key_b = test_keypos(0, 1);

    test_reset_stubs();

    layer_ownership_momentary_press(key_a, 2);
    layer_ownership_momentary_press(key_b, 2);

    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_on_calls[2] == 1);
    CHECK(layer_off_calls[2] == 0);

    CHECK(!layer_ownership_momentary_release(key_a));
    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_off_calls[2] == 0);

    CHECK(layer_ownership_momentary_release(key_b));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[2] == 1);
}

static void test_same_key_repress_same_layer_does_not_duplicate_refcount(void) {
    keypos_t key_pos = test_keypos(2, 4);

    test_reset_stubs();

    layer_ownership_momentary_press(key_pos, 1);
    layer_ownership_momentary_press(key_pos, 1);

    CHECK(layer_state == test_layer_mask(1));
    CHECK(layer_on_calls[1] == 1);
    CHECK(layer_off_calls[1] == 0);

    CHECK(layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[1] == 1);
}

static void test_same_key_can_move_its_momentary_binding_between_layers(void) {
    keypos_t key_pos = test_keypos(3, 5);

    test_reset_stubs();

    layer_ownership_momentary_press(key_pos, 1);
    CHECK(layer_state == test_layer_mask(1));
    CHECK(layer_on_calls[1] == 1);

    layer_ownership_momentary_press(key_pos, 4);
    CHECK(layer_state == test_layer_mask(4));
    CHECK(layer_on_calls[1] == 1);
    CHECK(layer_off_calls[1] == 1);
    CHECK(layer_on_calls[4] == 1);

    CHECK(layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[4] == 1);
}

static void test_toggle_lock_state_activates_and_deactivates_layer(void) {
    test_reset_stubs();

    CHECK(layer_ownership_toggle_lock_state(5));
    CHECK(layer_ownership_is_locked(5));
    CHECK(layer_state == test_layer_mask(5));
    CHECK(layer_on_calls[5] == 1);
    CHECK(layer_off_calls[5] == 0);

    CHECK(layer_ownership_toggle_lock_state(5));
    CHECK(!layer_ownership_is_locked(5));
    CHECK(layer_state == 0);
    CHECK(layer_on_calls[5] == 1);
    CHECK(layer_off_calls[5] == 1);
}

static void test_multiple_locked_layers_can_coexist(void) {
    test_reset_stubs();

    CHECK(layer_ownership_set_lock_state(1, true));
    CHECK(layer_ownership_set_lock_state(2, true));

    CHECK(layer_ownership_is_locked(1));
    CHECK(layer_ownership_is_locked(2));
    CHECK(layer_state == (test_layer_mask(1) | test_layer_mask(2)));
    CHECK(layer_on_calls[1] == 1);
    CHECK(layer_on_calls[2] == 1);

    CHECK(layer_ownership_set_lock_state(1, false));
    CHECK(!layer_ownership_is_locked(1));
    CHECK(layer_ownership_is_locked(2));
    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_off_calls[1] == 1);
    CHECK(layer_off_calls[2] == 0);
}

static void test_locked_layer_stays_active_after_momentary_release(void) {
    keypos_t key_pos = test_keypos(6, 1);

    test_reset_stubs();

    CHECK(layer_ownership_set_lock_state(4, true));
    CHECK(layer_ownership_is_locked(4));
    CHECK(layer_state == test_layer_mask(4));
    CHECK(layer_on_calls[4] == 1);

    layer_ownership_momentary_press(key_pos, 4);
    CHECK(layer_state == test_layer_mask(4));
    CHECK(layer_on_calls[4] == 1);

    CHECK(!layer_ownership_momentary_release(key_pos));
    CHECK(layer_ownership_is_locked(4));
    CHECK(layer_state == test_layer_mask(4));
    CHECK(layer_off_calls[4] == 0);

    CHECK(layer_ownership_set_lock_state(4, false));
    CHECK(!layer_ownership_is_locked(4));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[4] == 1);
}

static void test_locking_another_layer_keeps_existing_locks_and_holds(void) {
    keypos_t key_pos = test_keypos(7, 3);

    test_reset_stubs();

    layer_ownership_momentary_press(key_pos, 1);
    CHECK(layer_state == test_layer_mask(1));
    CHECK(layer_on_calls[1] == 1);

    CHECK(layer_ownership_set_lock_state(1, true));
    CHECK(layer_ownership_is_locked(1));
    CHECK(layer_state == test_layer_mask(1));
    CHECK(layer_off_calls[1] == 0);

    CHECK(layer_ownership_set_lock_state(2, true));
    CHECK(layer_ownership_is_locked(1));
    CHECK(layer_ownership_is_locked(2));
    CHECK(layer_state == (test_layer_mask(1) | test_layer_mask(2)));
    CHECK(layer_off_calls[1] == 0);
    CHECK(layer_on_calls[2] == 1);

    CHECK(!layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == (test_layer_mask(1) | test_layer_mask(2)));
    CHECK(layer_off_calls[1] == 0);

    CHECK(layer_ownership_set_lock_state(1, false));
    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_off_calls[1] == 1);

    CHECK(layer_ownership_set_lock_state(2, false));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[2] == 1);
}

static void test_invalid_layer_requests_are_ignored(void) {
    keypos_t key_pos = test_keypos(4, 4);

    test_reset_stubs();

    CHECK(!layer_ownership_set_lock_state(LAYER_COUNT, true));
    CHECK(!layer_ownership_set_lock_state(LAYER_COUNT, false));
    layer_ownership_momentary_press(key_pos, LAYER_COUNT);

    CHECK(layer_state == 0);
    CHECK(!layer_ownership_momentary_release(key_pos));
}

int main(void) {
    test_single_momentary_press_and_release_toggles_layer();
    test_multiple_keys_share_momentary_layer_reference();
    test_same_key_repress_same_layer_does_not_duplicate_refcount();
    test_same_key_can_move_its_momentary_binding_between_layers();
    test_toggle_lock_state_activates_and_deactivates_layer();
    test_multiple_locked_layers_can_coexist();
    test_locked_layer_stays_active_after_momentary_release();
    test_locking_another_layer_keeps_existing_locks_and_holds();
    test_invalid_layer_requests_are_ignored();

    puts("layer_ownership host tests passed");
    return 0;
}
