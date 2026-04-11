// ────────────────────────────────────────────────────────────────────────────
// RGB Preview Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_preview_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)

#    include "rgb_layer_stage.h"
#    include "../key/key_runtime_feedback.h"
#    include "../state/split_runtime_sync.h"

extern const layer_led_group_t *const layer_led_groups;
extern const uint8_t                  layer_led_group_count;

static uint8_t rgb_runtime_preview_stage_current_layer(void) {
    return is_keyboard_master() ? key_feedback_preview_layer() : split_runtime_sync_remote.key_preview_layer;
}

static bool rgb_runtime_preview_stage_led_group_intersects(const uint8_t *leds, uint8_t count, uint8_t led_min, uint8_t led_max) {
    for (uint8_t i = 0; i < count; i++) {
        if (leds[i] >= led_min && leds[i] < led_max) {
            return true;
        }
    }

    return false;
}

bool rgb_runtime_preview_stage_render(uint8_t led_min, uint8_t led_max) {
    uint8_t preview_layer = rgb_runtime_preview_stage_current_layer();
    if (preview_layer >= LAYER_COUNT || !rgb_runtime_layer_stage_has_solid_color(preview_layer)) {
        return false;
    }

    bool painted = rgb_runtime_layer_stage_paint_layer(preview_layer, led_min, led_max);

    for (uint8_t group = 0; group < layer_led_group_count; group++) {
        if (layer_led_groups[group].layer != preview_layer) {
            continue;
        }

        rgb_t group_rgb = hsv_to_rgb(layer_led_groups[group].color);
        rgb_set_led_group(layer_led_groups[group].leds, layer_led_groups[group].count, led_min, led_max, group_rgb);
        painted |= rgb_runtime_preview_stage_led_group_intersects(layer_led_groups[group].leds, layer_led_groups[group].count, led_min, led_max);
    }

    return painted;
}

#endif
