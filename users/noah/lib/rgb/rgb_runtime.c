// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include <string.h>

#include "rgb_runtime.h"
#include "rgb_automouse.h"
#include "rgb_helpers.h"
#include "rgb_validation.h"

#if defined(RGB_MATRIX_ENABLE)
#    include "keymap_introspection.h" // QMK
#endif
#if defined(RGB_MATRIX_ENABLE) && defined(RGB_MATRIX_WS2812)
#    include "ws2812.h" // QMK driver buffer access
#endif
#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#endif
#if defined(POINTING_DEVICE_ENABLE)
#    include "../pointing/pd_modes.h"
#endif
#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)
#    include "../key/key_runtime_feedback.h"
#    include "../state/split_runtime_sync.h"
#endif

// ─── Authored keymap data (defined in rgb_config.c) ──────────────────────
#ifdef RGB_MATRIX_ENABLE
extern const layer_color_config_t     layer_colors[];
extern const layer_led_group_t *const layer_led_groups;
extern const uint8_t                  layer_led_group_count;
#    ifdef POINTING_DEVICE_ENABLE
extern const pd_mode_color_t            pd_mode_colors[];
extern const uint8_t                    pd_mode_color_count;
extern const pd_mode_led_group_t *const pd_mode_led_groups;
extern const uint8_t                    pd_mode_led_group_count;
#    endif
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
extern const automouse_fade_end_config_t automouse_fade_end_config;
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
extern const key_behavior_feedback_color_config_t key_behavior_feedback_colors;
#    endif
#endif

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#    if RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS <= 0
#        error "RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS must be greater than zero"
#    endif
#endif

#ifdef RGB_MATRIX_ENABLE
typedef struct {
    rgb_t colors[RGB_MATRIX_LED_COUNT];
    bool  painted[RGB_MATRIX_LED_COUNT];
} rgb_runtime_frame_t;

static rgb_t               layer_rgb[LAYER_COUNT];
static bool                layer_key_led_map_dirty = true;
static bool                layer_key_led_map[LAYER_COUNT][RGB_MATRIX_LED_COUNT];
static rgb_runtime_frame_t rgb_runtime_frame_primary;
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
static rgb_runtime_frame_t rgb_runtime_frame_secondary;
static rgb_runtime_frame_t rgb_runtime_frame_base_effect;
static rgb_t               automouse_end_color_rgb;
#    endif
#    ifdef POINTING_DEVICE_ENABLE
static rgb_t pd_mode_rgb[PD_MODE_COUNT];
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static rgb_t key_behavior_feedback_multi_tap_pending_rgb;
static rgb_t key_behavior_feedback_hold_active_rgb;
static rgb_t key_behavior_feedback_long_hold_active_rgb;
#    endif
#endif

void noah_rgb_runtime_invalidate_layer_maps(void) {
#ifdef RGB_MATRIX_ENABLE
    layer_key_led_map_dirty = true;
#endif
}

void noah_rgb_runtime_post_init(void) {
#ifdef RGB_MATRIX_ENABLE
    noah_rgb_validate_config();

    for (uint8_t i = 0; i < LAYER_COUNT; i++) {
        layer_rgb[i] = hsv_to_rgb(layer_colors[i].color);
    }
    noah_rgb_runtime_invalidate_layer_maps();

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    automouse_end_color_rgb = hsv_to_rgb(automouse_fade_end_config.end_color);
#    endif

#    ifdef POINTING_DEVICE_ENABLE
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        for (uint8_t c = 0; c < pd_mode_color_count; c++) {
            if (pd_mode_colors[c].pointing_mode == pd_modes[i].mode_flag) {
                pd_mode_rgb[i] = hsv_to_rgb(pd_mode_colors[c].color);
                break;
            }
        }
    }
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    key_behavior_feedback_multi_tap_pending_rgb = hsv_to_rgb(key_behavior_feedback_colors.multi_tap_pending_color);
    key_behavior_feedback_hold_active_rgb       = hsv_to_rgb(key_behavior_feedback_colors.hold_active_color);
    key_behavior_feedback_long_hold_active_rgb  = hsv_to_rgb(key_behavior_feedback_colors.long_hold_active_color);
