// ────────────────────────────────────────────────────────────────────────────
// RGB Key Feedback Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_key_feedback_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)

#    include "../../key/runtime/feedback.h"

typedef struct {
    key_feedback_semantic_t semantic;
    uint8_t                 tap_branch;
} key_feedback_render_state_t;

#    if RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS <= 0
#        error "RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS must be greater than zero"
#    endif

void rgb_runtime_key_feedback_stage_post_init(void) {
    // Retained as a pipeline hook; effective colors are resolved per frame.
}

static uint8_t rgb_runtime_key_feedback_stage_tap_branch_color_index(const rgb_effective_key_feedback_t *config, uint8_t tap_branch) {
    uint8_t index;

    if (!config || config->tap_branch_color_count == 0u) {
        return 0u;
    }
    if (tap_branch <= 2u) {
        return 0u;
    }

    index = (uint8_t)(tap_branch - 2u);
    if (index >= config->tap_branch_color_count) {
        index = (uint8_t)(config->tap_branch_color_count - 1u);
    }
    return index;
}

static bool rgb_runtime_key_feedback_stage_semantic_color(const rgb_effective_key_feedback_t *config, key_feedback_semantic_t semantic, uint8_t tap_branch, rgb_t *out_color) {
    if (!(config && out_color)) {
        return false;
    }

    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING:
            if (config->tap_branch_color_count == 0u) {
                *out_color = (rgb_t){0};
                return false;
            }
            *out_color = hsv_to_rgb(config->tap_branch_colors[rgb_runtime_key_feedback_stage_tap_branch_color_index(config, tap_branch)]);
            return true;
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            *out_color = hsv_to_rgb(config->tap_committed_color);
            return true;
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            *out_color = hsv_to_rgb(config->hold_active_color);
            return true;
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            *out_color = hsv_to_rgb(config->long_hold_active_color);
            return true;
        case KEY_FEEDBACK_SEMANTIC_NONE:
        default:
            *out_color = (rgb_t){0};
            return false;
    }
}

static uint8_t rgb_runtime_key_feedback_stage_branch_for_key(const uint8_t *semantic_map, const uint8_t *tap_branch_map, keypos_t key_pos) {
    if (!(semantic_map && tap_branch_map)) {
        return 0u;
    }
    if (key_feedback_semantic_map_get(semantic_map, key_pos) != KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING) {
        return 0u;
    }
    return key_feedback_tap_branch_map_get(tap_branch_map, key_pos);
}

static bool rgb_runtime_key_feedback_stage_semantic_visible(key_feedback_semantic_t semantic, const uint8_t *flash_visibility_bitmap, keypos_t key_pos) {
    return !key_feedback_semantic_is_flashing(semantic) || key_origin_bitmap_has_keypos(flash_visibility_bitmap, key_pos);
}

static bool rgb_runtime_key_feedback_stage_group_semantic_matches(key_behavior_feedback_group_semantic_t group_semantic, key_feedback_semantic_t semantic) {
    switch (group_semantic) {
        case KEY_FEEDBACK_GROUP_TAP_BRANCH_PENDING:
            return semantic == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING;
        case KEY_FEEDBACK_GROUP_TAP_COMMITTED:
            return semantic == KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED;
        case KEY_FEEDBACK_GROUP_HOLD_ACTIVE:
            return semantic == KEY_FEEDBACK_SEMANTIC_HOLD_PENDING || semantic == KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING;
        case KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE:
            return semantic == KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY || semantic == KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING;
        default:
            return false;
    }
}

static bool rgb_runtime_key_feedback_stage_group_owner_slot(key_behavior_feedback_group_semantic_t group_semantic, key_feedback_broad_owner_slot_t *out_slot) {
    if (!out_slot) {
        return false;
    }

    switch (group_semantic) {
        case KEY_FEEDBACK_GROUP_TAP_BRANCH_PENDING:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_BRANCH_PENDING;
            return true;
        case KEY_FEEDBACK_GROUP_TAP_COMMITTED:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_COMMITTED;
            return true;
        case KEY_FEEDBACK_GROUP_HOLD_ACTIVE:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_HOLD_ACTIVE;
            return true;
        case KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_LONG_HOLD_ACTIVE;
            return true;
        default:
            return false;
    }
}

