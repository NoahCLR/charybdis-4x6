#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/pointing/pd_modes.h"
#include "users/noah/lib/rgb/rgb_config_helpers.h"
#include "users/noah/lib/rgb/rgb_validation.h"

static char log_buffer[4096];

static const uint8_t           invalid_layer_leds[] = {0, RGB_MATRIX_LED_COUNT};
static const uint8_t           invalid_mode_leds[]  = {RGB_MATRIX_LED_COUNT};
static const uint8_t           unknown_mode_leds[]  = {1};
static const layer_led_group_t layer_groups[]       = {
    {.layer = LAYER_COUNT, .color = HSV(0, 0, 0), .leds = invalid_layer_leds, .count = ARRAY_SIZE(invalid_layer_leds)},
};
static const pd_mode_led_group_t pd_mode_groups[] = {
    {.pointing_mode = PD_MODE_VOLUME, .color = HSV(1, 1, 1), .leds = invalid_mode_leds, .count = ARRAY_SIZE(invalid_mode_leds)},
    {.pointing_mode = (pd_mode_mask_t)0x8000u, .color = HSV(2, 2, 2), .leds = unknown_mode_leds, .count = ARRAY_SIZE(unknown_mode_leds)},
};

const layer_led_group_t *const   layer_led_groups        = layer_groups;
const uint8_t                    layer_led_group_count   = (uint8_t)ARRAY_SIZE(layer_groups);
const pd_mode_led_group_t *const pd_mode_led_groups      = pd_mode_groups;
const uint8_t                    pd_mode_led_group_count = (uint8_t)ARRAY_SIZE(pd_mode_groups);

const pd_mode_color_t pd_mode_colors[] = {
    {.pointing_mode = PD_MODE_VOLUME, .color = HSV(10, 10, 10)},
    {.pointing_mode = PD_MODE_VOLUME, .color = HSV(20, 20, 20)},
    {.pointing_mode = (pd_mode_mask_t)0x4000u, .color = HSV(30, 30, 30)},
};
const uint8_t pd_mode_color_count = (uint8_t)ARRAY_SIZE(pd_mode_colors);

const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {
    [PD_MODE_INDEX_DRAGSCROLL] = {.mode_flag = PD_MODE_DRAGSCROLL, .keycode = DRAGSCROLL}, [PD_MODE_INDEX_VOLUME] = {.mode_flag = PD_MODE_VOLUME, .keycode = VOLUME_MODE}, [PD_MODE_INDEX_BRIGHTNESS] = {.mode_flag = PD_MODE_BRIGHTNESS, .keycode = BRIGHTNESS_MODE}, [PD_MODE_INDEX_ZOOM] = {.mode_flag = PD_MODE_ZOOM, .keycode = ZOOM_MODE}, [PD_MODE_INDEX_ARROW] = {.mode_flag = PD_MODE_ARROW, .keycode = ARROW_MODE}, [PD_MODE_INDEX_PINCH] = {.mode_flag = PD_MODE_PINCH, .keycode = PINCH_MODE},
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

int uprintf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int written = vsnprintf(log_buffer + strlen(log_buffer), sizeof(log_buffer) - strlen(log_buffer), fmt, args);
    va_end(args);

    return written;
}

int main(void) {
    noah_rgb_validate_config();

    CHECK(strstr(log_buffer, "Invalid layer_led_groups") != NULL);
    CHECK(strstr(log_buffer, "Unknown pd_mode_colors") != NULL);
    CHECK(strstr(log_buffer, "Duplicate pd_mode_colors entries") != NULL);
    CHECK(strstr(log_buffer, "Missing pd_mode_colors entry") != NULL);
    CHECK(strstr(log_buffer, "Unknown pd_mode_led_groups") != NULL);
    CHECK(strstr(log_buffer, "Invalid pd_mode_led_groups") != NULL);

    puts("rgb_validation host tests passed");
    return 0;
}
