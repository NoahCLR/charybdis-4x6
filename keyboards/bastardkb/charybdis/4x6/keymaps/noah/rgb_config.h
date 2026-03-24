// ────────────────────────────────────────────────────────────────────────────
// RGB Color Configuration
// ────────────────────────────────────────────────────────────────────────────
//
// All RGB color definitions: layer indicators, mode overlays, LED group
// highlights, and auto-mouse gradient endpoints.
//
// Want to change a color?  Edit this file.
// Want to change the rendering logic?  Edit keymap.c.
// For the split-safe LED helper functions, see rgb_helpers.h.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_config.h"                // LAYER_COUNT, LAYER_BASE, etc.
#include "lib/pointing_device_modes.h" // PD_MODE_* flags
#include "lib/rgb_helpers.h"           // rgb_set_*, hsv_t, rgb_t

#if defined(RGB_MATRIX_ENABLE)

// ─── Layer colors ───────────────────────────────────────────────────────────
//
// Layer indicator colors (HSV), indexed by layer enum.
// {0,0,0} means "no solid color" — LAYER_BASE falls through to the default
// RGB matrix effect, LAYER_POINTER uses the auto-mouse gradient instead.
//                    {hue, sat, val}
static const hsv_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE]    = {0, 0, 0},                                 // no override
    [LAYER_NUM]     = {85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS},  // green
    [LAYER_LOWER]   = {169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, // blue
    [LAYER_RAISE]   = {180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, // purple
    [LAYER_POINTER] = {0, 0, 0},                                 // no override (gradient)
};

// ─── Pointing device mode colors ────────────────────────────────────────────
//
// Overlay colors for the right half when a trackball mode is active.
// Each entry is tagged with its mode flag so the order doesn't need to
// match pd_modes[] — adding or reordering modes won't silently break colors.
typedef struct {
    uint8_t mode_flag;
    hsv_t   color;
} pd_mode_color_t;

// mode_flag             {hue, sat, val}
static const pd_mode_color_t pd_mode_colors[] = {
    {PD_MODE_DRAGSCROLL, {21, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},  // orange
    {PD_MODE_VOLUME, {43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},      // yellow
    {PD_MODE_BRIGHTNESS, {213, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}}, // magenta
    {PD_MODE_ZOOM, {64, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},        // chartreuse
    {PD_MODE_ARROW, {127, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},      // cyan
};

#    define PD_MODE_COLOR_COUNT (sizeof(pd_mode_colors) / sizeof(pd_mode_colors[0]))

// ─── LED Index Map ──────────────────────────────────────────────────────────
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
//                       26  27  28     53  54  XX
//                           25  24     55  XX
//                     ╰────────────╯ ╰────────────╯

static const uint8_t raise_highlight_leds[]  = {33, 18};
static const uint8_t lower_highlight_leds[]  = {4, 47};
static const uint8_t volume_highlight_leds[] = {29, 36};

// ─── Per-layer LED group highlights ─────────────────────────────────────────
//
// Paint specific LEDs a different color when a layer is active (e.g. to mark
// modifier keys).  Uses the LED index arrays and map above.
// Adding a highlight: define an LED array above, add one line here.

typedef struct {
    uint8_t        layer;
    hsv_t          color;
    const uint8_t *leds;
    uint8_t        count;
} layer_led_group_t;

// layer         {hue, sat, val}                            leds                  count
static const layer_led_group_t layer_led_groups[] = {
    //    {LAYER_RAISE, {0,  255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, raise_highlight_leds, sizeof(raise_highlight_leds)},  // red
    //    {LAYER_LOWER, {43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, lower_highlight_leds, sizeof(lower_highlight_leds)},  // yellow
};

#    define LAYER_LED_GROUP_COUNT (sizeof(layer_led_groups) / sizeof(layer_led_groups[0]))

// ─── Per-mode LED group highlights ────────────────────────────────────────────
//
// Same as layer_led_groups but keyed on pointing device mode_flag.
// Paint specific LEDs a different color when a PD mode is active.// Uses the LED index arrays and map above.
typedef struct {
    uint8_t        mode_flag;
    hsv_t          color;
    const uint8_t *leds;
    uint8_t        count;
} pd_mode_led_group_t;

// mode_flag            {hue, sat, val}                            leds                  count
static const pd_mode_led_group_t pd_mode_led_groups[] = {
    // {PD_MODE_VOLUME, {43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, volume_highlight_leds, sizeof(volume_highlight_leds)},
};

#    define PD_MODE_LED_GROUP_COUNT (sizeof(pd_mode_led_groups) / sizeof(pd_mode_led_groups[0]))

// ─── Auto-mouse gradient ────────────────────────────────────────────────────
//
// Countdown gradient endpoints.  When the auto-mouse layer
// activates, the first 1/3 of AUTO_MOUSE_TIME is "dead time" — LEDs stay
// at the start color so active trackball use doesn't cause flicker.  The
// gradient animates only during the final 2/3 of the auto-mouse timeout.
// See AUTO_MOUSE_TIME in config.h
//
// White is capped at v=150 (not MAX_BRIGHTNESS) to limit current draw — all LEDs
// lit white at full brightness exceeds the USB power budget.
static const hsv_t automouse_color_start = {.h = 0, .s = 0, .v = 150};                             // white
static const hsv_t automouse_color_end   = {.h = 0, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS}; // red

#endif // RGB_MATRIX_ENABLE
