#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noah_real_profile_keyboard.h"
#include "users/noah/lib/compat/qmk_combo_origin.h"

enum {
    TEST_PENDING_CAPACITY = 4,
    TEST_COMBO_OUT_LEFT   = 0x7000u,
    TEST_COMBO_OUT_BOTH   = 0x7001u,
    TEST_COMBO_OUT_THREE  = 0x7002u,
    TEST_COMBO_OUT_DUP    = 0x7003u,
    TEST_COMBO_OUT_LONG   = 0x7004u,
};

layer_state_t layer_state         = 0;
layer_state_t default_layer_state = (layer_state_t)1u << LAYER_BASE;

static uint16_t test_keymaps[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
static uint16_t fake_time;

static const uint16_t combo_keys_left[] = {
    KC_A,
    KC_B,
    COMBO_END,
};

static const uint16_t combo_keys_cross_half[] = {
    KC_C,
    KC_D,
    COMBO_END,
};

static const uint16_t combo_keys_three[] = {
    KC_E,
    KC_F,
    KC_G,
    COMBO_END,
};

static const uint16_t combo_keys_dup_left[] = {
    KC_H,
    KC_I,
    COMBO_END,
};

static const uint16_t combo_keys_dup_right[] = {
    KC_J,
    KC_K,
    COMBO_END,
};

static const uint16_t combo_keys_long[] = {
    KC_A,
    KC_B,
    KC_L,
    COMBO_END,
};

combo_t key_combos[] = {
    {.keys = combo_keys_left, .keycode = TEST_COMBO_OUT_LEFT}, {.keys = combo_keys_cross_half, .keycode = TEST_COMBO_OUT_BOTH}, {.keys = combo_keys_three, .keycode = TEST_COMBO_OUT_THREE}, {.keys = combo_keys_dup_left, .keycode = TEST_COMBO_OUT_DUP}, {.keys = combo_keys_dup_right, .keycode = TEST_COMBO_OUT_DUP}, {.keys = combo_keys_long, .keycode = TEST_COMBO_OUT_LONG},
};

const uint8_t noah_combo_count = ARRAY_SIZE(key_combos);

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

static keypos_t test_key(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static void test_set_key(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode) {
    test_keymaps[layer][row][col] = keycode;
}

static void test_reset_keymaps(void) {
    memset(test_keymaps, 0, sizeof(test_keymaps));

    test_set_key(LAYER_BASE, 0, 0, KC_A);
    test_set_key(LAYER_BASE, 1, 0, KC_B);
    test_set_key(LAYER_BASE, 0, 1, KC_C);
    test_set_key(LAYER_BASE, 4, 0, KC_D);
    test_set_key(LAYER_BASE, 2, 0, KC_E);
    test_set_key(LAYER_BASE, 4, 1, KC_F);
    test_set_key(LAYER_BASE, 5, 1, KC_G);
    test_set_key(LAYER_BASE, 0, 2, KC_H);
    test_set_key(LAYER_BASE, 1, 2, KC_I);
    test_set_key(LAYER_BASE, 4, 2, KC_J);
    test_set_key(LAYER_BASE, 5, 2, KC_K);
    test_set_key(LAYER_BASE, 2, 2, KC_L);

    // Simulate a live remap on the active layer while combos still resolve
    // from the configured combo reference layer.
    test_set_key(LAYER_NUM, 0, 0, KC_X);
    test_set_key(LAYER_NUM, 1, 0, KC_Y);
}

static void test_reset_combo_state(void) {
    for (uint8_t index = 0; index < ARRAY_SIZE(key_combos); index++) {
#ifndef EXTRA_SHORT_COMBOS
        key_combos[index].active   = false;
        key_combos[index].disabled = false;
        key_combos[index].state    = 0;
#else
        key_combos[index].state = 0;
#endif
    }
}

static void test_set_combo_active(uint8_t index, bool active) {
#ifndef EXTRA_SHORT_COMBOS
    key_combos[index].active = active;
#else
    if (active) {
        key_combos[index].state |= 0x80u;
    } else {
        key_combos[index].state &= (uint8_t)~0x80u;
    }
#endif
}

static void test_set_combo_disabled(uint8_t index, bool disabled) {
#ifndef EXTRA_SHORT_COMBOS
    key_combos[index].disabled = disabled;
#else
    if (disabled) {
        key_combos[index].state |= 0x40u;
    } else {
        key_combos[index].state &= (uint8_t)~0x40u;
    }
#endif
}

static void test_reset(void) {
    layer_state         = 0;
    default_layer_state = (layer_state_t)1u << LAYER_BASE;
    fake_time           = 1000u;
    test_reset_keymaps();
    test_reset_combo_state();
    key_origin_registry_reset();
    noah_qmk_combo_origin_reset();
}

uint16_t timer_read(void) {
    return fake_time;
}

uint8_t get_highest_layer(layer_state_t state) {
    for (int8_t layer = (int8_t)(sizeof(layer_state_t) * 8 - 1); layer >= 0; layer--) {
        if ((state & ((layer_state_t)1u << layer)) != 0) {
            return (uint8_t)layer;
        }
    }

    return 0u;
}

uint8_t combo_ref_from_layer(uint8_t layer) {
    return layer == LAYER_NUM ? LAYER_BASE : layer;
}

uint16_t keymap_key_to_keycode(uint8_t layer, keypos_t key) {
    if (layer >= LAYER_COUNT || key.row >= MATRIX_ROWS || key.col >= MATRIX_COLS) {
        return KC_NO;
    }

    return test_keymaps[layer][key.row][key.col];
}

uint16_t get_record_keycode(keyrecord_t *record, bool update_layer_cache) {
    uint8_t layer;

    (void)update_layer_cache;

    if (!record) {
        return KC_NO;
    }

    layer = get_highest_layer(layer_state | default_layer_state);
    return keymap_key_to_keycode(layer, record->event.key);
}

static keyrecord_t test_physical_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .type    = KEY_EVENT,
                .key     = key_pos,
                .pressed = pressed,
            },
    };
}

