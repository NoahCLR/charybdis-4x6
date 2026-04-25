#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/rgb/core/rgb_config_helpers.h"
#include "users/noah/lib/rgb/stages/rgb_layer_stage.h"

enum test_layers {
    LAYER_BASE = 0,
    LAYER_NAV,
    LAYER_SYM,
};

static uint16_t test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

layer_state_t layer_state  = 0;
led_config_t  g_led_config = {0};

const layer_color_config_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE] =
        {
            .color = {.h = 1, .s = 2, .v = 3},
            .mode  = ALL_KEYS,
        },
    [LAYER_NAV] =
        {
            .color = {.h = 10, .s = 20, .v = 30},
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
    [LAYER_SYM] =
        {
            .color = {.h = 40, .s = 50, .v = 60},
            .mode  = ALL_KEYS,
        },
};

static const layer_led_group_t layer_led_groups_data[] = {
    {
        .layer     = LAYER_BASE,
        .color     = {.h = 7, .s = 8, .v = 9},
        .led_group = RGB_LED_GROUP(3),
    },
};

const layer_led_group_t *const layer_led_groups      = layer_led_groups_data;
const uint8_t                  layer_led_group_count = (uint8_t)ARRAY_SIZE(layer_led_groups_data);

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

static void check_frame_led(const rgb_runtime_frame_t *frame, uint8_t index, rgb_t expected) {
    CHECK(frame->painted[index]);
    CHECK(frame->colors[index].r == expected.r);
    CHECK(frame->colors[index].g == expected.g);
    CHECK(frame->colors[index].b == expected.b);
}

static void test_reset(void) {
    memset(test_keymap, 0, sizeof(test_keymap));
    memset(&g_led_config, 0xFF, sizeof(g_led_config));
    layer_state = 0;

    g_led_config.matrix_co[0][0] = 0;
    g_led_config.matrix_co[0][1] = 1;
    g_led_config.matrix_co[0][2] = 2;
    g_led_config.matrix_co[0][3] = 3;

    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                test_keymap[layer][row][col] = KC_TRNS;
            }
        }
    }

    rgb_runtime_layer_stage_post_init();
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return test_keymap[layer_num][row][column];
}

rgb_t hsv_to_rgb(hsv_t hsv) {
    return rgb_from_hsv(hsv);
}

void rgb_matrix_set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    (void)index;
    (void)red;
    (void)green;
    (void)blue;
}

int rgb_matrix_led_index(int index) {
    return index;
}

uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i) {
    uint8_t led = g_led_config.matrix_co[row][column];
    if (led == NO_LED) {
        return 0;
    }

    led_i[0] = led;
    return 1;
}

static void test_base_layer_is_visible_when_no_overlay_layer_is_active(void) {
    test_reset();

    rgb_runtime_frame_t frame;

    CHECK(rgb_runtime_layer_stage_render_frame(&frame, 0, 0, RGB_MATRIX_LED_COUNT));

    check_frame_led(&frame, 0, rgb_from_hsv(layer_colors[LAYER_BASE].color));
    check_frame_led(&frame, 1, rgb_from_hsv(layer_colors[LAYER_BASE].color));
    check_frame_led(&frame, 2, rgb_from_hsv(layer_colors[LAYER_BASE].color));
    check_frame_led(&frame, 3, rgb_from_hsv(layer_led_groups[0].color));
}

static void test_base_layer_remains_under_mapped_only_overlay_layers(void) {
    test_reset();

    test_keymap[LAYER_NAV][0][1] = 0x004Fu;
    layer_state                  = (layer_state_t)1u << LAYER_NAV;

    rgb_runtime_frame_t frame;

    CHECK(rgb_runtime_layer_stage_render_frame(&frame, layer_state, 0, RGB_MATRIX_LED_COUNT));

    check_frame_led(&frame, 0, rgb_from_hsv(layer_colors[LAYER_BASE].color));
    check_frame_led(&frame, 1, rgb_from_hsv(layer_colors[LAYER_NAV].color));
    check_frame_led(&frame, 2, rgb_from_hsv(layer_colors[LAYER_BASE].color));
    check_frame_led(&frame, 3, rgb_from_hsv(layer_led_groups[0].color));
}

int main(void) {
    test_base_layer_is_visible_when_no_overlay_layer_is_active();
    test_base_layer_remains_under_mapped_only_overlay_layers();

    puts("rgb_base_underlay host tests passed");
    return 0;
}
