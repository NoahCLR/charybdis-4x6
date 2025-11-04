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

// --- Brightness cap ---
#ifdef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#    undef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#endif
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 200

// --- Default mode/color (solid red) ---
#ifdef RGB_MATRIX_DEFAULT_MODE
#    undef RGB_MATRIX_DEFAULT_MODE
#endif
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_COLOR

#ifdef RGB_MATRIX_DEFAULT_HUE
#    undef RGB_MATRIX_DEFAULT_HUE
#endif
#define RGB_MATRIX_DEFAULT_HUE 0

#ifdef RGB_MATRIX_DEFAULT_SAT
#    undef RGB_MATRIX_DEFAULT_SAT
#endif
#define RGB_MATRIX_DEFAULT_SAT 255

#ifdef RGB_MATRIX_DEFAULT_VAL
#    undef RGB_MATRIX_DEFAULT_VAL
#endif
#define RGB_MATRIX_DEFAULT_VAL RGB_MATRIX_MAXIMUM_BRIGHTNESS

#ifdef SPLIT_LAYER_STATE_ENABLE
#    undef SPLIT_LAYER_STATE_ENABLE
#endif
#define SPLIT_LAYER_STATE_ENABLE

#ifdef VIA_ENABLE
/* VIA configuration. */
#    define DYNAMIC_KEYMAP_LAYER_COUNT 4
#endif // VIA_ENABLE

#ifndef __arm__
/* Disable unused features. */
#    define NO_ACTION_ONESHOT
#endif // __arm__

/* Charybdis-specific features. */

#ifdef POINTING_DEVICE_ENABLE
// Automatically enable the pointer layer when moving the trackball.  See also:
// - `CHARYBDIS_AUTO_POINTER_LAYER_TRIGGER_TIMEOUT_MS`
// - `CHARYBDIS_AUTO_POINTER_LAYER_TRIGGER_THRESHOLD`
// #define CHARYBDIS_AUTO_POINTER_LAYER_TRIGGER_ENABLE
#endif // POINTING_DEVICE_ENABLE

#define POINTING_DEVICE_AUTO_MOUSE_ENABLE
// Enable automatic mouse movement when on the pointer layer.