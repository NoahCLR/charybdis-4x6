#include "lib/profile/runtime/effective_settings_runtime.h"
// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────
//
// The lighting rule engine's entry point (docs/architecture/lighting-rules-v1.md).
// QMK basic indicators mark actual effect completion (including effects that
// render a whole frame at once). Capture raw driver storage, then compose one
// coherent local frame. Housekeeping returns the driver to raw effect ownership
// after the composed frame was copied for physical transfer. Diagnostics win outright and
// the RGB timeout still turns everything off.
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_runtime.h"
#include "rgb_helpers.h"
#include "rgb_validation.h"
#include "../../state/diagnostics/runtime_diag.h"

#ifdef RGB_MATRIX_ENABLE
#    include <string.h>

#    include "../lighting/lighting_engine.h"
#    include "../lighting/lighting_runtime.h"
#    include "../lighting/lighting_keymap.h"
#    include "../../profile/runtime/effective_rgb_runtime.h"
#    include "../../split/runtime_sync.h"
#    if __has_include("sync_timer.h")
#        include "sync_timer.h" // QMK
#    else
#        define sync_timer_read32() timer_read32()
#    endif
#    if defined(RGB_MATRIX_WS2812)
#        include "ws2812.h" // QMK driver buffer access
#    endif

static noah_lighting_facts_t        rgb_runtime_resolved_facts;
static noah_lighting_frame_t        rgb_runtime_frame;
static noah_lighting_resolve_info_t rgb_runtime_info;
static noah_lighting_scene_t        rgb_runtime_scene;
static uint32_t                     rgb_runtime_now;
static uint32_t                     rgb_runtime_program_generation;
static volatile uint32_t            rgb_runtime_keymap_generation;
static uint32_t                     rgb_runtime_resolved_keymap_generation;
static noah_lighting_leds_t         rgb_runtime_resolved_leds;
static bool                         rgb_runtime_resolved;
static bool                         rgb_runtime_basic_composed;

// This lifecycle is proved for the pinned RP2040/ChibiOS vendor WS2812 loop,
// not a portable delay measured in scans. Host builds exercise its exact order.
#    if !defined(NOAH_HOST_TEST_ENV) && !(defined(QMK_MCU_RP2040) && defined(PROTOCOL_CHIBIOS) && defined(RGB_MATRIX_WS2812) && defined(WS2812_VENDOR) && defined(RGB_MATRIX_SLEEP))
#        error "Review raw-effect flush ownership before using a different RGB driver/loop"
#    endif
#    if defined(RGB_MATRIX_WS2812) && (!defined(NOAH_HOST_TEST_ENV) || defined(RGB_RUNTIME_RENDER_TEST_BACKEND))
#        define NOAH_RGB_RAW_OWNER 1
#    else
#        define NOAH_RGB_RAW_OWNER 0
#    endif
#    if NOAH_RGB_RAW_OWNER
#        ifdef RGB_MATRIX_SPLIT
_Static_assert(RGB_LEFT_LED_COUNT > 0u && RGB_LEFT_LED_COUNT < RGB_MATRIX_LED_COUNT, "both physical halves have local LEDs");
enum { RGB_RUNTIME_RAW_PIXELS = RGB_LEFT_LED_COUNT > RGB_MATRIX_LED_COUNT - RGB_LEFT_LED_COUNT ? RGB_LEFT_LED_COUNT : RGB_MATRIX_LED_COUNT - RGB_LEFT_LED_COUNT };
#        else
enum { RGB_RUNTIME_RAW_PIXELS = RGB_MATRIX_LED_COUNT };
#        endif
_Static_assert(RGB_RUNTIME_RAW_PIXELS <= WS2812_LED_COUNT, "raw cache covers the largest local driver range");
static ws2812_led_t rgb_runtime_raw[RGB_RUNTIME_RAW_PIXELS];
#        if RGB_MATRIX_LED_COUNT == 58 && RGB_LEFT_LED_COUNT == 29 && !defined(WS2812_RGBW)
_Static_assert(sizeof rgb_runtime_raw == 87u, "default target local raw RGB cache; controls accounted separately");
#        endif
// 2: composed this loop; 1: next RGB step flushes or cancels with STARTING.
static uint8_t rgb_runtime_raw_phase;
#    endif

