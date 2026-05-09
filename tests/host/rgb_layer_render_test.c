#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_pd_fixture.h"
#include "host_runtime_reset_fixture.h"
#include "users/noah/lib/key/runtime/feedback.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/rgb/automouse/rgb_automouse.h"
#include "users/noah/lib/rgb/core/rgb_config_helpers.h"
#include "users/noah/lib/split/runtime_sync.h"
#include "users/noah/lib/state/diagnostics/runtime_diag.h"
#include "users/noah/lib/rgb/core/rgb_runtime.h"
#include "users/noah/lib/rgb/core/rgb_helpers.h"
#include "ws2812.h"

#ifndef RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE
#    define RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE 0
#endif
#ifndef RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
#    define RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED 0
#endif
#ifndef RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF
#    define RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF 0
#endif
#ifndef RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY
#    define RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY 0
#endif
#ifndef RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF
#    define RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF 0
#endif
#ifndef RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
#    define RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF 0
#endif
#ifndef RGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS
#    define RGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS 0
#endif
#if (RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF + RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY + RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF + RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF) > 1
#    error "Only one key-feedback render test mode variant may be enabled at a time"
#endif

enum test_layers {
    LAYER_BASE = 0,
    LAYER_NUM,
    LAYER_SYM,
    LAYER_NAV,
    LAYER_POINTER,
};

bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max);

HOST_RUNTIME_FIXTURE_DEFINE_LAYER_STUBS()

static uint16_t               test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
static rgb_t                  led_output[RGB_MATRIX_LED_COUNT];
static uint8_t                fake_preview_layer = UINT8_MAX;
static uint8_t                fake_combo_underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
static uint8_t                fake_combo_overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
static uint8_t                fake_feedback_semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
static uint8_t                fake_feedback_tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
static uint8_t                fake_feedback_flash_visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
static uint8_t                fake_feedback_broad_owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
static uint8_t                fake_auto_mouse_layer   = LAYER_POINTER;
static uint16_t               fake_auto_mouse_elapsed = 0;
static bool                   fake_auto_mouse_active  = true;
static host_runtime_fixture_t runtime_fixture         = HOST_RUNTIME_FIXTURE_INIT;
#define fake_is_master runtime_fixture.is_master
static pd_mode_mask_t    fake_pd_active_mode = 0;
static pd_mode_mask_t    fake_pd_locked_mode = 0;
static split_side_mask_t fake_pd_owner_sides = SPLIT_SIDE_MASK_NONE;
static uint8_t           fake_pd_owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];

static const rgb_t test_runtime_boot_indicator_rgb = {.r = 150u, .g = 150u, .b = 150u};

ws2812_led_t                ws2812_leds[WS2812_LED_COUNT];
split_runtime_sync_remote_t split_runtime_sync_remote = SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;

led_config_t g_led_config = {0};

const layer_color_config_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE] =
        {
            .color = HSV(0, 0, 0),
            .mode  = ALL_KEYS,
        },
    [LAYER_NUM] =
        {
            .color = HSV(10, 20, 30),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
    [LAYER_SYM] =
        {
            .color = HSV(40, 50, 60),
            .mode  = ALL_KEYS,
        },
    [LAYER_NAV] =
        {
            .color = HSV(70, 80, 90),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
    [LAYER_POINTER] =
        {
            .color = HSV(100, 110, 120),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
};

const pd_mode_color_t pd_mode_colors[] = {
    {.pointing_mode = PD_MODE_ARROW, .color = HSV(210, 211, 212), .locality = RGB_RIGHT_HALF}, {.pointing_mode = PD_MODE_VOLUME, .color = HSV(220, 221, 222), .locality = RGB_LEFT_HALF}, {.pointing_mode = PD_MODE_BRIGHTNESS, .color = HSV(223, 224, 225), .locality = RGB_BOTH_HALVES}, {.pointing_mode = PD_MODE_ZOOM, .color = HSV(226, 227, 228), .locality = RGB_KEY_HALF}, {.pointing_mode = PD_MODE_PINCH, .color = HSV(233, 234, 235), .locality = RGB_KEYS_ONLY},
};
const uint8_t                    pd_mode_color_count       = (uint8_t)(sizeof(pd_mode_colors) / sizeof(pd_mode_colors[0]));
static const pd_mode_led_group_t pd_mode_led_groups_data[] = RGB_LED_GROUP_TABLE({.pointing_mode = PD_MODE_VOLUME, .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP(1, 6)}, {.pointing_mode = RGB_PD_MODE_GROUP_ALL, .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP(6)}, );
EXPORT_PD_MODE_LED_GROUP_TABLE(pd_mode_led_groups_data);
const automouse_fade_end_config_t automouse_fade_end_config = {
#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE
    .mode = END_COLOR_ON_ALL_KEYS,
#elif RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
    .mode = END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW,
#else
    .mode = FOLLOW_REAL_DESTINATION,
#endif
    .end_color = HSV(200, 210, 220),
};
const combo_feedback_color_config_t combo_feedback_colors = {
    .color    = HSV(150, 151, 152),
    .locality = RGB_KEYS_ONLY,
};
#if RGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS
static const combo_feedback_led_group_t combo_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE({.color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP(3)}, );
EXPORT_COMBO_FEEDBACK_LED_GROUP_TABLE(combo_feedback_led_groups_data);
#endif
const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    .tap_pending_color = HSV(1, 2, 3),
    RGB_TAP_BRANCH_COLORS(HSV(13, 14, 15), HSV(16, 17, 18), HSV(19, 20, 21)),
    .tap_committed_color    = HSV(4, 5, 6),
    .hold_active_color      = HSV(7, 8, 9),
    .long_hold_active_color = HSV(10, 11, 12),
    .branch_confirm_mode    = KEY_FEEDBACK_BRANCH_CONFIRM_NON_BASE_TAPS,
    .tap_commit_mode        = KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS,
#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY
    .locality = RGB_KEYS_ONLY,
#elif RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF
    .locality = RGB_KEY_HALF,
#elif RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF
    .locality = RGB_LEFT_HALF,
#elif RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
    .locality = RGB_RIGHT_HALF,
#else
    .locality = RGB_BOTH_HALVES,
#endif
};
#if RGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS
static const key_behavior_feedback_led_group_t key_behavior_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE({.semantic = KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH, .color = HSV(11, 12, 13), .led_group = RGB_LED_GROUP(5)}, {.semantic = KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED, .color = HSV(14, 15, 16), .led_group = RGB_LED_GROUP(5)}, {.semantic = KEY_FEEDBACK_GROUP_TAP_COMMITTED, .color = HSV(17, 18, 19), .led_group = RGB_LED_GROUP(5)}, {.semantic = KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE, .color = HSV(20, 21, 22), .led_group = RGB_LED_GROUP(5)}, {.semantic = KEY_FEEDBACK_GROUP_ALL, .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP(5, 6)}, );
EXPORT_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_TABLE(key_behavior_feedback_led_groups_data);
#endif
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {
    [PD_MODE_INDEX_DRAGSCROLL] = {.mode_flag = PD_MODE_DRAGSCROLL}, [PD_MODE_INDEX_VOLUME] = {.mode_flag = PD_MODE_VOLUME}, [PD_MODE_INDEX_BRIGHTNESS] = {.mode_flag = PD_MODE_BRIGHTNESS}, [PD_MODE_INDEX_ZOOM] = {.mode_flag = PD_MODE_ZOOM}, [PD_MODE_INDEX_ARROW] = {.mode_flag = PD_MODE_ARROW}, [PD_MODE_INDEX_PINCH] = {.mode_flag = PD_MODE_PINCH},
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

static rgb_t rgb_from_hsv(hsv_t hsv) {
    return (rgb_t){.r = hsv.h, .g = hsv.s, .b = hsv.v};
}

static rgb_t rgb_from_ws2812(ws2812_led_t led) {
    return (rgb_t){.r = led.r, .g = led.g, .b = led.b};
}

static rgb_t rgb_from_combo_feedback(void) {
    return rgb_from_hsv(combo_feedback_colors.color);
}

static rgb_t rgb_from_tap_pending_color(uint8_t index) {
    (void)index;
    return rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color);
}

static rgb_t rgb_from_tap_branch_color(uint8_t index) {
    return rgb_from_hsv(key_behavior_feedback_colors.tap_branch_colors[index]);
}

#if RGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS
static rgb_t rgb_from_key_feedback_group(uint8_t index) {
    return rgb_from_hsv(key_behavior_feedback_led_groups[index].color);
}

static rgb_t rgb_from_feedback_semantic(key_feedback_semantic_t semantic, uint8_t tap_branch) {
    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH:
            return rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color);
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED: {
            uint8_t index = tap_branch <= 2u ? 0u : (uint8_t)(tap_branch - 2u);
            if (index >= key_behavior_feedback_colors.tap_branch_color_count) {
                index = (uint8_t)(key_behavior_feedback_colors.tap_branch_color_count - 1u);
            }
            return rgb_from_tap_branch_color(index);
        }
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            return rgb_from_hsv(key_behavior_feedback_colors.tap_committed_color);
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            return rgb_from_hsv(key_behavior_feedback_colors.hold_active_color);
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            return rgb_from_hsv(key_behavior_feedback_colors.long_hold_active_color);
        case KEY_FEEDBACK_SEMANTIC_NONE:
        default:
            return (rgb_t){0};
    }
}
#endif

