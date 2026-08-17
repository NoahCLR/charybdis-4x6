// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_runtime.h"
#include "rgb_helpers.h"
#include "../automouse/rgb_automouse_stage.h"
#include "../stages/rgb_combo_feedback_stage.h"
#include "../stages/rgb_key_feedback_stage.h"
#include "../stages/rgb_layer_stage.h"
#include "../stages/rgb_pd_mode_stage.h"
#include "../stages/rgb_preview_stage.h"
#include "../../state/diagnostics/runtime_diag.h"
#include "rgb_validation.h"

#if defined(RGB_MATRIX_ENABLE) && (defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE) || (defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)))
#    include "../../key/runtime/feedback.h"
#    include "../../split/runtime_sync.h"
#endif

#ifdef RGB_MATRIX_ENABLE
static rgb_runtime_frame_t rgb_runtime_frame_primary;

#    ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
static uint16_t rgb_runtime_stage_pipeline_count;
static uint16_t rgb_runtime_early_exit_count;

uint16_t rgb_runtime_test_stage_pipeline_count(void) {
    return rgb_runtime_stage_pipeline_count;
}

uint16_t rgb_runtime_test_early_exit_count(void) {
    return rgb_runtime_early_exit_count;
}
#    endif

#    if defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE) || (defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE))
typedef struct {
    bool    combo_valid;
    uint8_t combo_underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t combo_overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
#        ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    bool    key_feedback_valid;
    uint8_t key_feedback_semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t key_feedback_tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    uint8_t key_feedback_flash_visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t key_feedback_broad_owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
#        endif
} rgb_runtime_render_snapshot_t;

// Keep this source cache small enough that render-work optimization cannot
// silently grow into another LED-frame-sized allocation.
_Static_assert(sizeof(rgb_runtime_render_snapshot_t) <= 96u, "RGB render source snapshot exceeds its 96-byte RAM budget");

static rgb_runtime_render_snapshot_t rgb_runtime_render_snapshot;

static void rgb_runtime_render_snapshot_invalidate(void) {
    rgb_runtime_render_snapshot.combo_valid = false;
#        ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    rgb_runtime_render_snapshot.key_feedback_valid = false;
#        endif
}

static void rgb_runtime_render_snapshot_ensure_combo(void) {
    if (rgb_runtime_render_snapshot.combo_valid) {
        return;
    }

    if (is_keyboard_master()) {
        combo_feedback_bitmaps(rgb_runtime_render_snapshot.combo_underlay_bitmap, rgb_runtime_render_snapshot.combo_overlay_bitmap);
    } else {
        // A false read means the split worker was still publishing this
        // domain; the cached bitmaps are untouched, so the frame renders the
        // last coherent snapshot instead of a mixture of two packets.
        (void)split_runtime_sync_remote_read_combo(rgb_runtime_render_snapshot.combo_underlay_bitmap, rgb_runtime_render_snapshot.combo_overlay_bitmap);
    }

    rgb_runtime_render_snapshot.combo_valid = true;
}

#        ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
// Split staleness only. The mirrored key-feedback packet can lag the combo
// packet, so a key the slave now paints as part of a combo footprint may still
// carry a tap-branch semantic from before. Dropping it here keeps a stale color
// from stomping the live footprint; this is not a judgement about whether a combo
// member should ever show its branch, which the engine map decides.
static void rgb_runtime_render_snapshot_suppress_combo_tap_branch_semantics(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};
            bool     combo_key;

            combo_key = key_origin_bitmap_has_keypos(rgb_runtime_render_snapshot.combo_underlay_bitmap, key_pos) || key_origin_bitmap_has_keypos(rgb_runtime_render_snapshot.combo_overlay_bitmap, key_pos);
            if (combo_key && key_feedback_semantic_map_get(rgb_runtime_render_snapshot.key_feedback_semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING) {
                key_feedback_semantic_map_set(rgb_runtime_render_snapshot.key_feedback_semantic_map, key_pos, KEY_FEEDBACK_SEMANTIC_NONE);
            }
        }
    }
}

