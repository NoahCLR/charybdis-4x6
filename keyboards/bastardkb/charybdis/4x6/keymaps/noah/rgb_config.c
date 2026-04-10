// ────────────────────────────────────────────────────────────────────────────
// RGB Color Configuration
// ────────────────────────────────────────────────────────────────────────────
//
// All RGB color definitions: layer indicators, mode overlays, LED group
// highlights, and auto-mouse timeout fade config.
//
// Want to change a color?  Edit this file.
// Want to change the rendering logic?  Edit users/noah/lib/rgb/rgb_runtime.c.
// For config helper macros, see users/noah/lib/rgb/rgb_config_helpers.h.
// For split-safe LED helper functions, see users/noah/lib/rgb/rgb_helpers.h.
//
// ────────────────────────────────────────────────────────────────────────────

#include "noah_keymap.h" // layer enum, PD_MODE_* constants, hsv_t, rgb config types
#include "users/noah/lib/rgb/rgb_config_helpers.h"

#if defined(RGB_MATRIX_ENABLE)

// ─── Layer colors ───────────────────────────────────────────────────────────
//
// Layer indicator colors and render flags, indexed by layer enum.
// .color = {0,0,0} means "no solid color" — LAYER_BASE falls through to the
// default RGB matrix effect. The configured auto-mouse target layer uses its
// authored layer color as the timeout fade start state. In this keymap, that
// target defaults to LAYER_POINTER.
// .flags:
//   - ALL_KEYS = paint the whole layer color wash
//   - KEYS_MAPPED_ON_THIS_LAYER_ONLY = paint only keys that have a real key
//     assigned on that layer; lower active layers remain visible underneath
//     transparent positions
//                       {.color = {hue, sat, val}, .flags = ...}
const layer_color_config_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE]    = {.color = {0, 0, 0}, .flags = ALL_KEYS},                                                       // no override
    [LAYER_NUM]     = {.color = {85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, .flags = KEYS_MAPPED_ON_THIS_LAYER_ONLY},  // green
    [LAYER_SYM]     = {.color = {169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, .flags = KEYS_MAPPED_ON_THIS_LAYER_ONLY}, // blue
    [LAYER_NAV]     = {.color = {180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, .flags = KEYS_MAPPED_ON_THIS_LAYER_ONLY}, // purple
    [LAYER_POINTER] = {.color = {0, 0, 150}, .flags = KEYS_MAPPED_ON_THIS_LAYER_ONLY},                               // default auto-mouse layer: white mapped keys, capped at v=150 to limit current draw
};

// ─── Pointing device mode colors ────────────────────────────────────────────
//
// Overlay colors for the right half when a trackball mode is active.
// Each entry is tagged with its mode flag so the order doesn't need to
// match pd_modes[] — adding or reordering modes won't silently break colors.
// mode_flag             {hue, sat, val}
DEFINE_PD_MODE_COLORS(
    {PD_MODE_DRAGSCROLL, {21, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},  // orange
    {PD_MODE_VOLUME, {43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},      // yellow
    {PD_MODE_BRIGHTNESS, {213, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}}, // magenta
    {PD_MODE_ARROW, {127, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},      // cyan
    {PD_MODE_PINCH, {55, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},       // lime
    {PD_MODE_ZOOM, {70, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},        // light green
);

// ─── Per-layer LED group highlights ─────────────────────────────────────────
//
// Paint specific LEDs a different color when a layer is active (e.g. to mark
// modifier keys).  Define an LED index array, then add a row to the table.
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

// Optional examples. Uncomment any LED index lists you want, then uncomment
// DEFINE_LAYER_LED_GROUPS(...) below to enable per-layer highlights. If you
// leave the definition commented out, shared defaults keep the exported table
// empty.
// static const uint8_t nav_highlight_leds[] = {33, 18};
// static const uint8_t sym_highlight_leds[] = {4, 47};
// static const uint8_t left_thumb_leds[]    = {26, 27, 28, 25, 24};
// static const uint8_t right_thumb_leds[]   = {53, 54, 55};

// static const uint8_t trackball_led[] = {56}; // Custom trackball led soldered on the right half, not part of the standard RGB matrix.
// Highlight specific LEDs when a keyboard layer is active.
//
// layer                           {hue, sat, val}                            leds                  count
// DEFINE_LAYER_LED_GROUPS(
//     {LAYER_NAV, {0,  255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, nav_highlight_leds, sizeof(nav_highlight_leds)},  // red
//     {LAYER_SYM, {43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, sym_highlight_leds, sizeof(sym_highlight_leds)},  // yellow
// );

// Same as above, but keyed on pointing device mode instead of layers.
// Active while a trackball mode (volume, zoom, etc.) is active.
//
// mode_flag            {hue, sat, val}                            leds                  count
// Uncomment DEFINE_PD_MODE_LED_GROUPS(...) below if you want one or more
// per-mode LED highlights. If you leave it commented out, shared defaults keep
// the exported table empty.
// DEFINE_PD_MODE_LED_GROUPS(
//     {PD_MODE_VOLUME, {85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, trackball_led, sizeof(trackball_led)},
// );

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
    .end_color = {.h = 0, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS},
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
//   - primary momentary layer access uses the ambient layer color itself
//
// These paint both halves last, on top of the current layer and any pd-mode
// overlay, so authored tier feedback stays visible even on the trackball half
// while a mode color is active.

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
// Sequence pending: neutral white while the engine is still resolving the
// active tap index.
const hsv_t feedback_multi_tap_pending_color = {.h = 0, .s = 0, .v = 150};

// Hold-tier feedback: orange for authored hold-tier pending / active states
// and hold-tier commit pulses.
const hsv_t feedback_hold_active_color = {.h = 18, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS};

// Long-hold-tier feedback: icy cyan for authored long-hold-tier active states
// and long-hold-tier commit pulses; immediately distinct from the hold tier.
const hsv_t feedback_long_hold_active_color = {.h = 148, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS};
#    endif

#endif // RGB_MATRIX_ENABLE
