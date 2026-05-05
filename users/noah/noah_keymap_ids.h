// ────────────────────────────────────────────────────────────────────────────
// Noah Keymap IDs
// ────────────────────────────────────────────────────────────────────────────
//
// Shared layer ids, userspace keycodes, and materialized authored data symbols
// consumed by runtime modules. This header intentionally excludes authoring
// helpers and hook integration so runtime code does not depend on the broader
// keymap authoring surface.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "lib/pointing/defs/pd_mode_manifest.h"

// ─── Layers ─────────────────────────────────────────────────────────────────
//
// Named layer indices (LAYER_BASE, LAYER_NUM, …) and LAYER_COUNT are defined
// in the keymap's config.h as an enum. LAYER_COUNT is the sentinel last value
// and is derived automatically — no manual count to keep in sync.

#ifdef VIA_ENABLE
_Static_assert(LAYER_COUNT == DYNAMIC_KEYMAP_LAYER_COUNT, "LAYER_COUNT and DYNAMIC_KEYMAP_LAYER_COUNT are out of sync — update the keymap config.h");
#endif

#define VIA_MACRO_SLOT_COUNT 64
#ifdef VIA_ENABLE
_Static_assert(VIA_MACRO_SLOT_COUNT == DYNAMIC_KEYMAP_MACRO_COUNT, "VIA_MACRO_SLOT_COUNT and DYNAMIC_KEYMAP_MACRO_COUNT are out of sync");
#endif

// VIA_MACRO_0–63 are authored aliases for VIA's dynamic macro slots.
// The QMK-specific base keycodes stay here so keymap.c can stay declarative.
enum {
    VIA_MACRO_0  = QK_MACRO_0 + 0,
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
    VIA_MACRO_16 = QK_MACRO_0 + 16,
    VIA_MACRO_17 = QK_MACRO_0 + 17,
    VIA_MACRO_18 = QK_MACRO_0 + 18,
    VIA_MACRO_19 = QK_MACRO_0 + 19,
    VIA_MACRO_20 = QK_MACRO_0 + 20,
    VIA_MACRO_21 = QK_MACRO_0 + 21,
    VIA_MACRO_22 = QK_MACRO_0 + 22,
    VIA_MACRO_23 = QK_MACRO_0 + 23,
    VIA_MACRO_24 = QK_MACRO_0 + 24,
    VIA_MACRO_25 = QK_MACRO_0 + 25,
    VIA_MACRO_26 = QK_MACRO_0 + 26,
    VIA_MACRO_27 = QK_MACRO_0 + 27,
    VIA_MACRO_28 = QK_MACRO_0 + 28,
    VIA_MACRO_29 = QK_MACRO_0 + 29,
    VIA_MACRO_30 = QK_MACRO_0 + 30,
    VIA_MACRO_31 = QK_MACRO_0 + 31,
    VIA_MACRO_32 = QK_MACRO_0 + 32,
    VIA_MACRO_33 = QK_MACRO_0 + 33,
    VIA_MACRO_34 = QK_MACRO_0 + 34,
    VIA_MACRO_35 = QK_MACRO_0 + 35,
    VIA_MACRO_36 = QK_MACRO_0 + 36,
    VIA_MACRO_37 = QK_MACRO_0 + 37,
    VIA_MACRO_38 = QK_MACRO_0 + 38,
    VIA_MACRO_39 = QK_MACRO_0 + 39,
    VIA_MACRO_40 = QK_MACRO_0 + 40,
    VIA_MACRO_41 = QK_MACRO_0 + 41,
    VIA_MACRO_42 = QK_MACRO_0 + 42,
    VIA_MACRO_43 = QK_MACRO_0 + 43,
    VIA_MACRO_44 = QK_MACRO_0 + 44,
    VIA_MACRO_45 = QK_MACRO_0 + 45,
    VIA_MACRO_46 = QK_MACRO_0 + 46,
    VIA_MACRO_47 = QK_MACRO_0 + 47,
    VIA_MACRO_48 = QK_MACRO_0 + 48,
    VIA_MACRO_49 = QK_MACRO_0 + 49,
    VIA_MACRO_50 = QK_MACRO_0 + 50,
    VIA_MACRO_51 = QK_MACRO_0 + 51,
    VIA_MACRO_52 = QK_MACRO_0 + 52,
    VIA_MACRO_53 = QK_MACRO_0 + 53,
    VIA_MACRO_54 = QK_MACRO_0 + 54,
    VIA_MACRO_55 = QK_MACRO_0 + 55,
    VIA_MACRO_56 = QK_MACRO_0 + 56,
    VIA_MACRO_57 = QK_MACRO_0 + 57,
    VIA_MACRO_58 = QK_MACRO_0 + 58,
    VIA_MACRO_59 = QK_MACRO_0 + 59,
    VIA_MACRO_60 = QK_MACRO_0 + 60,
    VIA_MACRO_61 = QK_MACRO_0 + 61,
    VIA_MACRO_62 = QK_MACRO_0 + 62,
    VIA_MACRO_63 = QK_MACRO_0 + 63,
};