static void rgb_runtime_render_snapshot_ensure_key_feedback(void) {
    if (rgb_runtime_render_snapshot.key_feedback_valid) {
        return;
    }

    rgb_runtime_render_snapshot_ensure_combo();
    if (is_keyboard_master()) {
        key_feedback_semantic_map(rgb_runtime_render_snapshot.key_feedback_semantic_map);
        key_feedback_tap_branch_map(rgb_runtime_render_snapshot.key_feedback_tap_branch_map);
        key_feedback_flash_visibility_bitmap_for_semantic_map(rgb_runtime_render_snapshot.key_feedback_semantic_map, rgb_runtime_render_snapshot.key_feedback_flash_visibility_bitmap);
        key_feedback_broad_owner_map(rgb_runtime_render_snapshot.key_feedback_broad_owner_map);
    } else {
        // Semantic and branch state travel as two RPC domains, so each one is
        // captured as its own coherent generation. A false read leaves that
        // domain's cached maps untouched and keeps the last coherent snapshot.
        (void)split_runtime_sync_remote_read_key_feedback_semantic(rgb_runtime_render_snapshot.key_feedback_flash_visibility_bitmap, rgb_runtime_render_snapshot.key_feedback_semantic_map);
        (void)split_runtime_sync_remote_read_key_feedback_branch(rgb_runtime_render_snapshot.key_feedback_broad_owner_map, rgb_runtime_render_snapshot.key_feedback_tap_branch_map);
    }

    rgb_runtime_render_snapshot_suppress_combo_tap_branch_semantics();
    rgb_runtime_render_snapshot.key_feedback_valid = true;
}
#        endif
#    else
static void rgb_runtime_render_snapshot_invalidate(void) {}
#    endif

static bool rgb_runtime_range_starts_frame(uint8_t led_min, uint8_t led_max) {
    struct rgb_matrix_limits_t first_limits = rgb_matrix_get_limits(0u);

    return led_min == first_limits.led_min_index && led_max == first_limits.led_max_index;
}

static bool rgb_runtime_normalize_local_range(uint8_t *led_min, uint8_t *led_max) {
    uint8_t local_min = 0u;
    uint8_t local_max = RGB_MATRIX_LED_COUNT;

    if (!(led_min && led_max)) {
        return false;
    }

    if (*led_min > RGB_MATRIX_LED_COUNT) {
        *led_min = RGB_MATRIX_LED_COUNT;
    }
    if (*led_max > RGB_MATRIX_LED_COUNT) {
        *led_max = RGB_MATRIX_LED_COUNT;
    }

#    ifdef RGB_MATRIX_SPLIT
    if (is_keyboard_left()) {
        local_max = RGB_LEFT_LED_COUNT;
    } else {
        local_min = RGB_LEFT_LED_COUNT;
    }
#    endif

    if (*led_min < local_min) {
        *led_min = local_min;
    }
    if (*led_max > local_max) {
        *led_max = local_max;
    }

    return *led_min < *led_max;
}

typedef void (*rgb_runtime_stage_fn_t)(void);

#    define RGB_RUNTIME_BOOT_INDICATOR_HSV ((hsv_t){.h = 0u, .s = 0u, .v = 150u})

static bool rgb_runtime_render_runtime_diag_stage(uint8_t led_min, uint8_t led_max) {
    if (!noah_runtime_diag_indicator_active()) {
        return false;
    }

    rgb_set_both_halves(hsv_to_rgb(RGB_RUNTIME_BOOT_INDICATOR_HSV), led_min, led_max);
    return true;
}

