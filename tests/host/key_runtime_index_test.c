#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/key/runtime/key_runtime_index.h"
#include "users/noah/lib/key/runtime/key_runtime_state.h"
#include "users/noah/lib/state/runtime/runtime_context.h"
#include "users/noah/lib/state/runtime/runtime_shared_state.h"

static uint16_t fake_time;

enum {
    TEST_KEY_A = SAFE_RANGE + 0x01,
    TEST_KEY_B = SAFE_RANGE + 0x02,
    TEST_KEY_C = SAFE_RANGE + 0x03,
    TEST_KEY_D = SAFE_RANGE + 0x04,
    TEST_KEY_E = SAFE_RANGE + 0x05,
    TEST_KEY_F = SAFE_RANGE + 0x06,
    TEST_KEY_G = SAFE_RANGE + 0x07,
    TEST_KEY_H = SAFE_RANGE + 0x08,
    TEST_KEY_I = SAFE_RANGE + 0x09,
    TEST_KEY_J = SAFE_RANGE + 0x0A,
    TEST_TAP_1 = SAFE_RANGE + 0x11,
    TEST_TAP_2 = SAFE_RANGE + 0x12,
    TEST_TAP_3 = SAFE_RANGE + 0x13,
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

static active_key_state_t *test_slot(keypos_t key_pos) {
    return key_runtime_slot_for_position(key_pos);
}

static key_runtime_slot_interaction_t test_interaction(uint8_t preview_layer, uint16_t flags) {
    key_runtime_slot_interaction_t interaction = key_runtime_slot_interaction_default();
    interaction.flags                         = flags;
    interaction.contract.hold.preview_layer  = preview_layer;
    return interaction;
}

static void test_reset_state(void) {
    fake_time = 1000;
    noah_runtime_context_reset_for_test(noah_runtime_context());
}

static void test_expect_active_order(const keypos_t *positions, uint8_t count) {
    CHECK(key_runtime_active_slot_count() == count);
    for (uint8_t order = 0; order < count; order++) {
        CHECK(key_runtime_active_slot_by_order(order) == test_slot(positions[order]));
    }
    CHECK(key_runtime_active_slot_by_order(count) == NULL);
}

static void test_expect_pending_order(const keypos_t *positions, uint8_t count) {
    CHECK(key_runtime_pending_multi_tap_slot_count() == count);
    for (uint8_t order = 0; order < count; order++) {
        CHECK(key_runtime_pending_multi_tap_slot_by_order(order) == test_slot(positions[order]));
    }
    CHECK(key_runtime_pending_multi_tap_slot_by_order(count) == NULL);
}

static void test_active_slot_order_updates_after_track_and_reset(void) {
    static const keypos_t expected_initial[] = {
        {.row = 0, .col = 0},
        {.row = 0, .col = 2},
        {.row = 0, .col = 4},
    };
    static const keypos_t expected_after_middle_reset[] = {
        {.row = 0, .col = 0},
        {.row = 0, .col = 4},
    };
    static const keypos_t expected_after_first_reset[] = {
        {.row = 0, .col = 4},
    };

    test_reset_state();

    key_runtime_slot_track(test_slot(test_keypos(0, 4)), TEST_KEY_A, test_keypos(0, 4), test_interaction(UINT8_MAX, 0), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_track(test_slot(test_keypos(0, 0)), TEST_KEY_B, test_keypos(0, 0), test_interaction(UINT8_MAX, 0), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_track(test_slot(test_keypos(0, 2)), TEST_KEY_C, test_keypos(0, 2), test_interaction(UINT8_MAX, 0), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);

    test_expect_active_order(expected_initial, 3);

    key_runtime_slot_reset(test_slot(test_keypos(0, 2)));
    test_expect_active_order(expected_after_middle_reset, 2);

    key_runtime_slot_reset(test_slot(test_keypos(0, 0)));
    test_expect_active_order(expected_after_first_reset, 1);

    key_runtime_slot_reset(test_slot(test_keypos(0, 4)));
    test_expect_active_order(NULL, 0);
    CHECK(key_runtime_preview_owner_slot() == NULL);
    CHECK(key_runtime_pending_fallback_slot() == NULL);
}

static void test_pending_multi_tap_order_updates_after_begin_reset_and_flush(void) {
    static const keypos_t expected_initial[] = {
        {.row = 1, .col = 0},
        {.row = 1, .col = 2},
        {.row = 1, .col = 4},
    };
    static const keypos_t expected_after_flush[] = {
        {.row = 1, .col = 0},
        {.row = 1, .col = 4},
    };
    static const keypos_t expected_after_reset[] = {
        {.row = 1, .col = 4},
    };

    key_runtime_slot_pending_multi_tap_flush_t flush;

    test_reset_state();

    key_runtime_slot_begin_pending_multi_tap(test_slot(test_keypos(1, 4)), TEST_KEY_D, test_keypos(1, 4), TEST_TAP_1, 1, 120, 180, true);
    key_runtime_slot_begin_pending_multi_tap(test_slot(test_keypos(1, 0)), TEST_KEY_E, test_keypos(1, 0), TEST_TAP_2, 1, 120, 180, true);
    key_runtime_slot_begin_pending_multi_tap(test_slot(test_keypos(1, 2)), TEST_KEY_F, test_keypos(1, 2), TEST_TAP_3, 1, 120, 180, true);

    test_expect_pending_order(expected_initial, 3);

    flush = key_runtime_slot_take_pending_multi_tap_flush(test_slot(test_keypos(1, 2)));
    CHECK(flush.handled);
    CHECK(flush.action == TEST_TAP_3);
    CHECK(flush.repeat_count == 1);
    test_expect_pending_order(expected_after_flush, 2);

    key_runtime_slot_reset_pending_multi_tap(test_slot(test_keypos(1, 0)));
    test_expect_pending_order(expected_after_reset, 1);

    key_runtime_slot_reset_pending_multi_tap(test_slot(test_keypos(1, 4)));
    test_expect_pending_order(NULL, 0);
}

static void test_preview_owner_updates_immediately_after_mutations(void) {
    keypos_t later_preview   = test_keypos(2, 4);
    keypos_t earlier_preview = test_keypos(2, 1);

    test_reset_state();

    key_runtime_slot_track(test_slot(later_preview), TEST_KEY_G, later_preview, test_interaction(4, 0), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    CHECK(key_runtime_preview_owner_slot() == test_slot(later_preview));

    key_runtime_slot_track(test_slot(earlier_preview), TEST_KEY_H, earlier_preview, test_interaction(2, 0), KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW);
    CHECK(key_runtime_preview_owner_slot() == test_slot(earlier_preview));

    key_runtime_slot_set_held_action_keycode(test_slot(earlier_preview), TEST_KEY_H);
    CHECK(key_runtime_preview_owner_slot() == test_slot(later_preview));

    key_runtime_slot_reset(test_slot(later_preview));
    CHECK(key_runtime_preview_owner_slot() == NULL);
}

static void test_pending_fallback_updates_immediately_after_mutations(void) {
    keypos_t later_fallback   = test_keypos(3, 3);
    keypos_t earlier_fallback = test_keypos(3, 1);

    test_reset_state();

    key_runtime_slot_track(test_slot(later_fallback), TEST_KEY_I, later_fallback, test_interaction(UINT8_MAX, HANDLED_KEY_FLAG_FALLBACK_HOLD), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    CHECK(key_runtime_pending_fallback_slot() == test_slot(later_fallback));

    key_runtime_slot_track(test_slot(earlier_fallback), TEST_KEY_J, earlier_fallback, test_interaction(UINT8_MAX, HANDLED_KEY_FLAG_FALLBACK_HOLD), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    CHECK(key_runtime_pending_fallback_slot() == test_slot(earlier_fallback));

    key_runtime_slot_set_held_action_keycode(test_slot(earlier_fallback), TEST_KEY_J);
    CHECK(key_runtime_pending_fallback_slot() == test_slot(later_fallback));

    key_runtime_slot_reset(test_slot(later_fallback));
    CHECK(key_runtime_pending_fallback_slot() == NULL);
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

handled_key_resolution_t handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count) {
    return (handled_key_resolution_t){
        .keycode   = keycode,
        .tap_count = tap_count,
    };
}

handled_key_resolution_ctx_t handled_key_resolution_ctx_live(keypos_t key_pos) {
    return handled_key_resolution_ctx_make(key_pos, 0);
}

handled_key_materialized_t handled_key_materialize(handled_key_resolution_t resolution, handled_key_resolution_ctx_t ctx) {
    (void)ctx;
    handled_key_materialized_t materialized = handled_key_materialized_default(resolution);
    materialized.tap_action                 = resolution.keycode;
    materialized.tap_repeat_count           = 1;
    materialized.tap_has_more_taps          = false;
    materialized.tap_resolves_on_press      = false;
    return materialized;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
}

int main(void) {
    test_active_slot_order_updates_after_track_and_reset();
    test_pending_multi_tap_order_updates_after_begin_reset_and_flush();
    test_preview_owner_updates_immediately_after_mutations();
    test_pending_fallback_updates_immediately_after_mutations();
    puts("key_runtime_index host tests passed");
    return 0;
}
