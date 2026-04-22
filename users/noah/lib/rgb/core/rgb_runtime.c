// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_runtime.h"
#include "../automouse/rgb_automouse_stage.h"
#include "../stages/rgb_combo_feedback_stage.h"
#include "../stages/rgb_key_feedback_stage.h"
#include "../stages/rgb_layer_stage.h"
#include "../stages/rgb_pd_mode_stage.h"
#include "../stages/rgb_preview_stage.h"
#include "../../state/runtime/runtime_diag.h"
#include "rgb_validation.h"

#ifdef RGB_MATRIX_ENABLE
static rgb_runtime_frame_t rgb_runtime_frame_primary;

typedef void (*rgb_runtime_stage_fn_t)(void);
typedef bool (*rgb_runtime_render_stage_fn_t)(uint8_t led_min, uint8_t led_max);

// Reserved watchdog reboot palette. Keep every diagnostic stage color distinct
// within this table so a latched crash breadcrumb is unambiguous at boot.
static rgb_t rgb_runtime_diag_stage_color(noah_runtime_diag_stage_t stage) {
    switch (stage) {
        case NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD:
            return (rgb_t){.r = 255u, .g = 24u, .b = 24u};
        case NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD_FINALIZE:
            return (rgb_t){.r = 255u, .g = 64u, .b = 96u};
        case NOAH_RUNTIME_DIAG_STAGE_LAYER_STATE_SET:
            return (rgb_t){.r = 255u, .g = 128u, .b = 0u};
        case NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK:
            return (rgb_t){.r = 0u, .g = 120u, .b = 255u};
        case NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_VIA_DEFAULTS:
            return (rgb_t){.r = 0u, .g = 255u, .b = 0u};
        case NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_KEY_RUNTIME:
            return (rgb_t){.r = 255u, .g = 0u, .b = 196u};
        case NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_SPLIT_SYNC:
            return (rgb_t){.r = 0u, .g = 255u, .b = 255u};
        case NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER:
            return (rgb_t){.r = 255u, .g = 255u, .b = 0u};
        case NOAH_RUNTIME_DIAG_STAGE_HOUSEKEEPING:
            return (rgb_t){.r = 0u, .g = 255u, .b = 96u};
        case NOAH_RUNTIME_DIAG_STAGE_POST_INIT:
            return (rgb_t){.r = 160u, .g = 80u, .b = 255u};
        case NOAH_RUNTIME_DIAG_STAGE_IDLE:
        default:
            return (rgb_t){.r = 160u, .g = 160u, .b = 160u};
    }
}

static bool rgb_runtime_render_runtime_diag_stage(uint8_t led_min, uint8_t led_max) {
    if (!noah_runtime_diag_indicator_active()) {
        return false;
    }

    rgb_set_both_halves(rgb_runtime_diag_stage_color(noah_runtime_diag_watchdog_stage()), led_min, led_max);
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

static bool rgb_runtime_render_preview_stage(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_preview_stage_render(led_min, led_max);
}

static bool rgb_runtime_render_pd_mode_stage(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_pd_mode_stage_render(led_min, led_max);
}

static bool rgb_runtime_render_combo_underlay_stage(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_combo_feedback_stage_render_underlay(led_min, led_max);
}

static bool rgb_runtime_render_combo_overlay_stage(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_combo_feedback_stage_render_overlay(led_min, led_max);
}

static bool rgb_runtime_render_key_feedback_stage(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_key_feedback_stage_render(led_min, led_max);
}
#endif

void noah_rgb_runtime_invalidate_layer_maps(void) {
#ifdef RGB_MATRIX_ENABLE
    rgb_runtime_layer_stage_invalidate_maps();
#endif
}

void noah_rgb_runtime_post_init(void) {
#ifdef RGB_MATRIX_ENABLE
    static const rgb_runtime_stage_fn_t stages[] = {
        noah_rgb_validate_config, rgb_runtime_layer_stage_post_init, rgb_runtime_automouse_stage_post_init, rgb_runtime_pd_mode_stage_post_init, rgb_runtime_combo_feedback_stage_post_init, rgb_runtime_key_feedback_stage_post_init,
    };

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
#endif
}

#ifdef RGB_MATRIX_ENABLE
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    static const rgb_runtime_render_stage_fn_t underlay_stages[] = {
        rgb_runtime_render_combo_underlay_stage,
        rgb_runtime_render_preview_stage,
        rgb_runtime_render_pd_mode_stage,
        rgb_runtime_render_combo_overlay_stage,
        rgb_runtime_render_key_feedback_stage,
    };
    bool painted = false;

    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER);
    if (rgb_runtime_render_runtime_diag_stage(led_min, led_max)) {
        noah_runtime_diag_scope_leave();
        return true;
    }

    painted |= rgb_runtime_render_base_stage(led_min, led_max);

    for (uint8_t index = 0; index < ARRAY_SIZE(underlay_stages); index++) {
        painted |= underlay_stages[index](led_min, led_max);
    }

    noah_runtime_diag_scope_leave();
    return painted;
}
#else
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return true;
}
#endif // RGB_MATRIX_ENABLE