static bool rgb_runtime_key_feedback_stage_paint_range(rgb_runtime_frame_t *frame, rgb_t color, uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!frame || from >= led_max || to <= led_min) {
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

static bool rgb_runtime_key_feedback_stage_paint_key(rgb_runtime_frame_t *frame, rgb_t color, keypos_t key_pos, uint8_t led_min, uint8_t led_max) {
    uint8_t leds[RGB_MATRIX_LED_COUNT];
    uint8_t led_count = rgb_matrix_map_row_column_to_led(key_pos.row, key_pos.col, leds);
    bool    painted   = false;

    for (uint8_t index = 0u; index < led_count; index++) {
        uint8_t led = leds[index];

        if (frame && led >= led_min && led < led_max) {
            frame->colors[led]  = color;
            frame->painted[led] = true;
            painted             = true;
        }
    }
    return painted;
}

static bool rgb_runtime_key_feedback_stage_render_key_mode(const rgb_effective_key_feedback_t *config, rgb_runtime_frame_t *frame, const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!(config && frame && semantic_map && tap_branch_map && flash_visibility_bitmap)) {
        return false;
    }
    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
            keypos_t                key_pos  = {.row = row, .col = col};
            key_feedback_semantic_t semantic = key_feedback_semantic_map_get(semantic_map, key_pos);
            uint8_t                 tap_branch;
            rgb_t                   color;

            tap_branch = rgb_runtime_key_feedback_stage_branch_for_key(semantic_map, tap_branch_map, key_pos);
            if (!rgb_runtime_key_feedback_stage_semantic_color(config, semantic, tap_branch, &color) || !rgb_runtime_key_feedback_stage_semantic_visible(semantic, flash_visibility_bitmap, key_pos)) {
                continue;
            }
            painted |= rgb_runtime_key_feedback_stage_paint_key(frame, color, key_pos, led_min, led_max);
        }
    }
    return painted;
}

static key_feedback_render_state_t rgb_runtime_key_feedback_stage_state_for_owner(const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, key_feedback_broad_owner_slot_t slot) {
    key_feedback_render_state_t state = {.semantic = KEY_FEEDBACK_SEMANTIC_NONE, .tap_branch = 0u};
    keypos_t                    owner_key_pos;

    if (!(semantic_map && tap_branch_map && flash_visibility_bitmap && broad_owner_map)) {
        return state;
    }
    owner_key_pos = key_feedback_broad_owner_map_get(broad_owner_map, slot);
    if (!key_origin_keypos_valid(owner_key_pos)) {
        return state;
    }

    state.semantic   = key_feedback_semantic_map_get(semantic_map, owner_key_pos);
    state.tap_branch = rgb_runtime_key_feedback_stage_branch_for_key(semantic_map, tap_branch_map, owner_key_pos);
    if (!rgb_runtime_key_feedback_stage_semantic_visible(state.semantic, flash_visibility_bitmap, owner_key_pos)) {
        return (key_feedback_render_state_t){.semantic = KEY_FEEDBACK_SEMANTIC_NONE, .tap_branch = 0u};
    }
    return state;
}

static bool rgb_runtime_key_feedback_stage_paint_half(const rgb_effective_key_feedback_t *config, rgb_runtime_frame_t *frame, bool right_half, key_feedback_render_state_t state, uint8_t led_min, uint8_t led_max) {
    rgb_t color;

    if (!rgb_runtime_key_feedback_stage_semantic_color(config, state.semantic, state.tap_branch, &color)) {
        return false;
    }
    if (right_half) {
        return rgb_runtime_key_feedback_stage_paint_range(frame, color, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max);
    }
    return rgb_runtime_key_feedback_stage_paint_range(frame, color, 0u, RGB_LEFT_LED_COUNT, led_min, led_max);
}

static bool rgb_runtime_key_feedback_stage_render_locality(const rgb_effective_key_feedback_t *config, rgb_runtime_frame_t *frame, const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max) {
    key_feedback_render_state_t left_state;
    key_feedback_render_state_t right_state;
    key_feedback_render_state_t global_state;
    rgb_t                       color;

    if (config->locality == RGB_KEYS_ONLY) {
        return rgb_runtime_key_feedback_stage_render_key_mode(config, frame, semantic_map, tap_branch_map, flash_visibility_bitmap, led_min, led_max);
    }
    if (config->locality == RGB_KEY_HALF) {
        bool painted = false;

        left_state  = rgb_runtime_key_feedback_stage_state_for_owner(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF);
        right_state = rgb_runtime_key_feedback_stage_state_for_owner(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF);
        painted |= rgb_runtime_key_feedback_stage_paint_half(config, frame, false, left_state, led_min, led_max);
        painted |= rgb_runtime_key_feedback_stage_paint_half(config, frame, true, right_state, led_min, led_max);
        return painted;
    }

    global_state = rgb_runtime_key_feedback_stage_state_for_owner(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL);
    if (config->locality == RGB_LEFT_HALF) {
        return rgb_runtime_key_feedback_stage_paint_half(config, frame, false, global_state, led_min, led_max);
    }
    if (config->locality == RGB_RIGHT_HALF) {
        return rgb_runtime_key_feedback_stage_paint_half(config, frame, true, global_state, led_min, led_max);
    }
    if (!rgb_runtime_key_feedback_stage_semantic_color(config, global_state.semantic, global_state.tap_branch, &color)) {
        return false;
    }
    return rgb_runtime_key_feedback_stage_paint_range(frame, color, 0u, RGB_MATRIX_LED_COUNT, led_min, led_max);
}