static keyrecord_t test_combo_record(bool pressed) {
    return (keyrecord_t){
        .event = MAKE_COMBOEVENT(pressed),
    };
}

static void test_observe_physical_key(keypos_t key_pos, bool pressed) {
    keyrecord_t record = test_physical_record(key_pos, pressed);

    noah_qmk_combo_origin_observe_physical_key_event(KC_NO, &record);
}

static bool test_bitmap_has(const uint8_t *bitmap, uint8_t row, uint8_t col) {
    return key_origin_bitmap_has_keypos(bitmap, test_key(row, col));
}

static void test_single_half_combo_uses_last_key_from_combo_ref_layer(void) {
    keyrecord_t combo_record = test_combo_record(true);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];
    keypos_t    owner_key_pos;

    test_reset();
    layer_state = (layer_state_t)1u << LAYER_NUM;

    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_set_combo_active(0, true);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_record);

    CHECK(noah_qmk_combo_origin_event_owner_keypos(&combo_record, &owner_key_pos));
    CHECK(owner_key_pos.row == 1);
    CHECK(owner_key_pos.col == 0);
    CHECK(noah_qmk_combo_origin_event_bitmap(&combo_record, bitmap));
    CHECK(test_bitmap_has(bitmap, 0, 0));
    CHECK(test_bitmap_has(bitmap, 1, 0));
    CHECK(noah_qmk_combo_origin_event_side_mask(&combo_record) == SPLIT_SIDE_MASK_LEFT);
}

static void test_combo_owner_is_stable_when_member_press_order_changes(void) {
    keyrecord_t combo_record = test_combo_record(true);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];
    keypos_t    owner_key_pos;

    test_reset();

    test_observe_physical_key(test_key(1, 0), true);
    test_observe_physical_key(test_key(0, 0), true);
    test_set_combo_active(0, true);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_record);

    CHECK(noah_qmk_combo_origin_event_owner_keypos(&combo_record, &owner_key_pos));
    CHECK(owner_key_pos.row == 1);
    CHECK(owner_key_pos.col == 0);
    CHECK(noah_qmk_combo_origin_event_bitmap(&combo_record, bitmap));
    CHECK(test_bitmap_has(bitmap, 0, 0));
    CHECK(test_bitmap_has(bitmap, 1, 0));
}

static void test_pressed_combo_bitmap_reports_complete_physical_combo(void) {
    uint8_t bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();

    test_observe_physical_key(test_key(0, 0), true);
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);
    CHECK(!key_origin_bitmap_has_any(bitmap));

    test_observe_physical_key(test_key(1, 0), true);
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);
    CHECK(test_bitmap_has(bitmap, 0, 0));
    CHECK(test_bitmap_has(bitmap, 1, 0));
}

