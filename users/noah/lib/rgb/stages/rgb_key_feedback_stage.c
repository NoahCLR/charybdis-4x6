// ────────────────────────────────────────────────────────────────────────────
// RGB Key Feedback Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_key_feedback_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)

#    include <string.h>

#    include "../core/rgb_helpers.h"
#    include "../../key/runtime/feedback.h"
#    include "../../split/runtime_sync.h"

extern const key_behavior_feedback_color_config_t     key_behavior_feedback_colors;
extern const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups;
extern const uint8_t                                  key_behavior_feedback_led_group_count;

static rgb_t   key_behavior_feedback_tap_pending_rgb;
static rgb_t   key_behavior_feedback_tap_branch_rgb[KEY_BEHAVIOR_MAX_TAP_COUNT];
static uint8_t key_behavior_feedback_tap_branch_rgb_count;
static rgb_t   key_behavior_feedback_tap_committed_rgb;
static rgb_t   key_behavior_feedback_hold_active_rgb;
static rgb_t   key_behavior_feedback_long_hold_active_rgb;

typedef struct {
    key_feedback_semantic_t semantic;
    uint8_t                 tap_branch;
} key_feedback_render_state_t;

#    if RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS <= 0
#        error "RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS must be greater than zero"
#    endif

void rgb_runtime_key_feedback_stage_post_init(void) {
    key_behavior_feedback_tap_pending_rgb      = hsv_to_rgb(key_behavior_feedback_colors.tap_pending_color);
    key_behavior_feedback_tap_branch_rgb_count = key_behavior_feedback_colors.tap_branch_colors ? key_behavior_feedback_colors.tap_branch_color_count : 0u;
    if (key_behavior_feedback_tap_branch_rgb_count > KEY_BEHAVIOR_MAX_TAP_COUNT) {
        key_behavior_feedback_tap_branch_rgb_count = KEY_BEHAVIOR_MAX_TAP_COUNT;
    }

    for (uint8_t index = 0; index < key_behavior_feedback_tap_branch_rgb_count; index++) {
        key_behavior_feedback_tap_branch_rgb[index] = hsv_to_rgb(key_behavior_feedback_colors.tap_branch_colors[index]);
    }

    key_behavior_feedback_tap_committed_rgb    = hsv_to_rgb(key_behavior_feedback_colors.tap_committed_color);
    key_behavior_feedback_hold_active_rgb      = hsv_to_rgb(key_behavior_feedback_colors.hold_active_color);
    key_behavior_feedback_long_hold_active_rgb = hsv_to_rgb(key_behavior_feedback_colors.long_hold_active_color);
}

static bool rgb_runtime_key_feedback_stage_led_range_intersects(uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max) {
    return from < led_max && to > led_min;
}

static bool rgb_runtime_key_feedback_stage_led_group_intersects(const uint8_t *leds, uint8_t count, uint8_t led_min, uint8_t led_max) {
    for (uint8_t i = 0; i < count; i++) {
        if (leds[i] >= led_min && leds[i] < led_max) {
            return true;
        }
    }

    return false;
}

static void rgb_runtime_key_feedback_stage_current_semantic_map(uint8_t *out_map) {
    if (!out_map) {
        return;
    }

    if (is_keyboard_master()) {
        key_feedback_semantic_map(out_map);
        return;
    }

    memcpy(out_map, split_runtime_sync_remote.key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);
}

static void rgb_runtime_key_feedback_stage_current_tap_branch_map(uint8_t *out_map) {
    if (!out_map) {
        return;
    }

    if (is_keyboard_master()) {
        key_feedback_tap_branch_map(out_map);
        return;
    }

    memcpy(out_map, split_runtime_sync_remote.key_feedback_tap_branch_map, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE);
}

static void rgb_runtime_key_feedback_stage_current_broad_owner_map(uint8_t *out_map) {
    if (!out_map) {
        return;
    }

    if (is_keyboard_master()) {
        key_feedback_broad_owner_map(out_map);
        return;
    }

    memcpy(out_map, split_runtime_sync_remote.key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE);
}

static void rgb_runtime_key_feedback_stage_current_combo_bitmap(uint8_t *out_bitmap) {
    uint8_t overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];

    if (!out_bitmap) {
        return;
    }

    if (is_keyboard_master()) {
        combo_feedback_underlay_bitmap(out_bitmap);
        combo_feedback_overlay_bitmap(overlay_bitmap);
    } else {
        key_origin_bitmap_copy(out_bitmap, split_runtime_sync_remote.combo_underlay_bitmap);
        key_origin_bitmap_copy(overlay_bitmap, split_runtime_sync_remote.combo_overlay_bitmap);
    }

    key_origin_bitmap_or_inplace(out_bitmap, overlay_bitmap);
}

static void rgb_runtime_key_feedback_stage_clear_semantic_for_bitmap(uint8_t *semantic_map, const uint8_t *bitmap, key_feedback_semantic_t semantic) {
    if (!(semantic_map && bitmap && semantic != KEY_FEEDBACK_SEMANTIC_NONE)) {
        return;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};

            if (key_origin_bitmap_has_keypos(bitmap, key_pos) && key_feedback_semantic_map_get(semantic_map, key_pos) == semantic) {
                key_feedback_semantic_map_set(semantic_map, key_pos, KEY_FEEDBACK_SEMANTIC_NONE);
            }
        }
    }
}