#    endif
#endif
}

#ifdef RGB_MATRIX_ENABLE
static bool rgb_runtime_layer_has_solid_color(uint8_t layer) {
    return !(layer_colors[layer].color.s == 0 && layer_colors[layer].color.v == 0);
}

static bool rgb_runtime_layer_paints_only_keys_present_on_this_layer(uint8_t layer) {
    return layer_colors[layer].mode == KEYS_MAPPED_ON_THIS_LAYER_ONLY;
}

static bool rgb_runtime_keycode_is_mapped(uint16_t keycode) {
    return keycode != KC_NO && keycode != KC_TRNS;
}

static void rgb_runtime_rebuild_layer_key_led_map(void) {
    if (!layer_key_led_map_dirty) {
        return;
    }

    memset(layer_key_led_map, 0, sizeof(layer_key_led_map));

    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        if (!rgb_runtime_layer_paints_only_keys_present_on_this_layer(layer)) {
            continue;
        }

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                uint16_t keycode = keycode_at_keymap_location(layer, row, col);
                if (!rgb_runtime_keycode_is_mapped(keycode)) {
                    continue;
                }

                uint8_t leds[RGB_MATRIX_LED_COUNT];
                uint8_t led_count = rgb_matrix_map_row_column_to_led(row, col, leds);
                for (uint8_t i = 0; i < led_count; i++) {
                    if (leds[i] < RGB_MATRIX_LED_COUNT) {
                        layer_key_led_map[layer][leds[i]] = true;
                    }
                }
            }
        }
    }

    layer_key_led_map_dirty = false;
}

static void rgb_runtime_frame_clear(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    for (uint8_t led = led_min; led < led_max; led++) {
        frame->colors[led]  = (rgb_t){0};
        frame->painted[led] = false;
    }
}

static bool rgb_runtime_frame_fill(rgb_runtime_frame_t *frame, rgb_t color, uint8_t led_min, uint8_t led_max) {
    if (led_min >= led_max) {
        return false;
    }

    for (uint8_t led = led_min; led < led_max; led++) {
        frame->colors[led]  = color;
        frame->painted[led] = true;
    }

    return true;
}

static bool rgb_runtime_frame_paint_layer(rgb_runtime_frame_t *frame, uint8_t layer, uint8_t led_min, uint8_t led_max) {
    if (!rgb_runtime_layer_paints_only_keys_present_on_this_layer(layer)) {
        return rgb_runtime_frame_fill(frame, layer_rgb[layer], led_min, led_max);
    }

    rgb_runtime_rebuild_layer_key_led_map();

    bool painted = false;
    for (uint8_t led = led_min; led < led_max; led++) {
        if (!layer_key_led_map[layer][led]) {
            continue;
        }

        frame->colors[led]  = layer_rgb[layer];
        frame->painted[led] = true;
        painted             = true;
    }

    return painted;
}

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static bool rgb_runtime_paint_layer(uint8_t layer, uint8_t led_min, uint8_t led_max) {
    if (!rgb_runtime_layer_paints_only_keys_present_on_this_layer(layer)) {
        rgb_set_both_halves(layer_rgb[layer], led_min, led_max);
        return led_min < led_max;
    }

    rgb_runtime_rebuild_layer_key_led_map();

    bool painted = false;
    for (uint8_t led = led_min; led < led_max; led++) {
        if (!layer_key_led_map[layer][led]) {
            continue;
        }

        rgb_set_led_color(led, led_min, led_max, layer_rgb[layer]);
        painted = true;
    }

    return painted;
}
#    endif