static bool rgb_runtime_key_feedback_stage_paint_group(rgb_runtime_frame_t *frame, const rgb_effective_key_group_t *group, rgb_t color, uint8_t led_min, uint8_t led_max) {
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

static bool rgb_runtime_key_feedback_stage_render_group(const rgb_effective_key_feedback_t *config, rgb_runtime_frame_t *frame, const rgb_effective_key_group_t *group, const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max) {
    static const key_behavior_feedback_group_semantic_t all_semantics[] = {
        KEY_FEEDBACK_GROUP_TAP_BRANCH_PENDING,
        KEY_FEEDBACK_GROUP_TAP_COMMITTED,
        KEY_FEEDBACK_GROUP_HOLD_ACTIVE,
        KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE,
    };
    bool    painted        = false;
    uint8_t semantic_count = 1u;

    if (!(config && frame && group)) {
        return false;
    }
    if (group->selector == NOAH_PROFILE_RGB_V1_SELECTOR_ALL) {
        semantic_count = (uint8_t)ARRAY_SIZE(all_semantics);
    }

    for (uint8_t semantic_index = 0u; semantic_index < semantic_count; semantic_index++) {
        key_behavior_feedback_group_semantic_t group_semantic = group->selector == NOAH_PROFILE_RGB_V1_SELECTOR_ALL ? all_semantics[semantic_index] : (key_behavior_feedback_group_semantic_t)group->selector;
        key_feedback_broad_owner_slot_t        group_slot;
        key_feedback_render_state_t            group_state;
        rgb_t                                  group_rgb;

        if (!rgb_runtime_key_feedback_stage_group_owner_slot(group_semantic, &group_slot)) {
            continue;
        }
        group_state = rgb_runtime_key_feedback_stage_state_for_owner(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, group_slot);
        if (!rgb_runtime_key_feedback_stage_group_semantic_matches(group_semantic, group_state.semantic)) {
            continue;
        }
        if (rgb_hsv_is_inherit_color(group->color)) {
            if (!rgb_runtime_key_feedback_stage_semantic_color(config, group_state.semantic, group_state.tap_branch, &group_rgb)) {
                continue;
            }
        } else {
            group_rgb = hsv_to_rgb(group->color);
        }
        painted |= rgb_runtime_key_feedback_stage_paint_group(frame, group, group_rgb, led_min, led_max);
    }
    return painted;
}

static bool rgb_runtime_key_feedback_stage_render_groups(const rgb_effective_key_feedback_t *config, rgb_runtime_frame_t *frame, const noah_effective_rgb_frame_t *profile_frame, const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max) {
    bool    painted     = false;
    uint8_t group_count = rgb_effective_config_key_group_count(profile_frame);

    for (uint8_t pass = 0u; pass < 2u; pass++) {
        bool all_base_pass = pass == 0u;

        for (uint8_t group_index = 0u; group_index < group_count; group_index++) {
            rgb_effective_key_group_t group;

            if (!rgb_effective_config_key_group_at(profile_frame, group_index, &group)) {
                rgb_runtime_frame_clear(frame, led_min, led_max);
                return false;
            }
            if ((group.selector == NOAH_PROFILE_RGB_V1_SELECTOR_ALL) != all_base_pass) {
                continue;
            }
            painted |= rgb_runtime_key_feedback_stage_render_group(config, frame, &group, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, led_min, led_max);
        }
    }
    return painted;
}

bool rgb_runtime_key_feedback_stage_render_effective_frame(rgb_runtime_frame_t *frame, const noah_effective_rgb_frame_t *profile_frame, const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max) {
    rgb_effective_key_feedback_t config;
    bool                         painted = false;

    if (!frame) {
        return false;
    }
    rgb_runtime_frame_clear(frame, led_min, led_max);
    if (!(semantic_map && tap_branch_map && flash_visibility_bitmap && broad_owner_map) || !rgb_effective_config_key_stage_enabled(profile_frame) || !rgb_effective_config_key_feedback(profile_frame, &config)) {
        return false;
    }

    painted |= rgb_runtime_key_feedback_stage_render_locality(&config, frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, led_min, led_max);
    painted |= rgb_runtime_key_feedback_stage_render_groups(&config, frame, profile_frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, led_min, led_max);
    if (!rgb_effective_config_frame_current(profile_frame)) {
        rgb_runtime_frame_clear(frame, led_min, led_max);
        return false;
    }
    return painted;
}

#endif