static __attribute__((unused)) void test_feedback_bitmap_set(uint8_t *bitmap, uint8_t row, uint8_t col) {
    key_origin_bitmap_fill_single(bitmap, (keypos_t){.row = row, .col = col});
}

static __attribute__((unused)) void test_feedback_bitmap_add(uint8_t *bitmap, uint8_t row, uint8_t col) {
    key_origin_bitmap_add_keypos(bitmap, (keypos_t){.row = row, .col = col});
}

static void test_feedback_semantic_set(uint8_t *semantic_map, uint8_t row, uint8_t col, key_feedback_semantic_t semantic) {
    key_feedback_semantic_map_set(semantic_map, (keypos_t){.row = row, .col = col}, semantic);
}

static void test_feedback_tap_branch_set(uint8_t *tap_branch_map, uint8_t row, uint8_t col, uint8_t tap_branch) {
    key_feedback_tap_branch_map_set(tap_branch_map, (keypos_t){.row = row, .col = col}, tap_branch);
}

static key_feedback_broad_owner_slot_t test_feedback_group_slot_for_semantic(key_feedback_semantic_t semantic) {
    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH:
            return KEY_FEEDBACK_BROAD_OWNER_GROUP_UNRESOLVED_TAP_BRANCH;
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED:
            return KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_BRANCH_COMMITTED;
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            return KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_COMMITTED;
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            return KEY_FEEDBACK_BROAD_OWNER_GROUP_HOLD_ACTIVE;
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            return KEY_FEEDBACK_BROAD_OWNER_GROUP_LONG_HOLD_ACTIVE;
        case KEY_FEEDBACK_SEMANTIC_NONE:
        default:
            return KEY_FEEDBACK_BROAD_OWNER_COUNT;
    }
}

static void test_feedback_broad_owner_add(uint8_t *owner_map, uint8_t row, uint8_t col, key_feedback_semantic_t semantic) {
    keypos_t                        key_pos = {.row = row, .col = col};
    key_feedback_broad_owner_slot_t group_slot;

    key_feedback_broad_owner_map_set(owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, key_pos);
    if (split_half_from_keypos(key_pos) == SPLIT_HALF_RIGHT) {
        key_feedback_broad_owner_map_set(owner_map, KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF, key_pos);
    } else {
        key_feedback_broad_owner_map_set(owner_map, KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF, key_pos);
    }

    group_slot = test_feedback_group_slot_for_semantic(semantic);
    if (group_slot < KEY_FEEDBACK_BROAD_OWNER_COUNT) {
        key_feedback_broad_owner_map_set(owner_map, group_slot, key_pos);
    }
}

static void test_local_feedback_semantic_add(uint8_t row, uint8_t col, key_feedback_semantic_t semantic) {
    test_feedback_semantic_set(fake_feedback_semantic_map, row, col, semantic);
    test_feedback_broad_owner_add(fake_feedback_broad_owner_map, row, col, semantic);
}

static void test_local_feedback_tap_branch_add(uint8_t row, uint8_t col, uint8_t tap_branch) {
    test_feedback_tap_branch_set(fake_feedback_tap_branch_map, row, col, tap_branch);
}

static __attribute__((unused)) void test_local_feedback_visibility_add(uint8_t row, uint8_t col) {
    key_origin_bitmap_add_keypos(fake_feedback_flash_visibility_bitmap, (keypos_t){.row = row, .col = col});
}

static void test_remote_feedback_semantic_add(uint8_t row, uint8_t col, key_feedback_semantic_t semantic) {
    test_feedback_semantic_set(split_runtime_sync_remote.key_feedback_semantic_map, row, col, semantic);
    test_feedback_broad_owner_add(split_runtime_sync_remote.key_feedback_broad_owner_map, row, col, semantic);
}

static void test_remote_feedback_tap_branch_add(uint8_t row, uint8_t col, uint8_t tap_branch) {
    test_feedback_tap_branch_set(split_runtime_sync_remote.key_feedback_tap_branch_map, row, col, tap_branch);
}

static __attribute__((unused)) void test_remote_feedback_visibility_add(uint8_t row, uint8_t col) {
    key_origin_bitmap_add_keypos(split_runtime_sync_remote.key_feedback_flash_visibility_bitmap, (keypos_t){.row = row, .col = col});
}

