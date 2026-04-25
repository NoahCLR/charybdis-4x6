#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/rgb/core/rgb_config_helpers.h"
#include "users/noah/lib/rgb/core/rgb_validation.h"

static char log_buffer[4096];

static const layer_led_group_t layer_groups[] = {
    {.layer = LAYER_COUNT, .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP(0, RGB_MATRIX_LED_COUNT)},
};
static const pd_mode_led_group_t pd_mode_groups[] = {
    {.pointing_mode = PD_MODE_VOLUME, .color = HSV(1, 1, 1), .led_group = RGB_LED_GROUP(RGB_MATRIX_LED_COUNT)},
    {.pointing_mode = (pd_mode_mask_t)0x8000u, .color = HSV(2, 2, 2), .led_group = RGB_LED_GROUP(1)},
};
static const combo_feedback_led_group_t combo_feedback_groups[] = {
    {.color = HSV(4, 4, 4), .led_group = RGB_LED_GROUP(RGB_MATRIX_LED_COUNT)},
};
static const key_behavior_feedback_led_group_t key_behavior_feedback_groups[] = {
    {.semantic = (key_behavior_feedback_group_semantic_t)0xFFu, .color = HSV(5, 5, 5), .led_group = RGB_LED_GROUP(RGB_MATRIX_LED_COUNT)},
};

const layer_color_config_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE] = {.color = HSV(0, 0, 0), .mode = (uint8_t)0xFFu},
};
const layer_led_group_t *const    layer_led_groups          = layer_groups;
const uint8_t                     layer_led_group_count     = (uint8_t)ARRAY_SIZE(layer_groups);
const pd_mode_led_group_t *const  pd_mode_led_groups        = pd_mode_groups;
const uint8_t                     pd_mode_led_group_count   = (uint8_t)ARRAY_SIZE(pd_mode_groups);
const combo_feedback_led_group_t *const combo_feedback_led_groups      = combo_feedback_groups;
const uint8_t                           combo_feedback_led_group_count = (uint8_t)ARRAY_SIZE(combo_feedback_groups);
const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups      = key_behavior_feedback_groups;
const uint8_t                                  key_behavior_feedback_led_group_count = (uint8_t)ARRAY_SIZE(key_behavior_feedback_groups);
const automouse_fade_end_config_t automouse_fade_end_config = {
    .mode      = (automouse_fade_end_mode_t)0xFFu,
    .end_color = HSV(3, 3, 3),
};
const combo_feedback_color_config_t combo_feedback_colors = {
    .color    = HSV(0, 0, 4),
    .locality = (rgb_locality_t)0xFFu,
};

const pd_mode_color_t pd_mode_colors[] = {
    {.pointing_mode = PD_MODE_VOLUME, .color = HSV(10, 10, 10), .locality = RGB_RIGHT_HALF},
    {.pointing_mode = PD_MODE_VOLUME, .color = HSV(20, 20, 20), .locality = (rgb_locality_t)0xFFu},
    {.pointing_mode = PD_MODE_BRIGHTNESS, .color = HSV(30, 30, 30), .locality = RGB_KEY_HALF},
    {.pointing_mode = (pd_mode_mask_t)0x4000u, .color = HSV(40, 40, 40), .locality = RGB_LEFT_HALF},
};
const uint8_t                              pd_mode_color_count          = (uint8_t)ARRAY_SIZE(pd_mode_colors);
const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    .multi_tap_pending_color = HSV(0, 0, 1),
    .hold_active_color       = HSV(0, 0, 2),
    .long_hold_active_color  = HSV(0, 0, 3),
    .locality                = (rgb_locality_t)0xFFu,
};

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

    CHECK(strstr(log_buffer, "Invalid layer_colors") != NULL);
    CHECK(strstr(log_buffer, "Invalid layer_led_groups") != NULL);
    CHECK(strstr(log_buffer, "Invalid automouse_fade_end_config.mode") != NULL);
    CHECK(strstr(log_buffer, "Unknown pd_mode_colors") != NULL);
    CHECK(strstr(log_buffer, "Invalid pd_mode_colors[1].locality") != NULL);
#ifndef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(strstr(log_buffer, "RGB_PD_MODE_ACTIVE_HALF_ENABLE is disabled") != NULL);
#endif
    CHECK(strstr(log_buffer, "Duplicate pd_mode_colors entries") != NULL);
    CHECK(strstr(log_buffer, "Missing pd_mode_colors entry") != NULL);
    CHECK(strstr(log_buffer, "Unknown pd_mode_led_groups") != NULL);
    CHECK(strstr(log_buffer, "Invalid pd_mode_led_groups") != NULL);
    CHECK(strstr(log_buffer, "Invalid combo_feedback_colors.locality") != NULL);
    CHECK(strstr(log_buffer, "Invalid combo_feedback_led_groups") != NULL);
    CHECK(strstr(log_buffer, "Invalid key_behavior_feedback_colors.locality") != NULL);
    CHECK(strstr(log_buffer, "Invalid key_behavior_feedback_led_groups[0].semantic") != NULL);
    CHECK(strstr(log_buffer, "Invalid key_behavior_feedback_led_groups") != NULL);

    puts("rgb_validation host tests passed");
    return 0;
}
