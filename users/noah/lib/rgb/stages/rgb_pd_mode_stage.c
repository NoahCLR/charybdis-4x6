// ────────────────────────────────────────────────────────────────────────────
// RGB PD-Mode Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_pd_mode_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)

#    include "../../key/runtime/slot/origin_registry.h"

static bool rgb_runtime_pd_mode_stage_led_range_intersects(uint8_t led_min, uint8_t led_max, uint8_t from, uint8_t to) {
    return led_min < to && led_max > from;
}

static void rgb_runtime_pd_mode_stage_clear_frame(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    if (!frame) {
        return;
    }
    for (uint8_t led = led_min; led < led_max; led++) {
        frame->colors[led]  = (rgb_t){0};
        frame->painted[led] = false;
    }
}

static bool rgb_runtime_pd_mode_stage_paint_range(rgb_runtime_frame_t *frame, rgb_t color, uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!frame || !rgb_runtime_pd_mode_stage_led_range_intersects(led_min, led_max, from, to)) {
        return false;
    }
    if (from < led_min) {
        from = led_min;
    }
    if (to > led_max) {
        to = led_max;
    }
    for (uint8_t led = from; led < to; led++) {
        frame->colors[led]  = color;
        frame->painted[led] = true;
        painted             = true;
    }
    return painted;
}

static bool rgb_runtime_pd_mode_stage_paint_key(rgb_runtime_frame_t *frame, rgb_t color, keypos_t key_pos, uint8_t led_min, uint8_t led_max) {
    uint8_t leds[RGB_MATRIX_LED_COUNT];
    uint8_t led_count = rgb_matrix_map_row_column_to_led(key_pos.row, key_pos.col, leds);
    bool    painted   = false;

    for (uint8_t index = 0; index < led_count; index++) {
        uint8_t led = leds[index];

        if (frame && led >= led_min && led < led_max) {
            frame->colors[led]  = color;
            frame->painted[led] = true;
            painted             = true;
        }
    }

    return painted;
}

static bool rgb_runtime_pd_mode_stage_paint_owner_keys(rgb_runtime_frame_t *frame, rgb_t color, const uint8_t *bitmap, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!bitmap) {
        return false;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};

            if (!key_origin_bitmap_has_keypos(bitmap, key_pos)) {
                continue;
            }

            painted |= rgb_runtime_pd_mode_stage_paint_key(frame, color, key_pos, led_min, led_max);
        }
    }

    return painted;
}

void rgb_runtime_pd_mode_stage_post_init(void) {
    // Retained as a pipeline hook; effective colors are resolved per frame.
}

static split_side_mask_t rgb_runtime_pd_mode_stage_resolve_trigger_sides(pd_mode_snapshot_t snapshot) {
    split_side_mask_t owner_sides = snapshot.display.owner_sides;

    if (owner_sides != SPLIT_SIDE_MASK_NONE) {
        return owner_sides;
    }

    return SPLIT_SIDE_MASK_BOTH;
}

static bool rgb_runtime_pd_mode_stage_paint_locality(rgb_runtime_frame_t *frame, rgb_t color, rgb_locality_t locality, pd_mode_snapshot_t snapshot, const uint8_t *owner_bitmap, bool has_owner_keys, uint8_t led_min, uint8_t led_max) {
    split_side_mask_t sides = SPLIT_SIDE_MASK_NONE;

    switch (locality) {
        case RGB_LEFT_HALF:
            return rgb_runtime_pd_mode_stage_paint_range(frame, color, 0u, RGB_LEFT_LED_COUNT, led_min, led_max);
        case RGB_BOTH_HALVES:
            return rgb_runtime_pd_mode_stage_paint_range(frame, color, 0u, RGB_MATRIX_LED_COUNT, led_min, led_max);
        case RGB_KEY_HALF:
            sides = rgb_runtime_pd_mode_stage_resolve_trigger_sides(snapshot);
            break;
        case RGB_KEYS_ONLY:
            if (has_owner_keys) {
                return rgb_runtime_pd_mode_stage_paint_owner_keys(frame, color, owner_bitmap, led_min, led_max);
            }

            return rgb_runtime_pd_mode_stage_paint_range(frame, color, 0u, RGB_MATRIX_LED_COUNT, led_min, led_max);
        case RGB_RIGHT_HALF:
        default:
            sides = SPLIT_SIDE_MASK_RIGHT;
            break;
    }

    if (sides == SPLIT_SIDE_MASK_LEFT) {
        return rgb_runtime_pd_mode_stage_paint_range(frame, color, 0u, RGB_LEFT_LED_COUNT, led_min, led_max);
    }

    if (sides == SPLIT_SIDE_MASK_RIGHT) {
        return rgb_runtime_pd_mode_stage_paint_range(frame, color, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max);
    }

    return rgb_runtime_pd_mode_stage_paint_range(frame, color, 0u, RGB_MATRIX_LED_COUNT, led_min, led_max);
}