static rgb_t rgb_blend(rgb_t start, rgb_t end, uint8_t amount) {
    uint32_t inv = (uint32_t)UINT8_MAX - amount;

    return (rgb_t){
        .r = (uint8_t)(((uint32_t)start.r * inv + (uint32_t)end.r * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
        .g = (uint8_t)(((uint32_t)start.g * inv + (uint32_t)end.g * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
        .b = (uint8_t)(((uint32_t)start.b * inv + (uint32_t)end.b * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
    };
}

static pd_mode_mask_t test_display_locked_mode(void) {
    return host_runtime_fixture_display_locked_mode(pd_modes, PD_MODE_COUNT, fake_is_master, fake_pd_locked_mode, split_runtime_sync_remote);
}

static pd_mode_mask_t test_display_active_mode(void) {
    return host_runtime_fixture_display_active_mode(pd_modes, PD_MODE_COUNT, fake_is_master, fake_pd_active_mode, fake_pd_locked_mode, split_runtime_sync_remote);
}

static uint8_t automouse_blend_amount_from_elapsed(uint16_t elapsed) {
    uint16_t progress = automouse_rgb_progress(elapsed);
    if (test_display_locked_mode() != 0) {
        progress = 0;
    }
    if (progress > AUTOMOUSE_RGB_ACTIVE_SPAN) {
        progress = AUTOMOUSE_RGB_ACTIVE_SPAN;
    }

    return automouse_rgb_blend_amount(progress);
}

static void test_reset(void) {
    host_runtime_fixture_reset(&runtime_fixture);
    noah_runtime_diag_reset_for_test();
    memset(test_keymap, 0, sizeof(test_keymap));
    memset(led_output, 0, sizeof(led_output));
    memset(ws2812_leds, 0, sizeof(ws2812_leds));
    layer_state        = 0;
    fake_preview_layer = UINT8_MAX;
    key_origin_bitmap_clear(fake_combo_underlay_bitmap);
    key_origin_bitmap_clear(fake_combo_overlay_bitmap);
    key_feedback_semantic_map_clear(fake_feedback_semantic_map);
    key_feedback_tap_branch_map_clear(fake_feedback_tap_branch_map);
    key_origin_bitmap_clear(fake_feedback_flash_visibility_bitmap);
    key_feedback_broad_owner_map_clear(fake_feedback_broad_owner_map);
    fake_auto_mouse_layer   = LAYER_POINTER;
    fake_auto_mouse_elapsed = 0;
    fake_auto_mouse_active  = true;
    fake_pd_active_mode     = 0;
    fake_pd_locked_mode     = 0;
    fake_pd_owner_sides     = SPLIT_SIDE_MASK_NONE;
    key_origin_bitmap_clear(fake_pd_owner_bitmap);
    split_runtime_sync_remote = host_runtime_fixture_split_remote_init();

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            g_led_config.matrix_co[row][col] = NO_LED;
            for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
                test_keymap[layer][row][col] = KC_TRNS;
            }
        }
    }

    g_led_config.matrix_co[0][0] = 0;
    g_led_config.matrix_co[0][1] = 1;
    g_led_config.matrix_co[0][2] = 2;
    g_led_config.matrix_co[0][3] = 3;
    g_led_config.matrix_co[1][0] = 4;
    g_led_config.matrix_co[1][1] = 5;
    g_led_config.matrix_co[1][2] = 6;
    g_led_config.matrix_co[1][3] = 7;
    g_led_config.matrix_co[4][0] = 4;
    g_led_config.matrix_co[4][1] = 5;
    g_led_config.matrix_co[4][2] = 6;
    g_led_config.matrix_co[4][3] = 7;

    noah_rgb_runtime_post_init();
}

HOST_RUNTIME_FIXTURE_DEFINE_BASIC_QMK_STUBS(runtime_fixture)

pd_mode_snapshot_t pd_mode_snapshot(void) {
    return host_runtime_fixture_pd_mode_snapshot(pd_modes, PD_MODE_COUNT, fake_is_master, fake_pd_active_mode, fake_pd_locked_mode, fake_pd_owner_sides, split_runtime_sync_remote);
}

bool pd_mode_display_owner_bitmap_snapshot(uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    if (fake_is_master) {
        key_origin_bitmap_copy(out_bitmap, fake_pd_owner_bitmap);
    } else {
        key_origin_bitmap_copy(out_bitmap, split_runtime_sync_remote.pd_mode_owner_bitmap);
    }
#else
    (void)fake_is_master;
#endif

    return key_origin_bitmap_has_any(out_bitmap);
}

int rgb_matrix_led_index(int index) {
    if (index < 0 || index >= RGB_MATRIX_LED_COUNT) {
        return -1;
    }

    return index;
}

rgb_t hsv_to_rgb(hsv_t hsv) {
    return rgb_from_hsv(hsv);
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return test_keymap[layer_num][row][column];
}

void key_feedback_semantic_map(uint8_t *out_map) {
    memcpy(out_map, fake_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);
}

void key_feedback_tap_branch_map(uint8_t *out_map) {
    memcpy(out_map, fake_feedback_tap_branch_map, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE);
}

void key_feedback_flash_visibility_bitmap(uint8_t *out_bitmap) {
    key_origin_bitmap_copy(out_bitmap, fake_feedback_flash_visibility_bitmap);
}

void key_feedback_broad_owner_map(uint8_t *out_map) {
    memcpy(out_map, fake_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE);
}

void combo_feedback_underlay_bitmap(uint8_t *out_bitmap) {
    key_origin_bitmap_copy(out_bitmap, fake_combo_underlay_bitmap);
}

void combo_feedback_overlay_bitmap(uint8_t *out_bitmap) {
    key_origin_bitmap_copy(out_bitmap, fake_combo_overlay_bitmap);
}

uint8_t key_feedback_preview_layer(void) {
    return fake_preview_layer;
}

uint8_t get_auto_mouse_layer(void) {
    return fake_auto_mouse_layer;
}

uint16_t auto_mouse_get_time_elapsed(void) {
    return fake_auto_mouse_elapsed;
}

bool is_auto_mouse_active(void) {
    return fake_auto_mouse_active;
}

bool pd_any_local_mode_locked(void) {
    return fake_pd_locked_mode != 0;
}

bool pd_mode_local_active(pd_mode_mask_t mode) {
    return mode != 0 && fake_pd_active_mode == mode;
}

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    return mode != 0 && fake_pd_locked_mode == mode;
}

bool pd_any_local_mode_active(void) {
    return fake_pd_active_mode != 0;
}

pd_mode_mask_t pd_mode_local_active_snapshot(void) {
    return fake_pd_active_mode;
}

pd_mode_mask_t pd_mode_local_locked_snapshot(void) {
    return fake_pd_locked_mode;
}

bool pd_any_display_mode_locked(void) {
    return test_display_locked_mode() != 0;
}

bool pd_mode_display_active(pd_mode_mask_t mode) {
    return mode != 0 && test_display_active_mode() == mode;
}

pd_mode_mask_t pd_mode_display_active_snapshot(void) {
    return test_display_active_mode();
}

uint8_t pd_mode_display_active_index(void) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_display_active(pd_modes[i].mode_flag)) {
            return i;
        }
    }

    return PD_MODE_COUNT;
}

void rgb_matrix_set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    if (index < 0 || index >= RGB_MATRIX_LED_COUNT) {
        test_fail("rgb_matrix_set_color index in range", __FILE__, __LINE__);
    }

    led_output[index] = (rgb_t){.r = red, .g = green, .b = blue};
}

uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i) {
    uint8_t led = g_led_config.matrix_co[row][column];
    if (led == NO_LED) {
        return 0;
    }

    led_i[0] = led;
    return 1;
}

static void check_led(uint8_t index, rgb_t expected) {
    if (led_output[index].r != expected.r || led_output[index].g != expected.g || led_output[index].b != expected.b) {
        fprintf(stderr, "LED %u mismatch: got (%u,%u,%u), expected (%u,%u,%u)\n", (unsigned int)index, (unsigned int)led_output[index].r, (unsigned int)led_output[index].g, (unsigned int)led_output[index].b, (unsigned int)expected.r, (unsigned int)expected.g, (unsigned int)expected.b);
    }
    CHECK(led_output[index].r == expected.r);
    CHECK(led_output[index].g == expected.g);
    CHECK(led_output[index].b == expected.b);
}

static bool render_output(void) {
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        led_output[i] = rgb_from_ws2812(ws2812_leds[i]);
    }

    return noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT);
}

static void test_mapped_only_layers_compose_in_layer_order(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    test_keymap[LAYER_NUM][0][1] = 0x0021u;
    test_keymap[LAYER_NAV][0][1] = 0x0030u;
    test_keymap[LAYER_NAV][0][2] = 0x0031u;

    layer_state = ((layer_state_t)1u << LAYER_NUM) | ((layer_state_t)1u << LAYER_NAV);

    CHECK(render_output());

    check_led(0, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_NAV].color));
    check_led(2, rgb_from_hsv(layer_colors[LAYER_NAV].color));
    check_led(3, (rgb_t){0, 0, 0});
}

static void test_full_board_layer_fills_gaps_under_mapped_only_layer(void) {
    test_reset();

    test_keymap[LAYER_NAV][0][1] = 0x0030u;
    layer_state                  = ((layer_state_t)1u << LAYER_SYM) | ((layer_state_t)1u << LAYER_NAV);

    CHECK(render_output());

    for (uint8_t led = 0; led < RGB_MATRIX_LED_COUNT; led++) {
        rgb_t expected = rgb_from_hsv(layer_colors[LAYER_SYM].color);
        if (led == 1) {
            expected = rgb_from_hsv(layer_colors[LAYER_NAV].color);
        }
        check_led(led, expected);
    }
}

static void test_invalidating_layer_map_refreshes_dynamic_keymap_coverage(void) {
    test_reset();

    test_keymap[LAYER_NAV][0][1] = 0x0030u;
    layer_state                  = (layer_state_t)1u << LAYER_NAV;

    CHECK(render_output());
    check_led(1, rgb_from_hsv(layer_colors[LAYER_NAV].color));
    check_led(2, (rgb_t){0, 0, 0});

    memset(led_output, 0, sizeof(led_output));
    test_keymap[LAYER_NAV][0][1] = KC_TRNS;
    test_keymap[LAYER_NAV][0][2] = 0x0031u;
    noah_rgb_runtime_invalidate_layer_maps();

    CHECK(render_output());
    check_led(1, (rgb_t){0, 0, 0});
    check_led(2, rgb_from_hsv(layer_colors[LAYER_NAV].color));
}