static bool rgb_runtime_frame_paint_led_group(rgb_runtime_frame_t *frame, const uint8_t *leds, uint8_t count, rgb_t color, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    for (uint8_t i = 0; i < count; i++) {
        uint8_t led = leds[i];
        if (led < led_min || led >= led_max) {
            continue;
        }

        frame->colors[led]  = color;
        frame->painted[led] = true;
        painted             = true;
    }

    return painted;
}

static bool rgb_runtime_frame_render_layer_stage(rgb_runtime_frame_t *frame, layer_state_t state, uint8_t led_min, uint8_t led_max) {
    rgb_runtime_frame_clear(frame, led_min, led_max);

    bool painted = false;

    for (uint8_t layer = 1; layer < LAYER_COUNT; layer++) {
        if (!layer_state_cmp(state, layer)) {
            continue;
        }
        if (!rgb_runtime_layer_has_solid_color(layer)) {
            continue;
        }

        painted |= rgb_runtime_frame_paint_layer(frame, layer, led_min, led_max);
    }

    for (uint8_t g = 0; g < layer_led_group_count; g++) {
        if (!layer_state_cmp(state, layer_led_groups[g].layer)) {
            continue;
        }

        rgb_t group_rgb = hsv_to_rgb(layer_led_groups[g].color);
        painted |= rgb_runtime_frame_paint_led_group(frame, layer_led_groups[g].leds, layer_led_groups[g].count, group_rgb, led_min, led_max);
    }

    return painted;
}

static bool rgb_runtime_apply_frame(const rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    for (uint8_t led = led_min; led < led_max; led++) {
        if (!frame->painted[led]) {
            continue;
        }

        rgb_set_led_color(led, led_min, led_max, frame->colors[led]);
        painted = true;
    }

    return painted;
}

#    ifdef POINTING_DEVICE_ENABLE
static bool rgb_runtime_led_range_intersects(uint8_t led_min, uint8_t led_max, uint8_t from, uint8_t to) {
    return led_min < to && led_max > from;
}
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static uint8_t rgb_runtime_preview_layer(void) {
    return is_keyboard_master() ? key_feedback_preview_layer() : split_runtime_sync_remote.key_preview_layer;
}
#    endif