static bool rgb_runtime_pd_mode_stage_paint_group(rgb_runtime_frame_t *frame, const rgb_effective_pd_group_t *group, rgb_t color, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!(frame && group)) {
        return false;
    }
    for (uint8_t led = led_min; led < led_max; led++) {
        if (led >= NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT || (group->bitmap[led / 8u] & (uint8_t)(1u << (led % 8u))) == 0u) {
            continue;
        }
        frame->colors[led]  = color;
        frame->painted[led] = true;
        painted             = true;
    }
    return painted;
}

bool rgb_runtime_pd_mode_stage_render_frame(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_pd_mode_stage_render_effective_frame(frame, NULL, led_min, led_max);
}

bool rgb_runtime_pd_mode_stage_render_effective_frame(rgb_runtime_frame_t *frame, const noah_effective_rgb_frame_t *profile_frame, uint8_t led_min, uint8_t led_max) {
    // Mirrored identity and owner keys are rendered together, so they come from
    // one published generation rather than two independent reads.
    uint8_t                  owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    bool                     has_owner_keys = false;
    pd_mode_snapshot_t       snapshot;
    bool                     painted = false;
    pd_mode_id_t             active_mode;
    pd_mode_color_t          active_config;
    rgb_t                    active_rgb;
    rgb_effective_pd_group_t group_config;
    uint8_t                  group_count;

    if (!frame) {
        return false;
    }
    snapshot    = pd_mode_snapshot_with_owner_bitmap(owner_bitmap, &has_owner_keys);
    active_mode = snapshot.display.active_index;
    rgb_runtime_pd_mode_stage_clear_frame(frame, led_min, led_max);
    if (active_mode >= PD_MODE_COUNT || !rgb_effective_config_pd_stage_enabled(profile_frame) || !rgb_effective_config_pd_color(profile_frame, active_mode, &active_config)) {
        return false;
    }
    active_rgb = hsv_to_rgb(active_config.color);

    painted |= rgb_runtime_pd_mode_stage_paint_locality(frame, active_rgb, active_config.locality, snapshot, owner_bitmap, has_owner_keys, led_min, led_max);

    group_count = rgb_effective_config_pd_group_count(profile_frame);
    for (uint8_t group = 0u; group < group_count; group++) {
        if (!rgb_effective_config_pd_group_at(profile_frame, group, &group_config)) {
            rgb_runtime_pd_mode_stage_clear_frame(frame, led_min, led_max);
            return false;
        }
        if (group_config.selector != NOAH_PROFILE_RGB_V1_SELECTOR_ALL && group_config.selector != active_mode) {
            continue;
        }

        rgb_t group_rgb = rgb_hsv_is_inherit_color(group_config.color) ? active_rgb : hsv_to_rgb(group_config.color);
        painted |= rgb_runtime_pd_mode_stage_paint_group(frame, &group_config, group_rgb, led_min, led_max);
    }

    if (!rgb_effective_config_frame_current(profile_frame)) {
        rgb_runtime_pd_mode_stage_clear_frame(frame, led_min, led_max);
        return false;
    }
    return painted;
}

#endif
