// ────────────────────────────────────────────────────────────────────────────
// Effective RGB Configuration Adapter
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_effective_config.h"

#include <string.h>

extern const layer_color_config_t     layer_colors[];
extern const layer_led_group_t *const layer_led_groups;
extern const uint8_t                  layer_led_group_count;

// Narrow host/feature variants can compile RGB stages without the profile
// runtime. Production's source manifest supplies strong implementations.
__attribute__((weak)) noah_effective_rgb_result_t noah_effective_rgb_capture_frame(noah_effective_rgb_frame_t *frame) {
    if (!frame) {
        return NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
    }
    *frame       = (noah_effective_rgb_frame_t){0};
    frame->valid = true;
    return NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK;
}

__attribute__((weak)) noah_effective_rgb_result_t noah_effective_rgb_frame_status(const noah_effective_rgb_frame_t *frame) {
    return frame && frame->valid && !frame->live ? NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK : NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
}

static noah_effective_rgb_result_t frame_status(const noah_effective_rgb_frame_t *frame) {
    return frame ? noah_effective_rgb_frame_status(frame) : NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK;
}

static hsv_t native_hsv(noah_profile_rgb_v1_hsv_t color) {
    return (hsv_t){.h = color.h, .s = color.s, .v = color.v};
}

noah_effective_rgb_result_t rgb_effective_config_capture_frame(noah_effective_rgb_frame_t *frame) {
    return noah_effective_rgb_capture_frame(frame);
}

bool rgb_effective_config_frame_current(const noah_effective_rgb_frame_t *frame) {
    noah_effective_rgb_result_t status = frame_status(frame);

    return status == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK || status == NOAH_EFFECTIVE_RGB_OK;
}

bool rgb_effective_config_layer_stage_enabled(const noah_effective_rgb_frame_t *frame) {
    noah_effective_rgb_result_t status = frame_status(frame);

    if (status == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK) {
        return true;
    }
    return status == NOAH_EFFECTIVE_RGB_OK && (frame->view.stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u;
}

bool rgb_effective_config_layer_color(const noah_effective_rgb_frame_t *frame, uint8_t layer, layer_color_config_t *color) {
    noah_effective_rgb_result_t status = frame_status(frame);

    if (!color || layer >= LAYER_COUNT) {
        return false;
    }
    if (status == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK) {
        *color = layer_colors[layer];
        return true;
    }
    if (status == NOAH_EFFECTIVE_RGB_OK && (frame->view.stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u && layer < frame->view.layer_color_count) {
        noah_profile_rgb_v1_layer_color_t row;
        noah_profile_rgb_v1_error_t       error;

        if (noah_profile_rgb_v1_layer_color_at(&frame->view, layer, &row, &error) == NOAH_PROFILE_RGB_V1_OK && row.layer_id == layer) {
            *color = (layer_color_config_t){.color = native_hsv(row.color), .mode = row.mode};
            return true;
        }
    }
    return false;
}

uint8_t rgb_effective_config_layer_group_count(const noah_effective_rgb_frame_t *frame) {
    noah_effective_rgb_result_t status = frame_status(frame);

    if (status == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK) {
        return layer_led_group_count;
    }
    if (status == NOAH_EFFECTIVE_RGB_OK && (frame->view.stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u) {
        return frame->view.layer_group_count;
    }
    return 0u;
}

static bool compiled_layer_group_at(uint8_t index, rgb_effective_layer_group_t *group) {
    const layer_led_group_t *source;

    if (!group || index >= layer_led_group_count) {
        return false;
    }
    source = &layer_led_groups[index];
    memset(group, 0, sizeof(*group));
    group->selector = source->layer;
    group->color    = source->color;
    for (uint8_t led_index = 0u; led_index < source->led_group.count; led_index++) {
        uint8_t led = source->led_group.leds[led_index];

        if (led >= NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT) {
            return false;
        }
        group->bitmap[led / 8u] |= (uint8_t)(1u << (led % 8u));
    }
    return true;
}

bool rgb_effective_config_layer_group_at(const noah_effective_rgb_frame_t *frame, uint8_t index, rgb_effective_layer_group_t *group) {
    noah_effective_rgb_result_t status = frame_status(frame);

    if (!group) {
        return false;
    }
    if (status == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK) {
        return compiled_layer_group_at(index, group);
    }
    if (status == NOAH_EFFECTIVE_RGB_OK && (frame->view.stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u && index < frame->view.layer_group_count) {
        noah_profile_rgb_v1_group_row_t row;
        noah_profile_rgb_v1_group_t     dictionary_group;
        noah_profile_rgb_v1_error_t     error;

        if (noah_profile_rgb_v1_layer_group_at(&frame->view, index, &row, &error) != NOAH_PROFILE_RGB_V1_OK || row.group_id >= frame->view.group_count || noah_profile_rgb_v1_group_at(&frame->view, row.group_id, &dictionary_group, &error) != NOAH_PROFILE_RGB_V1_OK || dictionary_group.id != row.group_id) {
            return false;
        }
        group->selector = row.selector;
        group->color    = native_hsv(row.color);
        memcpy(group->bitmap, dictionary_group.bitmap, sizeof(group->bitmap));
        return true;
    }
    return false;
}
