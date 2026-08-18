#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/key/runtime/queue/pending_release_queue.h"

enum {
    TEST_ACTION_BASE = 0x6200u,
};

static key_runtime_core_state_t test_state;
static uint8_t                  test_blocker_count;

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

key_runtime_core_state_t *key_runtime_core_state(void) {
    return &test_state;
}

uint8_t key_runtime_core_deferred_release_blocker_count(void) {
    return test_blocker_count;
}

static keypos_t test_keypos(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static keyboard_mod_state_t test_mods(uint8_t value) {
    return (keyboard_mod_state_t){
        .real           = value,
        .weak           = (uint8_t)(value + 1u),
        .oneshot        = (uint8_t)(value + 2u),
        .oneshot_locked = (uint8_t)(value + 3u),
    };
}

static bool test_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static void test_reset(void) {
    key_runtime_core_state_reset(&test_state);
    test_blocker_count = 0u;
    CHECK(key_runtime_core_pending_release_validate());
}

static void test_expect_order(uint8_t order, keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, uint16_t owner_token_id) {
    pending_release_t pending = {0};

    CHECK(key_runtime_core_pending_release_at_order(order, &pending));
    CHECK(pending.active);
    CHECK(test_keypos_equal(pending.key_pos, key_pos));
    CHECK(pending.action == action);
    CHECK(pending.mods.real == mods.real);
    CHECK(pending.mods.weak == mods.weak);
    CHECK(pending.mods.oneshot == mods.oneshot);
    CHECK(pending.mods.oneshot_locked == mods.oneshot_locked);
    CHECK(pending.owner_token_id == owner_token_id);
}

static void test_layout_and_reset_contract(void) {
    test_reset();

    CHECK(sizeof(pending_release_slot_t) == 12u);
    CHECK(sizeof(pending_release_t) == 12u);
    // Two 60-slot active bitmaps add 16 bytes; cached authored branch state
    // fits existing press-token and tap-series padding. The default-report
    // owner bitmap adds one bit per matrix key, 8 bytes, with no padding.
    // Retiring the branch-confirm window took tap_series_t from 81 to 60 bytes,
    // which is 1280 bytes across the 60 slots.
    CHECK(sizeof(key_runtime_core_state_t) == 20524u);
    CHECK(test_state.pending_release_count == 0u);
    CHECK(test_state.pending_release_head_index == KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE);
    CHECK(test_state.pending_release_tail_index == KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE);
    CHECK(test_state.pending_release_high_water_mark == 0u);
    CHECK(test_state.pending_release_validation_failure_count == 0u);
}

static void test_fifo_order_survives_slot_reuse(void) {
    const keypos_t             first_key  = test_keypos(0u, 0u);
    const keypos_t             second_key = test_keypos(0u, 1u);
    const keypos_t             third_key  = test_keypos(0u, 2u);
    const keyboard_mod_state_t mods       = test_mods(1u);
    pending_release_t          drained    = {0};

    test_reset();
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(first_key, TEST_ACTION_BASE, mods, false, 0u));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(second_key, TEST_ACTION_BASE + 1u, mods, false, 0u));
    CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
    CHECK(test_keypos_equal(drained.key_pos, first_key));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(third_key, TEST_ACTION_BASE + 2u, mods, false, 0u));

    test_expect_order(0u, second_key, TEST_ACTION_BASE + 1u, mods, 0u);
    test_expect_order(1u, third_key, TEST_ACTION_BASE + 2u, mods, 0u);
    CHECK(!key_runtime_core_pending_release_at_order(2u, &drained));
    CHECK(key_runtime_core_pending_release_validate());
}

