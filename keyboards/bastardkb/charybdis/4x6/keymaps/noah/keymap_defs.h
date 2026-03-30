// ────────────────────────────────────────────────────────────────────────────
// Shared Keymap Declarations
// ────────────────────────────────────────────────────────────────────────────
//
// This header exposes the shared layer and custom-keycode definitions used by
// the keymap data and runtime modules.
//
// Authored behavior tables, macro implementations, combos, and physical
// layouts live in keymap.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H // QMK

// ─── Layers ─────────────────────────────────────────────────────────────────
//
// If you add or remove a layer, also update DYNAMIC_KEYMAP_LAYER_COUNT in config.h.
enum charybdis_keymap_layers {
    LAYER_BASE = 0, // Default QWERTY typing layer
    LAYER_NUM,      // Numpad on the right half
    LAYER_LOWER,    // Symbols and DPI controls
    LAYER_RAISE,    // Navigation, media, and mouse buttons (sniping enabled)
    LAYER_POINTER,  // Auto-mouse layer: activates on trackball movement, deactivates after timeout
    LAYER_COUNT,
};

#ifdef VIA_ENABLE
_Static_assert(LAYER_COUNT == DYNAMIC_KEYMAP_LAYER_COUNT, "LAYER_COUNT and DYNAMIC_KEYMAP_LAYER_COUNT are out of sync — update config.h");
#endif

// ─── Custom Keycodes ────────────────────────────────────────────────────────
//
// Custom keycodes are assigned values starting from SAFE_RANGE so they don't
// collide with any built-in QMK or Charybdis keycodes.
//
// MACRO_0–15 are generic macro slots (some used, rest reserved for VIA).
// VOLUME_MODE / BRIGHTNESS_MODE / ARROW_MODE / ZOOM_MODE activate pointing
// device modes while held.
// DRAGSCROLL: tap = base-layer key, double-tap = lock dragscroll, hold = momentary.
// The hold path is handled by the pointing-device mode key runtime.
// Pointing-device modes can dispatch LOCK_PD_MODE(mode_keycode) via the
// key_behaviors[] multi-tap system.
// PD_MODE_LOCK_BASE reserves one lock/toggle action per pd-mode keycode.
// LAYER_LOCK_BASE reserves LAYER_COUNT keycodes for layer locking via
// actions authored in key_behaviors[].  Use the LOCK_LAYER(n) macro there.

enum custom_keycodes {
    MACRO_0 = SAFE_RANGE,
    MACRO_1,
    MACRO_2,
    MACRO_3,
    MACRO_4,
    MACRO_5,
    MACRO_6,  // reserved for VIA
    MACRO_7,  // reserved for VIA
    MACRO_8,  // reserved for VIA
    MACRO_9,  // reserved for VIA
    MACRO_10, // reserved for VIA
    MACRO_11, // reserved for VIA
    MACRO_12, // reserved for VIA
    MACRO_13, // reserved for VIA
    MACRO_14, // reserved for VIA
    MACRO_15, // reserved for VIA
    VOLUME_MODE,
    BRIGHTNESS_MODE,
    ARROW_MODE,
    ZOOM_MODE,
    DRAGSCROLL,
    PD_MODE_LOCK_BASE,                                                        // reserves one lock/toggle action per pd-mode keycode — use LOCK_PD_MODE(mode_keycode)
    LAYER_LOCK_BASE     = PD_MODE_LOCK_BASE + (DRAGSCROLL - VOLUME_MODE + 1), // reserves LAYER_COUNT keycodes — use LOCK_LAYER(n) macro
    CUSTOM_KEYCODES_END = LAYER_LOCK_BASE + LAYER_COUNT,
};

#define LOCK_PD_MODE(mode_keycode_) (PD_MODE_LOCK_BASE + ((mode_keycode_) - VOLUME_MODE))
#define LOCK_LAYER(layer_) (LAYER_LOCK_BASE + (layer_))

bool                          macro_dispatch(uint16_t keycode);
extern const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS];

// Authored key behavior data, macro implementations, combo definitions, and
// physical keymap layouts live in keymap.c so the main QMK entrypoint shows
// the actual authored keymap.