_Static_assert((QK_MACRO_0 + VIA_MACRO_SLOT_COUNT - 1) <= QK_MACRO_MAX, "VIA macro slot count exceeds QMK macro keycode range");

// ─── Custom Keycodes ────────────────────────────────────────────────────────
//
// Custom keycodes are assigned values starting from SAFE_RANGE so they don't
// collide with any built-in QMK or Charybdis keycodes.
//
// MACRO_0–15 are hardcoded custom macro slots used by key_behaviors[] and
// dispatched by macro_dispatch().
// VIA macros use the VIA_MACRO_0–63 aliases.
// Plain pointing-device mode keycodes work as default momentary holds.
// Add a key_behaviors[] row when you want those keys to grow explicit tap,
// hold, longer-hold, or multi-tap behavior on top of that default.
// Use the generated *_LOCK keycode for a persistent toggle inside tap/hold rows.
// Each pd mode gets an explicit generated lock keycode, so mode identity no
// longer depends on contiguous enum math.
// LAYER_LOCK_BASE reserves LAYER_COUNT keycodes for layer locking via
// actions authored in key_behaviors[]. Use the LOCK_LAYER(n) macro there.
// Keymap-local custom keycodes are declared in keymap.c's
// enum keymap_custom_keycodes. Keep the sentinel there, then add real
// keycodes below it so the first one lands on NOAH_KEYMAP_SAFE_RANGE.
// Those keycodes can be handled in process_record_user() and used directly in
// key_behaviors[] actions such as TAP_SENDS(...).

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
#define NOAH_PD_MODE_KEYCODE(name, keycode, handler, key_handler, reset, dpi, traits, lifecycle) keycode,
    NOAH_PD_MODE_LIST(NOAH_PD_MODE_KEYCODE)
#undef NOAH_PD_MODE_KEYCODE
#define NOAH_PD_MODE_LOCK_KEYCODE(name, keycode, handler, key_handler, reset, dpi, traits, lifecycle) keycode##_LOCK,
        NOAH_PD_MODE_LIST(NOAH_PD_MODE_LOCK_KEYCODE)
#undef NOAH_PD_MODE_LOCK_KEYCODE
            LAYER_LOCK_BASE,
    CUSTOM_KEYCODES_END = LAYER_LOCK_BASE + LAYER_COUNT,
};

#define PD_MODE_KEYCODE_COUNT PD_MODE_COUNT
#define PD_MODE_LOCK_KEYCODE_COUNT PD_MODE_COUNT
#define HARDCODED_MACRO_SLOT_COUNT ((MACRO_15 - MACRO_0) + 1)
#define LOCK_LAYER(layer_) (LAYER_LOCK_BASE + (layer_))
#define NOAH_KEYMAP_SAFE_RANGE CUSTOM_KEYCODES_END

extern const char *const via_macro_payloads[VIA_MACRO_SLOT_COUNT];
extern const char *const hardcoded_macro_payloads[HARDCODED_MACRO_SLOT_COUNT];
#ifdef COMBO_ENABLE
extern combo_t       key_combos[];
extern const uint8_t noah_combo_count;
#endif
extern const uint16_t *const  noah_combo_output_keycodes;
extern const uint8_t          noah_combo_output_count;
extern const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS];
