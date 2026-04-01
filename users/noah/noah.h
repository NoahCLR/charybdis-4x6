// ────────────────────────────────────────────────────────────────────────────
// Noah Userspace Header
// ────────────────────────────────────────────────────────────────────────────
//
// Shared layer and custom-keycode definitions used by the keymap data and
// userspace runtime modules.
//
// Authored behavior tables, macro implementations, combos, and physical
// layouts still live in the keyboard keymap.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H // QMK
#include "lib/key/key_behavior.h"

// ─── Layers ─────────────────────────────────────────────────────────────────
//
// If you add or remove a layer, also update DYNAMIC_KEYMAP_LAYER_COUNT in the
// active keymap's config.h.
enum charybdis_keymap_layers {
    LAYER_BASE = 0, // Default QWERTY typing layer
    LAYER_NUM,      // Numpad on the right half
    LAYER_SYM,      // Symbols, brackets, and DPI controls
    LAYER_NAV,      // Navigation, media, macros, and mouse buttons (sniping enabled)
    LAYER_POINTER,  // Auto-mouse layer: activates on trackball movement, deactivates after timeout
    LAYER_COUNT,
};

#ifdef VIA_ENABLE
_Static_assert(LAYER_COUNT == DYNAMIC_KEYMAP_LAYER_COUNT, "LAYER_COUNT and DYNAMIC_KEYMAP_LAYER_COUNT are out of sync — update the keymap config.h");
#endif

// ─── Custom Keycodes ────────────────────────────────────────────────────────
//
// Custom keycodes are assigned values starting from SAFE_RANGE so they don't
// collide with any built-in QMK or Charybdis keycodes.
//
// MACRO_0–15 are generic macro slots (some used, rest reserved for VIA).
// VOLUME_MODE / BRIGHTNESS_MODE / ARROW_MODE / ZOOM_MODE / PINCH_MODE
// activate pointing-device modes while held.
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
    PINCH_MODE,
    PD_MODE_LOCK_BASE,                                                        // reserves one lock/toggle action per pd-mode keycode — use LOCK_PD_MODE(mode_keycode)
    LAYER_LOCK_BASE     = PD_MODE_LOCK_BASE + (PINCH_MODE - VOLUME_MODE + 1), // reserves LAYER_COUNT keycodes — use LOCK_LAYER(n) macro
    CUSTOM_KEYCODES_END = LAYER_LOCK_BASE + LAYER_COUNT,
};

#define LOCK_PD_MODE(mode_keycode_) (PD_MODE_LOCK_BASE + ((mode_keycode_) - VOLUME_MODE))
#define LOCK_LAYER(layer_) (LAYER_LOCK_BASE + (layer_))

bool                          macro_dispatch(uint16_t keycode);
extern const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS];

// Shared userspace hook implementations. users/noah/noah.c exposes the QMK
// *_user hooks and forwards to these helpers before the optional weak
// *_keymap delegates in keymap.c.
bool           noah_get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record);
bool           noah_process_record_user(uint16_t keycode, keyrecord_t *record);
void           noah_matrix_scan_user(void);
void           noah_keyboard_post_init_user(void);
layer_state_t  noah_layer_state_set_user(layer_state_t state);
report_mouse_t noah_pointing_device_task_user(report_mouse_t mouse_report);
void           noah_pointing_device_init_user(void);
bool           noah_is_mouse_record_user(uint16_t keycode, keyrecord_t *record);
bool           noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max);

// Authored macros and physical keymap layouts live in the keyboard keymap so
// the main QMK entrypoint still shows the actual authored layout.