#    ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
static uint16_t rgb_runtime_stage_pipeline_count;
static uint16_t rgb_runtime_early_exit_count;
static uint16_t rgb_runtime_resolve_count;

uint16_t rgb_runtime_test_stage_pipeline_count(void) {
    return rgb_runtime_stage_pipeline_count;
}

uint16_t rgb_runtime_test_early_exit_count(void) {
    return rgb_runtime_early_exit_count;
}

uint16_t rgb_runtime_test_resolve_count(void) {
    return rgb_runtime_resolve_count;
}
#    endif

uint32_t noah_lighting_keymap_generation(void) {
    return rgb_runtime_keymap_generation;
}

#    if defined(RGB_MATRIX_WS2812)
extern ws2812_led_t ws2812_leds[WS2812_LED_COUNT];

// The colour the matrix effect drew this frame, read back from the driver
// buffer before this chunk's lighting writes over it.
bool noah_lighting_effect_colour(uint8_t led, rgb_t *colour) {
    int driver_index = rgb_matrix_led_index(led);

    if (driver_index < 0) return false;
    *colour = (rgb_t){.r = ws2812_leds[driver_index].r, .g = ws2812_leds[driver_index].g, .b = ws2812_leds[driver_index].b};
    return true;
}
#    else
__attribute__((weak)) bool noah_lighting_effect_colour(uint8_t led, rgb_t *colour) {
    (void)led;
    (void)colour;
    return false;
}
#    endif

#    if NOAH_LIGHTING_EXTENSION_TAP_COMMIT_FLASH
key_feedback_tap_commit_mode_t key_feedback_tap_commit_mode(void) {
    return noah_effective_lighting_program(NULL)->tap_commit_flash ? KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS : KEY_FEEDBACK_TAP_COMMIT_OFF;
}
#    endif

static noah_lighting_leds_t rgb_runtime_local_leds(void) {
#    ifdef RGB_MATRIX_SPLIT
    return is_keyboard_left() ? NOAH_LIGHTING_LEFT_LEDS : NOAH_LIGHTING_RIGHT_LEDS;
#    else
    return NOAH_LIGHTING_ALL_LEDS;
#    endif
}

// Each half collects the facts both halves share; the USB half also collects
// the synced ones, which the other half reads from the link. A read that finds
// the link mid-publication keeps the last coherent frame.
#    if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
noah_lighting_facts_t *noah_rgb_runtime_history_facts(void){return &rgb_runtime_resolved_facts;}
#    endif
#    if NOAH_RGB_RAW_OWNER
static void rgb_runtime_raw_capture(void) {
    // A second completion before housekeeping would violate the pinned loop.
    if (rgb_runtime_raw_phase) __builtin_trap();
    uint8_t first = 0u, count = RGB_MATRIX_LED_COUNT;
#        ifdef RGB_MATRIX_SPLIT
    first = is_keyboard_left() ? 0u : RGB_LEFT_LED_COUNT;
    count = is_keyboard_left() ? RGB_LEFT_LED_COUNT : RGB_MATRIX_LED_COUNT - RGB_LEFT_LED_COUNT;
#        endif
    for (uint8_t i = 0; i < count; i++) {
        int index = rgb_matrix_led_index(first + i);
        if (index < 0 || index >= WS2812_LED_COUNT) __builtin_trap();
        rgb_runtime_raw[i] = ws2812_leds[index];
    }
    rgb_runtime_raw_phase = 2u;
}
#    endif
static bool rgb_runtime_capture_facts(void) {
#    if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
    if(noah_lighting_runtime_remote_scene())return false;
#    endif
    if (is_keyboard_master()) {
        return noah_lighting_facts_collect_changed(&rgb_runtime_resolved_facts, true, true);
    }
    bool changed = noah_lighting_facts_collect_changed(&rgb_runtime_resolved_facts, true, false);
#    if NOAH_LIGHTING_HAS_SYNCED_FACTS
    changed |= split_runtime_sync_remote_collect_lighting(&rgb_runtime_resolved_facts);
#    endif
    return changed;
}