static void test_unlink_head_middle_tail_and_only_entry(void) {
    const keypos_t             first_key  = test_keypos(1u, 0u);
    const keypos_t             second_key = test_keypos(1u, 1u);
    const keypos_t             third_key  = test_keypos(1u, 2u);
    const keyboard_mod_state_t mods       = test_mods(2u);

    test_reset();
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(first_key, TEST_ACTION_BASE, mods, false, 0u));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(second_key, TEST_ACTION_BASE + 1u, mods, false, 0u));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(third_key, TEST_ACTION_BASE + 2u, mods, false, 0u));

    key_runtime_core_observe_release_dispatch_drained(second_key, TEST_ACTION_BASE + 1u, mods);
    CHECK(key_runtime_core_pending_release_count() == 2u);
    test_expect_order(0u, first_key, TEST_ACTION_BASE, mods, 0u);
    test_expect_order(1u, third_key, TEST_ACTION_BASE + 2u, mods, 0u);

    key_runtime_core_observe_release_dispatch_drained(first_key, TEST_ACTION_BASE, mods);
    CHECK(key_runtime_core_pending_release_count() == 1u);
    test_expect_order(0u, third_key, TEST_ACTION_BASE + 2u, mods, 0u);

    key_runtime_core_observe_release_dispatch_drained(third_key, TEST_ACTION_BASE + 2u, mods);
    CHECK(key_runtime_core_pending_release_count() == 0u);
    CHECK(test_state.pending_release_head_index == KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE);
    CHECK(test_state.pending_release_tail_index == KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE);

    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(second_key, TEST_ACTION_BASE + 3u, mods, false, 0u));
    key_runtime_core_observe_release_dispatch_drained(second_key, TEST_ACTION_BASE + 3u, mods);
    CHECK(key_runtime_core_pending_release_count() == 0u);
    CHECK(key_runtime_core_pending_release_validate());
}

static void test_active_owner_is_skipped_without_reordering(void) {
    const keypos_t             blocked_key = test_keypos(2u, 0u);
    const keypos_t             second_key  = test_keypos(2u, 1u);
    const keypos_t             third_key   = test_keypos(2u, 2u);
    const keyboard_mod_state_t mods        = test_mods(3u);
    pending_release_t          drained     = {0};
    press_token_t             *owner;

    test_reset();
    owner  = &test_state.press_tokens[0];
    *owner = (press_token_t){
        .active   = true,
        .token_id = 42u,
        .phase    = PRESS_TOKEN_PHASE_HELD,
    };
    test_state.press_token_count = 1u;

    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(blocked_key, TEST_ACTION_BASE, mods, false, 42u));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(second_key, TEST_ACTION_BASE + 1u, mods, false, 0u));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(third_key, TEST_ACTION_BASE + 2u, mods, false, 0u));
    CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
    CHECK(test_keypos_equal(drained.key_pos, second_key));
    test_expect_order(0u, blocked_key, TEST_ACTION_BASE, mods, 42u);
    test_expect_order(1u, third_key, TEST_ACTION_BASE + 2u, mods, 0u);

    owner->active                   = false;
    owner->phase                    = PRESS_TOKEN_PHASE_RELEASE_PENDING;
    owner->pending_release_emission = true;
    test_state.press_token_count    = 0u;
    CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
    CHECK(test_keypos_equal(drained.key_pos, blocked_key));
    CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
    CHECK(test_keypos_equal(drained.key_pos, third_key));
    CHECK(key_runtime_core_pending_release_validate());
}

static void test_duplicate_match_unlinks_oldest(void) {
    const keypos_t             duplicate_key = test_keypos(3u, 0u);
    const keypos_t             middle_key    = test_keypos(3u, 1u);
    const keyboard_mod_state_t mods          = test_mods(4u);

    test_reset();
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(duplicate_key, TEST_ACTION_BASE, mods, false, 11u));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(middle_key, TEST_ACTION_BASE + 1u, mods, false, 0u));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(duplicate_key, TEST_ACTION_BASE, mods, false, 22u));

    key_runtime_core_observe_release_dispatch_drained(duplicate_key, TEST_ACTION_BASE, mods);
    CHECK(key_runtime_core_pending_release_count() == 2u);
    test_expect_order(0u, middle_key, TEST_ACTION_BASE + 1u, mods, 0u);
    test_expect_order(1u, duplicate_key, TEST_ACTION_BASE, mods, 22u);
    CHECK(key_runtime_core_pending_release_validate());
}

static void test_owner_flag_clears_after_final_unlink(void) {
    const keypos_t             first_key  = test_keypos(4u, 0u);
    const keypos_t             second_key = test_keypos(4u, 1u);
    const keyboard_mod_state_t mods       = test_mods(5u);
    press_token_t             *owner;

    test_reset();
    owner  = &test_state.press_tokens[0];
    *owner = (press_token_t){
        .active   = false,
        .token_id = 7u,
        .phase    = PRESS_TOKEN_PHASE_RELEASED,
    };

    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(first_key, TEST_ACTION_BASE, mods, false, 7u));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(second_key, TEST_ACTION_BASE + 1u, mods, false, 7u));
    CHECK(owner->pending_release_emission);
    CHECK(owner->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING);

    key_runtime_core_observe_release_dispatch_drained(first_key, TEST_ACTION_BASE, mods);
    CHECK(owner->pending_release_emission);
    CHECK(owner->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING);

    key_runtime_core_observe_release_dispatch_drained(second_key, TEST_ACTION_BASE + 1u, mods);
    CHECK(!owner->pending_release_emission);
    CHECK(owner->phase == PRESS_TOKEN_PHASE_RELEASED);
    CHECK(key_runtime_core_pending_release_validate());
}