static void test_pressed_combo_match_reports_pending_output_owner(void) {
    test_reset();

    test_observe_physical_key(test_key(1, 0), true);
    CHECK(!noah_qmk_combo_origin_pressed_combo_matches(TEST_COMBO_OUT_LEFT, test_key(1, 0), fake_time, COMBO_TERM));

    fake_time = (uint16_t)(fake_time + 20u);
    test_observe_physical_key(test_key(0, 0), true);
    CHECK(noah_qmk_combo_origin_pressed_combo_matches(TEST_COMBO_OUT_LEFT, test_key(1, 0), (uint16_t)(fake_time - 20u), 20u));
    CHECK(!noah_qmk_combo_origin_pressed_combo_matches(TEST_COMBO_OUT_LEFT, test_key(1, 0), (uint16_t)(fake_time - 21u), 20u));
    CHECK(!noah_qmk_combo_origin_pressed_combo_matches(TEST_COMBO_OUT_LEFT, test_key(0, 0), (uint16_t)(fake_time - 20u), 20u));
    CHECK(!noah_qmk_combo_origin_pressed_combo_matches(TEST_COMBO_OUT_BOTH, test_key(1, 0), (uint16_t)(fake_time - 20u), 20u));
}

static void test_pending_combo_output_survives_member_release(void) {
    keyrecord_t combo_press   = test_combo_record(true);
    keyrecord_t combo_release = test_combo_record(false);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];
    keypos_t    owner_key_pos;

    test_reset();

    test_observe_physical_key(test_key(0, 0), true);
    fake_time = (uint16_t)(fake_time + 20u);
    test_observe_physical_key(test_key(1, 0), true);
    CHECK(noah_qmk_combo_origin_pressed_combo_matches(TEST_COMBO_OUT_LEFT, test_key(1, 0), (uint16_t)(fake_time - 20u), 20u));

    test_observe_physical_key(test_key(1, 0), false);
    test_set_combo_active(0, true);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_press);

    CHECK(noah_qmk_combo_origin_event_owner_keypos(&combo_press, &owner_key_pos));
    CHECK(owner_key_pos.row == 1);
    CHECK(owner_key_pos.col == 0);
    CHECK(noah_qmk_combo_origin_event_bitmap(&combo_press, bitmap));
    CHECK(test_bitmap_has(bitmap, 0, 0));
    CHECK(test_bitmap_has(bitmap, 1, 0));
    CHECK(noah_qmk_combo_origin_pressed_combo_matches(TEST_COMBO_OUT_LEFT, test_key(1, 0), (uint16_t)(fake_time - 20u), 20u));

    test_set_combo_active(0, false);
    test_observe_physical_key(test_key(0, 0), false);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_release);

    CHECK(!noah_qmk_combo_origin_pressed_combo_matches(TEST_COMBO_OUT_LEFT, test_key(1, 0), (uint16_t)(fake_time - 20u), 20u));
}

static void test_combo_release_uses_cached_footprint_after_physical_releases(void) {
    keyrecord_t combo_press   = test_combo_record(true);
    keyrecord_t combo_release = test_combo_record(false);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();

    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_set_combo_active(0, true);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_press);

    test_set_combo_active(0, false);
    test_observe_physical_key(test_key(1, 0), false);
    test_observe_physical_key(test_key(0, 0), false);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_release);

    CHECK(combo_release.event.key.row == 1);
    CHECK(combo_release.event.key.col == 0);
    CHECK(noah_qmk_combo_origin_event_bitmap(&combo_release, bitmap));
    CHECK(test_bitmap_has(bitmap, 0, 0));
    CHECK(test_bitmap_has(bitmap, 1, 0));
    CHECK(noah_qmk_combo_origin_event_side_mask(&combo_release) == SPLIT_SIDE_MASK_LEFT);
}

static void test_cross_half_combo_reports_both_sides(void) {
    keyrecord_t combo_record = test_combo_record(true);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();

    test_observe_physical_key(test_key(0, 1), true);
    test_observe_physical_key(test_key(4, 0), true);
    test_set_combo_active(1, true);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_BOTH, &combo_record);

    CHECK(combo_record.event.key.row == 4);
    CHECK(combo_record.event.key.col == 0);
    CHECK(noah_qmk_combo_origin_event_bitmap(&combo_record, bitmap));
    CHECK(test_bitmap_has(bitmap, 0, 1));
    CHECK(test_bitmap_has(bitmap, 4, 0));
    CHECK(noah_qmk_combo_origin_event_side_mask(&combo_record) == SPLIT_SIDE_MASK_BOTH);
}