static void rgb_runtime_begin_frame(void) {
    noah_lighting_runtime_sync_role();
    noah_lighting_leds_t leds = rgb_runtime_local_leds();
    uint32_t             generation;
    bool                 wake_due;
    uint32_t             keymap_generation;

    bool facts_changed = rgb_runtime_capture_facts();
    rgb_runtime_scene.program = noah_effective_lighting_program(&generation);
    rgb_runtime_now           = timer_read32();
    rgb_runtime_scene.paint_time = sync_timer_read32();
    if (generation != rgb_runtime_program_generation) {
        rgb_runtime_program_generation = generation;
        rgb_runtime_resolved           = false;
        noah_lighting_engine_program_changed();
#    if NOAH_LIGHTING_KEYMAP_CACHE
        noah_lighting_keymap_invalidate();
#    endif
    }
    keymap_generation = rgb_runtime_keymap_generation;
#    if NOAH_LIGHTING_KEYMAP_CACHE
    if (!noah_lighting_keymap_refresh(keymap_generation)) {
        rgb_runtime_resolved = false; // keep old pixels, never rerender with an incomplete basis
        return;
    }
#    endif
#    if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
    rgb_runtime_scene.facts=&rgb_runtime_resolved_facts;
    if(!noah_lighting_runtime_prepare(&rgb_runtime_scene,generation,rgb_runtime_now))return;
    bool history_changed=rgb_runtime_scene.working && rgb_runtime_scene.working->render_dirty;
#    else
    bool history_changed=false;
#    endif
    wake_due = rgb_runtime_info.wake_set && (int32_t)(rgb_runtime_now - rgb_runtime_info.wake) >= 0;
    if (rgb_runtime_resolved && !rgb_runtime_info.live && !rgb_runtime_info.effect_used && !wake_due && rgb_runtime_resolved_leds == leds && rgb_runtime_resolved_keymap_generation == keymap_generation && !facts_changed && !history_changed) return;

    rgb_runtime_resolved_keymap_generation = keymap_generation;
    rgb_runtime_scene.facts                = &rgb_runtime_resolved_facts;
    rgb_runtime_scene.keymap_generation    = keymap_generation;
    noah_lighting_resolve(&rgb_runtime_scene, leds, rgb_runtime_now, true, &rgb_runtime_frame, &rgb_runtime_info);
#    if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
    noah_lighting_runtime_resolved();
#    endif
    rgb_runtime_resolved_leds = leds;
    rgb_runtime_resolved      = true;
#    ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
    rgb_runtime_resolve_count++;
#    endif
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

#    define RGB_RUNTIME_BOOT_INDICATOR_HSV ((hsv_t){.h = 0u, .s = 0u, .v = 150u})

static bool rgb_runtime_render_range(uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!rgb_runtime_normalize_local_range(&led_min, &led_max)) {
#    ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
        rgb_runtime_early_exit_count++;
#    endif
        return false;
    }

    uint8_t local_min = 0u, local_max = RGB_MATRIX_LED_COUNT;
    (void)rgb_runtime_normalize_local_range(&local_min, &local_max);
    // Production BASIC supplies the complete local range on true effect
    // completion. Preserve the independent advanced entry's legacy final-range
    // behavior; production never infers completion from that range.
    if (led_max != local_max) return false;
    led_min = local_min;
    rgb_runtime_begin_frame();

#    ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
    rgb_runtime_stage_pipeline_count++;
#    endif

    if (noah_runtime_diag_indicator_active()) {
        rgb_set_both_halves(hsv_to_rgb(RGB_RUNTIME_BOOT_INDICATOR_HSV), led_min, led_max);
        return true;
    }

    for (uint8_t led = led_min; led < led_max; led++) {
        if ((rgb_runtime_frame.painted & NOAH_LIGHTING_LED_BIT(led)) == 0u) continue;
        rgb_set_led_color(led, led_min, led_max, rgb_runtime_frame.colour[led]);
        painted = true;
    }

#    ifdef NOAH_PORTABLE_PROFILE_ENABLE
    uint32_t timeout = noah_setting(NOAH_SETTING_RGB_TIMEOUT, 900000);
    if (timeout && last_input_activity_elapsed() > timeout) {
        for (uint8_t led = led_min; led < led_max; led++)
            rgb_matrix_set_color(led, 0, 0, 0);
        return true;
    }
#    endif
    return painted;
}
#endif // RGB_MATRIX_ENABLE

void noah_rgb_runtime_invalidate_layer_maps(void) {
#ifdef RGB_MATRIX_ENABLE
    rgb_runtime_keymap_generation++;
#endif
}