static bool rgb_runtime_render_layer_base_stage(uint8_t led_min, uint8_t led_max) {
    rgb_runtime_layer_stage_render_frame(&rgb_runtime_frame_primary, layer_state, led_min, led_max);
    return rgb_runtime_layer_stage_apply_frame(&rgb_runtime_frame_primary, led_min, led_max);
}

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
static bool rgb_runtime_render_base_stage(uint8_t led_min, uint8_t led_max) {
    // The synthetic automouse destination is not a persistent board state.
    // Once this branch stops running, the next frame falls back to ordinary
    // layer rendering below.
    if (rgb_runtime_automouse_stage_should_render(layer_state)) {
        return rgb_runtime_automouse_stage_render(layer_state, led_min, led_max);
    }

    return rgb_runtime_render_layer_base_stage(led_min, led_max);
}
#    else
static bool rgb_runtime_render_base_stage(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_render_layer_base_stage(led_min, led_max);
}
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static bool rgb_runtime_render_preview_stage(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_preview_stage_render(&rgb_runtime_frame_primary, led_min, led_max);
}
#    endif

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
static bool rgb_runtime_render_pd_mode_stage(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_pd_mode_stage_render(led_min, led_max);
}
#    endif

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
static bool rgb_runtime_render_combo_underlay_stage(uint8_t led_min, uint8_t led_max) {
    rgb_runtime_render_snapshot_ensure_combo();
    return rgb_runtime_combo_feedback_stage_render_underlay(rgb_runtime_render_snapshot.combo_underlay_bitmap, led_min, led_max);
}

static bool rgb_runtime_render_combo_overlay_stage(uint8_t led_min, uint8_t led_max) {
    rgb_runtime_render_snapshot_ensure_combo();
    return rgb_runtime_combo_feedback_stage_render_overlay(rgb_runtime_render_snapshot.combo_overlay_bitmap, led_min, led_max);
}
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static bool rgb_runtime_render_key_feedback_stage(uint8_t led_min, uint8_t led_max) {
    rgb_runtime_render_snapshot_ensure_key_feedback();
    return rgb_runtime_key_feedback_stage_render(rgb_runtime_render_snapshot.key_feedback_semantic_map, rgb_runtime_render_snapshot.key_feedback_tap_branch_map, rgb_runtime_render_snapshot.key_feedback_flash_visibility_bitmap, rgb_runtime_render_snapshot.key_feedback_broad_owner_map, led_min, led_max);
}
#    endif
#endif

void noah_rgb_runtime_invalidate_layer_maps(void) {
#ifdef RGB_MATRIX_ENABLE
    rgb_runtime_layer_stage_invalidate_maps();
#endif
}

void noah_rgb_runtime_post_init(void) {
#ifdef RGB_MATRIX_ENABLE
    static const rgb_runtime_stage_fn_t stages[] = {
        noah_rgb_validate_config,
        rgb_runtime_layer_stage_post_init,
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
        rgb_runtime_automouse_stage_post_init,
#    endif
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
        rgb_runtime_pd_mode_stage_post_init,
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
        rgb_runtime_combo_feedback_stage_post_init,
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
        rgb_runtime_key_feedback_stage_post_init,
#    endif
    };

    rgb_runtime_render_snapshot_invalidate();
#    ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
    rgb_runtime_stage_pipeline_count = 0u;
    rgb_runtime_early_exit_count     = 0u;
#    endif
    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
#endif
}

#ifdef RGB_MATRIX_ENABLE
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (rgb_runtime_range_starts_frame(led_min, led_max)) {
        rgb_runtime_render_snapshot_invalidate();
    }

    if (!rgb_runtime_normalize_local_range(&led_min, &led_max)) {
#    ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
        rgb_runtime_early_exit_count++;
#    endif
        return false;
    }

#    ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
    rgb_runtime_stage_pipeline_count++;
#    endif

    if (rgb_runtime_render_runtime_diag_stage(led_min, led_max)) {
        return true;
    }

    painted |= rgb_runtime_render_base_stage(led_min, led_max);

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    painted |= rgb_runtime_render_combo_underlay_stage(led_min, led_max);
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    painted |= rgb_runtime_render_preview_stage(led_min, led_max);
#    endif
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    painted |= rgb_runtime_render_pd_mode_stage(led_min, led_max);
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    painted |= rgb_runtime_render_combo_overlay_stage(led_min, led_max);
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    painted |= rgb_runtime_render_key_feedback_stage(led_min, led_max);
#    endif

    return painted;
}
#else
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return true;
}
#endif // RGB_MATRIX_ENABLE