static void test_three_key_combo_bitmap_contains_all_members(void) {
    keyrecord_t combo_record = test_combo_record(true);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();

    test_observe_physical_key(test_key(2, 0), true);
    test_observe_physical_key(test_key(4, 1), true);
    test_observe_physical_key(test_key(5, 1), true);
    test_set_combo_active(2, true);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_THREE, &combo_record);

    CHECK(combo_record.event.key.row == 5);
    CHECK(combo_record.event.key.col == 1);
    CHECK(noah_qmk_combo_origin_event_bitmap(&combo_record, bitmap));
    CHECK(test_bitmap_has(bitmap, 2, 0));
    CHECK(test_bitmap_has(bitmap, 4, 1));
    CHECK(test_bitmap_has(bitmap, 5, 1));
    CHECK(noah_qmk_combo_origin_event_side_mask(&combo_record) == SPLIT_SIDE_MASK_BOTH);
}

static void test_duplicate_output_active_combos_keep_exact_origins(void) {
    keyrecord_t first_combo_record  = test_combo_record(true);
    keyrecord_t second_combo_record = test_combo_record(true);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();

    test_observe_physical_key(test_key(0, 2), true);
    test_observe_physical_key(test_key(1, 2), true);
    test_observe_physical_key(test_key(4, 2), true);
    test_observe_physical_key(test_key(5, 2), true);
    test_set_combo_active(3, true);
    test_set_combo_active(4, true);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_DUP, &first_combo_record);

    CHECK(noah_qmk_combo_origin_event_bitmap(&first_combo_record, bitmap));
    CHECK(test_bitmap_has(bitmap, 0, 2));
    CHECK(test_bitmap_has(bitmap, 1, 2));
    CHECK(!test_bitmap_has(bitmap, 4, 2));
    CHECK(!test_bitmap_has(bitmap, 5, 2));
    CHECK(noah_qmk_combo_origin_event_side_mask(&first_combo_record) == SPLIT_SIDE_MASK_LEFT);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_DUP, &second_combo_record);
    CHECK(noah_qmk_combo_origin_event_bitmap(&second_combo_record, bitmap));
    CHECK(!test_bitmap_has(bitmap, 0, 2));
    CHECK(!test_bitmap_has(bitmap, 1, 2));
    CHECK(test_bitmap_has(bitmap, 4, 2));
    CHECK(test_bitmap_has(bitmap, 5, 2));
    CHECK(noah_qmk_combo_origin_event_side_mask(&second_combo_record) == SPLIT_SIDE_MASK_RIGHT);
}

static void test_pending_combo_press_uses_exact_pending_footprint(void) {
    keyrecord_t combo_record = test_combo_record(true);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();

    test_observe_physical_key(test_key(0, 1), true);
    test_observe_physical_key(test_key(4, 0), true);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_BOTH, &combo_record);

    CHECK(combo_record.event.key.row == 4);
    CHECK(combo_record.event.key.col == 0);
    CHECK(noah_qmk_combo_origin_event_bitmap(&combo_record, bitmap));
    CHECK(test_bitmap_has(bitmap, 0, 1));
    CHECK(test_bitmap_has(bitmap, 4, 0));
    CHECK(noah_qmk_combo_origin_event_side_mask(&combo_record) == SPLIT_SIDE_MASK_BOTH);
}

static void test_pending_combo_release_uses_cached_exact_footprint(void) {
    keyrecord_t combo_press   = test_combo_record(true);
    keyrecord_t combo_release = test_combo_record(false);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();

    test_observe_physical_key(test_key(0, 1), true);
    test_observe_physical_key(test_key(4, 0), true);

    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_BOTH, &combo_press);

    test_observe_physical_key(test_key(4, 0), false);
    test_observe_physical_key(test_key(0, 1), false);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_BOTH, &combo_release);

    CHECK(combo_release.event.key.row == 4);
    CHECK(combo_release.event.key.col == 0);
    CHECK(noah_qmk_combo_origin_event_bitmap(&combo_release, bitmap));
    CHECK(test_bitmap_has(bitmap, 0, 1));
    CHECK(test_bitmap_has(bitmap, 4, 0));
    CHECK(noah_qmk_combo_origin_event_side_mask(&combo_release) == SPLIT_SIDE_MASK_BOTH);
}

