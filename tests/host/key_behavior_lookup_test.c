#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/custom_key_shortcut.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/key/behavior/handled_key.h"
#include "users/noah/lib/key/behavior/key_behavior_lookup.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/profile/runtime/effective_key_behavior_runtime.h"
#include "users/noah/lib/profile/runtime/profile_action_runtime_v1.h"

enum {
    TEST_LAYER_TAP_KEY              = 0x04,
    TEST_AUTHORED_LAYER_TAP         = LT(2, TEST_LAYER_TAP_KEY),
    TEST_BARE_LAYER_TAP             = LT(3, TEST_LAYER_TAP_KEY),
    TEST_PD_MODE_KEY                = SAFE_RANGE + 0x0Fu,
    TEST_TAP_ACTION                 = SAFE_RANGE + 0x10u,
    TEST_PD_MODE_LOCK_KEY           = SAFE_RANGE + 0x11u,
    TEST_TRANSPARENT_PD_KEY         = SAFE_RANGE + 0x12u,
    TEST_TRANSPARENT_KEY            = SAFE_RANGE + 0x13u,
    TEST_MULTI_TAP_KEY              = SAFE_RANGE + 0x14u,
    TEST_TRANSPARENT_HOLD_KEY       = SAFE_RANGE + 0x15u,
    TEST_TRANSPARENT_HOLD_OTHER_KEY = SAFE_RANGE + 0x16u,
    TEST_TRANSPARENT_LONG_HOLD_KEY  = SAFE_RANGE + 0x17u,
    TEST_HOLD_ACTION                = SAFE_RANGE + 0x18u,
    TEST_LONG_HOLD_ACTION           = SAFE_RANGE + 0x19u,
    TEST_HOLD_BEHAVIOR_KEY          = SAFE_RANGE + 0x1Au,
    TEST_LONG_HOLD_BEHAVIOR_KEY     = SAFE_RANGE + 0x1Bu,
    TEST_CHAIN_MULTI_TAP_KEY        = SAFE_RANGE + 0x1Cu,
    TEST_STACKED_PD_KEY             = SAFE_RANGE + 0x1Du,
    TEST_OTHER_PD_MODE_KEY          = SAFE_RANGE + 0x1Eu,
    TEST_REPEAT_KEY                 = SAFE_RANGE + 0x1Fu,
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

static void test_check_momentary_helper_matches_materialization(handled_key_resolution_t resolution, handled_key_resolution_ctx_t ctx, bool expected) {
    handled_key_materialized_t materialized = handled_key_materialize(resolution, ctx);

    CHECK(handled_key_resolution_materializes_momentary_layer(&resolution, &ctx) == expected);
    CHECK(((materialized.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0u) == expected);
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
        .keycode = TEST_REPEAT_KEY,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_C)},
                [1] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HOLD_ACTION)},
                [2] = {.long_hold = TAP_AT_HOLD_THRESHOLD(TEST_LONG_HOLD_ACTION)},
            },
    },
    {
        .keycode = TT(5),
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_A)},
            },
    },
    {
        .keycode = CUSTOM_KEY_100,
        .tap_counts =
            {
                [1] = {.tap = TAP_SENDS(KC_B)},
            },
    },
    // Rows without steps, with hold times of their own.
    {.keycode = KC_DELETE, .tap_hold_term = 120},
    {.keycode = TT(4), .tap_hold_term = 120, .multi_tap_term = 180},
};

// CUSTOM_KEY_100 (with a row) and CUSTOM_KEY_101 (without) have shortcuts;
// CUSTOM_KEY_102 has neither (D-F26).
noah_custom_key_shortcut_t noah_custom_key_shortcut(uint8_t slot) {
    if (slot == 100u || slot == 101u) return (noah_custom_key_shortcut_t){.mods = 0x41u, .key = KC_K};
    return (noah_custom_key_shortcut_t){0};
}

const uint8_t key_behavior_count = ARRAY_SIZE(key_behaviors);