static void rgb_runtime_key_feedback_stage_suppress_combo_unresolved_semantics(uint8_t *semantic_map) {
    uint8_t combo_bitmap[KEY_ORIGIN_BITMAP_SIZE];

    if (!semantic_map) {
        return;
    }

    rgb_runtime_key_feedback_stage_current_combo_bitmap(combo_bitmap);
    rgb_runtime_key_feedback_stage_clear_semantic_for_bitmap(semantic_map, combo_bitmap, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
}

static void rgb_runtime_key_feedback_stage_current_flash_visibility_bitmap(uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return;
    }

    if (is_keyboard_master()) {
        key_feedback_flash_visibility_bitmap(out_bitmap);
        return;
    }

    key_origin_bitmap_copy(out_bitmap, split_runtime_sync_remote.key_feedback_flash_visibility_bitmap);
}

static uint8_t rgb_runtime_key_feedback_stage_tap_branch_color_index(uint8_t tap_branch) {
    uint8_t index;

    if (key_behavior_feedback_tap_branch_rgb_count == 0u) {
        return 0u;
    }

    if (tap_branch <= 1u) {
        return 0u;
    }

    index = (uint8_t)(tap_branch - 1u);
    if (index >= key_behavior_feedback_tap_branch_rgb_count) {
        index = (uint8_t)(key_behavior_feedback_tap_branch_rgb_count - 1u);
    }

    return index;
}

static bool rgb_runtime_key_feedback_stage_semantic_color(key_feedback_semantic_t semantic, uint8_t tap_branch, rgb_t *out_color) {
    if (!out_color) {
        return false;
    }

    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH:
            *out_color = key_behavior_feedback_tap_pending_rgb;
            return true;
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED:
            if (key_behavior_feedback_tap_branch_rgb_count == 0u) {
                *out_color = (rgb_t){0};
                return false;
            }
            *out_color = key_behavior_feedback_tap_branch_rgb[rgb_runtime_key_feedback_stage_tap_branch_color_index(tap_branch)];
            return true;
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            *out_color = key_behavior_feedback_tap_committed_rgb;
            return true;
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            *out_color = key_behavior_feedback_hold_active_rgb;
            return true;
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            *out_color = key_behavior_feedback_long_hold_active_rgb;
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

    if (key_feedback_semantic_map_get(semantic_map, key_pos) != KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED) {
        return 0u;
    }

    return key_feedback_tap_branch_map_get(tap_branch_map, key_pos);
}

static bool rgb_runtime_key_feedback_stage_semantic_visible(key_feedback_semantic_t semantic, const uint8_t *flash_visibility_bitmap, keypos_t key_pos) {
    return !key_feedback_semantic_is_flashing(semantic) || key_origin_bitmap_has_keypos(flash_visibility_bitmap, key_pos);
}