static void test_reset_clears_cached_combo_origin_state(void) {
    keyrecord_t combo_press   = test_combo_record(true);
    keyrecord_t combo_release = test_combo_record(false);

    test_reset();

    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_set_combo_active(0, true);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_press);

    noah_qmk_combo_origin_reset();
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_release);

    CHECK(combo_release.event.key.row == 0);
    CHECK(combo_release.event.key.col == 0);
}

static void test_active_combo_partition_routes_preview_owner_to_underlay(void) {
    keyrecord_t left_combo_record  = test_combo_record(true);
    keyrecord_t both_combo_record  = test_combo_record(true);
    keypos_t    left_owner_key_pos = {0};
    uint8_t     underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t     overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();

    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_set_combo_active(0, true);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &left_combo_record);
    CHECK(noah_qmk_combo_origin_event_owner_keypos(&left_combo_record, &left_owner_key_pos));

    test_observe_physical_key(test_key(0, 1), true);
    test_observe_physical_key(test_key(4, 0), true);
    test_set_combo_active(1, true);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_BOTH, &both_combo_record);

    noah_qmk_combo_origin_active_bitmaps_partitioned(left_owner_key_pos, (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS}, underlay_bitmap, overlay_bitmap);

    CHECK(test_bitmap_has(underlay_bitmap, 0, 0));
    CHECK(test_bitmap_has(underlay_bitmap, 1, 0));
    CHECK(!test_bitmap_has(underlay_bitmap, 0, 1));
    CHECK(!test_bitmap_has(underlay_bitmap, 4, 0));

    CHECK(!test_bitmap_has(overlay_bitmap, 0, 0));
    CHECK(!test_bitmap_has(overlay_bitmap, 1, 0));
    CHECK(test_bitmap_has(overlay_bitmap, 0, 1));
    CHECK(test_bitmap_has(overlay_bitmap, 4, 0));
}

static void test_active_combo_partition_routes_preview_and_pd_owners_to_underlay(void) {
    keyrecord_t left_combo_record  = test_combo_record(true);
    keyrecord_t both_combo_record  = test_combo_record(true);
    keypos_t    left_owner_key_pos = {0};
    keypos_t    both_owner_key_pos = {0};
    uint8_t     underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t     overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();

    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_set_combo_active(0, true);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &left_combo_record);
    CHECK(noah_qmk_combo_origin_event_owner_keypos(&left_combo_record, &left_owner_key_pos));

    test_observe_physical_key(test_key(0, 1), true);
    test_observe_physical_key(test_key(4, 0), true);
    test_set_combo_active(1, true);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_BOTH, &both_combo_record);
    CHECK(noah_qmk_combo_origin_event_owner_keypos(&both_combo_record, &both_owner_key_pos));

    noah_qmk_combo_origin_active_bitmaps_partitioned(left_owner_key_pos, both_owner_key_pos, underlay_bitmap, overlay_bitmap);

    CHECK(test_bitmap_has(underlay_bitmap, 0, 0));
    CHECK(test_bitmap_has(underlay_bitmap, 1, 0));
    CHECK(test_bitmap_has(underlay_bitmap, 0, 1));
    CHECK(test_bitmap_has(underlay_bitmap, 4, 0));
    CHECK(!key_origin_bitmap_has_any(overlay_bitmap));
}

static void test_overlap_disabled_candidate_retires_before_feedback_bitmap(void) {
    noah_qmk_combo_origin_debug_snapshot_t snapshot;
    uint8_t                                bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();
    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_set_combo_disabled(0, true);

    noah_qmk_combo_origin_scan();
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);
    CHECK(!test_bitmap_has(bitmap, 0, 0));
    CHECK(!test_bitmap_has(bitmap, 1, 0));

    noah_qmk_combo_origin_debug_snapshot(&snapshot);
    CHECK(snapshot.pending_count == 0u);
    CHECK(snapshot.suppressed_retirement_count == 1u);
}

