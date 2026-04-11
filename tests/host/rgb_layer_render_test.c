#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/key/key_runtime_feedback.h"
#include "users/noah/lib/pointing/pd_modes.h"
#include "users/noah/lib/rgb/rgb_automouse.h"
#include "users/noah/lib/rgb/rgb_config_helpers.h"
#include "users/noah/lib/state/split_runtime_sync.h"
#include "users/noah/lib/rgb/rgb_runtime.h"
#include "users/noah/lib/rgb/rgb_helpers.h"
#include "ws2812.h"

#ifndef RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE
#    define RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE 0
#endif
#ifndef RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED
#    define RGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED 0
#endif

enum test_layers {
    LAYER_BASE = 0,
    LAYER_NUM,
    LAYER_SYM,
    LAYER_NAV,
    LAYER_POINTER,
};

bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max);

layer_state_t layer_state;

static uint16_t       test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
static rgb_t          led_output[RGB_MATRIX_LED_COUNT];
static uint8_t        fake_preview_layer      = UINT8_MAX;
static uint8_t        fake_auto_mouse_layer   = LAYER_POINTER;
static uint16_t       fake_auto_mouse_elapsed = 0;
static bool           fake_auto_mouse_active  = true;
static bool           fake_is_master          = true;
static pd_mode_mask_t fake_pd_active_flags    = 0;
static pd_mode_mask_t fake_pd_locked_flags    = 0;

ws2812_led_t                ws2812_leds[WS2812_LED_COUNT];
split_runtime_sync_packet_t split_runtime_sync_remote = {
    .key_preview_layer = UINT8_MAX,
};

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

DEFINE_PD_MODE_COLORS({.pointing_mode = PD_MODE_ARROW, .color = HSV(210, 211, 212)}, {.pointing_mode = PD_MODE_VOLUME, .color = HSV(220, 221, 222)});
static const uint8_t volume_mode_group_leds[] = {1, 6};
DEFINE_PD_MODE_LED_GROUPS({.pointing_mode = PD_MODE_VOLUME, .color = HSV(230, 231, 232), .leds = volume_mode_group_leds, .count = ARRAY_SIZE(volume_mode_group_leds)});
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
DEFINE_KEY_BEHAVIOR_FEEDBACK_COLORS(.multi_tap_pending_color = HSV(1, 2, 3), .hold_active_color = HSV(4, 5, 6), .long_hold_active_color = HSV(7, 8, 9), );
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

static rgb_t rgb_blend(rgb_t start, rgb_t end, uint8_t amount) {
    uint32_t inv = (uint32_t)UINT8_MAX - amount;

    return (rgb_t){
        .r = (uint8_t)(((uint32_t)start.r * inv + (uint32_t)end.r * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
        .g = (uint8_t)(((uint32_t)start.g * inv + (uint32_t)end.g * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
        .b = (uint8_t)(((uint32_t)start.b * inv + (uint32_t)end.b * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
    };
}

static uint8_t automouse_blend_amount_from_elapsed(uint16_t elapsed) {
    uint16_t progress = automouse_rgb_progress(elapsed);
    if (fake_pd_locked_flags != 0) {
        progress = 0;
    }
    if (progress > AUTOMOUSE_RGB_ACTIVE_SPAN) {
        progress = AUTOMOUSE_RGB_ACTIVE_SPAN;
    }

    return automouse_rgb_blend_amount(progress);
}

static void test_reset(void) {
    memset(test_keymap, 0, sizeof(test_keymap));
    memset(led_output, 0, sizeof(led_output));
    memset(ws2812_leds, 0, sizeof(ws2812_leds));
    layer_state               = 0;
    fake_preview_layer        = UINT8_MAX;
    fake_auto_mouse_layer     = LAYER_POINTER;
    fake_auto_mouse_elapsed   = 0;
    fake_auto_mouse_active    = true;
    fake_is_master            = true;
    fake_pd_active_flags      = 0;
    fake_pd_locked_flags      = 0;
    split_runtime_sync_remote = (split_runtime_sync_packet_t){
        .key_preview_layer = UINT8_MAX,
    };

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

    noah_rgb_runtime_post_init();
}

bool is_keyboard_master(void) {
    return fake_is_master;
}

int rgb_matrix_led_index(int index) {
    if (index < 0 || index >= RGB_MATRIX_LED_COUNT) {
        return -1;
    }

    return index;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return (state & ((layer_state_t)1u << layer)) != 0;
}

rgb_t hsv_to_rgb(hsv_t hsv) {
    return rgb_from_hsv(hsv);
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return test_keymap[layer_num][row][column];
}

uint8_t key_feedback_pack(void) {
    return 0;
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

bool pd_any_mode_locked(void) {
    return fake_pd_locked_flags != 0;
}

bool pd_mode_active(pd_mode_mask_t mode) {
    return (fake_pd_active_flags & mode) != 0;
}

bool pd_mode_locked(pd_mode_mask_t mode) {
    return (fake_pd_locked_flags & mode) != 0;
}

bool pd_any_mode_active(void) {
    return fake_pd_active_flags != 0;
}

pd_mode_mask_t pd_mode_active_snapshot(void) {
    return fake_pd_active_flags;
}

pd_mode_mask_t pd_mode_locked_snapshot(void) {
    return fake_pd_locked_flags;
}

uint8_t pd_mode_first_active_index(void) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag)) {
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

static void test_slave_feedback_uses_remote_flags_and_flash_phase(void) {
    test_reset();

    fake_is_master                               = false;
    layer_state                                  = (layer_state_t)1u << LAYER_SYM;
    split_runtime_sync_remote.key_feedback_flags = KEY_FEEDBACK_FLAG_HOLD_ACTIVE | KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE | KEY_FEEDBACK_FLAG_LEVEL_FLASH;

    CHECK(render_output());
    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(7, rgb_from_hsv(layer_colors[LAYER_SYM].color));

    memset(led_output, 0, sizeof(led_output));
    split_runtime_sync_remote.key_feedback_flags |= KEY_FEEDBACK_FLAG_FLASH_PHASE;

    CHECK(render_output());
    check_led(0, rgb_from_hsv(key_behavior_feedback_colors.long_hold_active_color));
    check_led(7, rgb_from_hsv(key_behavior_feedback_colors.long_hold_active_color));
}

static void test_pointer_mode_overlay_paints_right_half_and_groups(void) {
    test_reset();

    layer_state          = (layer_state_t)1u << LAYER_SYM;
    fake_pd_active_flags = PD_MODE_VOLUME;

    CHECK(render_output());

    check_led(0, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(1, rgb_from_hsv(pd_mode_led_groups[0].color));
    check_led(2, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(3, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_led(4, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(5, rgb_from_hsv(pd_mode_colors[1].color));
    check_led(6, rgb_from_hsv(pd_mode_led_groups[0].color));
    check_led(7, rgb_from_hsv(pd_mode_colors[1].color));
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
    test_slave_feedback_uses_remote_flags_and_flash_phase();
    test_pointer_mode_overlay_paints_right_half_and_groups();
    test_automouse_uses_configured_target_layer();
    test_timeout_end_keeps_automouse_at_destination_on_master();
    test_timeout_window_keeps_fading_after_auto_mouse_active_drops_on_master();
    test_timeout_end_keeps_automouse_at_destination_on_slave();
    test_slave_timeout_window_renders_without_live_auto_mouse_active_flag();
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