#    if defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE) || defined(POINTING_DEVICE_ENABLE)
static bool rgb_runtime_led_group_intersects(const uint8_t *leds, uint8_t count, uint8_t led_min, uint8_t led_max) {
    for (uint8_t i = 0; i < count; i++) {
        if (leds[i] >= led_min && leds[i] < led_max) {
            return true;
        }
    }

    return false;
}
#    endif

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
static rgb_t rgb_runtime_blend_rgb(rgb_t start, rgb_t end, uint8_t amount) {
    uint32_t inv = (uint32_t)UINT8_MAX - amount;

    return (rgb_t){
        .r = (uint8_t)(((uint32_t)start.r * inv + (uint32_t)end.r * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
        .g = (uint8_t)(((uint32_t)start.g * inv + (uint32_t)end.g * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
        .b = (uint8_t)(((uint32_t)start.b * inv + (uint32_t)end.b * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
    };
}

#        if defined(RGB_MATRIX_WS2812)
extern ws2812_led_t ws2812_leds[WS2812_LED_COUNT];

static bool rgb_runtime_frame_capture_base_effect(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    rgb_runtime_frame_clear(frame, led_min, led_max);

    bool captured = false;

    for (uint8_t led = led_min; led < led_max; led++) {
        int driver_index = rgb_matrix_led_index(led);
        if (driver_index < 0) {
            continue;
        }

        frame->colors[led] = (rgb_t){
            .r = ws2812_leds[driver_index].r,
            .g = ws2812_leds[driver_index].g,
            .b = ws2812_leds[driver_index].b,
        };
        frame->painted[led] = true;
        captured            = true;
    }

    return captured;
}
#        else
static bool rgb_runtime_frame_capture_base_effect(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    rgb_runtime_frame_clear(frame, led_min, led_max);
    (void)frame;
    (void)led_min;
    (void)led_max;
    return false;
}
#        endif

static bool rgb_runtime_automouse_end_mode_is(automouse_fade_end_mode_t mode) {
    return automouse_fade_end_config.mode == mode;
}

static layer_state_t rgb_runtime_layer_state_without_layer(layer_state_t state, uint8_t layer) {
    if (layer >= sizeof(layer_state_t) * 8u) {
        return state;
    }

    return state & ~((layer_state_t)1u << layer);
}

// Auto-mouse renders as a two-frame blend:
//   start = the current layer stack, including the auto-mouse layer
//   end   = the layer stack after removing the auto-mouse layer, optionally
//           replaced or filled by automouse_fade_end_config.mode
//
// This stage only owns the base layer render. Pointing-device overlays and key
// feedback still paint later in the frame, so they can intentionally override
// the fade result on top.
static bool rgb_runtime_render_automouse_layer_stage(layer_state_t state, uint8_t led_min, uint8_t led_max) {
    uint8_t  auto_mouse_layer = get_auto_mouse_layer();
    uint16_t progress         = automouse_rgb_current_progress();
    uint8_t  blend            = automouse_rgb_blend_amount(progress);

    bool start_painted = rgb_runtime_frame_render_layer_stage(&rgb_runtime_frame_primary, state, led_min, led_max);
    bool end_painted;

    if (rgb_runtime_automouse_end_mode_is(END_COLOR_ON_ALL_KEYS)) {
        rgb_runtime_frame_clear(&rgb_runtime_frame_secondary, led_min, led_max);
        end_painted = rgb_runtime_frame_fill(&rgb_runtime_frame_secondary, automouse_end_color_rgb, led_min, led_max);
    } else {
        layer_state_t end_state = rgb_runtime_layer_state_without_layer(state, auto_mouse_layer);
        end_painted             = rgb_runtime_frame_render_layer_stage(&rgb_runtime_frame_secondary, end_state, led_min, led_max);
        rgb_runtime_frame_capture_base_effect(&rgb_runtime_frame_base_effect, led_min, led_max);

        for (uint8_t led = led_min; led < led_max; led++) {
            if (rgb_runtime_frame_secondary.painted[led]) {
                continue;
            }

            if (rgb_runtime_automouse_end_mode_is(END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW)) {
                rgb_runtime_frame_secondary.colors[led]  = automouse_end_color_rgb;
                rgb_runtime_frame_secondary.painted[led] = true;
                end_painted                              = true;
                continue;
            }

            if (!rgb_runtime_frame_base_effect.painted[led]) {
                continue;
            }

            rgb_runtime_frame_secondary.colors[led]  = rgb_runtime_frame_base_effect.colors[led];
            rgb_runtime_frame_secondary.painted[led] = true;
            end_painted                              = true;
        }
    }

    if (!start_painted && !end_painted) {
        return false;
    }

    bool painted = false;

    for (uint8_t led = led_min; led < led_max; led++) {
        bool start_visible = rgb_runtime_frame_primary.painted[led];
        bool end_visible   = rgb_runtime_frame_secondary.painted[led];

        if (start_visible && end_visible) {
            rgb_t blended = rgb_runtime_blend_rgb(rgb_runtime_frame_primary.colors[led], rgb_runtime_frame_secondary.colors[led], blend);
            rgb_set_led_color(led, led_min, led_max, blended);
            painted = true;
            continue;
        }

        if (start_visible) {
            if (blend == UINT8_MAX) {
                continue;
            }

            rgb_set_led_color(led, led_min, led_max, rgb_runtime_frame_primary.colors[led]);
            painted = true;
            continue;
        }

        if (end_visible && blend != 0) {
            rgb_set_led_color(led, led_min, led_max, rgb_runtime_frame_secondary.colors[led]);
            painted = true;
        }
    }

    return painted;
}
#    endif

bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool painted = false;

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    uint8_t preview_layer = rgb_runtime_preview_layer();
#    endif

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    // The synthetic automouse destination is not a persistent board state.
    // Once this branch stops running, the next frame falls back to ordinary
    // layer rendering below.
    if (layer_state_cmp(layer_state, get_auto_mouse_layer()) && automouse_rgb_should_render()) {
        painted |= rgb_runtime_render_automouse_layer_stage(layer_state, led_min, led_max);
    } else
#    endif
    {
        rgb_runtime_frame_render_layer_stage(&rgb_runtime_frame_primary, layer_state, led_min, led_max);
        painted |= rgb_runtime_apply_frame(&rgb_runtime_frame_primary, led_min, led_max);
    }

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    if (preview_layer < LAYER_COUNT && rgb_runtime_layer_has_solid_color(preview_layer)) {
        painted |= rgb_runtime_paint_layer(preview_layer, led_min, led_max);

        for (uint8_t g = 0; g < layer_led_group_count; g++) {
            if (layer_led_groups[g].layer != preview_layer) {
                continue;
            }

            rgb_t grp_rgb = hsv_to_rgb(layer_led_groups[g].color);
            rgb_set_led_group(layer_led_groups[g].leds, layer_led_groups[g].count, led_min, led_max, grp_rgb);
            painted |= rgb_runtime_led_group_intersects(layer_led_groups[g].leds, layer_led_groups[g].count, led_min, led_max);
        }
    }
#    endif

    // ─── Pointing-device mode overlay ───────────────────────────────────
    //
    // Paint active pointer-mode color and groups, then let interaction
    // feedback paint over that when needed.
#    ifdef POINTING_DEVICE_ENABLE
    uint8_t active_mode = pd_mode_first_active_index();
    if (active_mode < PD_MODE_COUNT) {
        rgb_set_right_half(pd_mode_rgb[active_mode], led_min, led_max);
        painted |= rgb_runtime_led_range_intersects(led_min, led_max, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT);
    }

    for (uint8_t g = 0; g < pd_mode_led_group_count; g++) {
        if (pd_mode_active(pd_mode_led_groups[g].pointing_mode)) {
            rgb_t grp_rgb = hsv_to_rgb(pd_mode_led_groups[g].color);
            rgb_set_led_group(pd_mode_led_groups[g].leds, pd_mode_led_groups[g].count, led_min, led_max, grp_rgb);
            painted |= rgb_runtime_led_group_intersects(pd_mode_led_groups[g].leds, pd_mode_led_groups[g].count, led_min, led_max);
        }
    }
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    // ─── Key behavior feedback ──────────────────────────────────────────
    //
    // Paint last so threshold/flash feedback remains visible even on the
    // trackball half while a pd mode is active.
    // Master reads live state; slave reads the synced feedback flags from the
    // split packet, including the flash-phase bit used to keep both halves in
    // lockstep.
    // Multi-tap pending > hold feedback > nothing (priority order).
    uint8_t fb = is_keyboard_master() ? key_feedback_pack() : split_runtime_sync_remote.key_feedback_flags;

    if (key_feedback_flags_multi_tap_pending(fb)) {
        rgb_set_both_halves(key_behavior_feedback_multi_tap_pending_rgb, led_min, led_max);
        painted = true;
    } else if (key_feedback_flags_hold_active(fb)) {
        if (!key_feedback_flags_level_flash(fb) || key_feedback_flags_flash_phase(fb)) {
            if (key_feedback_flags_long_hold_active(fb)) {
                rgb_set_both_halves(key_behavior_feedback_long_hold_active_rgb, led_min, led_max);
            } else {
                rgb_set_both_halves(key_behavior_feedback_hold_active_rgb, led_min, led_max);
            }
            painted = true;
        }
    } else if (key_feedback_flags_hold_pending(fb)) {
        rgb_set_both_halves(key_behavior_feedback_hold_active_rgb, led_min, led_max);
        painted = true;
    }
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