static void test_delayed_output_gets_final_deadline_scan_opportunity(void) {
    keyrecord_t combo_press = test_combo_record(true);
    uint8_t     bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();
    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_observe_physical_key(test_key(1, 0), false);
    test_observe_physical_key(test_key(0, 0), false);

    fake_time = (uint16_t)(fake_time + COMBO_TERM + 1u);
    noah_qmk_combo_origin_scan();
    test_set_combo_active(0, true);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_press);

    CHECK(noah_qmk_combo_origin_event_bitmap(&combo_press, bitmap));
    CHECK(test_bitmap_has(bitmap, 0, 0));
    CHECK(test_bitmap_has(bitmap, 1, 0));
}

static void test_deadline_expiry_boundaries_and_timer_wrap(void) {
    noah_qmk_combo_origin_debug_snapshot_t snapshot;
    uint8_t                                bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();
    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_observe_physical_key(test_key(1, 0), false);
    test_observe_physical_key(test_key(0, 0), false);

    fake_time = (uint16_t)(1000u + COMBO_TERM - 1u);
    noah_qmk_combo_origin_scan();
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);
    CHECK(test_bitmap_has(bitmap, 0, 0));

    fake_time = (uint16_t)(1000u + COMBO_TERM);
    noah_qmk_combo_origin_scan();
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);
    CHECK(test_bitmap_has(bitmap, 0, 0));

    fake_time = (uint16_t)(1000u + COMBO_TERM + 1u);
    noah_qmk_combo_origin_scan();
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);
    CHECK(test_bitmap_has(bitmap, 0, 0));
    noah_qmk_combo_origin_scan();
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);
    CHECK(!key_origin_bitmap_has_any(bitmap));
    noah_qmk_combo_origin_debug_snapshot(&snapshot);
    CHECK(snapshot.deadline_expiry_count == 1u);

    test_reset();
    fake_time = (uint16_t)(UINT16_MAX - 20u);
    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_observe_physical_key(test_key(1, 0), false);
    test_observe_physical_key(test_key(0, 0), false);
    fake_time = (uint16_t)(fake_time + COMBO_TERM + 1u);
    noah_qmk_combo_origin_scan();
    noah_qmk_combo_origin_scan();
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);
    CHECK(!key_origin_bitmap_has_any(bitmap));
}

static void test_full_pending_cache_refuses_then_recovers_capacity(void) {
    noah_qmk_combo_origin_debug_snapshot_t snapshot;
    uint8_t                                bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();
    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_observe_physical_key(test_key(0, 1), true);
    test_observe_physical_key(test_key(4, 0), true);
    test_observe_physical_key(test_key(2, 0), true);
    test_observe_physical_key(test_key(4, 1), true);
    test_observe_physical_key(test_key(5, 1), true);
    test_observe_physical_key(test_key(0, 2), true);
    test_observe_physical_key(test_key(1, 2), true);
    test_observe_physical_key(test_key(2, 2), true);

    noah_qmk_combo_origin_debug_snapshot(&snapshot);
    CHECK(snapshot.pending_count == TEST_PENDING_CAPACITY);
    CHECK(snapshot.pending_high_water == TEST_PENDING_CAPACITY);
    CHECK(snapshot.cache_full_refusal_count == 1u);

    // Once the refused long combo is no longer physically complete, its
    // unique member must not survive as an attributed pending origin.
    test_observe_physical_key(test_key(2, 2), false);
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);
    CHECK(!test_bitmap_has(bitmap, 2, 2));

    for (uint8_t index = 0; index < TEST_PENDING_CAPACITY; index++) {
        test_set_combo_disabled(index, true);
    }
    noah_qmk_combo_origin_scan();
    noah_qmk_combo_origin_debug_snapshot(&snapshot);
    CHECK(snapshot.pending_count == 0u);

    test_observe_physical_key(test_key(4, 2), false);
    test_observe_physical_key(test_key(5, 2), false);
    test_observe_physical_key(test_key(4, 2), true);
    test_observe_physical_key(test_key(5, 2), true);
    noah_qmk_combo_origin_debug_snapshot(&snapshot);
    CHECK(snapshot.pending_count == 1u);
}

static void test_unmatched_output_is_observable_and_does_not_create_origin(void) {
    keyrecord_t                            combo_press = test_combo_record(true);
    noah_qmk_combo_origin_debug_snapshot_t snapshot;
    uint8_t                                bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset();
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_LEFT, &combo_press);
    noah_qmk_combo_origin_debug_snapshot(&snapshot);
    noah_qmk_combo_origin_pressed_combo_bitmap(bitmap);

    CHECK(snapshot.unmatched_delayed_output_count == 1u);
    CHECK(snapshot.pending_count == 0u);
    CHECK(snapshot.active_count == 0u);
    CHECK(!key_origin_bitmap_has_any(bitmap));
}