const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {
    [0] = {.mode_flag = 1u, .keycode = TEST_PD_MODE_KEY, .lock_action = TEST_PD_MODE_LOCK_KEY},
    [1] = {.mode_flag = 2u, .keycode = TEST_OTHER_PD_MODE_KEY, .lock_action = TEST_OTHER_PD_MODE_KEY + 1u},
};

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

bool layer_ownership_goto(uint8_t layer) {
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

void pointer_layer_policy_sync_layer_ownership_anchor(void) {}

void pointer_layer_policy_settle_record(void) {}

void pointer_layer_policy_take_back_qmk_toggle(uint16_t keycode, const keyrecord_t *record) {
    (void)keycode;
    (void)record;
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

// Every LT/MT/OSM key is the runtime's, row or not (D-F27): a bare LT() holds
// its layer past the dual-role time, like an empty row. It fell back to QMK
// before. Without its row (a participation bypass) an authored one is the same.
static void test_bare_dual_role_keys_use_the_runtime_default(void) {
    const uint16_t bare[] = {TEST_BARE_LAYER_TAP, MT(MOD_LCTL, KC_B), OSM(MOD_RSFT)};

    for (uint8_t i = 0; i < ARRAY_SIZE(bare); i++) {
        key_behavior_view_t behavior = key_behavior_lookup(bare[i]);

        CHECK(behavior.handled);
        CHECK(!behavior.is_momentary_layer);
        CHECK(behavior.is_layer_tap == (i == 0u));
        CHECK(behavior.tap_hold_term == TAPPING_TERM);
        CHECK(!key_behavior_step_present(behavior.single));
    }

    key_behavior_view_t bypassed = key_behavior_lookup_without_row(TEST_AUTHORED_LAYER_TAP);
    CHECK(bypassed.handled && bypassed.is_layer_tap && !bypassed.has_multi_tap);
    CHECK(bypassed.tap_hold_term == TAPPING_TERM);
    CHECK(key_behavior_lookup(TT(3)).tap_hold_term == TAPPING_TERM);
    CHECK(key_behavior_lookup(OSL(3)).tap_hold_term == TAPPING_TERM);
}

static void test_authored_lt_uses_custom_runtime(void) {
    key_behavior_view_t behavior = key_behavior_lookup(TEST_AUTHORED_LAYER_TAP);

    CHECK(behavior.handled);
    // Its layer is a hold past the tap-hold term, not held from the press
    // like MO(): a tap must never turn the layer on.
    CHECK(!behavior.is_momentary_layer);
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

// TT(n) holds its layer like MO(n), and its TAPPING_TOGGLE-th tap (QMK's
// default, 5) sends LOCK_LAYER(n). Every other tap does nothing.
static void test_tap_toggle_layer_holds_and_locks_on_its_toggle_tap(void) {
    key_behavior_view_t behavior = key_behavior_lookup(TT(3));

    CHECK(behavior.handled);
    CHECK(behavior.is_momentary_layer);
    CHECK(!behavior.is_layer_tap);
    CHECK(behavior.has_multi_tap);
    CHECK(behavior.authored_tap_depth == 5u);
    CHECK(!key_behavior_step_present(behavior.single));
    for (uint8_t tap_count = 2u; tap_count < 5u; tap_count++) {
        CHECK(!key_behavior_step_present(key_behavior_view_step(&behavior, tap_count)));
    }
    key_behavior_step_t toggle = key_behavior_view_step(&behavior, 5u);
    CHECK(toggle.tap.present && toggle.tap.action == LOCK_LAYER(3));
    CHECK(!toggle.hold.present && !toggle.long_hold.present);
    CHECK(!key_behavior_view_has_more_taps(&behavior, 5u));

    handled_key_resolution_t resolution = handled_key_lookup(TT(3));
    CHECK(handled_key_resolution_is_handled(resolution));
    CHECK(resolution.layer == 3u);
}

// An authored row replaces TT()'s built-in taps; a layer the keyboard does not
// have is not a momentary layer at all.
static void test_tap_toggle_layer_defers_to_an_authored_row_and_the_layer_count(void) {
    key_behavior_view_t authored = key_behavior_lookup(TT(5));

    CHECK(authored.handled);
    CHECK(authored.is_momentary_layer);
    CHECK(authored.authored_tap_depth == 1u);
    CHECK(authored.single.tap.action == KC_A);
    CHECK(!key_behavior_step_present(key_behavior_view_step(&authored, 5u)));

    key_behavior_view_t outside = key_behavior_lookup(TT(LAYER_COUNT));
    CHECK(!outside.handled);
    CHECK(!outside.is_momentary_layer);
    CHECK(outside.authored_tap_depth == 0u);
}

// A row without steps is no row for the key's taps and for whether the
// runtime handles it (D-F27): a plain key stays QMK's, and TT() keeps its
// built-in taps. Its hold times apply to a key the runtime handles anyway.
static void test_row_without_steps_is_no_row(void) {
    key_behavior_view_t plain = key_behavior_lookup(KC_DELETE);
    CHECK(!plain.handled);
    CHECK(plain.authored_tap_depth == 0u && !key_behavior_step_present(plain.single));

    key_behavior_view_t toggle = key_behavior_lookup(TT(4));
    CHECK(toggle.handled && toggle.is_momentary_layer);
    CHECK(toggle.authored_tap_depth == 5u && toggle.has_multi_tap);
    CHECK(key_behavior_view_step(&toggle, 5u).tap.action == LOCK_LAYER(4));
    CHECK(toggle.tap_hold_term == 120u && toggle.multi_tap_term == 180u);
}

// An empty tap at a later count repeats the first tap the row sets, once per
// press; a row without one repeats the key's own tap (D-F33).
static void test_empty_later_tap_repeats_the_first_tap(void) {
    keypos_t                   key_pos = test_keypos(1, 1);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(0, key_pos, TEST_REPEAT_KEY);
    layer_state = 1u;

    materialized = test_materialize(handled_key_lookup_tap_count(TEST_REPEAT_KEY, 2u), key_pos);
    CHECK(materialized.tap_action == KC_C);
    CHECK(materialized.tap_repeat_count == 2u);
    materialized = test_materialize(handled_key_lookup_tap_count(TEST_REPEAT_KEY, 3u), key_pos);
    CHECK(materialized.tap_action == KC_C);
    CHECK(materialized.tap_repeat_count == 3u);
    materialized = test_materialize(handled_key_lookup_tap_count(KC_DELETE, 2u), key_pos);
    CHECK(materialized.tap_action == KC_DELETE);
    CHECK(materialized.tap_repeat_count == 2u);
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

static void test_handled_resolution_searches_authored_rows_once(void) {
    key_behavior_lookup_test_counters_t counters;

    key_behavior_lookup_test_counters_reset();
    (void)handled_key_lookup(TEST_MULTI_TAP_KEY);
    key_behavior_lookup_test_counters_snapshot(&counters);
    CHECK(counters.search_count == 1u);
    CHECK(counters.row_comparison_count > 0u);

    key_behavior_lookup_test_counters_reset();
    (void)handled_key_lookup_tap_count(TEST_MULTI_TAP_KEY, 2u);
    key_behavior_lookup_test_counters_snapshot(&counters);
    CHECK(counters.search_count == 1u);
    CHECK(counters.row_comparison_count > 0u);

    key_behavior_lookup_test_counters_reset();
    (void)handled_key_lookup(KC_A);
    key_behavior_lookup_test_counters_snapshot(&counters);
    CHECK(counters.search_count == 1u);
    CHECK(counters.row_comparison_count == key_behavior_count);
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

    // A lower LT() lends its hold: MO(layer) once held past the term, not a
    // layer held from the press.
    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY), key_pos);
    CHECK(materialized.hold.present);
    CHECK(materialized.hold.action == MO(3));
    CHECK(materialized.hold.mode == HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE);
    CHECK(materialized.hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT);
    CHECK(materialized.layer == UINT8_MAX);
    CHECK((materialized.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) == 0);
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
    CHECK(materialized.hold.present);
    CHECK(materialized.hold.action == MO(3));
    CHECK(materialized.layer == UINT8_MAX);
    CHECK((materialized.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) == 0);
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

static void test_momentary_layer_query_matches_hold_materialization(void) {
    keypos_t                     direct_key_pos    = test_keypos(2, 0);
    keypos_t                     inherited_key_pos = test_keypos(2, 1);
    keypos_t                     chained_key_pos   = test_keypos(2, 2);
    keypos_t                     plain_key_pos     = test_keypos(2, 3);
    keypos_t                     tap_only_key_pos  = test_keypos(2, 4);
    keypos_t                     long_only_key_pos = test_keypos(2, 5);
    handled_key_resolution_t     resolution;
    handled_key_resolution_ctx_t ctx;

    test_reset_keymap();
    resolution = handled_key_lookup(MO(4));
    ctx        = handled_key_resolution_ctx_live(direct_key_pos);
    test_check_momentary_helper_matches_materialization(resolution, ctx, true);

    test_reset_keymap();
    test_set_keymap_key(1, inherited_key_pos, TEST_BARE_LAYER_TAP);
    test_set_keymap_key(2, inherited_key_pos, TEST_TRANSPARENT_HOLD_KEY);
    resolution = handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY);
    ctx        = handled_key_resolution_ctx_make(inherited_key_pos, (layer_state_t)1u << 2);
    test_check_momentary_helper_matches_materialization(resolution, ctx, false);
    // A lower LT() lends a hold past the term, never a layer from the press.
    ctx.active_layers |= (layer_state_t)1u << 1;
    test_check_momentary_helper_matches_materialization(resolution, ctx, false);

    test_reset_keymap();
    test_set_keymap_key(0, chained_key_pos, MO(3));
    test_set_keymap_key(1, chained_key_pos, TEST_TRANSPARENT_HOLD_KEY);
    test_set_keymap_key(2, chained_key_pos, TEST_TRANSPARENT_HOLD_KEY);
    resolution = handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY);
    ctx        = handled_key_resolution_ctx_make(chained_key_pos, ((layer_state_t)1u << 0) | ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2));
    test_check_momentary_helper_matches_materialization(resolution, ctx, true);

    test_reset_keymap();
    test_set_keymap_key(1, plain_key_pos, KC_V);
    test_set_keymap_key(2, plain_key_pos, TEST_TRANSPARENT_HOLD_KEY);
    resolution = handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY);
    ctx        = handled_key_resolution_ctx_make(plain_key_pos, ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2));
    test_check_momentary_helper_matches_materialization(resolution, ctx, false);

    test_reset_keymap();
    test_set_keymap_key(2, inherited_key_pos, TEST_TRANSPARENT_HOLD_KEY);
    resolution = handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY);
    ctx        = handled_key_resolution_ctx_make(inherited_key_pos, (layer_state_t)1u << 2);
    test_check_momentary_helper_matches_materialization(resolution, ctx, false);

    test_reset_keymap();
    test_set_keymap_key(1, tap_only_key_pos, TEST_BARE_LAYER_TAP);
    test_set_keymap_key(2, tap_only_key_pos, TEST_TRANSPARENT_KEY);
    resolution = handled_key_lookup(TEST_TRANSPARENT_KEY);
    ctx        = handled_key_resolution_ctx_make(tap_only_key_pos, ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2));
    test_check_momentary_helper_matches_materialization(resolution, ctx, false);

    test_reset_keymap();
    test_set_keymap_key(1, long_only_key_pos, TEST_BARE_LAYER_TAP);
    test_set_keymap_key(2, long_only_key_pos, TEST_TRANSPARENT_LONG_HOLD_KEY);
    resolution = handled_key_lookup(TEST_TRANSPARENT_LONG_HOLD_KEY);
    ctx        = handled_key_resolution_ctx_make(long_only_key_pos, ((layer_state_t)1u << 1) | ((layer_state_t)1u << 2));
    test_check_momentary_helper_matches_materialization(resolution, ctx, false);
}

