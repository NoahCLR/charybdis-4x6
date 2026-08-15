// ────────────────────────────────────────────────────────────────────────────
// RGB Preview Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_preview_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)

#    include "rgb_layer_stage.h"
#    include "../../key/runtime/feedback.h"
#    include "../../split/runtime_sync.h"

static uint8_t rgb_runtime_preview_stage_current_layer(void) {
    return is_keyboard_master() ? key_feedback_preview_layer() : split_runtime_sync_remote.key_preview_layer;
}

bool rgb_runtime_preview_stage_render(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    uint8_t preview_layer = rgb_runtime_preview_stage_current_layer();
    if (!frame || preview_layer >= LAYER_COUNT) {
        return false;
    }

    rgb_runtime_layer_stage_render_selected_frame(frame, preview_layer, led_min, led_max);
    return rgb_runtime_layer_stage_apply_frame(frame, led_min, led_max);
}

#endif
