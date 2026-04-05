// ────────────────────────────────────────────────────────────────────────────
// RGB Color Configuration
// ────────────────────────────────────────────────────────────────────────────
//
// All RGB color definitions: layer indicators, mode overlays, LED group
// highlights, and auto-mouse gradient endpoints.
//
// Want to change a color?  Edit this file.
// Want to change the rendering logic?  Edit users/noah/lib/rgb/rgb_runtime.c.
// For the split-safe LED helper functions, see users/noah/lib/rgb/rgb_helpers.h.
//
// ────────────────────────────────────────────────────────────────────────────

#include "noah_keymap.h" // layer enum, PD_MODE_* constants, hsv_t, rgb config types

#if defined(RGB_MATRIX_ENABLE)

// ─── Layer colors ───────────────────────────────────────────────────────────
//
// Layer indicator colors (HSV), indexed by layer enum.
// {0,0,0} means "no solid color" — LAYER_BASE falls through to the default
// RGB matrix effect, and the configured auto-mouse target layer uses the
// auto-mouse gradient instead. In this keymap, that target defaults to
// LAYER_POINTER.
//                  {hue, sat, val}
const hsv_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE]    = {0, 0, 0},                                 // no override
    [LAYER_NUM]     = {85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS},  // green
    [LAYER_SYM]     = {169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, // blue
    [LAYER_NAV]     = {180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, // purple
    [LAYER_POINTER] = {0, 0, 0},                                 // default auto-mouse layer: gradient
};

// ─── Pointing device mode colors ────────────────────────────────────────────
//
// Overlay colors for the right half when a trackball mode is active.
// Each entry is tagged with its mode flag so the order doesn't need to
// match pd_modes[] — adding or reordering modes won't silently break colors.
// mode_flag             {hue, sat, val}
const pd_mode_color_t pd_mode_colors[] = {
    {PD_MODE_DRAGSCROLL, {21, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},  // orange
    {PD_MODE_VOLUME, {43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},      // yellow
    {PD_MODE_BRIGHTNESS, {213, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}}, // magenta
    {PD_MODE_ARROW, {127, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},      // cyan
    {PD_MODE_PINCH, {55, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},       // lime
    {PD_MODE_ZOOM, {70, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},        // light green
};
const uint8_t pd_mode_color_count = sizeof(pd_mode_colors) / sizeof(pd_mode_colors[0]);

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

// Reserved example groups. Activate them by uncommenting the table entries below.
// static const uint8_t nav_highlight_leds[] = {33, 18};
// static const uint8_t sym_highlight_leds[] = {4, 47};
// static const uint8_t left_thumb_leds[]    = {26, 27, 28, 25, 24};
// static const uint8_t right_thumb_leds[]   = {53, 54, 55};

// static const uint8_t trackball_led[] = {56}; // Custom trackball led soldered on the right half, not part of the standard RGB matrix.
// Highlight specific LEDs when a keyboard layer is active.
//
// layer                           {hue, sat, val}                            leds                  count
const layer_led_group_t layer_led_groups[] = {
    //    {LAYER_NAV, {0,  255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, nav_highlight_leds, sizeof(nav_highlight_leds)},  // red
    //    {LAYER_SYM, {43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, sym_highlight_leds, sizeof(sym_highlight_leds)},  // yellow
};
const uint8_t layer_led_group_count = sizeof(layer_led_groups) / sizeof(layer_led_groups[0]);

// Same as above, but keyed on pointing device mode instead of layers.
// Active while a trackball mode (volume, zoom, etc.) is being held.
//
// mode_flag            {hue, sat, val}                            leds                  count
const pd_mode_led_group_t pd_mode_led_groups[] = {
    //    {PD_MODE_VOLUME, {85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, trackball_led, sizeof(trackball_led)},
};
const uint8_t pd_mode_led_group_count = sizeof(pd_mode_led_groups) / sizeof(pd_mode_led_groups[0]);

// ─── Auto-mouse gradient ────────────────────────────────────────────────────
//
// Countdown gradient endpoints. When the auto-mouse layer activates, the first
// AUTOMOUSE_RGB_DEAD_TIME ms are "dead time" — LEDs stay at the start color so
// active trackball use doesn't cause flicker. The gradient animates only
// during the remaining portion of the auto-mouse timeout after that dead time.
// See AUTO_MOUSE_TIME and AUTOMOUSE_RGB_DEAD_TIME in config.h.
//
// Split sync only transmits the animated portion of the gradient. Dead time
// and locked modes collapse to a single stable value, which cuts split chatter
// without changing the rendered look. Lower sync steps give a smoother slave-
// half transition; higher values reduce split transactions.
//
// White is capped at v=150 (not MAX_BRIGHTNESS) to limit current draw — all LEDs
// lit white at full brightness exceeds the USB power budget.
#    ifdef RGB_AUTOMOUSE_GRADIENT_ENABLE
const hsv_t automouse_color_start = {.h = 0, .s = 0, .v = 150};                             // white
const hsv_t automouse_color_end   = {.h = 0, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS}; // red
#    endif

// ─── Key behavior feedback ──────────────────────────────────────────────────
//
// Visual feedback for the custom key behavior engine state.
//
// Uses a neutral pending color for open multi-tap windows, a hold-tier color
// for hold pending / hold trigger / active held non-layer actions, and a
// longer-hold-tier color for longer-hold trigger / active longer-hold states.
//
// These paint both halves last, on top of the current layer and any pd-mode
// overlay, so active threshold / hold feedback stays visible even on the
// trackball half while a mode color is active.

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
// Multi-tap pending: neutral white while the tap window is still open.
const hsv_t feedback_multi_tap_pending_color = {.h = 0, .s = 0, .v = 120};

// Hold-tier feedback: bright yellow for strong peripheral-vision visibility.
const hsv_t feedback_hold_active_color = {.h = 43, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS};

// Longer-hold-tier feedback: bright green so the escalated state reads
// immediately distinct from the normal hold tier.
const hsv_t feedback_long_hold_active_color = {.h = 85, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS};
#    endif

#endif // RGB_MATRIX_ENABLE
