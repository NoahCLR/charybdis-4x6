// ────────────────────────────────────────────────────────────────────────────
// RGB Layer Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_layer_stage.h"

#ifdef RGB_MATRIX_ENABLE

#    include <string.h>

#    include "keymap_introspection.h" // QMK

extern const layer_color_config_t     layer_colors[];
extern const layer_led_group_t *const layer_led_groups;
extern const uint8_t                  layer_led_group_count;

static rgb_t layer_rgb[LAYER_COUNT];
static bool  layer_key_led_map_dirty = true;
static bool  layer_key_led_map[LAYER_COUNT][RGB_MATRIX_LED_COUNT];

static bool rgb_runtime_layer_stage_paints_only_keys_present_on_this_layer(uint8_t layer) {
    return layer_colors[layer].mode == KEYS_MAPPED_ON_THIS_LAYER_ONLY;
}

static bool rgb_runtime_layer_stage_layer_is_effectively_active(layer_state_t state, uint8_t layer) {
    return layer == 0u || layer_state_cmp(state, layer);
}

static bool rgb_runtime_layer_stage_keycode_is_mapped(uint16_t keycode) {
    return keycode != KC_NO && keycode != KC_TRNS;
}

static void rgb_runtime_layer_stage_rebuild_key_led_map(void) {
    if (!layer_key_led_map_dirty) {
        return;
    }

    memset(layer_key_led_map, 0, sizeof(layer_key_led_map));

    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        if (!rgb_runtime_layer_stage_paints_only_keys_present_on_this_layer(layer)) {
            continue;
        }

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                uint16_t keycode = keycode_at_keymap_location(layer, row, col);
                if (!rgb_runtime_layer_stage_keycode_is_mapped(keycode)) {
                    continue;
                }

                uint8_t leds[RGB_MATRIX_LED_COUNT];
                uint8_t led_count = rgb_matrix_map_row_column_to_led(row, col, leds);
                for (uint8_t i = 0; i < led_count; i++) {
                    if (leds[i] < RGB_MATRIX_LED_COUNT) {
                        layer_key_led_map[layer][leds[i]] = true;
                    }
                }
            }
        }
    }

    layer_key_led_map_dirty = false;
}

static bool rgb_runtime_frame_paint_layer(rgb_runtime_frame_t *frame, uint8_t layer, uint8_t led_min, uint8_t led_max) {
    if (!rgb_runtime_layer_stage_paints_only_keys_present_on_this_layer(layer)) {
        return rgb_runtime_frame_fill(frame, layer_rgb[layer], led_min, led_max);
    }

    rgb_runtime_layer_stage_rebuild_key_led_map();

    bool painted = false;
    for (uint8_t led = led_min; led < led_max; led++) {
        if (!layer_key_led_map[layer][led]) {
            continue;
        }

        frame->colors[led]  = layer_rgb[layer];
        frame->painted[led] = true;
        painted             = true;
    }

    return painted;
}

static bool rgb_runtime_frame_paint_led_group(rgb_runtime_frame_t *frame, const uint8_t *leds, uint8_t count, rgb_t color, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    for (uint8_t i = 0; i < count; i++) {
        uint8_t led = leds[i];
        if (led < led_min || led >= led_max) {
            continue;
        }

        frame->colors[led]  = color;
        frame->painted[led] = true;
        painted             = true;
    }

    return painted;
}

void rgb_runtime_layer_stage_post_init(void) {
    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        layer_rgb[layer] = hsv_to_rgb(layer_colors[layer].color);
    }

    rgb_runtime_layer_stage_invalidate_maps();
}

void rgb_runtime_layer_stage_invalidate_maps(void) {
    layer_key_led_map_dirty = true;
}

bool rgb_runtime_layer_stage_has_solid_color(uint8_t layer) {
    return !(layer_colors[layer].color.s == 0 && layer_colors[layer].color.v == 0);
}

void rgb_runtime_frame_clear(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    for (uint8_t led = led_min; led < led_max; led++) {
        frame->colors[led]  = (rgb_t){0};
        frame->painted[led] = false;
    }
}

bool rgb_runtime_frame_fill(rgb_runtime_frame_t *frame, rgb_t color, uint8_t led_min, uint8_t led_max) {
    if (led_min >= led_max) {
        return false;
    }

    for (uint8_t led = led_min; led < led_max; led++) {
        frame->colors[led]  = color;
        frame->painted[led] = true;
    }

    return true;
}

bool rgb_runtime_layer_stage_render_frame(rgb_runtime_frame_t *frame, layer_state_t state, uint8_t led_min, uint8_t led_max) {
    rgb_runtime_frame_clear(frame, led_min, led_max);

    bool painted = false;

    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        if (!rgb_runtime_layer_stage_layer_is_effectively_active(state, layer)) {
            continue;
        }
        if (!rgb_runtime_layer_stage_has_solid_color(layer)) {
            continue;
        }

        painted |= rgb_runtime_frame_paint_layer(frame, layer, led_min, led_max);
    }

    for (uint8_t group = 0; group < layer_led_group_count; group++) {
        if (!rgb_runtime_layer_stage_layer_is_effectively_active(state, layer_led_groups[group].layer)) {
            continue;
        }

        const rgb_led_group_t *led_group = &layer_led_groups[group].led_group;
        rgb_t                  group_rgb = hsv_to_rgb(layer_led_groups[group].color);
        painted |= rgb_runtime_frame_paint_led_group(frame, led_group->leds, led_group->count, group_rgb, led_min, led_max);
    }

    return painted;
}

bool rgb_runtime_layer_stage_apply_frame(const rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    for (uint8_t led = led_min; led < led_max; led++) {
        if (!frame->painted[led]) {
            continue;
        }

        rgb_set_led_color(led, led_min, led_max, frame->colors[led]);
        painted = true;
    }

    return painted;
}

bool rgb_runtime_layer_stage_paint_layer(uint8_t layer, uint8_t led_min, uint8_t led_max) {
    if (!rgb_runtime_layer_stage_paints_only_keys_present_on_this_layer(layer)) {
        rgb_set_both_halves(layer_rgb[layer], led_min, led_max);
        return led_min < led_max;
    }

    rgb_runtime_layer_stage_rebuild_key_led_map();

    bool painted = false;
    for (uint8_t led = led_min; led < led_max; led++) {
        if (!layer_key_led_map[layer][led]) {
            continue;
        }

        rgb_set_led_color(led, led_min, led_max, layer_rgb[layer]);
        painted = true;
    }

    return painted;
}

#endif