static void test_preview_layer_overlays_existing_active_layers(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    test_keymap[LAYER_NUM][0][1] = 0x0021u;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;
    fake_preview_layer           = LAYER_NUM;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(2, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_slave_preview_layer_uses_remote_sync_state(void) {
    test_reset();

    fake_is_master                              = false;
    test_keymap[LAYER_NUM][0][0]                = 0x0020u;
    test_keymap[LAYER_NUM][0][1]                = 0x0021u;
    layer_state                                 = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.key_preview_layer = LAYER_NUM;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(2, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_slave_preview_handoff_to_matching_remote_layer_stays_continuous(void) {
    test_reset();

    fake_is_master               = false;
    test_keymap[LAYER_NUM][0][0] = 0x0020u;

    layer_state                                 = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.key_preview_layer = LAYER_NUM;
    CHECK(render_output());
    check_led(0, rgb_from_hsv(layer_colors[LAYER_NUM].color));

    memset(led_output, 0, sizeof(led_output));
    layer_state                                 = ((layer_state_t)1u << LAYER_SYM) | ((layer_state_t)1u << LAYER_NUM);
    split_runtime_sync_remote.key_preview_layer = LAYER_NUM;
    CHECK(render_output());
    check_led(0, rgb_from_hsv(layer_colors[LAYER_NUM].color));

    memset(led_output, 0, sizeof(led_output));
    layer_state                                 = (layer_state_t)1u << LAYER_NUM;
    split_runtime_sync_remote.key_preview_layer = UINT8_MAX;
    CHECK(render_output());
    check_led(0, rgb_from_hsv(layer_colors[LAYER_NUM].color));
}

static void test_combo_overlay_paints_exact_combo_keys(void) {
    test_reset();

    layer_state = (layer_state_t)1u << LAYER_SYM;
    test_feedback_bitmap_set(fake_combo_overlay_bitmap, 0, 0);
    test_feedback_bitmap_add(fake_combo_overlay_bitmap, 4, 2);

    CHECK(render_output());

    check_led(0, rgb_from_combo_feedback());
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(6, rgb_from_combo_feedback());
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_combo_overlay_stays_visible_over_preview(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;
    fake_preview_layer           = LAYER_NUM;
    test_feedback_bitmap_set(fake_combo_overlay_bitmap, 0, 0);

    CHECK(render_output());

    check_led(0, rgb_from_combo_feedback());
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_combo_underlay_stays_below_preview(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;
    fake_preview_layer           = LAYER_NUM;
    test_feedback_bitmap_set(fake_combo_underlay_bitmap, 0, 0);

    CHECK(render_output());

    check_led(0, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_combo_overlay_stays_visible_over_pd_mode(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_ARROW;
    test_feedback_bitmap_set(fake_combo_overlay_bitmap, 4, 0);

    CHECK(render_output());

    check_led(4, rgb_from_combo_feedback());
    check_led(5, rgb_from_hsv(pd_mode_colors[0].color));
}

static void test_combo_underlay_stays_below_pd_mode(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_ARROW;
    test_feedback_bitmap_set(fake_combo_underlay_bitmap, 4, 0);

    CHECK(render_output());

    check_led(4, rgb_from_hsv(pd_mode_colors[0].color));
    check_led(5, rgb_from_hsv(pd_mode_colors[0].color));
}

#if RGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS
static void test_combo_overlay_led_groups_repaint_after_combo_locality(void) {
    test_reset();

    layer_state = (layer_state_t)1u << LAYER_SYM;
    test_feedback_bitmap_set(fake_combo_overlay_bitmap, 0, 0);

    CHECK(render_output());

    check_led(0, rgb_from_combo_feedback());
    check_led(3, rgb_from_combo_feedback());
}

static void test_combo_underlay_led_groups_stay_below_preview(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][3] = 0x0020u;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;
    fake_preview_layer           = LAYER_NUM;
    test_feedback_bitmap_set(fake_combo_underlay_bitmap, 0, 0);

    CHECK(render_output());

    check_led(3, rgb_from_hsv(layer_colors[LAYER_NUM].color));
}

static void test_combo_overlay_led_groups_repaint_after_pd_mode(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_VOLUME;
    test_feedback_bitmap_set(fake_combo_overlay_bitmap, 0, 0);

    CHECK(render_output());

    check_led(3, rgb_from_combo_feedback());
}
#endif

static void test_slave_combo_overlay_stays_visible_over_remote_preview(void) {
    test_reset();

    fake_is_master                              = false;
    test_keymap[LAYER_NUM][0][0]                = 0x0020u;
    layer_state                                 = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.key_preview_layer = LAYER_NUM;
    test_feedback_bitmap_set(split_runtime_sync_remote.combo_overlay_bitmap, 0, 0);

    CHECK(render_output());

    check_led(0, rgb_from_combo_feedback());
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_slave_combo_underlay_stays_below_remote_preview(void) {
    test_reset();

    fake_is_master                              = false;
    test_keymap[LAYER_NUM][0][0]                = 0x0020u;
    layer_state                                 = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.key_preview_layer = LAYER_NUM;
    test_feedback_bitmap_set(split_runtime_sync_remote.combo_underlay_bitmap, 0, 0);

    CHECK(render_output());

    check_led(0, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_slave_combo_overlay_stays_visible_over_remote_pd_mode(void) {
    test_reset();

    fake_is_master                           = false;
    layer_state                              = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.active_mode_id = pd_mode_id_from_mask(PD_MODE_ARROW);
    test_feedback_bitmap_set(split_runtime_sync_remote.combo_overlay_bitmap, 4, 0);

    CHECK(render_output());

    check_led(4, rgb_from_combo_feedback());
    check_led(5, rgb_from_hsv(pd_mode_colors[0].color));
}

static void test_slave_combo_underlay_stays_below_remote_pd_mode(void) {
    test_reset();

    fake_is_master                           = false;
    layer_state                              = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.active_mode_id = pd_mode_id_from_mask(PD_MODE_ARROW);
    test_feedback_bitmap_set(split_runtime_sync_remote.combo_underlay_bitmap, 4, 0);

    CHECK(render_output());

    check_led(4, rgb_from_hsv(pd_mode_colors[0].color));
    check_led(5, rgb_from_hsv(pd_mode_colors[0].color));
}

static void test_slave_remote_combo_overlay_suppresses_stale_pending_feedback(void) {
    test_reset();

    fake_is_master = false;
    layer_state    = (layer_state_t)1u << LAYER_SYM;
    test_feedback_bitmap_set(split_runtime_sync_remote.combo_overlay_bitmap, 0, 0);
    test_remote_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(render_output());

    check_led(0, rgb_from_combo_feedback());
}

static void test_slave_remote_combo_underlay_suppresses_stale_pending_feedback(void) {
    test_reset();

    fake_is_master = false;
    layer_state    = (layer_state_t)1u << LAYER_SYM;
    test_feedback_bitmap_set(split_runtime_sync_remote.combo_underlay_bitmap, 0, 0);
    test_remote_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(render_output());

    check_led(0, rgb_from_combo_feedback());
}

static void test_slave_tap_commit_feedback_uses_configured_color(void) {
    test_reset();

    fake_is_master = false;
    layer_state    = (layer_state_t)1u << LAYER_SYM;
    test_remote_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED);

    CHECK(render_output());

#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_committed_color));
#else
    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_committed_color));
#endif
}

static void test_slave_feedback_uses_remote_semantics_and_visibility_bitmap(void) {
    test_reset();

    fake_is_master = false;
    layer_state    = (layer_state_t)1u << LAYER_SYM;
    test_remote_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING);

    CHECK(render_output());
    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));

    memset(led_output, 0, sizeof(led_output));
    test_remote_feedback_visibility_add(0, 0);

    CHECK(render_output());
#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
#else
    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.long_hold_active_color));
#endif
#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF || RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY || RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF
#    if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
#    endif
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
#else
    check_led(7, rgb_from_hsv(key_behavior_feedback_colors.long_hold_active_color));
#endif
}

static void check_pending_feedback_from_left_source(rgb_t expected_color) {
#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, expected_color);
#elif RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY
    check_led(0, expected_color);
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
#else
    check_led(0, expected_color);
#endif
}

static void test_multi_tap_pending_uses_pending_color(void) {
    test_reset();

    layer_state = (layer_state_t)1u << LAYER_SYM;
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    test_local_feedback_tap_branch_add(0, 0, 2u);

    CHECK(render_output());

    check_pending_feedback_from_left_source(rgb_from_tap_pending_color(0));
}

static void test_tap_branch_commit_uses_branch_color(void) {
    test_reset();

    layer_state = (layer_state_t)1u << LAYER_SYM;
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    test_local_feedback_tap_branch_add(0, 0, 2u);

    CHECK(render_output());

    check_pending_feedback_from_left_source(rgb_from_tap_branch_color(0));
}

static void test_tap_branch_commit_color_clamps_to_last_configured_color(void) {
    test_reset();

    layer_state = (layer_state_t)1u << LAYER_SYM;
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    test_local_feedback_tap_branch_add(0, 0, KEY_BEHAVIOR_MAX_TAP_COUNT);

    CHECK(render_output());

    check_pending_feedback_from_left_source(rgb_from_tap_branch_color(2));
}

static void test_slave_tap_branch_commit_uses_remote_branch_color(void) {
    test_reset();

    fake_is_master = false;
    layer_state    = (layer_state_t)1u << LAYER_SYM;
    test_remote_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    test_remote_feedback_tap_branch_add(0, 0, 3u);

    CHECK(render_output());

    check_pending_feedback_from_left_source(rgb_from_tap_branch_color(1));
}

#if RGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS
static void test_key_feedback_led_groups_repaint_after_feedback_locality(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(render_output());

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(5, rgb_from_key_feedback_group(0));
    check_led(6, rgb_from_feedback_semantic(KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH, 0u));
}

static void test_key_feedback_tap_branch_commit_led_group_repaints_after_feedback_locality(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);

    CHECK(render_output());

    check_led(5, rgb_from_key_feedback_group(1));
    check_led(6, rgb_from_feedback_semantic(KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED, 0u));
}

static void test_key_feedback_tap_commit_led_group_repaints_after_feedback_locality(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED);

    CHECK(render_output());

    check_led(5, rgb_from_key_feedback_group(2));
    check_led(6, rgb_from_feedback_semantic(KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED, 0u));
}

