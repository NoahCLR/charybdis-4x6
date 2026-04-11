// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_runtime.h"
#include "rgb_automouse_stage.h"
#include "rgb_key_feedback_stage.h"
#include "rgb_layer_stage.h"
#include "rgb_pd_mode_stage.h"
#include "rgb_preview_stage.h"
#include "rgb_validation.h"

#ifdef RGB_MATRIX_ENABLE
static rgb_runtime_frame_t rgb_runtime_frame_primary;
#endif

void noah_rgb_runtime_invalidate_layer_maps(void) {
#ifdef RGB_MATRIX_ENABLE
    rgb_runtime_layer_stage_invalidate_maps();
#endif
}

void noah_rgb_runtime_post_init(void) {
#ifdef RGB_MATRIX_ENABLE
    noah_rgb_validate_config();
    rgb_runtime_layer_stage_post_init();
    rgb_runtime_automouse_stage_post_init();
    rgb_runtime_pd_mode_stage_post_init();
    rgb_runtime_key_feedback_stage_post_init();
#endif
}

#ifdef RGB_MATRIX_ENABLE
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool painted = false;

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    // The synthetic automouse destination is not a persistent board state.
    // Once this branch stops running, the next frame falls back to ordinary
    // layer rendering below.
    if (rgb_runtime_automouse_stage_should_render(layer_state)) {
        painted |= rgb_runtime_automouse_stage_render(layer_state, led_min, led_max);
    } else
#    endif
    {
        rgb_runtime_layer_stage_render_frame(&rgb_runtime_frame_primary, layer_state, led_min, led_max);
        painted |= rgb_runtime_layer_stage_apply_frame(&rgb_runtime_frame_primary, led_min, led_max);
    }

    painted |= rgb_runtime_preview_stage_render(led_min, led_max);

    // ─── Pointing-device mode overlay ───────────────────────────────────
    //
    // Paint active pointer-mode color and groups, then let interaction
    // feedback paint over that when needed.
    painted |= rgb_runtime_pd_mode_stage_render(led_min, led_max);

    // ─── Key behavior feedback ──────────────────────────────────────────
    //
    // Paint last so threshold/flash feedback remains visible even on the
    // trackball half while a pd mode is active.
    painted |= rgb_runtime_key_feedback_stage_render(led_min, led_max);

    return painted;
}
#else
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return true;
}
#endif // RGB_MATRIX_ENABLE
