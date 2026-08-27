// ────────────────────────────────────────────────────────────────────────────
// RGB Layer Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_layer_stage.h"

#ifdef RGB_MATRIX_ENABLE

#    include <string.h>

#    include "keymap_introspection.h" // QMK

static bool  layer_key_led_map_dirty = true;
static bool  layer_key_led_map[LAYER_COUNT][RGB_MATRIX_LED_COUNT];
#    ifdef RGB_LAYER_STAGE_TEST_BACKEND
static uint32_t layer_group_scan_count = 0;
#    endif

typedef struct {
    rgb_t   colors[LAYER_COUNT];
    uint8_t modes[LAYER_COUNT];
    bool    solid[LAYER_COUNT];
} rgb_effective_layer_profile_t;

static bool rgb_runtime_layer_stage_profile(const noah_effective_rgb_frame_t *frame, rgb_effective_layer_profile_t *profile) {
    if (!profile || !rgb_effective_config_layer_stage_enabled(frame)) {
        return false;
    }
    memset(profile, 0, sizeof(*profile));
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        layer_color_config_t config;

        if (!rgb_effective_config_layer_color(frame, layer, &config)) {
            return false;
        }
        profile->colors[layer] = hsv_to_rgb(config.color);
        profile->modes[layer]  = config.mode;
        profile->solid[layer]  = !(config.color.s == 0u && config.color.v == 0u);
    }
    return true;
}

static bool rgb_runtime_layer_stage_paints_only_keys_present_on_this_layer(const rgb_effective_layer_profile_t *profile, uint8_t layer) {
    return profile && layer < LAYER_COUNT && profile->modes[layer] == KEYS_MAPPED_ON_THIS_LAYER_ONLY;
}

static bool rgb_runtime_layer_stage_layer_is_effectively_active(layer_state_t state, uint8_t layer) {
    return layer == 0u || layer_state_cmp(state, layer);
}

static bool rgb_runtime_layer_stage_selection_contains(layer_state_t state, uint8_t selected_layer, uint8_t layer) {
    if (selected_layer < LAYER_COUNT) {
        return layer == selected_layer;
    }

    return rgb_runtime_layer_stage_layer_is_effectively_active(state, layer);
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

static bool rgb_runtime_frame_paint_layer(rgb_runtime_frame_t *frame, const rgb_effective_layer_profile_t *profile, uint8_t layer, uint8_t led_min, uint8_t led_max) {
    if (!rgb_runtime_layer_stage_paints_only_keys_present_on_this_layer(profile, layer)) {
        return rgb_runtime_frame_fill(frame, profile->colors[layer], led_min, led_max);
    }

    rgb_runtime_layer_stage_rebuild_key_led_map();

    bool painted = false;
    for (uint8_t led = led_min; led < led_max; led++) {
        if (!layer_key_led_map[layer][led]) {
            continue;
        }

        frame->colors[led]  = profile->colors[layer];
        frame->painted[led] = true;
        painted             = true;
    }

    return painted;
}

void rgb_runtime_layer_stage_post_init(void) {
    rgb_runtime_layer_stage_invalidate_maps();
}

void rgb_runtime_layer_stage_invalidate_maps(void) {
    layer_key_led_map_dirty = true;
}

static bool rgb_runtime_layer_stage_has_solid_color(const rgb_effective_layer_profile_t *profile, uint8_t layer) {
    return profile && layer < LAYER_COUNT && profile->solid[layer];
}

static bool rgb_runtime_layer_stage_group_color(const rgb_effective_layer_profile_t *profile, uint8_t layer, hsv_t group_color, rgb_t *out_color) {
    if (!profile || !out_color || layer >= LAYER_COUNT) {
        return false;
    }

    if (rgb_hsv_is_inherit_color(group_color)) {
        if (!rgb_runtime_layer_stage_has_solid_color(profile, layer)) {
            return false;
        }

        *out_color = profile->colors[layer];
        return true;
    }

    *out_color = hsv_to_rgb(group_color);
    return true;
}

static bool rgb_runtime_layer_stage_paint_group_for_layer(rgb_runtime_frame_t *frame, const rgb_effective_layer_profile_t *profile, const rgb_effective_layer_group_t *group, uint8_t layer, uint8_t led_min, uint8_t led_max) {
    rgb_t group_rgb;

    if (!(frame && group)) {
        return false;
    }

    if (!rgb_runtime_layer_stage_group_color(profile, layer, group->color, &group_rgb)) {
        return false;
    }

    bool painted = false;
    for (uint8_t led = led_min; led < led_max; led++) {
        if (led >= NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT) {
            continue;
        }
        if ((group->bitmap[led / 8u] & (uint8_t)(1u << (led % 8u))) == 0u) {
            continue;
        }
        frame->colors[led]  = group_rgb;
        frame->painted[led] = true;
        painted             = true;
    }
    return painted;
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

static bool rgb_runtime_layer_stage_render_selection(rgb_runtime_frame_t *frame, layer_state_t state, uint8_t selected_layer, const noah_effective_rgb_frame_t *profile_frame, uint8_t led_min, uint8_t led_max) {
    rgb_effective_layer_profile_t profile;
    bool                          painted = false;

    if (!rgb_runtime_layer_stage_profile(profile_frame, &profile)) {
        return false;
    }

    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        if (!rgb_runtime_layer_stage_selection_contains(state, selected_layer, layer)) {
            continue;
        }
        if (!rgb_runtime_layer_stage_has_solid_color(&profile, layer)) {
            continue;
        }

        painted |= rgb_runtime_frame_paint_layer(frame, &profile, layer, led_min, led_max);
    }

    uint8_t group_count = rgb_effective_config_layer_group_count(profile_frame);
    for (uint8_t group = 0; group < group_count; group++) {
#    ifdef RGB_LAYER_STAGE_TEST_BACKEND
        layer_group_scan_count++;
#    endif
        rgb_effective_layer_group_t group_config;

        if (!rgb_effective_config_layer_group_at(profile_frame, group, &group_config)) {
            rgb_runtime_frame_clear(frame, led_min, led_max);
            return false;
        }

        if (group_config.selector == RGB_LAYER_GROUP_ALL) {
            for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
                if (rgb_runtime_layer_stage_selection_contains(state, selected_layer, layer)) {
                    painted |= rgb_runtime_layer_stage_paint_group_for_layer(frame, &profile, &group_config, layer, led_min, led_max);
                }
            }
            continue;
        }

        if (group_config.selector < LAYER_COUNT && rgb_runtime_layer_stage_selection_contains(state, selected_layer, group_config.selector)) {
            painted |= rgb_runtime_layer_stage_paint_group_for_layer(frame, &profile, &group_config, group_config.selector, led_min, led_max);
        }
    }

    return painted;
}