static void test_key_feedback_all_led_group_uses_hold_color(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_HOLD_PENDING);

    CHECK(render_output());

    check_led(5, rgb_from_feedback_semantic(KEY_FEEDBACK_SEMANTIC_HOLD_PENDING, 0u));
    check_led(6, rgb_from_feedback_semantic(KEY_FEEDBACK_SEMANTIC_HOLD_PENDING, 0u));
}

static void test_key_feedback_led_groups_follow_flash_visibility(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING);

    CHECK(render_output());
    check_led(5, rgb_from_hsv(layer_colors[LAYER_SYM].color));

    test_local_feedback_visibility_add(0, 0);

    CHECK(render_output());
    check_led(5, rgb_from_key_feedback_group(3));
    check_led(6, rgb_from_feedback_semantic(KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING, 0u));
}
#endif

#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF
static void test_key_half_feedback_paints_only_master_half(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(3, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_key_half_feedback_paints_only_slave_half_from_remote_snapshot(void) {
    test_reset();

    layer_state    = (1UL << LAYER_SYM);
    fake_is_master = false;
    test_remote_feedback_semantic_add(4, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
}

static void test_key_half_feedback_paints_both_halves_for_combo_footprint(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    test_local_feedback_semantic_add(4, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(3, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
}

static void test_key_half_feedback_uses_independent_priority_per_half(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING);
    test_local_feedback_visibility_add(0, 0);
    test_local_feedback_semantic_add(4, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.long_hold_active_color));
    check_led(3, rgb_from_hsv(key_behavior_feedback_colors.long_hold_active_color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
}

static void test_key_half_feedback_follows_newest_owner_without_visible_fallback(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING);
    test_local_feedback_visibility_add(0, 0);
    test_local_feedback_semantic_add(0, 1, KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}
#endif

#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY
static void test_key_feedback_paints_only_master_key(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 1, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(1, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(2, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_key_feedback_paints_only_slave_key_from_remote_snapshot(void) {
    test_reset();

    layer_state    = (1UL << LAYER_SYM);
    fake_is_master = false;
    test_remote_feedback_semantic_add(4, 2, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(5, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(6, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_key_feedback_paints_all_combo_keys_on_master(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    test_local_feedback_semantic_add(0, 2, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    test_local_feedback_semantic_add(4, 1, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(2, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(5, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_key_feedback_paints_all_remote_combo_keys_from_snapshot(void) {
    test_reset();

    layer_state    = (1UL << LAYER_SYM);
    fake_is_master = false;
    test_remote_feedback_semantic_add(0, 1, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    test_remote_feedback_semantic_add(4, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    test_remote_feedback_semantic_add(4, 2, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(1, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(5, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(6, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}
#endif

#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF
static void test_key_left_half_feedback_paints_fixed_left_half_from_any_source_side(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(4, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(3, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_key_left_half_feedback_uses_newest_global_owner(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_HOLD_PENDING);
    test_local_feedback_semantic_add(4, 0, KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING);
    test_local_feedback_visibility_add(4, 0);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.long_hold_active_color));
    check_led(3, rgb_from_hsv(key_behavior_feedback_colors.long_hold_active_color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_key_left_half_feedback_does_not_fallback_from_hidden_newest_owner(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_HOLD_PENDING);
    test_local_feedback_semantic_add(4, 0, KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}
#endif

#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
static void test_key_right_half_feedback_paints_fixed_right_half_from_any_source_side(void) {
    test_reset();

    layer_state = (1UL << LAYER_SYM);
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
}

static void test_key_right_half_feedback_uses_remote_snapshot(void) {
    test_reset();

    fake_is_master = false;
    layer_state    = (1UL << LAYER_SYM);
    test_remote_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
}
#endif

static void test_slave_full_scene_preserves_remote_preview_and_locked_pd_mode_when_feedback_flash_is_hidden(void) {
    test_reset();

    fake_is_master                              = false;
    test_keymap[LAYER_NUM][0][0]                = 0x0020u;
    test_keymap[LAYER_NUM][0][1]                = 0x0021u;
    test_keymap[LAYER_NUM][1][0]                = 0x0022u;
    layer_state                                 = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.key_preview_layer = LAYER_NUM;
    split_runtime_sync_remote.locked_mode_id    = pd_mode_id_from_mask(PD_MODE_VOLUME);
    test_remote_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING);

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(1, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(2, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(6, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

#if !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
static void test_slave_full_scene_feedback_overrides_remote_preview_and_locked_pd_mode(void) {
    test_reset();

    fake_is_master                              = false;
    test_keymap[LAYER_NUM][0][0]                = 0x0020u;
    test_keymap[LAYER_NUM][0][1]                = 0x0021u;
    test_keymap[LAYER_NUM][1][0]                = 0x0022u;
    layer_state                                 = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.key_preview_layer = LAYER_NUM;
    split_runtime_sync_remote.locked_mode_id    = pd_mode_id_from_mask(PD_MODE_VOLUME);
    test_remote_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(render_output());

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(1, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(2, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(6, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
}
#endif

static void test_render_order_preview_then_pd_mode_then_pd_group(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    test_keymap[LAYER_NUM][1][0] = 0x0021u;
    test_keymap[LAYER_NUM][1][2] = 0x0022u;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;
    fake_preview_layer           = LAYER_NUM;
    fake_pd_active_mode          = PD_MODE_VOLUME;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(2, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(6, rgb_from_hsv(pd_mode_colors[1].color));
}

#if !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
static void test_render_order_base_then_preview_then_pd_mode_then_feedback(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    test_keymap[LAYER_NUM][1][2] = 0x0021u;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;
    fake_preview_layer           = LAYER_NUM;
    fake_pd_active_mode          = PD_MODE_VOLUME;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(2, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(6, rgb_from_hsv(pd_mode_colors[1].color));

    memset(led_output, 0, sizeof(led_output));
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(render_output());

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(2, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(6, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
}
#endif

static void test_runtime_boot_indicator_overrides_scene(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    test_keymap[LAYER_NUM][1][0] = 0x0021u;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;
    fake_preview_layer           = LAYER_NUM;
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    fake_pd_active_mode = PD_MODE_VOLUME;

    noah_runtime_diag_post_init();

    CHECK(render_output());

    for (uint8_t led = 0; led < RGB_MATRIX_LED_COUNT; led++) {
        check_led(led, test_runtime_boot_indicator_rgb);
    }
}

#if !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
static void test_multi_tap_pending_feedback_overrides_preview_and_pd_mode(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    test_keymap[LAYER_NUM][1][0] = 0x0021u;
    test_keymap[LAYER_NUM][1][2] = 0x0022u;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;
    fake_preview_layer           = LAYER_NUM;
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    fake_pd_active_mode = PD_MODE_VOLUME;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(2, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(6, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
}
#endif

#if !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
static void test_hold_pending_feedback_overrides_preview_and_pd_mode(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    test_keymap[LAYER_NUM][1][0] = 0x0021u;
    test_keymap[LAYER_NUM][1][2] = 0x0022u;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;
    fake_preview_layer           = LAYER_NUM;
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_HOLD_PENDING);
    fake_pd_active_mode = PD_MODE_VOLUME;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.hold_active_color));
    check_led(2, rgb_from_hsv(key_behavior_feedback_colors.hold_active_color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.hold_active_color));
    check_led(6, rgb_from_hsv(key_behavior_feedback_colors.hold_active_color));
}

static void test_newest_global_feedback_owner_wins_over_older_higher_priority(void) {
    test_reset();

    layer_state = (layer_state_t)1u << LAYER_SYM;
    test_local_feedback_semantic_add(0, 0, KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING);
    test_local_feedback_visibility_add(0, 0);
    test_local_feedback_semantic_add(4, 0, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    CHECK(render_output());

    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(3, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(4, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
    check_led(7, rgb_from_hsv(key_behavior_feedback_colors.tap_pending_color));
}
#endif

static void test_pointer_mode_overlay_paints_right_half(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_ARROW;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(2, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(pd_mode_colors[0].color));
    check_led(5, rgb_from_hsv(pd_mode_colors[0].color));
    check_led(6, rgb_from_hsv(pd_mode_colors[0].color));
    check_led(7, rgb_from_hsv(pd_mode_colors[0].color));
}

static void test_pointer_mode_overlay_paints_left_half_and_groups(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_VOLUME;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(1, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(2, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(3, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(5, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(6, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_pointer_mode_overlay_paints_both_halves(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_BRIGHTNESS;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[2].color));
    check_led(2, rgb_from_hsv(pd_mode_colors[2].color));
    check_led(4, rgb_from_hsv(pd_mode_colors[2].color));
    check_led(7, rgb_from_hsv(pd_mode_colors[2].color));
}

static void test_pointer_mode_overlay_paints_trigger_half_on_master(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_ZOOM;
    fake_pd_owner_sides = SPLIT_SIDE_MASK_LEFT;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(3, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_pointer_mode_overlay_paints_trigger_both_halves_for_combo_origin(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_ZOOM;
    fake_pd_owner_sides = SPLIT_SIDE_MASK_BOTH;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(3, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(4, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(7, rgb_from_hsv(pd_mode_colors[3].color));
}

static void test_pointer_mode_overlay_trigger_half_falls_back_both_without_owner(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_ZOOM;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(3, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(4, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(7, rgb_from_hsv(pd_mode_colors[3].color));
}

static void test_pointer_mode_overlay_paints_trigger_keys_on_master(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_PINCH;
    key_origin_bitmap_fill_single(fake_pd_owner_bitmap, (keypos_t){.row = 0, .col = 0});
    key_origin_bitmap_add_keypos(fake_pd_owner_bitmap, (keypos_t){.row = 4, .col = 2});

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[4].color));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(6, rgb_from_hsv(pd_mode_colors[4].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_pointer_mode_overlay_trigger_keys_falls_back_both_without_owner(void) {
    test_reset();

    layer_state         = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_mode = PD_MODE_PINCH;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[4].color));
    check_led(3, rgb_from_hsv(pd_mode_colors[4].color));
    check_led(4, rgb_from_hsv(pd_mode_colors[4].color));
    check_led(7, rgb_from_hsv(pd_mode_colors[4].color));
}

static void test_slave_pointer_mode_overlay_uses_remote_display_state_and_trigger_half(void) {
    test_reset();

    fake_is_master                                = false;
    layer_state                                   = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.active_mode_id      = pd_mode_id_from_mask(PD_MODE_ZOOM);
    split_runtime_sync_remote.pd_mode_owner_sides = SPLIT_SIDE_MASK_LEFT;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(3, rgb_from_hsv(pd_mode_colors[3].color));
    check_led(4, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_slave_pointer_mode_overlay_uses_remote_trigger_keys(void) {
    test_reset();

    fake_is_master                           = false;
    layer_state                              = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.active_mode_id = pd_mode_id_from_mask(PD_MODE_PINCH);
    key_origin_bitmap_fill_single(split_runtime_sync_remote.pd_mode_owner_bitmap, (keypos_t){.row = 4, .col = 1});

    CHECK(render_output());

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(5, rgb_from_hsv(pd_mode_colors[4].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));
}

static void test_automouse_uses_configured_target_layer(void) {
    test_reset();

    fake_auto_mouse_layer        = LAYER_NAV;
    test_keymap[LAYER_NAV][0][0] = 0x0030u;
    test_keymap[LAYER_NAV][0][1] = 0x0031u;

    layer_state             = ((layer_state_t)1u << LAYER_SYM) | ((layer_state_t)1u << LAYER_NAV);
    fake_auto_mouse_elapsed = AUTOMOUSE_RGB_DEAD_TIME + (AUTOMOUSE_RGB_ACTIVE_SPAN / 2u);

    CHECK(render_output());

    rgb_t   nav_rgb = rgb_from_hsv(layer_colors[LAYER_NAV].color);
    rgb_t   sym_rgb = rgb_from_hsv(layer_colors[LAYER_SYM].color);
    uint8_t blend   = automouse_blend_amount_from_elapsed(fake_auto_mouse_elapsed);

#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE
    rgb_t end_override = rgb_from_hsv(automouse_fade_end_config.end_color);

    check_led(0, rgb_blend(nav_rgb, end_override, blend));
    check_led(1, rgb_blend(nav_rgb, end_override, blend));
    check_led(2, rgb_blend(sym_rgb, end_override, blend));
#else
    check_led(0, rgb_blend(nav_rgb, sym_rgb, blend));
    check_led(1, rgb_blend(nav_rgb, sym_rgb, blend));
    check_led(2, sym_rgb);
#endif
}

static void test_timeout_end_keeps_automouse_at_destination_on_master(void) {
    test_reset();

    ws2812_leds[0]                   = (ws2812_led_t){.r = 5, .g = 6, .b = 7};
    ws2812_leds[1]                   = (ws2812_led_t){.r = 8, .g = 9, .b = 10};
    test_keymap[LAYER_POINTER][0][0] = 0x0040u;
    test_keymap[LAYER_POINTER][0][1] = 0x0041u;
    fake_auto_mouse_active           = false;
    fake_auto_mouse_elapsed          = AUTO_MOUSE_TIME;
    layer_state                      = (layer_state_t)1u << LAYER_POINTER;

    CHECK(render_output());

#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE || RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
    check_led(0, rgb_from_hsv(automouse_fade_end_config.end_color));
    check_led(1, rgb_from_hsv(automouse_fade_end_config.end_color));
    check_led(2, rgb_from_hsv(automouse_fade_end_config.end_color));
#else
    check_led(0, rgb_from_ws2812(ws2812_leds[0]));
    check_led(1, rgb_from_ws2812(ws2812_leds[1]));
    check_led(2, (rgb_t){0, 0, 0});
#endif
}

static void test_timeout_window_keeps_fading_after_auto_mouse_active_drops_on_master(void) {
    test_reset();

    ws2812_leds[0] = (ws2812_led_t){.r = 5, .g = 6, .b = 7};
    ws2812_leds[1] = (ws2812_led_t){.r = 8, .g = 9, .b = 10};

    test_keymap[LAYER_POINTER][0][0] = 0x0040u;
    test_keymap[LAYER_POINTER][0][1] = 0x0041u;
    fake_auto_mouse_active           = false;
    fake_auto_mouse_elapsed          = AUTOMOUSE_RGB_DEAD_TIME + (AUTOMOUSE_RGB_ACTIVE_SPAN / 2u);
    layer_state                      = (layer_state_t)1u << LAYER_POINTER;

    CHECK(render_output());

    rgb_t   pointer_rgb = rgb_from_hsv(layer_colors[LAYER_POINTER].color);
    uint8_t blend       = automouse_blend_amount_from_elapsed(fake_auto_mouse_elapsed);

#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE || RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
    rgb_t end_rgb = rgb_from_hsv(automouse_fade_end_config.end_color);
#else
    rgb_t end_rgb = rgb_from_ws2812(ws2812_leds[0]);
#endif

    check_led(0, rgb_blend(pointer_rgb, end_rgb, blend));
#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE || RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
    check_led(1, rgb_blend(pointer_rgb, rgb_from_hsv(automouse_fade_end_config.end_color), blend));
    check_led(2, rgb_from_hsv(automouse_fade_end_config.end_color));
#else
    check_led(1, rgb_blend(pointer_rgb, rgb_from_ws2812(ws2812_leds[1]), blend));
    check_led(2, (rgb_t){0, 0, 0});
#endif
}

static void test_timeout_end_keeps_automouse_at_destination_on_slave(void) {
    test_reset();

    fake_is_master                               = false;
    ws2812_leds[0]                               = (ws2812_led_t){.r = 5, .g = 6, .b = 7};
    ws2812_leds[1]                               = (ws2812_led_t){.r = 8, .g = 9, .b = 10};
    test_keymap[LAYER_POINTER][0][0]             = 0x0040u;
    test_keymap[LAYER_POINTER][0][1]             = 0x0041u;
    layer_state                                  = (layer_state_t)1u << LAYER_POINTER;
    split_runtime_sync_remote.automouse_progress = AUTOMOUSE_RGB_ACTIVE_SPAN;

    CHECK(render_output());

#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE || RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
    check_led(0, rgb_from_hsv(automouse_fade_end_config.end_color));
    check_led(1, rgb_from_hsv(automouse_fade_end_config.end_color));
    check_led(2, rgb_from_hsv(automouse_fade_end_config.end_color));
#else
    check_led(0, rgb_from_ws2812(ws2812_leds[0]));
    check_led(1, rgb_from_ws2812(ws2812_leds[1]));
    check_led(2, (rgb_t){0, 0, 0});
#endif
}

static void test_slave_timeout_window_renders_without_live_auto_mouse_active_flag(void) {
    test_reset();

    fake_is_master                               = false;
    ws2812_leds[0]                               = (ws2812_led_t){.r = 5, .g = 6, .b = 7};
    ws2812_leds[1]                               = (ws2812_led_t){.r = 8, .g = 9, .b = 10};
    test_keymap[LAYER_POINTER][0][0]             = 0x0040u;
    test_keymap[LAYER_POINTER][0][1]             = 0x0041u;
    layer_state                                  = (layer_state_t)1u << LAYER_POINTER;
    split_runtime_sync_remote.automouse_progress = AUTOMOUSE_RGB_ACTIVE_SPAN / 2u;

    CHECK(render_output());

    rgb_t   pointer_rgb = rgb_from_hsv(layer_colors[LAYER_POINTER].color);
    uint8_t blend       = automouse_rgb_blend_amount(split_runtime_sync_remote.automouse_progress);

#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE || RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
    rgb_t end_rgb = rgb_from_hsv(automouse_fade_end_config.end_color);
#else
    rgb_t end_rgb = rgb_from_ws2812(ws2812_leds[0]);
#endif

    check_led(0, rgb_blend(pointer_rgb, end_rgb, blend));
#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE || RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
    check_led(1, rgb_blend(pointer_rgb, rgb_from_hsv(automouse_fade_end_config.end_color), blend));
    check_led(2, rgb_from_hsv(automouse_fade_end_config.end_color));
#else
    check_led(1, rgb_blend(pointer_rgb, rgb_from_ws2812(ws2812_leds[1]), blend));
    check_led(2, (rgb_t){0, 0, 0});
#endif
}

static void test_slave_locked_pd_mode_clamps_remote_automouse_progress(void) {
    test_reset();

    fake_is_master                               = false;
    ws2812_leds[0]                               = (ws2812_led_t){.r = 5, .g = 6, .b = 7};
    ws2812_leds[1]                               = (ws2812_led_t){.r = 8, .g = 9, .b = 10};
    test_keymap[LAYER_POINTER][0][0]             = 0x0040u;
    test_keymap[LAYER_POINTER][0][1]             = 0x0041u;
    layer_state                                  = (layer_state_t)1u << LAYER_POINTER;
    split_runtime_sync_remote.automouse_progress = AUTOMOUSE_RGB_ACTIVE_SPAN / 2u;
    split_runtime_sync_remote.locked_mode_id     = pd_mode_id_from_mask(PD_MODE_VOLUME);

    CHECK(render_output());

    check_led(0, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(1, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(2, rgb_from_hsv(pd_mode_colors[1].color));
}

#if !RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE && !RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
static void test_automouse_updates_target_when_underlying_layer_appears_mid_fade(void) {
    test_reset();

    ws2812_leds[0] = (ws2812_led_t){.r = 5, .g = 6, .b = 7};
    ws2812_leds[1] = (ws2812_led_t){.r = 8, .g = 9, .b = 10};
    ws2812_leds[2] = (ws2812_led_t){.r = 11, .g = 12, .b = 13};

    test_keymap[LAYER_POINTER][0][0] = 0x0040u;
    test_keymap[LAYER_POINTER][0][1] = 0x0041u;
    test_keymap[LAYER_NAV][0][1]     = 0x0030u;
    test_keymap[LAYER_NAV][0][2]     = 0x0031u;

    fake_auto_mouse_elapsed = AUTOMOUSE_RGB_DEAD_TIME + (AUTOMOUSE_RGB_ACTIVE_SPAN / 2u);
    layer_state             = (layer_state_t)1u << LAYER_POINTER;

    CHECK(render_output());
    check_led(1, rgb_blend(rgb_from_hsv(layer_colors[LAYER_POINTER].color), rgb_from_ws2812(ws2812_leds[1]), automouse_blend_amount_from_elapsed(fake_auto_mouse_elapsed)));
    check_led(2, rgb_from_ws2812(ws2812_leds[2]));

    layer_state |= (layer_state_t)1u << LAYER_NAV;

    CHECK(render_output());

    rgb_t   pointer_rgb = rgb_from_hsv(layer_colors[LAYER_POINTER].color);
    rgb_t   nav_rgb     = rgb_from_hsv(layer_colors[LAYER_NAV].color);
    uint8_t blend       = automouse_blend_amount_from_elapsed(fake_auto_mouse_elapsed);

    check_led(0, rgb_blend(pointer_rgb, rgb_from_ws2812(ws2812_leds[0]), blend));
    check_led(1, rgb_blend(pointer_rgb, nav_rgb, blend));
    check_led(2, nav_rgb);
}

static void test_automouse_updates_target_when_underlying_layer_returns_to_base_mid_fade(void) {
    test_reset();

    ws2812_leds[0] = (ws2812_led_t){.r = 5, .g = 6, .b = 7};
    ws2812_leds[1] = (ws2812_led_t){.r = 8, .g = 9, .b = 10};
    ws2812_leds[2] = (ws2812_led_t){.r = 11, .g = 12, .b = 13};
    ws2812_leds[3] = (ws2812_led_t){.r = 14, .g = 15, .b = 16};

    test_keymap[LAYER_POINTER][0][0] = 0x0040u;
    test_keymap[LAYER_POINTER][0][1] = 0x0041u;

    fake_auto_mouse_elapsed = AUTOMOUSE_RGB_DEAD_TIME + (AUTOMOUSE_RGB_ACTIVE_SPAN / 2u);
    layer_state             = ((layer_state_t)1u << LAYER_SYM) | ((layer_state_t)1u << LAYER_POINTER);

    CHECK(render_output());
    check_led(2, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));

    layer_state = (layer_state_t)1u << LAYER_POINTER;

    CHECK(render_output());

    rgb_t   pointer_rgb = rgb_from_hsv(layer_colors[LAYER_POINTER].color);
    uint8_t blend       = automouse_blend_amount_from_elapsed(fake_auto_mouse_elapsed);

    check_led(0, rgb_blend(pointer_rgb, rgb_from_ws2812(ws2812_leds[0]), blend));
    check_led(1, rgb_blend(pointer_rgb, rgb_from_ws2812(ws2812_leds[1]), blend));
    check_led(2, rgb_from_ws2812(ws2812_leds[2]));
    check_led(3, rgb_from_ws2812(ws2812_leds[3]));
}
#endif

#if !RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE && !RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
static void test_automouse_fades_pointer_layer_into_underlying_layers(void) {
    test_reset();

    ws2812_leds[0] = (ws2812_led_t){.r = 5, .g = 6, .b = 7};
    ws2812_leds[3] = (ws2812_led_t){.r = 8, .g = 9, .b = 10};

    test_keymap[LAYER_POINTER][0][0] = 0x0040u;
    test_keymap[LAYER_POINTER][0][1] = 0x0041u;
    test_keymap[LAYER_NAV][0][1]     = 0x0030u;
    test_keymap[LAYER_NAV][0][2]     = 0x0031u;

    layer_state             = ((layer_state_t)1u << LAYER_NAV) | ((layer_state_t)1u << LAYER_POINTER);
    fake_auto_mouse_elapsed = AUTOMOUSE_RGB_DEAD_TIME + (AUTOMOUSE_RGB_ACTIVE_SPAN / 2u);

    CHECK(render_output());

    rgb_t   pointer_rgb = rgb_from_hsv(layer_colors[LAYER_POINTER].color);
    rgb_t   nav_rgb     = rgb_from_hsv(layer_colors[LAYER_NAV].color);
    rgb_t   base_rgb_0  = rgb_from_ws2812(ws2812_leds[0]);
    rgb_t   base_rgb_3  = rgb_from_ws2812(ws2812_leds[3]);
    uint8_t blend       = automouse_blend_amount_from_elapsed(fake_auto_mouse_elapsed);

    check_led(0, rgb_blend(pointer_rgb, base_rgb_0, blend));
    check_led(1, rgb_blend(pointer_rgb, nav_rgb, blend));
    check_led(2, nav_rgb);
    check_led(3, base_rgb_3);
}

static void test_automouse_lands_on_base_effect_at_timeout_end(void) {
    test_reset();

    ws2812_leds[0] = (ws2812_led_t){.r = 5, .g = 6, .b = 7};
    ws2812_leds[3] = (ws2812_led_t){.r = 8, .g = 9, .b = 10};

    test_keymap[LAYER_POINTER][0][0] = 0x0040u;
    test_keymap[LAYER_POINTER][0][1] = 0x0041u;
    test_keymap[LAYER_NAV][0][1]     = 0x0030u;
    test_keymap[LAYER_NAV][0][2]     = 0x0031u;

    layer_state             = ((layer_state_t)1u << LAYER_NAV) | ((layer_state_t)1u << LAYER_POINTER);
    fake_auto_mouse_elapsed = AUTO_MOUSE_TIME;

    CHECK(render_output());

    check_led(0, rgb_from_ws2812(ws2812_leds[0]));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_NAV].color));
    check_led(2, rgb_from_hsv(layer_colors[LAYER_NAV].color));
    check_led(3, rgb_from_ws2812(ws2812_leds[3]));
}
#endif

#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
static void test_automouse_end_fill_unpainted_preserves_layer_destinations(void) {
    test_reset();

    test_keymap[LAYER_POINTER][0][0] = 0x0040u;
    test_keymap[LAYER_POINTER][0][1] = 0x0041u;
    test_keymap[LAYER_NAV][0][1]     = 0x0030u;
    test_keymap[LAYER_NAV][0][2]     = 0x0031u;

    layer_state             = ((layer_state_t)1u << LAYER_NAV) | ((layer_state_t)1u << LAYER_POINTER);
    fake_auto_mouse_elapsed = AUTOMOUSE_RGB_DEAD_TIME + (AUTOMOUSE_RGB_ACTIVE_SPAN / 2u);

    CHECK(render_output());

    rgb_t   pointer_rgb  = rgb_from_hsv(layer_colors[LAYER_POINTER].color);
    rgb_t   nav_rgb      = rgb_from_hsv(layer_colors[LAYER_NAV].color);
    rgb_t   end_fallback = rgb_from_hsv(automouse_fade_end_config.end_color);
    uint8_t blend        = automouse_blend_amount_from_elapsed(fake_auto_mouse_elapsed);

    check_led(0, rgb_blend(pointer_rgb, end_fallback, blend));
    check_led(1, rgb_blend(pointer_rgb, nav_rgb, blend));
    check_led(2, nav_rgb);
    check_led(3, end_fallback);
}
#endif

#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE
static void test_automouse_end_override_replaces_layer_stack_destination(void) {
    test_reset();

    test_keymap[LAYER_POINTER][0][0] = 0x0040u;
    test_keymap[LAYER_NAV][0][1]     = 0x0030u;

    layer_state             = ((layer_state_t)1u << LAYER_NAV) | ((layer_state_t)1u << LAYER_POINTER);
    fake_auto_mouse_elapsed = AUTOMOUSE_RGB_DEAD_TIME + (AUTOMOUSE_RGB_ACTIVE_SPAN / 2u);

    CHECK(render_output());

    rgb_t   pointer_rgb  = rgb_from_hsv(layer_colors[LAYER_POINTER].color);
    rgb_t   nav_rgb      = rgb_from_hsv(layer_colors[LAYER_NAV].color);
    rgb_t   end_override = rgb_from_hsv(automouse_fade_end_config.end_color);
    uint8_t blend        = automouse_blend_amount_from_elapsed(fake_auto_mouse_elapsed);

    check_led(0, rgb_blend(pointer_rgb, end_override, blend));
    check_led(1, rgb_blend(nav_rgb, end_override, blend));
    check_led(2, end_override);
}
#endif

int main(void) {
    test_mapped_only_layers_compose_in_layer_order();
    test_full_board_layer_fills_gaps_under_mapped_only_layer();
    test_invalidating_layer_map_refreshes_dynamic_keymap_coverage();
    test_preview_layer_overlays_existing_active_layers();
    test_slave_preview_layer_uses_remote_sync_state();
    test_slave_preview_handoff_to_matching_remote_layer_stays_continuous();
    test_combo_overlay_paints_exact_combo_keys();
    test_combo_overlay_stays_visible_over_preview();
    test_combo_underlay_stays_below_preview();
    test_combo_overlay_stays_visible_over_pd_mode();
    test_combo_underlay_stays_below_pd_mode();
#if RGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS
    test_combo_overlay_led_groups_repaint_after_combo_locality();
    test_combo_underlay_led_groups_stay_below_preview();
    test_combo_overlay_led_groups_repaint_after_pd_mode();
#endif
    test_slave_combo_overlay_stays_visible_over_remote_preview();
    test_slave_combo_underlay_stays_below_remote_preview();
    test_slave_combo_overlay_stays_visible_over_remote_pd_mode();
    test_slave_combo_underlay_stays_below_remote_pd_mode();
    test_slave_remote_combo_overlay_suppresses_stale_pending_feedback();
    test_slave_remote_combo_underlay_suppresses_stale_pending_feedback();
    test_slave_tap_commit_feedback_uses_configured_color();
    test_slave_feedback_uses_remote_semantics_and_visibility_bitmap();
    test_multi_tap_pending_uses_pending_color();
    test_tap_branch_commit_uses_branch_color();
    test_tap_branch_commit_color_clamps_to_last_configured_color();
    test_slave_tap_branch_commit_uses_remote_branch_color();
#if RGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS
    test_key_feedback_led_groups_repaint_after_feedback_locality();
    test_key_feedback_tap_branch_commit_led_group_repaints_after_feedback_locality();
    test_key_feedback_tap_commit_led_group_repaints_after_feedback_locality();
    test_key_feedback_all_led_group_uses_hold_color();
    test_key_feedback_led_groups_follow_flash_visibility();
#endif
#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF
    test_key_half_feedback_paints_only_master_half();
    test_key_half_feedback_paints_only_slave_half_from_remote_snapshot();
    test_key_half_feedback_paints_both_halves_for_combo_footprint();
    test_key_half_feedback_uses_independent_priority_per_half();
    test_key_half_feedback_follows_newest_owner_without_visible_fallback();
#endif
#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY
    test_key_feedback_paints_only_master_key();
    test_key_feedback_paints_only_slave_key_from_remote_snapshot();
    test_key_feedback_paints_all_combo_keys_on_master();
    test_key_feedback_paints_all_remote_combo_keys_from_snapshot();
#endif
#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF
    test_key_left_half_feedback_paints_fixed_left_half_from_any_source_side();
    test_key_left_half_feedback_uses_newest_global_owner();
    test_key_left_half_feedback_does_not_fallback_from_hidden_newest_owner();
#endif
#if RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
    test_key_right_half_feedback_paints_fixed_right_half_from_any_source_side();
    test_key_right_half_feedback_uses_remote_snapshot();
#endif
    test_slave_full_scene_preserves_remote_preview_and_locked_pd_mode_when_feedback_flash_is_hidden();
#if !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
    test_slave_full_scene_feedback_overrides_remote_preview_and_locked_pd_mode();
#endif
    test_render_order_preview_then_pd_mode_then_pd_group();
#if !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
    test_render_order_base_then_preview_then_pd_mode_then_feedback();
#endif
    test_runtime_boot_indicator_overrides_scene();
#if !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF && !RGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF
    test_multi_tap_pending_feedback_overrides_preview_and_pd_mode();
    test_hold_pending_feedback_overrides_preview_and_pd_mode();
    test_newest_global_feedback_owner_wins_over_older_higher_priority();
#endif
    test_pointer_mode_overlay_paints_right_half();
    test_pointer_mode_overlay_paints_left_half_and_groups();
    test_pointer_mode_overlay_paints_both_halves();
    test_pointer_mode_overlay_paints_trigger_half_on_master();
    test_pointer_mode_overlay_paints_trigger_both_halves_for_combo_origin();
    test_pointer_mode_overlay_trigger_half_falls_back_both_without_owner();
    test_pointer_mode_overlay_paints_trigger_keys_on_master();
    test_pointer_mode_overlay_trigger_keys_falls_back_both_without_owner();
    test_slave_pointer_mode_overlay_uses_remote_display_state_and_trigger_half();
    test_slave_pointer_mode_overlay_uses_remote_trigger_keys();
    test_automouse_uses_configured_target_layer();
    test_timeout_end_keeps_automouse_at_destination_on_master();
    test_timeout_window_keeps_fading_after_auto_mouse_active_drops_on_master();
    test_timeout_end_keeps_automouse_at_destination_on_slave();
    test_slave_timeout_window_renders_without_live_auto_mouse_active_flag();
    test_slave_locked_pd_mode_clamps_remote_automouse_progress();
#if !RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE && !RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
    test_automouse_updates_target_when_underlying_layer_appears_mid_fade();
    test_automouse_updates_target_when_underlying_layer_returns_to_base_mid_fade();
    test_automouse_fades_pointer_layer_into_underlying_layers();
    test_automouse_lands_on_base_effect_at_timeout_end();
#endif
#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
    test_automouse_end_fill_unpainted_preserves_layer_destinations();
#endif
#if RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE
    test_automouse_end_override_replaces_layer_stack_destination();
#endif
    return 0;
}
