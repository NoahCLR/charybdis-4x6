// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include <string.h>

#include "rgb_runtime.h"
#include "rgb_automouse.h"
#include "rgb_helpers.h"

#if defined(RGB_MATRIX_ENABLE)
#    include "keymap_introspection.h" // QMK
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
extern const layer_color_config_t layer_colors[];
extern const layer_led_group_t    layer_led_groups[];
extern const uint8_t              layer_led_group_count;
#    ifdef POINTING_DEVICE_ENABLE
extern const pd_mode_color_t     pd_mode_colors[];
extern const uint8_t             pd_mode_color_count;
extern const pd_mode_led_group_t pd_mode_led_groups[];
extern const uint8_t             pd_mode_led_group_count;
#    endif
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
extern const hsv_t automouse_color_start;
extern const hsv_t automouse_color_end;
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
extern const hsv_t feedback_multi_tap_pending_color;
extern const hsv_t feedback_hold_active_color;
extern const hsv_t feedback_long_hold_active_color;
#    endif
#endif

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#    if RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS <= 0
#        error "RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS must be greater than zero"
#    endif
#endif

#ifdef RGB_MATRIX_ENABLE
static rgb_t layer_rgb[LAYER_COUNT];
static bool  layer_key_led_map_dirty = true;
static bool  layer_key_led_map[LAYER_COUNT][RGB_MATRIX_LED_COUNT];
#    ifdef POINTING_DEVICE_ENABLE
static rgb_t pd_mode_rgb[PD_MODE_COUNT];
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static rgb_t feedback_multi_tap_pending_rgb;
static rgb_t feedback_hold_active_rgb;
static rgb_t feedback_long_hold_active_rgb;
#    endif
#endif

void noah_rgb_runtime_invalidate_layer_maps(void) {
#ifdef RGB_MATRIX_ENABLE
    layer_key_led_map_dirty = true;
#endif
}

void noah_rgb_runtime_post_init(void) {
#ifdef RGB_MATRIX_ENABLE
    for (uint8_t i = 0; i < LAYER_COUNT; i++) {
        layer_rgb[i] = hsv_to_rgb(layer_colors[i].color);
    }
    noah_rgb_runtime_invalidate_layer_maps();

#    ifdef POINTING_DEVICE_ENABLE
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        for (uint8_t c = 0; c < pd_mode_color_count; c++) {
            if (pd_mode_colors[c].mode_flag == pd_modes[i].mode_flag) {
                pd_mode_rgb[i] = hsv_to_rgb(pd_mode_colors[c].color);
                break;
            }
        }
    }
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    feedback_multi_tap_pending_rgb = hsv_to_rgb(feedback_multi_tap_pending_color);
    feedback_hold_active_rgb       = hsv_to_rgb(feedback_hold_active_color);
    feedback_long_hold_active_rgb  = hsv_to_rgb(feedback_long_hold_active_color);
#    endif
#endif
}

#ifdef RGB_MATRIX_ENABLE
static bool rgb_runtime_layer_has_solid_color(uint8_t layer) {
    return !(layer_colors[layer].color.s == 0 && layer_colors[layer].color.v == 0);
}

static bool rgb_runtime_layer_paints_mapped_keys_only(uint8_t layer) {
    return (layer_colors[layer].flags & LAYER_COLOR_FLAG_MAPPED_KEYS_ONLY) != 0;
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
        if (!rgb_runtime_layer_paints_mapped_keys_only(layer)) {
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

static bool rgb_runtime_paint_layer(uint8_t layer, uint8_t led_min, uint8_t led_max) {
    if (!rgb_runtime_layer_paints_mapped_keys_only(layer)) {
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

static bool rgb_runtime_led_group_intersects(const uint8_t *leds, uint8_t count, uint8_t led_min, uint8_t led_max) {
    for (uint8_t i = 0; i < count; i++) {
        if (leds[i] >= led_min && leds[i] < led_max) {
            return true;
        }
    }

    return false;
}

bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    bool layer_painted = false;
#    endif
    bool painted = false;

    uint8_t preview_layer = UINT8_MAX;
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    preview_layer = rgb_runtime_preview_layer();
#    endif

    for (uint8_t i = 1; i < LAYER_COUNT; i++) {
        if (!layer_state_cmp(layer_state, i)) continue;
#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
        if (i == get_auto_mouse_layer()) continue;
#    endif
        if (!rgb_runtime_layer_has_solid_color(i)) continue;

        bool this_layer_painted = rgb_runtime_paint_layer(i, led_min, led_max);
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
        layer_painted |= this_layer_painted;
#    endif
        painted |= this_layer_painted;
    }

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    if (preview_layer >= LAYER_COUNT && !layer_painted && layer_state_cmp(layer_state, get_auto_mouse_layer())) {
        automouse_rgb_render(led_min, led_max, automouse_color_start, automouse_color_end);
        layer_painted = true;
        painted       = true;
    }
#    endif

    for (uint8_t g = 0; g < layer_led_group_count; g++) {
        bool group_active = layer_state_cmp(layer_state, layer_led_groups[g].layer);
        if (group_active) {
            rgb_t grp_rgb = hsv_to_rgb(layer_led_groups[g].color);
            rgb_set_led_group(layer_led_groups[g].leds, layer_led_groups[g].count, led_min, led_max, grp_rgb);
            painted |= rgb_runtime_led_group_intersects(layer_led_groups[g].leds, layer_led_groups[g].count, led_min, led_max);
        }
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
        if (pd_mode_active(pd_mode_led_groups[g].mode_flag)) {
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
        rgb_set_both_halves(feedback_multi_tap_pending_rgb, led_min, led_max);
        painted = true;
    } else if (key_feedback_flags_hold_active(fb)) {
        if (!key_feedback_flags_level_flash(fb) || key_feedback_flags_flash_phase(fb)) {
            if (key_feedback_flags_long_hold_active(fb)) {
                rgb_set_both_halves(feedback_long_hold_active_rgb, led_min, led_max);
            } else {
                rgb_set_both_halves(feedback_hold_active_rgb, led_min, led_max);
            }
            painted = true;
        }
    } else if (key_feedback_flags_hold_pending(fb)) {
        rgb_set_both_halves(feedback_hold_active_rgb, led_min, led_max);
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
