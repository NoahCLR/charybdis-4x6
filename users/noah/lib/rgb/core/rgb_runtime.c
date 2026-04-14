// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_runtime.h"
#include "../automouse/rgb_automouse_stage.h"
#include "../stages/rgb_key_feedback_stage.h"
#include "../stages/rgb_layer_stage.h"
#include "../stages/rgb_pd_mode_stage.h"
#include "../stages/rgb_preview_stage.h"
#include "rgb_validation.h"

#ifdef RGB_MATRIX_ENABLE
static rgb_runtime_frame_t rgb_runtime_frame_primary;

typedef void (*rgb_runtime_stage_fn_t)(void);
typedef bool (*rgb_runtime_render_stage_fn_t)(uint8_t led_min, uint8_t led_max);

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
        noah_rgb_validate_config, rgb_runtime_layer_stage_post_init, rgb_runtime_automouse_stage_post_init, rgb_runtime_pd_mode_stage_post_init, rgb_runtime_key_feedback_stage_post_init,
    };

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
#endif
}

#ifdef RGB_MATRIX_ENABLE
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    static const rgb_runtime_render_stage_fn_t overlay_stages[] = {
        rgb_runtime_render_preview_stage,
        rgb_runtime_render_pd_mode_stage,
        rgb_runtime_render_key_feedback_stage,
    };
    bool painted = false;

    painted |= rgb_runtime_render_base_stage(led_min, led_max);

    for (uint8_t index = 0; index < ARRAY_SIZE(overlay_stages); index++) {
        painted |= overlay_stages[index](led_min, led_max);
    }

    return painted;
}
#else
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return true;
}
#endif // RGB_MATRIX_ENABLE
