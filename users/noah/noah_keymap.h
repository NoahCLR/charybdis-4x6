// ────────────────────────────────────────────────────────────────────────────
// Noah Keymap IDs
// ────────────────────────────────────────────────────────────────────────────
//
// Shared layer and custom-keycode declarations used by the keymap-authored
// data tables and by runtime modules that need to interpret those IDs.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H // IWYU pragma: keep

// ─── Layers ─────────────────────────────────────────────────────────────────
//
// Named layer indices (LAYER_BASE, LAYER_NUM, …) and LAYER_COUNT are defined
// in the keymap's config.h as an enum. LAYER_COUNT is the sentinel last value
// and is derived automatically — no manual count to keep in sync.

#ifdef VIA_ENABLE
_Static_assert(LAYER_COUNT == DYNAMIC_KEYMAP_LAYER_COUNT, "LAYER_COUNT and DYNAMIC_KEYMAP_LAYER_COUNT are out of sync — update the keymap config.h");
#endif

#define VIA_MACRO_SLOT_COUNT 16
#ifdef VIA_ENABLE
_Static_assert(VIA_MACRO_SLOT_COUNT == DYNAMIC_KEYMAP_MACRO_COUNT, "VIA_MACRO_SLOT_COUNT and DYNAMIC_KEYMAP_MACRO_COUNT are out of sync");
#endif

// VIA_MACRO_0–15 are authored aliases for VIA's dynamic macro slots.
// The QMK-specific base keycodes stay here so keymap.c can stay declarative.
enum {
    VIA_MACRO_0  = QK_MACRO_0,
    VIA_MACRO_1  = QK_MACRO_0 + 1,
    VIA_MACRO_2  = QK_MACRO_0 + 2,
    VIA_MACRO_3  = QK_MACRO_0 + 3,
    VIA_MACRO_4  = QK_MACRO_0 + 4,
    VIA_MACRO_5  = QK_MACRO_0 + 5,
    VIA_MACRO_6  = QK_MACRO_0 + 6,
    VIA_MACRO_7  = QK_MACRO_0 + 7,
    VIA_MACRO_8  = QK_MACRO_0 + 8,
    VIA_MACRO_9  = QK_MACRO_0 + 9,
    VIA_MACRO_10 = QK_MACRO_0 + 10,
    VIA_MACRO_11 = QK_MACRO_0 + 11,
    VIA_MACRO_12 = QK_MACRO_0 + 12,
    VIA_MACRO_13 = QK_MACRO_0 + 13,
    VIA_MACRO_14 = QK_MACRO_0 + 14,
    VIA_MACRO_15 = QK_MACRO_0 + 15,
};

// ─── Custom Keycodes ────────────────────────────────────────────────────────
//
// Custom keycodes are assigned values starting from SAFE_RANGE so they don't
// collide with any built-in QMK or Charybdis keycodes.
//
// MACRO_0–15 are hardcoded custom macro slots used by key_behaviors[] and
// dispatched by macro_dispatch().
// VIA macros use the VIA_MACRO_0–15 aliases.
// Pointing-device mode taps are authored entirely through key_behaviors[].
// Their single-hold behavior still defaults to momentary mode activation
// unless key_behaviors[] overrides that hold tier explicitly.
// Use LOCK_PD_MODE(mode_keycode) for a persistent toggle inside tap/hold rows.
// PD_MODE_LOCK_BASE reserves one lock/toggle action per pd-mode keycode.
// LAYER_LOCK_BASE reserves LAYER_COUNT keycodes for layer locking via
// actions authored in key_behaviors[]. Use the LOCK_LAYER(n) macro there.
// Keymap-local custom keycodes are declared in keymap.c's
// enum keymap_custom_keycodes. Keep the sentinel there, then add real
// keycodes below it so the first one lands on NOAH_KEYMAP_SAFE_RANGE.
// Those keycodes can be handled in process_record_user() and used directly in
// key_behaviors[] actions such as TAP_SENDS(...).
//
// Example in keymap.c:
//   enum keymap_custom_keycodes {
//       KEYMAP_CUSTOM_KEYCODE_SENTINEL = NOAH_KEYMAP_SAFE_RANGE - 1,
//       MY_CUSTOM_KEY,
//       MY_OTHER_KEY,
//   };

enum custom_keycodes {
    MACRO_0 = SAFE_RANGE,
    MACRO_1,
    MACRO_2,
    MACRO_3,
    MACRO_4,
    MACRO_5,
    MACRO_6,
    MACRO_7,
    MACRO_8,
    MACRO_9,
    MACRO_10,
    MACRO_11,
    MACRO_12,
    MACRO_13,
    MACRO_14,
    MACRO_15,
    VOLUME_MODE,
    BRIGHTNESS_MODE,
    ARROW_MODE,
    ZOOM_MODE,
    DRAGSCROLL,
    PINCH_MODE,
    PD_MODE_LOCK_BASE,                                                        // reserves one lock/toggle action per pd-mode keycode — use LOCK_PD_MODE(mode_keycode)
    LAYER_LOCK_BASE     = PD_MODE_LOCK_BASE + (PD_MODE_LOCK_BASE - VOLUME_MODE), // reserves LAYER_COUNT keycodes — use LOCK_LAYER(n) macro
    CUSTOM_KEYCODES_END = LAYER_LOCK_BASE + LAYER_COUNT,
};

#define PD_MODE_KEYCODE_COUNT (PD_MODE_LOCK_BASE - VOLUME_MODE)
#define HARDCODED_MACRO_SLOT_COUNT ((MACRO_15 - MACRO_0) + 1)
#define LOCK_PD_MODE(mode_keycode_) (PD_MODE_LOCK_BASE + ((mode_keycode_) - VOLUME_MODE))
#define LOCK_LAYER(layer_) (LAYER_LOCK_BASE + (layer_))
#define NOAH_KEYMAP_SAFE_RANGE CUSTOM_KEYCODES_END

bool                          macro_dispatch(uint16_t keycode);
extern const char *const      via_macro_payloads[VIA_MACRO_SLOT_COUNT];
extern const char *const      hardcoded_macro_payloads[HARDCODED_MACRO_SLOT_COUNT];
extern const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS];

// keymap.c uses this header as its authored keymap surface, so re-export the
// shared runtime helpers, authoring schema, and keymap table helpers here.
#include "noah_runtime.h" // IWYU pragma: export
#include "lib/key/key_behavior.h"           // IWYU pragma: export
#include "lib/keymap_materialize.h"         // IWYU pragma: export
#include "lib/pointing/pd_mode_flags.h"     // IWYU pragma: export
#include "lib/rgb/rgb_helpers.h"            // IWYU pragma: export