static void test_live_profile_behavior_replaces_compiled_rows_atomically(void) {
    uint8_t                               payload[256];
    size_t                                written;
    noah_profile_codec_v1_error_t         codec_error;
    noah_key_behavior_domain_v1_t         domain;
    noah_profile_action_v1_t              target;
    noah_profile_action_v1_t              tap_zero;
    noah_profile_action_v1_t              tap_one;
    noah_key_behavior_step_v1_t           steps[2];
    noah_key_behavior_row_v1_t            rows[1];
    noah_effective_key_behavior_runtime_t runtime;
    noah_effective_profile_snapshot_t     callback_view = {0};
    noah_effective_profile_identity_t     active        = {.generation = 7u, .payload_digest = 0x1111u, .kind = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE};
    noah_effective_key_behavior_row_t     token;
    key_behavior_view_t                   behavior;
    key_behavior_step_t                   step;

    CHECK(noah_profile_action_runtime_v1_from_native(TEST_MULTI_TAP_KEY, &target) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
    CHECK(noah_profile_action_runtime_v1_from_native(KC_Z, &tap_zero) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
    CHECK(noah_profile_action_runtime_v1_from_native(VIA_MACRO_7, &tap_one) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
    steps[0] = (noah_key_behavior_step_v1_t){
        .tap_index     = 0u,
        .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP | NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD,
        .tap           = tap_zero,
        .hold =
            {
                .mode   = NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE,
                .action = {.kind = NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY, .operand = 2u},
            },
    };
    steps[1] = (noah_key_behavior_step_v1_t){
        .tap_index     = 1u,
        .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP,
        .tap           = tap_one,
    };
    rows[0] = (noah_key_behavior_row_v1_t){
        .target           = target,
        .tap_hold_term    = 123u,
        .longer_hold_term = 456u,
        .multi_tap_term   = 78u,
        .flags            = NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_AUTO_MOUSE,
        .steps            = steps,
        .step_count       = ARRAY_SIZE(steps),
    };
    CHECK(noah_key_behavior_domain_v1_encode(rows, ARRAY_SIZE(rows), NULL, NULL, payload, sizeof(payload), &written, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    CHECK(noah_key_behavior_domain_v1_decode(payload, written, NULL, NULL, &domain, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);

    noah_effective_key_behavior_runtime_init(&runtime);
    CHECK(noah_effective_key_behavior_runtime_install(&runtime));
    callback_view.identity              = active;
    callback_view.profile.domain_mask   = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    callback_view.profile.key_behaviors = domain;
    noah_effective_key_behavior_runtime_invalidate(&runtime, 1u, (noah_effective_profile_identity_t){0}, active, &callback_view);

    behavior = key_behavior_lookup(TEST_MULTI_TAP_KEY);
    CHECK(behavior.source_is_live && behavior.config == NULL && behavior.handled);
    CHECK(behavior.tap_hold_term == 123u && behavior.longer_hold_term == 456u && behavior.multi_tap_term == 78u);
    CHECK(behavior.has_multi_tap && behavior.authored_tap_depth == 2u);
    CHECK(behavior.single.tap.present && behavior.single.tap.action == KC_Z);
    CHECK(behavior.single.hold.present && behavior.single.hold.action == MO(2));
    CHECK(key_behavior_keeps_auto_mouse_anchored(TEST_MULTI_TAP_KEY));
    step = key_behavior_view_step(&behavior, 2u);
    CHECK(step.tap.present && step.tap.action == VIA_MACRO_7);

    // A live behavior domain is a complete replacement: an omitted compiled
    // row is removed rather than silently leaking through from source data.
    // The LT() is still the runtime's, with its default (D-F27).
    behavior = key_behavior_lookup(TEST_AUTHORED_LAYER_TAP);
    CHECK(!behavior.source_is_live && behavior.config == NULL && !behavior.has_multi_tap && behavior.authored_tap_depth == 0u);

    CHECK(noah_effective_key_behavior_lookup(TEST_MULTI_TAP_KEY, &token) == NOAH_EFFECTIVE_KEY_BEHAVIOR_OK);
    active.generation = 8u;
    active.payload_digest++;
    callback_view.identity = active;
    noah_effective_key_behavior_runtime_invalidate(&runtime, 2u, (noah_effective_profile_identity_t){0}, active, &callback_view);
    CHECK(noah_effective_key_behavior_runtime_step(&runtime, token.epoch, token.row_index, 1u, &step) == NOAH_EFFECTIVE_KEY_BEHAVIOR_STALE);

    // A validated RGB-only profile deliberately returns to direct compiled
    // behavior tables; it never replays the compiled virtual blob on this path.
    active.generation = 9u;
    active.payload_digest++;
    callback_view.identity            = active;
    callback_view.profile.domain_mask = 0u;
    noah_effective_key_behavior_runtime_invalidate(&runtime, 3u, (noah_effective_profile_identity_t){0}, active, &callback_view);
    behavior = key_behavior_lookup(TEST_AUTHORED_LAYER_TAP);
    CHECK(!behavior.source_is_live && behavior.config != NULL && behavior.handled);

    noah_effective_key_behavior_runtime_uninstall(&runtime);
}

// The whole table at the shared depth, through the live runtime's row index:
// the first, middle and last rows, every step of each, and a missing key.
// A live row with holds only past its first tap count repeats its first tap,
// read from the row's steps as the compiled table is (D-F33).
static void test_live_profile_row_empty_later_tap_repeats_its_first_tap(void) {
    uint8_t                               payload[128];
    size_t                                written;
    noah_profile_codec_v1_error_t         codec_error;
    noah_key_behavior_domain_v1_t         domain;
    noah_key_behavior_step_v1_t           steps[2];
    noah_key_behavior_row_v1_t            row;
    noah_effective_key_behavior_runtime_t runtime;
    noah_effective_profile_snapshot_t     callback_view = {0};
    noah_effective_profile_identity_t     active        = {.generation = 9u, .payload_digest = 0x3333u, .kind = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE};
    key_behavior_view_t                   behavior;
    handled_key_materialized_t            materialized;

    steps[0] = (noah_key_behavior_step_v1_t){.tap_index = 0u, .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP};
    CHECK(noah_profile_action_runtime_v1_from_native(KC_Z, &steps[0].tap) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
    steps[1] = (noah_key_behavior_step_v1_t){
        .tap_index     = 2u,
        .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD,
        .hold =
            {
                .mode   = NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE,
                .action = {.kind = NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY, .operand = 2u},
            },
    };
    row = (noah_key_behavior_row_v1_t){.allowed_layers = 0xffffu, .steps = steps, .step_count = ARRAY_SIZE(steps)};
    CHECK(noah_profile_action_runtime_v1_from_native(KC_DELETE, &row.target) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
    CHECK(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, payload, sizeof(payload), &written, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    CHECK(noah_key_behavior_domain_v1_decode(payload, written, NULL, NULL, &domain, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);

    noah_effective_key_behavior_runtime_init(&runtime);
    CHECK(noah_effective_key_behavior_runtime_install(&runtime));
    callback_view.identity              = active;
    callback_view.profile.domain_mask   = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    callback_view.profile.key_behaviors = domain;
    noah_effective_key_behavior_runtime_invalidate(&runtime, 1u, (noah_effective_profile_identity_t){0}, active, &callback_view);

    behavior = key_behavior_lookup(KC_DELETE);
    CHECK(behavior.source_is_live && behavior.authored_tap_depth == 3u);
    test_reset_keymap();
    test_set_keymap_key(0, test_keypos(1, 1), KC_DELETE);
    layer_state = 1u;
    materialized = test_materialize(handled_key_lookup_tap_count(KC_DELETE, 2u), test_keypos(1, 1));
    CHECK(materialized.tap_action == KC_Z && materialized.tap_repeat_count == 2u);
    noah_effective_key_behavior_runtime_uninstall(&runtime);
}

static void test_live_profile_full_table_through_the_row_index(void) {
    enum { ROWS = NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS, DEPTH = KEY_BEHAVIOR_MAX_TAP_COUNT };
    static uint8_t                     payload[NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_PAYLOAD_SIZE];
    static noah_key_behavior_step_v1_t steps[ROWS][DEPTH];
    static noah_key_behavior_row_v1_t  rows[ROWS];
    size_t                             written;
    noah_profile_codec_v1_error_t      codec_error;
    noah_key_behavior_domain_v1_t      domain;
    noah_effective_key_behavior_runtime_t runtime;
    noah_effective_profile_snapshot_t  callback_view = {0};
    noah_effective_profile_identity_t  active        = {.generation = 11u, .payload_digest = 0x2222u, .kind = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE};
    noah_effective_key_behavior_snapshot_t status;

    for (uint8_t row = 0u; row < ROWS; row++) {
        for (uint8_t step = 0u; step < DEPTH; step++) {
            steps[row][step] = (noah_key_behavior_step_v1_t){.tap_index = step, .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP};
            CHECK(noah_profile_action_runtime_v1_from_native((uint16_t)(KC_A + step), &steps[row][step].tap) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
        }
        rows[row] = (noah_key_behavior_row_v1_t){.allowed_layers = 0xffffu, .steps = steps[row], .step_count = DEPTH};
        CHECK(noah_profile_action_runtime_v1_from_native((uint16_t)(0x0100u + row), &rows[row].target) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
    }
    CHECK(noah_key_behavior_domain_v1_encode(rows, ROWS, NULL, NULL, payload, sizeof(payload), &written, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    CHECK(noah_key_behavior_domain_v1_decode(payload, written, NULL, NULL, &domain, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    CHECK(domain.row_count == ROWS && domain.populated_step_count == ROWS * DEPTH);

    noah_effective_key_behavior_runtime_init(&runtime);
    CHECK(noah_effective_key_behavior_runtime_install(&runtime));
    callback_view.identity              = active;
    callback_view.profile.domain_mask   = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    callback_view.profile.key_behaviors = domain;
    noah_effective_key_behavior_runtime_invalidate(&runtime, 1u, (noah_effective_profile_identity_t){0}, active, &callback_view);
    CHECK(noah_effective_key_behavior_runtime_status(&runtime, &status) == NOAH_EFFECTIVE_KEY_BEHAVIOR_OK && status.indexed);

    static const uint8_t probes[] = {0u, ROWS / 2u, ROWS - 1u};
    for (uint8_t probe = 0u; probe < ARRAY_SIZE(probes); probe++) {
        key_behavior_view_t behavior = key_behavior_lookup((uint16_t)(0x0100u + probes[probe]));
        CHECK(behavior.source_is_live && behavior.handled && behavior.source_row == probes[probe]);
        CHECK(behavior.authored_tap_depth == DEPTH && behavior.single.tap.action == KC_A);
        for (uint8_t tap = 1u; tap <= DEPTH; tap++) {
            key_behavior_step_t step = key_behavior_view_step(&behavior, tap);
            CHECK(step.tap.present && step.tap.action == KC_A + tap - 1u);
        }
        CHECK(!key_behavior_view_has_more_taps(&behavior, DEPTH));
    }
    CHECK(!key_behavior_lookup((uint16_t)(0x0100u + ROWS)).source_is_live);
    noah_effective_key_behavior_runtime_uninstall(&runtime);
}

// A custom key with a shortcut and no row is a native key: it holds the
// shortcut from the press until the release and taps nothing after (D-F26).
static void test_custom_key_shortcut_without_a_row_holds_from_the_press(void) {
    keypos_t                   key_pos = test_keypos(2, 0);
    handled_key_resolution_t   resolution;
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(0, key_pos, CUSTOM_KEY_101);
    layer_state = 1u;

    resolution = handled_key_lookup(CUSTOM_KEY_101);
    CHECK(handled_key_resolution_is_handled(resolution));
    materialized = test_materialize(resolution, key_pos);
    CHECK(materialized.tap_action == KC_NO);
    CHECK(materialized.hold.present && materialized.hold.action == CUSTOM_KEY_101);
    CHECK(materialized.hold.mode == HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE);
    CHECK(materialized.hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT);

    // Without a shortcut or a row it stays out of the handled runtime.
    CHECK(!handled_key_resolution_is_handled(handled_key_lookup(CUSTOM_KEY_102)));
}

// With a row, the shortcut is what an ordinary key's keycode is: the tap of a
// cell the row leaves unauthored, and its fallback hold.
static void test_custom_key_shortcut_fills_unauthored_cells_of_its_row(void) {
    keypos_t                   key_pos = test_keypos(2, 1);
    handled_key_materialized_t materialized;

    test_reset_keymap();
    test_set_keymap_key(0, key_pos, CUSTOM_KEY_100);
    layer_state = 1u;

    materialized = test_materialize(handled_key_lookup(CUSTOM_KEY_100), key_pos);
    CHECK(materialized.tap_action == CUSTOM_KEY_100);
    CHECK(materialized.hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK);
    materialized = test_materialize(handled_key_lookup_tap_count(CUSTOM_KEY_100, 2u), key_pos);
    CHECK(materialized.tap_action == KC_B);

    // Bypassed (participation, the behaviour switch, a bypassed combo output),
    // the row is not read and the key holds its shortcut natively.
    handled_key_resolution_t bypassed;
    handled_key_lookup_row_tap_count_into(CUSTOM_KEY_100, 1u, false, &bypassed);
    CHECK(handled_key_resolution_is_handled(bypassed));
    materialized = test_materialize(bypassed, key_pos);
    CHECK(materialized.tap_action == KC_NO);
    CHECK(materialized.hold.present && materialized.hold.action == CUSTOM_KEY_100);
    CHECK(materialized.hold.mode == HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE);
}

// Read through a transparent cell, a custom key with a shortcut is the lower
// key's keycode, as an ordinary key is: its tap sends the shortcut, and a
// transparent hold gives no hold from the press (D-F26).
static void test_transparent_cell_reads_a_custom_key_shortcut_as_its_keycode(void) {
    keypos_t                   key_pos = test_keypos(2, 2);
    handled_key_materialized_t materialized;
    handled_key_materialized_t literal;

    test_reset_keymap();
    test_set_keymap_key(0, key_pos, CUSTOM_KEY_101);
    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_KEY);
    layer_state = ((layer_state_t)1u << 0) | ((layer_state_t)1u << 2);

    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_KEY), key_pos);
    CHECK(materialized.tap_action == CUSTOM_KEY_101);
    CHECK(materialized.tap_repeat_count == 1);

    test_set_keymap_key(2, key_pos, TEST_TRANSPARENT_HOLD_KEY);
    materialized = test_materialize(handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY), key_pos);
    test_set_keymap_key(0, key_pos, KC_V);
    literal = test_materialize(handled_key_lookup(TEST_TRANSPARENT_HOLD_KEY), key_pos);
    CHECK(!materialized.hold.present);
    CHECK(materialized.hold_strategy != KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT);
    CHECK(materialized.hold.present == literal.hold.present);
}

int main(void) {
    test_custom_key_shortcut_without_a_row_holds_from_the_press();
    test_custom_key_shortcut_fills_unauthored_cells_of_its_row();
    test_transparent_cell_reads_a_custom_key_shortcut_as_its_keycode();
    test_bare_dual_role_keys_use_the_runtime_default();
    test_authored_lt_uses_custom_runtime();
    test_momentary_layer_stays_handled();
    test_tap_toggle_layer_holds_and_locks_on_its_toggle_tap();
    test_tap_toggle_layer_defers_to_an_authored_row_and_the_layer_count();
    test_row_without_steps_is_no_row();
    test_empty_later_tap_repeats_the_first_tap();
    test_plain_pd_mode_key_is_handled_without_authored_behavior();
    test_handled_resolution_searches_authored_rows_once();
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
    test_momentary_layer_query_matches_hold_materialization();
    test_live_profile_behavior_replaces_compiled_rows_atomically();
    test_live_profile_row_empty_later_tap_repeats_its_first_tap();
    test_live_profile_full_table_through_the_row_index();

    puts("key_behavior_lookup host tests passed");
    return 0;
}

uint16_t keymap_key_to_keycode(uint8_t layer, keypos_t key) {
    return keycode_at_keymap_location(layer, key.row, key.col);
}
