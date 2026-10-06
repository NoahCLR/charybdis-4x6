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
// The names Charybdis Ark shows until a layer, macro or custom key is renamed
// there: a layer name holds 23 UTF-8 bytes, a macro or custom-key name 20
// printable ASCII characters, each followed by its terminator.
#define NOAH_LAYER_NAME_SIZE 24
#define NOAH_MACRO_NAME_SIZE 21
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

// ─── Userspace Keycodes ─────────────────────────────────────────────────────
//
// QMK's user range starts at SAFE_RANGE (0x7e40). Each family owns one fixed,
// aligned block, reserved beyond what is supported today, so adding pointing
// modes or layers never moves another keycode:
//
//   0x7e40–0x7e7f  CUSTOM_KEY_0–63      named keys that do what their behaviour says
//   0x7e80–0x7e9f  PD_SLOT_n            hold pointing slot n    (32, all used)
//   0x7ea0–0x7ebf  PD_SLOT_n_LOCK       toggle pointing slot n  (32, all used)
//   0x7ec0–0x7edf  LOCK_LAYER(n)        toggle layer n's lock   (32 reserved)
//   0x7ee0–0x7fff  unassigned: inert, refused as a step or combo output
//
// A custom key does nothing by itself: give it a key_behaviors[] row and place
// it on a layer or as a combo output. Its name comes from CUSTOM_KEYS in
// keymap.c until Charybdis Ark renames it. A behaviour step cannot send one.
// PD_SLOT_n holds a configurable pointing slot; add a key_behaviors[] row for
// explicit tap, hold, longer-hold or multi-tap behavior on top of that default.
// LOCK_LAYER(n) toggles a layer lock from actions authored in key_behaviors[].

#define NOAH_KEYCODE_CUSTOM_KEY_BASE (SAFE_RANGE + 0x00)
#define NOAH_KEYCODE_PD_HOLD_BASE (SAFE_RANGE + 0x40)
#define NOAH_KEYCODE_PD_LOCK_BASE (SAFE_RANGE + 0x60)
#define NOAH_KEYCODE_LAYER_LOCK_BASE (SAFE_RANGE + 0x80)
#define NOAH_KEYCODE_USERSPACE_END (SAFE_RANGE + 0xa0)
#define NOAH_KEYCODE_CUSTOM_KEY_RESERVED 64
#define NOAH_KEYCODE_PD_RESERVED 32
#define NOAH_KEYCODE_LAYER_LOCK_RESERVED 32
#define CUSTOM_KEY_SLOT_COUNT 64

enum {
    CUSTOM_KEY_0  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 0,
    CUSTOM_KEY_1  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 1,
    CUSTOM_KEY_2  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 2,
    CUSTOM_KEY_3  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 3,
    CUSTOM_KEY_4  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 4,
    CUSTOM_KEY_5  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 5,
    CUSTOM_KEY_6  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 6,
    CUSTOM_KEY_7  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 7,
    CUSTOM_KEY_8  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 8,
    CUSTOM_KEY_9  = NOAH_KEYCODE_CUSTOM_KEY_BASE + 9,
    CUSTOM_KEY_10 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 10,
    CUSTOM_KEY_11 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 11,
    CUSTOM_KEY_12 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 12,
    CUSTOM_KEY_13 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 13,
    CUSTOM_KEY_14 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 14,
    CUSTOM_KEY_15 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 15,
    CUSTOM_KEY_16 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 16,
    CUSTOM_KEY_17 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 17,
    CUSTOM_KEY_18 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 18,
    CUSTOM_KEY_19 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 19,
    CUSTOM_KEY_20 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 20,
    CUSTOM_KEY_21 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 21,
    CUSTOM_KEY_22 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 22,
    CUSTOM_KEY_23 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 23,
    CUSTOM_KEY_24 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 24,
    CUSTOM_KEY_25 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 25,
    CUSTOM_KEY_26 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 26,
    CUSTOM_KEY_27 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 27,
    CUSTOM_KEY_28 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 28,
    CUSTOM_KEY_29 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 29,
    CUSTOM_KEY_30 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 30,
    CUSTOM_KEY_31 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 31,
    CUSTOM_KEY_32 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 32,
    CUSTOM_KEY_33 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 33,
    CUSTOM_KEY_34 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 34,
    CUSTOM_KEY_35 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 35,
    CUSTOM_KEY_36 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 36,
    CUSTOM_KEY_37 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 37,
    CUSTOM_KEY_38 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 38,
    CUSTOM_KEY_39 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 39,
    CUSTOM_KEY_40 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 40,
    CUSTOM_KEY_41 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 41,
    CUSTOM_KEY_42 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 42,
    CUSTOM_KEY_43 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 43,
    CUSTOM_KEY_44 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 44,
    CUSTOM_KEY_45 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 45,
    CUSTOM_KEY_46 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 46,
    CUSTOM_KEY_47 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 47,
    CUSTOM_KEY_48 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 48,
    CUSTOM_KEY_49 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 49,
    CUSTOM_KEY_50 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 50,
    CUSTOM_KEY_51 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 51,
    CUSTOM_KEY_52 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 52,
    CUSTOM_KEY_53 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 53,
    CUSTOM_KEY_54 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 54,
    CUSTOM_KEY_55 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 55,
    CUSTOM_KEY_56 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 56,
    CUSTOM_KEY_57 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 57,
    CUSTOM_KEY_58 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 58,
    CUSTOM_KEY_59 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 59,
    CUSTOM_KEY_60 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 60,
    CUSTOM_KEY_61 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 61,
    CUSTOM_KEY_62 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 62,
    CUSTOM_KEY_63 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 63,
};

