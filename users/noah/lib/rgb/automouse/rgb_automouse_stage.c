// ────────────────────────────────────────────────────────────────────────────
// RGB Automouse Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_automouse_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)

#    include "rgb_automouse.h"
#    include "../core/rgb_helpers.h"
#    include "../stages/rgb_layer_stage.h"
#    include "../../compat/qmk_auto_mouse_contract.h"

#    if defined(RGB_MATRIX_WS2812)
#        include "ws2812.h" // QMK driver buffer access
#    endif

extern const automouse_fade_end_config_t automouse_fade_end_config;

static rgb_runtime_frame_t rgb_runtime_frame_primary;
static rgb_runtime_frame_t rgb_runtime_frame_secondary;
static rgb_runtime_frame_t rgb_runtime_frame_base_effect;
static rgb_t               automouse_end_color_rgb;

static rgb_t rgb_runtime_automouse_stage_blend_rgb(rgb_t start, rgb_t end, uint8_t amount) {
    uint32_t inv = (uint32_t)UINT8_MAX - amount;

    return (rgb_t){
        .r = (uint8_t)(((uint32_t)start.r * inv + (uint32_t)end.r * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
        .g = (uint8_t)(((uint32_t)start.g * inv + (uint32_t)end.g * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
        .b = (uint8_t)(((uint32_t)start.b * inv + (uint32_t)end.b * amount + (UINT8_MAX / 2u)) / UINT8_MAX),
    };
}

#    if defined(RGB_MATRIX_WS2812)
extern ws2812_led_t ws2812_leds[WS2812_LED_COUNT];

static bool rgb_runtime_automouse_stage_capture_base_effect(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
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
#    else
static bool rgb_runtime_automouse_stage_capture_base_effect(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    rgb_runtime_frame_clear(frame, led_min, led_max);
    (void)frame;
    (void)led_min;
    (void)led_max;
    return false;
}
#    endif

static bool rgb_runtime_automouse_stage_end_mode_is(automouse_fade_end_mode_t mode) {
    return automouse_fade_end_config.mode == mode;
}

static layer_state_t rgb_runtime_automouse_stage_state_without_layer(layer_state_t state, uint8_t layer) {
    if (layer >= sizeof(layer_state_t) * 8u) {
        return state;
    }

    return state & ~((layer_state_t)1u << layer);
}

void rgb_runtime_automouse_stage_post_init(void) {
    automouse_end_color_rgb = hsv_to_rgb(automouse_fade_end_config.end_color);
}

bool rgb_runtime_automouse_stage_should_render(layer_state_t state) {
    return layer_state_cmp(state, noah_qmk_contract_auto_mouse_layer()) && automouse_rgb_should_render();
}

bool rgb_runtime_automouse_stage_render(layer_state_t state, uint8_t led_min, uint8_t led_max) {
    uint8_t  auto_mouse_layer = noah_qmk_contract_auto_mouse_layer();
    uint16_t progress         = automouse_rgb_current_progress();
    uint8_t  blend            = automouse_rgb_blend_amount(progress);

    bool start_painted = rgb_runtime_layer_stage_render_frame(&rgb_runtime_frame_primary, state, led_min, led_max);
    bool end_painted;

    if (rgb_runtime_automouse_stage_end_mode_is(END_COLOR_ON_ALL_KEYS)) {
        rgb_runtime_frame_clear(&rgb_runtime_frame_secondary, led_min, led_max);
        end_painted = rgb_runtime_frame_fill(&rgb_runtime_frame_secondary, automouse_end_color_rgb, led_min, led_max);
    } else {
        layer_state_t end_state = rgb_runtime_automouse_stage_state_without_layer(state, auto_mouse_layer);
        end_painted             = rgb_runtime_layer_stage_render_frame(&rgb_runtime_frame_secondary, end_state, led_min, led_max);
        rgb_runtime_automouse_stage_capture_base_effect(&rgb_runtime_frame_base_effect, led_min, led_max);

        for (uint8_t led = led_min; led < led_max; led++) {
            if (rgb_runtime_frame_secondary.painted[led]) {
                continue;
            }

            if (rgb_runtime_automouse_stage_end_mode_is(END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW)) {
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
            rgb_t blended = rgb_runtime_automouse_stage_blend_rgb(rgb_runtime_frame_primary.colors[led], rgb_runtime_frame_secondary.colors[led], blend);
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

#endif