static void test_capacity_overflow_drain_and_complete_reuse(void) {
    const keypos_t             key_pos = test_keypos(5u, 0u);
    const keyboard_mod_state_t mods    = test_mods(6u);
    pending_release_t          drained = {0};

    test_reset();
    for (uint16_t index = 0u; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, (uint16_t)(TEST_ACTION_BASE + index), mods, false, 0u));
    }
    CHECK(!key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, 0x7fffu, mods, false, 0u));
    CHECK(test_state.pending_release_high_water_mark == KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY);
    CHECK(key_runtime_core_pending_release_validate());

    for (uint16_t index = 0u; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
        CHECK(drained.action == (uint16_t)(TEST_ACTION_BASE + index));
    }
    CHECK(key_runtime_core_pending_release_count() == 0u);

    for (uint16_t index = 0u; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, (uint16_t)(TEST_ACTION_BASE + index), mods, false, 0u));
    }
    CHECK(key_runtime_core_pending_release_validate());
}

static void test_long_lived_head_survives_more_than_uint16_operations(void) {
    const keypos_t             blocked_key   = test_keypos(6u, 0u);
    const keypos_t             transient_key = test_keypos(6u, 1u);
    const keyboard_mod_state_t mods          = test_mods(7u);
    pending_release_t          drained       = {0};
    press_token_t             *owner;

    test_reset();
    owner  = &test_state.press_tokens[0];
    *owner = (press_token_t){
        .active   = true,
        .token_id = 42u,
        .phase    = PRESS_TOKEN_PHASE_HELD,
    };
    test_state.press_token_count = 1u;
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(blocked_key, TEST_ACTION_BASE, mods, false, 42u));

    for (uint32_t operation = 0u; operation < 65537u; operation++) {
        uint16_t action = (uint16_t)(TEST_ACTION_BASE + 1u + (operation % 1024u));

        CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(transient_key, action, mods, false, 0u));
        CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
        CHECK(drained.action == action);
        CHECK(test_keypos_equal(drained.key_pos, transient_key));
        CHECK(key_runtime_core_pending_release_count() == 1u);
        if ((operation % 1024u) == 0u) {
            CHECK(key_runtime_core_pending_release_validate());
        }
    }

    owner->active                   = false;
    owner->phase                    = PRESS_TOKEN_PHASE_RELEASE_PENDING;
    owner->pending_release_emission = true;
    test_state.press_token_count    = 0u;
    CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
    CHECK(test_keypos_equal(drained.key_pos, blocked_key));
    CHECK(test_state.pending_release_high_water_mark == 2u);
    CHECK(key_runtime_core_pending_release_validate());
}

static void test_structural_validator_detects_cycle_and_orphan(void) {
    const keypos_t             key_pos = test_keypos(7u, 0u);
    const keyboard_mod_state_t mods    = test_mods(8u);

    test_reset();
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, TEST_ACTION_BASE, mods, false, 0u));
    test_state.pending_releases[test_state.pending_release_tail_index].next_queue_index = test_state.pending_release_head_index;
    CHECK(!key_runtime_core_pending_release_validate());
    CHECK(test_state.pending_release_validation_failure_count == 1u);

    test_reset();
    test_state.pending_releases[3] = (pending_release_slot_t){
        .action           = TEST_ACTION_BASE,
        .next_queue_index = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
        .packed_key_pos   = key_runtime_keypos_pack(key_pos),
        .flags            = KEY_RUNTIME_PENDING_RELEASE_FLAG_ACTIVE,
    };
    CHECK(!key_runtime_core_pending_release_validate());
    CHECK(test_state.pending_release_validation_failure_count == 1u);
}

int main(void) {
    test_layout_and_reset_contract();
    test_fifo_order_survives_slot_reuse();
    test_unlink_head_middle_tail_and_only_entry();
    test_active_owner_is_skipped_without_reordering();
    test_duplicate_match_unlinks_oldest();
    test_owner_flag_clears_after_final_unlink();
    test_capacity_overflow_drain_and_complete_reuse();
    test_long_lived_head_survives_more_than_uint16_operations();
    test_structural_validator_detects_cycle_and_orphan();

    puts("pending release queue tests passed");
    return 0;
}