void noah_rgb_runtime_post_init(void) {
#ifdef RGB_MATRIX_ENABLE
    rgb_runtime_basic_composed = false;
#    if NOAH_RGB_RAW_OWNER
    rgb_runtime_raw_phase = 0u; // fresh driver/keyboard initialization only
#    endif
    // Initialize the actual compiled program/history before the first input.
    (void)noah_effective_lighting_program(NULL);
    noah_lighting_runtime_sync_role();
    noah_rgb_validate_config();
    // Until the other half hears its first frame, the synced facts are at rest.
    noah_lighting_facts_collect(&rgb_runtime_resolved_facts, true, false);
    noah_lighting_facts_init_synced(&rgb_runtime_resolved_facts);
    rgb_runtime_resolved  = false;
    rgb_runtime_keymap_generation++;
    memset(&rgb_runtime_info, 0, sizeof(rgb_runtime_info));
#    ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
    rgb_runtime_stage_pipeline_count = 0u;
    rgb_runtime_early_exit_count     = 0u;
    rgb_runtime_resolve_count        = 0u;
#    endif
#endif
}

void noah_rgb_runtime_suspend(void) {
#if defined(RGB_MATRIX_ENABLE) && NOAH_RGB_RAW_OWNER
    // Pinned suspend_power_down_quantum calls the userspace hook BEFORE its
    // synchronous NONE render/flush. That complete black write supersedes the
    // old composed frame/cache even if resume precedes ordinary housekeeping.
    rgb_runtime_raw_phase = 0u;
#endif
}
void noah_rgb_runtime_housekeeping(void) {
#if defined(RGB_MATRIX_ENABLE) && NOAH_RGB_RAW_OWNER
    // Slave transactions apply one suspend observation before this loop's RGB
    // step. A true observation synchronously copied black for physical output;
    // the older raw cache must not overwrite that newer complete driver write.
    if (rgb_matrix_get_suspend_state()) { rgb_runtime_raw_phase = 0u; return; }
    if (rgb_runtime_raw_phase == 2u) {
        rgb_runtime_raw_phase = 1u; // SAME-loop housekeeping: never restore yet
        return;
    }
    if (rgb_runtime_raw_phase != 1u) return;
    // The following keyboard_task has either flushed the composed bytes into
    // WS2812_BUFFER or a mode/enable setter replaced FLUSHING by STARTING,
    // which performs no effect writes. USB suspend stalls both ordinary calls;
    // its synchronous NONE flush also copies before restoration. No cache is
    // consumed by DMA: ws2812_flush packs into its separate transfer buffer.
    uint8_t first = 0u, count = RGB_MATRIX_LED_COUNT;
#    ifdef RGB_MATRIX_SPLIT
    first = is_keyboard_left() ? 0u : RGB_LEFT_LED_COUNT;
    count = is_keyboard_left() ? RGB_LEFT_LED_COUNT : RGB_MATRIX_LED_COUNT - RGB_LEFT_LED_COUNT;
#    endif
    for (uint8_t i = 0; i < count; i++) {
        int index = rgb_matrix_led_index(first + i);
        if (index < 0 || index >= WS2812_LED_COUNT) __builtin_trap();
        ws2812_leds[index] = rgb_runtime_raw[i];
    }
    rgb_runtime_raw_phase = 0u;
#endif
}

bool noah_rgb_matrix_indicators_user(void) {
#ifdef RGB_MATRIX_ENABLE
#    if NOAH_RGB_RAW_OWNER
    rgb_runtime_raw_capture(); // all local raw pixels, including unpainted ones
#    endif
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER);
    bool painted = rgb_runtime_render_range(0u, RGB_MATRIX_LED_COUNT);
    noah_runtime_diag_scope_leave();
    rgb_runtime_basic_composed = true;
    return painted;
#else
    return true;
#endif
}

#ifdef RGB_MATRIX_ENABLE
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    // Pinned QMK invokes advanced immediately after the completion hook,
    // even for whole-frame effects whose supplied range is only iter0 (and
    // can be wholly outside the right half). Suppress it BEFORE range checks.
    if (rgb_runtime_basic_composed) {
        rgb_runtime_basic_composed = false;
        return false;
    }
    bool painted;

    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER);
    painted = rgb_runtime_render_range(led_min, led_max);
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