bool rgb_runtime_layer_stage_render_frame(rgb_runtime_frame_t *frame, layer_state_t state, uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_layer_stage_render_effective_frame(frame, state, NULL, led_min, led_max);
}

bool rgb_runtime_layer_stage_render_effective_frame(rgb_runtime_frame_t *frame, layer_state_t state, const noah_effective_rgb_frame_t *profile_frame, uint8_t led_min, uint8_t led_max) {
    bool painted;

    if (!frame) {
        return false;
    }

    rgb_runtime_frame_clear(frame, led_min, led_max);
    painted = rgb_runtime_layer_stage_render_selection(frame, state, RGB_LAYER_GROUP_ALL, profile_frame, led_min, led_max);
    if (!rgb_effective_config_frame_current(profile_frame)) {
        rgb_runtime_frame_clear(frame, led_min, led_max);
        return false;
    }
    return painted;
}

bool rgb_runtime_layer_stage_render_selected_frame(rgb_runtime_frame_t *frame, uint8_t layer, uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_layer_stage_render_effective_selected_frame(frame, layer, NULL, led_min, led_max);
}

bool rgb_runtime_layer_stage_render_effective_selected_frame(rgb_runtime_frame_t *frame, uint8_t layer, const noah_effective_rgb_frame_t *profile_frame, uint8_t led_min, uint8_t led_max) {
    bool painted;

    if (!frame || layer >= LAYER_COUNT) {
        return false;
    }

    rgb_runtime_frame_clear(frame, led_min, led_max);
    painted = rgb_runtime_layer_stage_render_selection(frame, 0, layer, profile_frame, led_min, led_max);
    if (!rgb_effective_config_frame_current(profile_frame)) {
        rgb_runtime_frame_clear(frame, led_min, led_max);
        return false;
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

#    ifdef RGB_LAYER_STAGE_TEST_BACKEND
void rgb_runtime_layer_stage_test_reset_group_scan_count(void) {
    layer_group_scan_count = 0;
}

uint32_t rgb_runtime_layer_stage_test_group_scan_count(void) {
    return layer_group_scan_count;
}
#    endif

#endif