static bool rgb_runtime_key_feedback_stage_group_semantic_matches(key_behavior_feedback_group_semantic_t group_semantic, key_feedback_semantic_t semantic) {
    switch (group_semantic) {
        case KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH:
            return semantic == KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH;
        case KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED:
            return semantic == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED;
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
        case KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_UNRESOLVED_TAP_BRANCH;
            return true;
        case KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_BRANCH_COMMITTED;
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

static bool rgb_runtime_key_feedback_stage_paint_key(rgb_t color, keypos_t key_pos, uint8_t led_min, uint8_t led_max) {
    uint8_t leds[RGB_MATRIX_LED_COUNT];
    uint8_t led_count = rgb_matrix_map_row_column_to_led(key_pos.row, key_pos.col, leds);
    bool    painted   = false;

    for (uint8_t i = 0; i < led_count; i++) {
        uint8_t led = leds[i];

        if (led >= led_min && led < led_max) {
            rgb_set_led_color(led, led_min, led_max, color);
            painted = true;
        }
    }

    return painted;
}

static bool rgb_runtime_key_feedback_stage_render_key_mode(const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!(semantic_map && tap_branch_map && flash_visibility_bitmap)) {
        return false;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t                key_pos  = {.row = row, .col = col};
            key_feedback_semantic_t semantic = key_feedback_semantic_map_get(semantic_map, key_pos);
            uint8_t                 tap_branch;
            rgb_t                   color;

            tap_branch = rgb_runtime_key_feedback_stage_branch_for_key(semantic_map, tap_branch_map, key_pos);
            if (!rgb_runtime_key_feedback_stage_semantic_color(semantic, tap_branch, &color) || !rgb_runtime_key_feedback_stage_semantic_visible(semantic, flash_visibility_bitmap, key_pos)) {
                continue;
            }

            painted |= rgb_runtime_key_feedback_stage_paint_key(color, key_pos, led_min, led_max);
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

static bool rgb_runtime_key_feedback_stage_paint_half(bool right_half, key_feedback_render_state_t state, uint8_t led_min, uint8_t led_max) {
    rgb_t color;

    if (!rgb_runtime_key_feedback_stage_semantic_color(state.semantic, state.tap_branch, &color)) {
        return false;
    }

    if (right_half) {
        if (!rgb_runtime_key_feedback_stage_led_range_intersects(RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max)) {
            return false;
        }

        rgb_set_right_half(color, led_min, led_max);
        return true;
    }

    if (!rgb_runtime_key_feedback_stage_led_range_intersects(0, RGB_LEFT_LED_COUNT, led_min, led_max)) {
        return false;
    }

    rgb_set_left_half(color, led_min, led_max);
    return true;
}

static bool rgb_runtime_key_feedback_stage_render_locality(const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max) {
    key_feedback_render_state_t left_state;
    key_feedback_render_state_t right_state;
    key_feedback_render_state_t global_state;
    rgb_t                       color;

    if (key_behavior_feedback_colors.locality == RGB_KEYS_ONLY) {
        return rgb_runtime_key_feedback_stage_render_key_mode(semantic_map, tap_branch_map, flash_visibility_bitmap, led_min, led_max);
    }

    if (key_behavior_feedback_colors.locality == RGB_KEY_HALF) {
        bool painted = false;

        left_state  = rgb_runtime_key_feedback_stage_state_for_owner(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF);
        right_state = rgb_runtime_key_feedback_stage_state_for_owner(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF);
        painted |= rgb_runtime_key_feedback_stage_paint_half(false, left_state, led_min, led_max);
        painted |= rgb_runtime_key_feedback_stage_paint_half(true, right_state, led_min, led_max);
        return painted;
    }

    global_state = rgb_runtime_key_feedback_stage_state_for_owner(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL);
    if (key_behavior_feedback_colors.locality == RGB_LEFT_HALF) {
        return rgb_runtime_key_feedback_stage_paint_half(false, global_state, led_min, led_max);
    }

    if (key_behavior_feedback_colors.locality == RGB_RIGHT_HALF) {
        return rgb_runtime_key_feedback_stage_paint_half(true, global_state, led_min, led_max);
    }

    if (!rgb_runtime_key_feedback_stage_semantic_color(global_state.semantic, global_state.tap_branch, &color)) {
        return false;
    }

    rgb_set_both_halves(color, led_min, led_max);
    return led_min < led_max;
}

static bool rgb_runtime_key_feedback_stage_render_groups(const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    for (uint8_t group_index = 0; group_index < key_behavior_feedback_led_group_count; group_index++) {
        const key_behavior_feedback_led_group_t     *group           = &key_behavior_feedback_led_groups[group_index];
        const key_behavior_feedback_group_semantic_t all_semantics[] = {
            KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH, KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED, KEY_FEEDBACK_GROUP_TAP_COMMITTED, KEY_FEEDBACK_GROUP_HOLD_ACTIVE, KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE,
        };
        uint8_t semantic_count = 1u;

        if (group->semantic == KEY_FEEDBACK_GROUP_ALL) {
            semantic_count = (uint8_t)ARRAY_SIZE(all_semantics);
        }

        for (uint8_t semantic_index = 0; semantic_index < semantic_count; semantic_index++) {
            key_behavior_feedback_group_semantic_t group_semantic = group->semantic == KEY_FEEDBACK_GROUP_ALL ? all_semantics[semantic_index] : group->semantic;
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

            if (group->semantic == KEY_FEEDBACK_GROUP_ALL) {
                if (!rgb_runtime_key_feedback_stage_semantic_color(group_state.semantic, group_state.tap_branch, &group_rgb)) {
                    continue;
                }
            } else {
                group_rgb = hsv_to_rgb(group->color);
            }

            const rgb_led_group_t *led_group = &group->led_group;
            rgb_set_led_group(led_group->leds, led_group->count, led_min, led_max, group_rgb);
            painted |= rgb_runtime_key_feedback_stage_led_group_intersects(led_group->leds, led_group->count, led_min, led_max);
        }
    }

    return painted;
}

static bool rgb_runtime_key_feedback_stage_render_impl(uint8_t led_min, uint8_t led_max) {
    uint8_t semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    uint8_t flash_visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t broad_owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
    bool    painted = false;

    rgb_runtime_key_feedback_stage_current_semantic_map(semantic_map);
    rgb_runtime_key_feedback_stage_current_tap_branch_map(tap_branch_map);
    rgb_runtime_key_feedback_stage_current_flash_visibility_bitmap(flash_visibility_bitmap);
    rgb_runtime_key_feedback_stage_current_broad_owner_map(broad_owner_map);
    rgb_runtime_key_feedback_stage_suppress_combo_unresolved_semantics(semantic_map);

    painted |= rgb_runtime_key_feedback_stage_render_locality(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, led_min, led_max);
    painted |= rgb_runtime_key_feedback_stage_render_groups(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, led_min, led_max);

    return painted;
}

bool rgb_runtime_key_feedback_stage_render(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_key_feedback_stage_render_impl(led_min, led_max);
}

#endif
