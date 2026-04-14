// ────────────────────────────────────────────────────────────────────────────
// RGB Color Configuration
// ────────────────────────────────────────────────────────────────────────────
//
// All authored RGB data tables: layer indicators, mode overlays, LED group
// highlights, and auto-mouse timeout fade config.
//
// Want to change layer, mode, LED-group, automouse, or key-behavior feedback
// colors?  Edit this file.
// Want to change the rendering logic?  Edit users/noah/lib/rgb/core/rgb_runtime.c
// and the stage modules under users/noah/lib/rgb/.
// For shared RGB config types and the HSV helper, see
// users/noah/lib/rgb/core/rgb_config_helpers.h.
// For split-safe LED helper functions, see users/noah/lib/rgb/core/rgb_helpers.h.
//
// ────────────────────────────────────────────────────────────────────────────

#include "noah_keymap.h" // layer enum, PD_MODE_* constants, hsv_t, rgb config types, RGB config helpers

#if defined(RGB_MATRIX_ENABLE)

// ─── Layer colors ───────────────────────────────────────────────────────────
//
// Layer indicator colors and render modes, indexed by layer enum.
// HSV(0, 0, 0) means "no solid color" — LAYER_BASE falls through to the
// default RGB matrix effect. The configured auto-mouse target layer uses its
// authored layer color as the timeout fade start state. In this keymap, that
// target defaults to LAYER_POINTER.
// .mode:
//   - ALL_KEYS = paint the whole layer color wash
//   - KEYS_MAPPED_ON_THIS_LAYER_ONLY = paint only keys that have a real key
//     assigned on that layer; lower active layers remain visible underneath
//     transparent positions
//                       { .color = HSV(hue, sat, val), .mode = ... }
const layer_color_config_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE] =
        {
            .color = HSV(0, 0, 0),
            .mode  = ALL_KEYS,
        }, // no override
    [LAYER_NUM] =
        {
            .color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        }, // green
    [LAYER_SYM] =
        {
            .color = HSV(169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        }, // blue
    [LAYER_NAV] =
        {
            .color = HSV(180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        }, // purple
    [LAYER_POINTER] =
        {
            .color = HSV(0, 0, 150),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        }, // default auto-mouse layer: white mapped keys, capped at v=150 to limit current draw
};

// ─── Pointing device mode colors ────────────────────────────────────────────
//
// Overlay colors for the right half when a trackball mode is active.
// Each entry is tagged with its pointing mode so the order doesn't need to
// match pd_modes[] — adding or reordering modes won't silently break colors.
// { .pointing_mode = ..., .color = HSV(hue, sat, val) }
const pd_mode_color_t pd_mode_colors[] = {
    {
        .pointing_mode = PD_MODE_DRAGSCROLL,
        .color         = HSV(21, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    }, // orange
    {
        .pointing_mode = PD_MODE_VOLUME,
        .color         = HSV(43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    }, // yellow
    {
        .pointing_mode = PD_MODE_BRIGHTNESS,
        .color         = HSV(213, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    }, // magenta
    {
        .pointing_mode = PD_MODE_ARROW,
        .color         = HSV(127, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    }, // cyan
    {
        .pointing_mode = PD_MODE_PINCH,
        .color         = HSV(55, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    }, // lime
    {
        .pointing_mode = PD_MODE_ZOOM,
        .color         = HSV(70, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    }, // light green
};
const uint8_t pd_mode_color_count = (uint8_t)(sizeof(pd_mode_colors) / sizeof(pd_mode_colors[0]));

// ─── Layer LED Groups ───────────────────────────────────────────────────────
//
// Paint specific LEDs a different color when a layer is active. Define an LED
// index array, then add a row to the table.
//
// ╭────────────────────────╮                 ╭────────────────────────╮
//    0   7   8  15  16  20                     49  45  44  37  36  29
// ├────────────────────────┤                 ├────────────────────────┤
//    1   6   9  14  17  21                     50  46  43  38  35  30
// ├────────────────────────┤                 ├────────────────────────┤
//    2   5  10  13  18  22                     51  47  42  39  34  31
// ├────────────────────────┤                 ├────────────────────────┤
//    3   4  11  12  19  23                     52  48  41  40  33  32
// ╰────────────────────────╯                 ╰────────────────────────╯
//                       26  27  28     53  54
//                           25  24     55     (56)
//                     ╰────────────╯ ╰────────╯
//
// Optional examples. Uncomment any LED index lists you want, then uncomment
// the layer_led_groups_data block below plus EXPORT_LAYER_LED_GROUPS(...) to
// enable per-layer highlights. If you leave it commented out, shared defaults
// keep the exported table empty.
//
// static const uint8_t nav_highlight_leds[] = {33, 18};
// static const uint8_t sym_highlight_leds[] = {4, 47};
// static const uint8_t left_thumb_leds[]    = {26, 27, 28, 25, 24};
// static const uint8_t right_thumb_leds[]   = {53, 54, 55};
//
// static const layer_led_group_t layer_led_groups_data[] = {
//     { .layer = LAYER_NAV, .color = HSV(0, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .leds = nav_highlight_leds, .count = ARRAY_SIZE(nav_highlight_leds) },  // red
//     { .layer = LAYER_SYM, .color = HSV(43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .leds = sym_highlight_leds, .count = ARRAY_SIZE(sym_highlight_leds) }  // yellow
// };
// EXPORT_LAYER_LED_GROUPS(layer_led_groups_data);
//
//
// ─── Pointing-Device Mode LED Groups ────────────────────────────────────────
//
// Paint specific LEDs a different color while a pointing mode is active.
//
// Uncomment the pd_mode_led_groups_data block below plus
// EXPORT_PD_MODE_LED_GROUPS(...) if you want one or more per-mode LED
// highlights. If you leave it commented out, shared defaults keep the
// exported table empty.
//
// static const uint8_t trackball_led[] = {56}; // Custom trackball led soldered on the right half, not part of the standard RGB matrix.
//
// static const pd_mode_led_group_t pd_mode_led_groups_data[] = {
//     { .pointing_mode = PD_MODE_VOLUME, .color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .leds = trackball_led, .count = ARRAY_SIZE(trackball_led) }
// };
// EXPORT_PD_MODE_LED_GROUPS(pd_mode_led_groups_data);
//
//
// ─── Auto-mouse timeout fade ────────────────────────────────────────────────
//
// Auto-mouse starts from the authored auto-mouse layer rendering above and
// fades toward the destination chosen below. The first AUTOMOUSE_RGB_DEAD_TIME
// ms are dead time; only the remaining portion of AUTO_MOUSE_TIME animates.
//
// .mode chooses how the fade picks its destination:
//   - FOLLOW_REAL_DESTINATION = land on the real rendered board state after
//     the auto-mouse layer drops out
//   - END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW = keep the real destination where
//     layers paint, but use end_color for keys that would otherwise reveal the
//     base RGB effect
//   - END_COLOR_ON_ALL_KEYS = use end_color as the fade destination on every
//     key while the automouse renderer is active
//
// .end_color is used only by the END_COLOR_* modes above. Later overlays such
// as pd-mode color or key feedback can still repaint on top of the visible
// result.
#    ifdef RGB_AUTOMOUSE_GRADIENT_ENABLE
const automouse_fade_end_config_t automouse_fade_end_config = {
    .mode      = FOLLOW_REAL_DESTINATION,
    .end_color = HSV(0, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
};
#    endif

// ─── Key behavior feedback ──────────────────────────────────────────────────
//
// Visual feedback for the custom key behavior engine state.
//
// Terms used below:
//   - tap index = tap_counts[n] branch (single press, double press, ...)
//   - hold tier = .hold on the winning tap index
//   - long-hold tier = .long_hold on the winning tap index
//
// Feedback rule:
//   - white = multi-tap sequence still resolving which tap index wins
//   - orange = authored hold-tier feedback or hold-tier commit
//   - cyan = authored long-hold-tier feedback or long-hold-tier commit
//   - missing tiers stay quiet; e.g. a long-hold-only surface does not show
//     orange before the long-hold tier commits
//   - the active tier picks the color: .hold always uses orange, .long_hold
//     always uses cyan, regardless of which helper authored that tier
//   - TAP_AT_HOLD_THRESHOLD(...) pulses once when that tier commits
//   - TAP_ON_RELEASE_AFTER_HOLD(...) stays steady while that tier is pending
//     release
//   - PRESS_AND_HOLD_UNTIL_RELEASE(...) and REPEAT_WHILE_HELD(...) flash while
//     that tier stays active
//
// These paint both halves last, on top of the current layer and any pd-mode
// overlay, so authored tier feedback stays visible even on the trackball half
// while a mode color is active.
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    // Neutral white while the engine is still resolving the active tap index.
    .multi_tap_pending_color = HSV(0, 0, 150),

    // Orange for authored hold-tier pending / active states and hold-tier
    // commit pulses.
    .hold_active_color = HSV(18, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),

    // Icy cyan for authored long-hold-tier active states and long-hold-tier
    // commit pulses.
    .long_hold_active_color = HSV(148, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
};
#    endif

#endif // RGB_MATRIX_ENABLE
