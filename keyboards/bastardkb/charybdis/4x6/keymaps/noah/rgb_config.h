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

#include "rgb_helpers.h"

#if defined(RGB_MATRIX_ENABLE)

// ─── Layer colors ───────────────────────────────────────────────────────────
//
// Layer indicator colors (HSV), indexed by layer enum.
// {0,0,0} means "no solid color" — LAYER_BASE falls through to the default
// RGB matrix effect, LAYER_POINTER uses the auto-mouse gradient instead.
static const hsv_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE]    = {0,   0,   0},                                  // no override
    [LAYER_NUM]     = {85,  255, RGB_MATRIX_MAXIMUM_BRIGHTNESS},      // green
    [LAYER_LOWER]   = {169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS},      // blue
    [LAYER_RAISE]   = {180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS},      // purple
    [LAYER_POINTER] = {0,   0,   0},                                  // no override (gradient)
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

static const pd_mode_color_t pd_mode_colors[] = {
    {PD_MODE_DRAGSCROLL,  {21,  255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},  // orange
    {PD_MODE_VOLUME,      {43,  255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},  // yellow
    {PD_MODE_BRIGHTNESS,  {213, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},  // magenta
    {PD_MODE_ZOOM,        {64,  255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},  // chartreuse
    {PD_MODE_ARROW,       {127, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},  // cyan
};

#    define PD_MODE_COLOR_COUNT (sizeof(pd_mode_colors) / sizeof(pd_mode_colors[0]))

// ─── Per-layer LED group highlights ─────────────────────────────────────────
//
// Paint specific LEDs a different color when a layer is active (e.g. to mark
// modifier keys).  Each entry references an LED index array defined above it.
// Adding a highlight: define the LED array, add one line to layer_led_groups[].
static const uint8_t raise_highlight_leds[] = {33, 18};
static const uint8_t lower_highlight_leds[] = {4, 47};

typedef struct {
    uint8_t        layer;
    hsv_t          color;
    const uint8_t *leds;
    uint8_t        count;
} layer_led_group_t;

static const layer_led_group_t layer_led_groups[] = {
    {LAYER_RAISE, {0,  255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, raise_highlight_leds, 2},  // red
    {LAYER_LOWER, {43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}, lower_highlight_leds, 2},  // yellow
};

#    define LAYER_LED_GROUP_COUNT (sizeof(layer_led_groups) / sizeof(layer_led_groups[0]))

// ─── Auto-mouse gradient ────────────────────────────────────────────────────
//
// Countdown gradient endpoints (white → red).
// White is capped at v=150 (not MAX_BRIGHTNESS) to limit current draw — all LEDs
// lit white at full brightness exceeds the USB power budget.
static const hsv_t automouse_color_start = {.h = 0, .s = 0,   .v = 150};                           // white
static const hsv_t automouse_color_end   = {.h = 0, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS}; // red

#endif // RGB_MATRIX_ENABLE
