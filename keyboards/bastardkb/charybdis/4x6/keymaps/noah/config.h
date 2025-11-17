/**
 * Copyright 2021 Charly Delay <charly@codesink.dev> (@0xcharly)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

/* ────────────────────────────────
 * Core / feature toggles
 * ──────────────────────────────── */
#ifdef VIA_ENABLE
#    undef DYNAMIC_KEYMAP_LAYER_COUNT
#    define DYNAMIC_KEYMAP_LAYER_COUNT 4
#endif

#ifndef __arm__
/* Save space on non-ARM builds. */
#    define NO_ACTION_ONESHOT
#endif

/* ────────────────────────────────
 * RGB Matrix configuration
 * ──────────────────────────────── */
#ifdef RGB_MATRIX_LED_COUNT
#    undef RGB_MATRIX_LED_COUNT
#endif
#define RGB_MATRIX_LED_COUNT 58 // Total LEDs

#ifdef RGB_MATRIX_SPLIT
#    undef RGB_MATRIX_SPLIT
#endif
#define RGB_MATRIX_SPLIT {29, 29} // Left, Right

/* Keep per-layer sync across halves. */
#ifdef SPLIT_LAYER_STATE_ENABLE
#    undef SPLIT_LAYER_STATE_ENABLE
#endif
#define SPLIT_LAYER_STATE_ENABLE

/* Brightness (cap + default). */
#ifdef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#    undef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#endif
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 150

#ifdef RGB_MATRIX_DEFAULT_VAL
#    undef RGB_MATRIX_DEFAULT_VAL
#endif
#define RGB_MATRIX_DEFAULT_VAL RGB_MATRIX_MAXIMUM_BRIGHTNESS

/* Default solid red. */
#ifdef RGB_MATRIX_DEFAULT_MODE
#    undef RGB_MATRIX_DEFAULT_MODE
#endif
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_COLOR

#ifdef RGB_MATRIX_DEFAULT_HUE
#    undef RGB_MATRIX_DEFAULT_HUE
#endif
#define RGB_MATRIX_DEFAULT_HUE 0 // Red

#ifdef RGB_MATRIX_DEFAULT_SAT
#    undef RGB_MATRIX_DEFAULT_SAT
#endif
#define RGB_MATRIX_DEFAULT_SAT 255 // Full saturation

/* Idle timeout + split-side activity wake. */
#define RGB_MATRIX_TIMEOUT 900000 // ms before auto-off
#define SPLIT_ACTIVITY_ENABLE

/* ────────────────────────────────
 * Pointing device / auto-mouse
 * ──────────────────────────────── */
#ifdef POINTING_DEVICE_ENABLE
/* Auto pointer layer on movement + 16-bit motion reports. */
#    define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    define MOUSE_EXTENDED_REPORT
#endif

#ifdef AUTO_MOUSE_TIME
#    undef AUTO_MOUSE_TIME
#endif
#define AUTO_MOUSE_TIME 1200 // ms to switch back after movement

/* ────────────────────────────────
 * Scroll configuration
 * ──────────────────────────────── */
#define CHARYBDIS_DRAGSCROLL_REVERSE_Y
#define POINTING_DEVICE_HIRES_SCROLL_ENABLE
#define POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER 120
#define POINTING_DEVICE_HIRES_SCROLL_EXPONENT 1
#define CHARYBDIS_DRAGSCROLL_DPI 5
#define CHARYBDIS_DRAGSCROLL_BUFFER_SIZE 6
#define WHEEL_EXTENDED_REPORT true