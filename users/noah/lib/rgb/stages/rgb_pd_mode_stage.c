// ────────────────────────────────────────────────────────────────────────────
// RGB PD-Mode Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_pd_mode_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)

#    include "../core/rgb_helpers.h"
#    include "../../key/runtime/origin_registry.h"
#    include "../../pointing/defs/pd_modes.h"

extern const pd_mode_color_t            pd_mode_colors[];
extern const uint8_t                    pd_mode_color_count;
extern const pd_mode_led_group_t *const pd_mode_led_groups;
extern const uint8_t                    pd_mode_led_group_count;

static rgb_t          pd_mode_rgb[PD_MODE_COUNT];
static rgb_locality_t pd_mode_render_locality[PD_MODE_COUNT];

static bool rgb_runtime_pd_mode_stage_led_range_intersects(uint8_t led_min, uint8_t led_max, uint8_t from, uint8_t to) {
    return led_min < to && led_max > from;
}

static bool rgb_runtime_pd_mode_stage_led_group_intersects(const uint8_t *leds, uint8_t count, uint8_t led_min, uint8_t led_max) {
    for (uint8_t i = 0; i < count; i++) {
        if (leds[i] >= led_min && leds[i] < led_max) {
            return true;
        }
    }

    return false;
}

static bool rgb_runtime_pd_mode_stage_paint_key(rgb_t color, keypos_t key_pos, uint8_t led_min, uint8_t led_max) {
    uint8_t leds[RGB_MATRIX_LED_COUNT];
    uint8_t led_count = rgb_matrix_map_row_column_to_led(key_pos.row, key_pos.col, leds);
    bool    painted   = false;

    for (uint8_t index = 0; index < led_count; index++) {
        uint8_t led = leds[index];

        if (led >= led_min && led < led_max) {
            rgb_set_led_color(led, led_min, led_max, color);
            painted = true;
        }
    }

    return painted;
}

static bool rgb_runtime_pd_mode_stage_paint_owner_keys(rgb_t color, const uint8_t *bitmap, uint8_t led_min, uint8_t led_max) {
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

            painted |= rgb_runtime_pd_mode_stage_paint_key(color, key_pos, led_min, led_max);
        }
    }

    return painted;
}

void rgb_runtime_pd_mode_stage_post_init(void) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        pd_mode_rgb[i]             = (rgb_t){0};
        pd_mode_render_locality[i] = RGB_RIGHT_HALF;

        for (uint8_t c = 0; c < pd_mode_color_count; c++) {
            if (pd_mode_colors[c].pointing_mode == pd_modes[i].mode_flag) {
                pd_mode_rgb[i]             = hsv_to_rgb(pd_mode_colors[c].color);
                pd_mode_render_locality[i] = pd_mode_colors[c].locality;
                break;
            }
        }
    }
}

static split_side_mask_t rgb_runtime_pd_mode_stage_resolve_trigger_sides(pd_mode_snapshot_t snapshot) {
    split_side_mask_t owner_sides = snapshot.display.owner_sides;

    if (owner_sides != SPLIT_SIDE_MASK_NONE) {
        return owner_sides;
    }

    return SPLIT_SIDE_MASK_BOTH;
}

static bool rgb_runtime_pd_mode_stage_paint_locality(rgb_t color, rgb_locality_t locality, pd_mode_snapshot_t snapshot, uint8_t led_min, uint8_t led_max) {
    split_side_mask_t sides = SPLIT_SIDE_MASK_NONE;

    switch (locality) {
        case RGB_LEFT_HALF:
            if (!rgb_runtime_pd_mode_stage_led_range_intersects(led_min, led_max, 0, RGB_LEFT_LED_COUNT)) {
                return false;
            }
            rgb_set_left_half(color, led_min, led_max);
            return true;
        case RGB_BOTH_HALVES:
            rgb_set_both_halves(color, led_min, led_max);
            return led_min < led_max;
        case RGB_KEY_HALF:
            sides = rgb_runtime_pd_mode_stage_resolve_trigger_sides(snapshot);
            break;
        case RGB_KEYS_ONLY: {
            uint8_t owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];

            if (pd_mode_display_owner_bitmap_snapshot(owner_bitmap)) {
                return rgb_runtime_pd_mode_stage_paint_owner_keys(color, owner_bitmap, led_min, led_max);
            }

            rgb_set_both_halves(color, led_min, led_max);
            return led_min < led_max;
        }
        case RGB_RIGHT_HALF:
        default:
            sides = SPLIT_SIDE_MASK_RIGHT;
            break;
    }

    if (sides == SPLIT_SIDE_MASK_LEFT) {
        if (!rgb_runtime_pd_mode_stage_led_range_intersects(led_min, led_max, 0, RGB_LEFT_LED_COUNT)) {
            return false;
        }
        rgb_set_left_half(color, led_min, led_max);
        return true;
    }

    if (sides == SPLIT_SIDE_MASK_RIGHT) {
        if (!rgb_runtime_pd_mode_stage_led_range_intersects(led_min, led_max, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT)) {
            return false;
        }

        rgb_set_right_half(color, led_min, led_max);
        return true;
    }

    rgb_set_both_halves(color, led_min, led_max);
    return led_min < led_max;
}

bool rgb_runtime_pd_mode_stage_render(uint8_t led_min, uint8_t led_max) {
    pd_mode_snapshot_t snapshot    = pd_mode_snapshot();
    bool               painted     = false;
    uint8_t            active_mode = snapshot.display.active_index;

    if (active_mode < PD_MODE_COUNT) {
        painted |= rgb_runtime_pd_mode_stage_paint_locality(pd_mode_rgb[active_mode], pd_mode_render_locality[active_mode], snapshot, led_min, led_max);
    }

    for (uint8_t group = 0; group < pd_mode_led_group_count; group++) {
        if (snapshot.display.active_mode != pd_mode_led_groups[group].pointing_mode) {
            continue;
        }

        const rgb_led_group_t *led_group = &pd_mode_led_groups[group].led_group;
        rgb_t                  group_rgb = hsv_to_rgb(pd_mode_led_groups[group].color);
        rgb_set_led_group(led_group->leds, led_group->count, led_min, led_max, group_rgb);
        painted |= rgb_runtime_pd_mode_stage_led_group_intersects(led_group->leds, led_group->count, led_min, led_max);
    }

    return painted;
}

#endif
