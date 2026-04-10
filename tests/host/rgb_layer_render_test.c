#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/rgb/rgb_runtime.h"
#include "users/noah/lib/rgb/rgb_helpers.h"

enum test_layers {
    LAYER_BASE = 0,
    LAYER_NUM,
    LAYER_SYM,
    LAYER_NAV,
    LAYER_POINTER,
};

bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max);

layer_state_t layer_state;

static uint16_t test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
static rgb_t    led_output[RGB_MATRIX_LED_COUNT];

led_config_t g_led_config = {0};

const layer_color_config_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE] = {.color = {0, 0, 0}, .flags = LAYER_COLOR_FLAG_NONE}, [LAYER_NUM] = {.color = {10, 20, 30}, .flags = LAYER_COLOR_FLAG_MAPPED_KEYS_ONLY}, [LAYER_SYM] = {.color = {40, 50, 60}, .flags = LAYER_COLOR_FLAG_NONE}, [LAYER_NAV] = {.color = {70, 80, 90}, .flags = LAYER_COLOR_FLAG_MAPPED_KEYS_ONLY}, [LAYER_POINTER] = {.color = {0, 0, 0}, .flags = LAYER_COLOR_FLAG_NONE},
};

const layer_led_group_t layer_led_groups[1]   = {0};
const uint8_t           layer_led_group_count = 0;

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

static void test_reset(void) {
    memset(test_keymap, 0, sizeof(test_keymap));
    memset(led_output, 0, sizeof(led_output));
    layer_state = 0;

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
    return true;
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
    CHECK(led_output[index].r == expected.r);
    CHECK(led_output[index].g == expected.g);
    CHECK(led_output[index].b == expected.b);
}

static void test_mapped_only_layers_compose_in_layer_order(void) {
    test_reset();

    test_keymap[LAYER_NUM][0][0] = 0x0020u;
    test_keymap[LAYER_NUM][0][1] = 0x0021u;
    test_keymap[LAYER_NAV][0][1] = 0x0030u;
    test_keymap[LAYER_NAV][0][2] = 0x0031u;

    layer_state = ((layer_state_t)1u << LAYER_NUM) | ((layer_state_t)1u << LAYER_NAV);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

    check_led(0, rgb_from_hsv(layer_colors[LAYER_NUM].color));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_NAV].color));
    check_led(2, rgb_from_hsv(layer_colors[LAYER_NAV].color));
    check_led(3, (rgb_t){0, 0, 0});
}

static void test_full_board_layer_fills_gaps_under_mapped_only_layer(void) {
    test_reset();

    test_keymap[LAYER_NAV][0][1] = 0x0030u;
    layer_state                  = ((layer_state_t)1u << LAYER_SYM) | ((layer_state_t)1u << LAYER_NAV);

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));

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

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));
    check_led(1, rgb_from_hsv(layer_colors[LAYER_NAV].color));
    check_led(2, (rgb_t){0, 0, 0});

    memset(led_output, 0, sizeof(led_output));
    test_keymap[LAYER_NAV][0][1] = KC_TRNS;
    test_keymap[LAYER_NAV][0][2] = 0x0031u;
    noah_rgb_runtime_invalidate_layer_maps();

    CHECK(noah_rgb_matrix_indicators_advanced_user(0, RGB_MATRIX_LED_COUNT));
    check_led(1, (rgb_t){0, 0, 0});
    check_led(2, rgb_from_hsv(layer_colors[LAYER_NAV].color));
}

int main(void) {
    test_mapped_only_layers_compose_in_layer_order();
    test_full_board_layer_fills_gaps_under_mapped_only_layer();
    test_invalidating_layer_map_refreshes_dynamic_keymap_coverage();
    return 0;
}
