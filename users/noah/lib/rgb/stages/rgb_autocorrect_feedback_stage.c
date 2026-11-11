// ────────────────────────────────────────────────────────────────────────────
// RGB Replacement Feedback Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_autocorrect_feedback_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_AUTOCORRECT_FEEDBACK_ENABLE)

#    if RGB_AUTOCORRECT_FEEDBACK_FLASH_MS <= 0
#        error "RGB_AUTOCORRECT_FEEDBACK_FLASH_MS must be greater than zero"
#    endif

_Static_assert((int)NOAH_AUTOCORRECT_FEEDBACK_TEXT == (int)NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_TEXT + 1 && (int)NOAH_AUTOCORRECT_FEEDBACK_MACRO == (int)NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_MACRO + 1 && (int)NOAH_AUTOCORRECT_FEEDBACK_UNDO == (int)NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_UNDO + 1, "a feedback kind is its lighting semantic plus one");

// A semantic's colour of saturation and value 0 paints nothing, so each of
// the three can be off by itself (D-F70).
static bool rgb_runtime_autocorrect_feedback_stage_color_paints(hsv_t color) {
    return !(color.s == 0u && color.v == 0u);
}

static bool rgb_runtime_autocorrect_feedback_stage_paint_range(rgb_runtime_frame_t *frame, rgb_t color, uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

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

static bool rgb_runtime_autocorrect_feedback_stage_paint_keys(rgb_runtime_frame_t *frame, rgb_t color, const uint8_t *keys, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
            uint8_t leds[RGB_MATRIX_LED_COUNT];
            uint8_t led_count;

            if (!key_origin_bitmap_has_keypos(keys, (keypos_t){.row = row, .col = col})) {
                continue;
            }
            led_count = rgb_matrix_map_row_column_to_led(row, col, leds);
            for (uint8_t index = 0u; index < led_count; index++) {
                uint8_t led = leds[index];

                if (led >= led_min && led < led_max) {
                    frame->colors[led]  = color;
                    frame->painted[led] = true;
                    painted             = true;
                }
            }
        }
    }
    return painted;
}

// A flash whose key is not known paints as a pointing mode with no owner key
// does: both halves under RGB_KEY_HALF, every key under RGB_KEYS_ONLY.
static bool rgb_runtime_autocorrect_feedback_stage_paint_locality(rgb_runtime_frame_t *frame, rgb_t color, rgb_locality_t locality, const uint8_t *keys, uint8_t led_min, uint8_t led_max) {
    split_side_mask_t sides = SPLIT_SIDE_MASK_BOTH;

    switch (locality) {
        case RGB_LEFT_HALF:
            sides = SPLIT_SIDE_MASK_LEFT;
            break;
        case RGB_RIGHT_HALF:
            sides = SPLIT_SIDE_MASK_RIGHT;
            break;
        case RGB_KEY_HALF:
            sides = key_origin_bitmap_side_mask(keys);
            break;
        case RGB_KEYS_ONLY:
            if (key_origin_bitmap_has_any(keys)) {
                return rgb_runtime_autocorrect_feedback_stage_paint_keys(frame, color, keys, led_min, led_max);
            }
            break;
        case RGB_BOTH_HALVES:
        default:
            break;
    }

    if (sides == SPLIT_SIDE_MASK_LEFT) {
        return rgb_runtime_autocorrect_feedback_stage_paint_range(frame, color, 0u, RGB_LEFT_LED_COUNT, led_min, led_max);
    }
    if (sides == SPLIT_SIDE_MASK_RIGHT) {
        return rgb_runtime_autocorrect_feedback_stage_paint_range(frame, color, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max);
    }
    return rgb_runtime_autocorrect_feedback_stage_paint_range(frame, color, 0u, RGB_MATRIX_LED_COUNT, led_min, led_max);
}

static bool rgb_runtime_autocorrect_feedback_stage_paint_group(rgb_runtime_frame_t *frame, const rgb_active_profile_autocorrect_group_t *group, rgb_t color, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

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

bool rgb_runtime_autocorrect_feedback_stage_render_active_profile_frame(rgb_runtime_frame_t *frame, const noah_active_profile_rgb_frame_t *profile_frame, const noah_autocorrect_feedback_t *feedback, uint8_t led_min, uint8_t led_max) {
    rgb_active_profile_autocorrect_feedback_t config;
    rgb_active_profile_group_selectors_t      selectors;
    uint8_t                                   semantic;
    hsv_t                                     stage_color;
    bool                                      painted = false;

    if (!frame) {
        return false;
    }
    rgb_runtime_frame_clear(frame, led_min, led_max);
    // The slave half's kind came over the link: one this build does not know
    // paints nothing.
    if (!feedback || feedback->kind == NOAH_AUTOCORRECT_FEEDBACK_NONE || feedback->kind > NOAH_AUTOCORRECT_FEEDBACK_UNDO || !rgb_active_profile_config_autocorrect_stage_enabled(profile_frame) || !rgb_active_profile_config_autocorrect_feedback(profile_frame, &config)) {
        return false;
    }

    semantic    = (uint8_t)(feedback->kind - 1u);
    stage_color = config.colors[semantic];
    if (rgb_runtime_autocorrect_feedback_stage_color_paints(stage_color)) {
        painted |= rgb_runtime_autocorrect_feedback_stage_paint_locality(frame, hsv_to_rgb(stage_color), config.locality, feedback->keys, led_min, led_max);
    }

    if (!rgb_active_profile_config_group_selectors(profile_frame, &selectors)) {
        rgb_runtime_frame_clear(frame, led_min, led_max);
        return false;
    }
    // All-semantics rows paint first, then the rows naming this one. Only the
    // rows that paint are read (D-F63).
    for (uint8_t pass = 0u; pass < 2u; pass++) {
        uint8_t wanted = pass == 0u ? NOAH_PROFILE_RGB_V1_SELECTOR_ALL : semantic;

        for (uint8_t group_index = 0u; group_index < selectors.autocorrect_count; group_index++) {
            rgb_active_profile_autocorrect_group_t group;

            if (selectors.autocorrect[group_index] != wanted) {
                continue;
            }
            if (!rgb_active_profile_config_autocorrect_group_at(profile_frame, group_index, &group)) {
                rgb_runtime_frame_clear(frame, led_min, led_max);
                return false;
            }
            if (rgb_hsv_is_inherit_color(group.color)) {
                if (!rgb_runtime_autocorrect_feedback_stage_color_paints(stage_color)) {
                    continue;
                }
                painted |= rgb_runtime_autocorrect_feedback_stage_paint_group(frame, &group, hsv_to_rgb(stage_color), led_min, led_max);
            } else {
                painted |= rgb_runtime_autocorrect_feedback_stage_paint_group(frame, &group, hsv_to_rgb(group.color), led_min, led_max);
            }
        }
    }
    if (!rgb_active_profile_config_frame_current(profile_frame)) {
        rgb_runtime_frame_clear(frame, led_min, led_max);
        return false;
    }
    return painted;
}

#endif