static void test_same_output_generations_promote_and_release_independently(void) {
    keyrecord_t                            first_press    = test_combo_record(true);
    keyrecord_t                            second_press   = test_combo_record(true);
    keyrecord_t                            first_release  = test_combo_record(false);
    keyrecord_t                            second_release = test_combo_record(false);
    noah_qmk_combo_origin_debug_snapshot_t snapshot;

    test_reset();
    test_observe_physical_key(test_key(0, 2), true);
    test_observe_physical_key(test_key(1, 2), true);
    test_set_combo_active(3, true);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_DUP, &first_press);
    CHECK(first_press.event.key.row == 1u);
    CHECK(first_press.event.key.col == 2u);

    test_observe_physical_key(test_key(4, 2), true);
    test_observe_physical_key(test_key(5, 2), true);
    test_set_combo_active(4, true);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_DUP, &second_press);
    CHECK(second_press.event.key.row == 5u);
    CHECK(second_press.event.key.col == 2u);

    test_observe_physical_key(test_key(5, 2), false);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_DUP, &second_release);
    CHECK(second_release.event.key.row == 5u);
    CHECK(second_release.event.key.col == 2u);
    test_set_combo_active(4, false);
    noah_qmk_combo_origin_debug_snapshot(&snapshot);
    CHECK(snapshot.active_count == 1u);

    test_observe_physical_key(test_key(1, 2), false);
    noah_qmk_combo_origin_normalize_record(TEST_COMBO_OUT_DUP, &first_release);
    CHECK(first_release.event.key.row == 1u);
    CHECK(first_release.event.key.col == 2u);
    test_set_combo_active(3, false);
    noah_qmk_combo_origin_debug_snapshot(&snapshot);
    CHECK(snapshot.active_count == 0u);
}

static void test_reset_clears_candidate_diagnostics(void) {
    noah_qmk_combo_origin_debug_snapshot_t snapshot;

    test_reset();
    test_observe_physical_key(test_key(0, 0), true);
    test_observe_physical_key(test_key(1, 0), true);
    test_set_combo_disabled(0, true);
    noah_qmk_combo_origin_scan();
    noah_qmk_combo_origin_reset();
    noah_qmk_combo_origin_debug_snapshot(&snapshot);
    CHECK(snapshot.pending_count == 0u);
    CHECK(snapshot.active_count == 0u);
    CHECK(snapshot.pending_high_water == 0u);
    CHECK(snapshot.suppressed_retirement_count == 0u);
    CHECK(snapshot.deadline_expiry_count == 0u);
    CHECK(snapshot.cache_full_refusal_count == 0u);
    CHECK(snapshot.unmatched_delayed_output_count == 0u);
}

int main(void) {
    test_single_half_combo_uses_last_key_from_combo_ref_layer();
    test_combo_owner_is_stable_when_member_press_order_changes();
    test_pressed_combo_bitmap_reports_complete_physical_combo();
    test_pressed_combo_match_reports_pending_output_owner();
    test_pending_combo_output_survives_member_release();
    test_combo_release_uses_cached_footprint_after_physical_releases();
    test_cross_half_combo_reports_both_sides();
    test_three_key_combo_bitmap_contains_all_members();
    test_duplicate_output_active_combos_keep_exact_origins();
    test_pending_combo_press_uses_exact_pending_footprint();
    test_pending_combo_release_uses_cached_exact_footprint();
    test_reset_clears_cached_combo_origin_state();
    test_active_combo_partition_routes_preview_owner_to_underlay();
    test_active_combo_partition_routes_preview_and_pd_owners_to_underlay();
    test_overlap_disabled_candidate_retires_before_feedback_bitmap();
    test_delayed_output_gets_final_deadline_scan_opportunity();
    test_deadline_expiry_boundaries_and_timer_wrap();
    test_full_pending_cache_refuses_then_recovers_capacity();
    test_unmatched_output_is_observable_and_does_not_create_origin();
    test_same_output_generations_promote_and_release_independently();
    test_reset_clears_candidate_diagnostics();

    puts("qmk_combo_origin host tests passed");
    return 0;
}