enum {
    NOAH_KEYCODE_PD_HOLD_BEFORE = NOAH_KEYCODE_PD_HOLD_BASE - 1,
#define NOAH_PD_MODE_KEYCODE(name, keycode) keycode,
    NOAH_PD_MODE_LIST(NOAH_PD_MODE_KEYCODE)
#undef NOAH_PD_MODE_KEYCODE
};
enum {
    NOAH_KEYCODE_PD_LOCK_BEFORE = NOAH_KEYCODE_PD_LOCK_BASE - 1,
#define NOAH_PD_MODE_LOCK_KEYCODE(name, keycode) keycode##_LOCK,
    NOAH_PD_MODE_LIST(NOAH_PD_MODE_LOCK_KEYCODE)
#undef NOAH_PD_MODE_LOCK_KEYCODE
};
#define LAYER_LOCK_BASE NOAH_KEYCODE_LAYER_LOCK_BASE

_Static_assert(NOAH_KEYCODE_CUSTOM_KEY_BASE == 0x7e40 && NOAH_KEYCODE_PD_HOLD_BASE == 0x7e80 && NOAH_KEYCODE_PD_LOCK_BASE == 0x7ea0 && NOAH_KEYCODE_LAYER_LOCK_BASE == 0x7ec0, "userspace keycode blocks are the action ABI");
_Static_assert(CUSTOM_KEY_SLOT_COUNT <= NOAH_KEYCODE_CUSTOM_KEY_RESERVED && NOAH_KEYCODE_CUSTOM_KEY_BASE + NOAH_KEYCODE_CUSTOM_KEY_RESERVED == NOAH_KEYCODE_PD_HOLD_BASE, "custom keys fill their block");
_Static_assert(PD_SLOT_0 == 0x7e80 && PD_SLOT_31 == 0x7e9f && PD_SLOT_31 - PD_SLOT_0 + 1 <= NOAH_KEYCODE_PD_RESERVED && NOAH_KEYCODE_PD_HOLD_BASE + NOAH_KEYCODE_PD_RESERVED == NOAH_KEYCODE_PD_LOCK_BASE, "pointing holds stay inside their block");
_Static_assert(PD_SLOT_0_LOCK == 0x7ea0 && PD_SLOT_31_LOCK == 0x7ebf && PD_SLOT_31_LOCK - PD_SLOT_0_LOCK + 1 <= NOAH_KEYCODE_PD_RESERVED && NOAH_KEYCODE_PD_LOCK_BASE + NOAH_KEYCODE_PD_RESERVED == NOAH_KEYCODE_LAYER_LOCK_BASE, "pointing locks stay inside their block");
_Static_assert(LAYER_COUNT <= NOAH_KEYCODE_LAYER_LOCK_RESERVED && NOAH_KEYCODE_LAYER_LOCK_BASE + NOAH_KEYCODE_LAYER_LOCK_RESERVED == NOAH_KEYCODE_USERSPACE_END, "layer locks stay inside their block");
_Static_assert(NOAH_KEYCODE_USERSPACE_END - 1 <= QK_USER_MAX, "userspace keycodes stay in QMK's user range");

#define PD_MODE_KEYCODE_COUNT PD_MODE_COUNT
#define PD_MODE_LOCK_KEYCODE_COUNT PD_MODE_COUNT
#define LOCK_LAYER(layer_) (LAYER_LOCK_BASE + (layer_))
#define NOAH_KEYCODE_IS_CUSTOM_KEY(keycode_) ((keycode_) >= CUSTOM_KEY_0 && (keycode_) <= CUSTOM_KEY_63)

extern const char *const via_macro_payloads[VIA_MACRO_SLOT_COUNT];
extern const char        via_macro_names[VIA_MACRO_SLOT_COUNT][NOAH_MACRO_NAME_SIZE];
extern const char        custom_key_names[CUSTOM_KEY_SLOT_COUNT][NOAH_MACRO_NAME_SIZE];
extern const char        layer_names[LAYER_COUNT][NOAH_LAYER_NAME_SIZE];
#ifdef COMBO_ENABLE
extern combo_t       key_combos[];
extern const uint8_t noah_combo_count;
// Each combo's own window in ms; zero follows COMBO_TERM.
extern const uint16_t noah_combo_terms[];
#endif
extern const uint16_t *const  noah_combo_output_keycodes;
extern const uint8_t          noah_combo_output_count;
extern const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS];
