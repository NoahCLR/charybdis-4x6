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

#include "lib/pointing/defs/pointing_mode_manifest.h"
#include "lib/action/custom_key_shortcut.h"

// ─── Layers ─────────────────────────────────────────────────────────────────
//
// Named layer indices (LAYER_BASE, LAYER_NUM, …) and LAYER_COUNT are defined
// in the keymap's config.h as an enum. LAYER_COUNT is the sentinel last value
// and is derived automatically — no manual count to keep in sync.

#define MACRO_SLOT_COUNT 128
// The names Charybdis Ark shows until a layer, macro or custom key is renamed
// there: each holds at most 32 bytes of UTF-8, followed by its terminator.
#define NOAH_LAYER_NAME_SIZE 33
#define NOAH_MACRO_NAME_SIZE 33

// MACRO_0–127 are the authored aliases for QMK's macro keycodes, one per slot.
// The QMK-specific base keycodes stay here so keymap.c can stay declarative.
enum {
    MACRO_0  = QK_MACRO_0 + 0,
    MACRO_1  = QK_MACRO_0 + 1,
    MACRO_2  = QK_MACRO_0 + 2,
    MACRO_3  = QK_MACRO_0 + 3,
    MACRO_4  = QK_MACRO_0 + 4,
    MACRO_5  = QK_MACRO_0 + 5,
    MACRO_6  = QK_MACRO_0 + 6,
    MACRO_7  = QK_MACRO_0 + 7,
    MACRO_8  = QK_MACRO_0 + 8,
    MACRO_9  = QK_MACRO_0 + 9,
    MACRO_10 = QK_MACRO_0 + 10,
    MACRO_11 = QK_MACRO_0 + 11,
    MACRO_12 = QK_MACRO_0 + 12,
    MACRO_13 = QK_MACRO_0 + 13,
    MACRO_14 = QK_MACRO_0 + 14,
    MACRO_15 = QK_MACRO_0 + 15,
    MACRO_16 = QK_MACRO_0 + 16,
    MACRO_17 = QK_MACRO_0 + 17,
    MACRO_18 = QK_MACRO_0 + 18,
    MACRO_19 = QK_MACRO_0 + 19,
    MACRO_20 = QK_MACRO_0 + 20,
    MACRO_21 = QK_MACRO_0 + 21,
    MACRO_22 = QK_MACRO_0 + 22,
    MACRO_23 = QK_MACRO_0 + 23,
    MACRO_24 = QK_MACRO_0 + 24,
    MACRO_25 = QK_MACRO_0 + 25,
    MACRO_26 = QK_MACRO_0 + 26,
    MACRO_27 = QK_MACRO_0 + 27,
    MACRO_28 = QK_MACRO_0 + 28,
    MACRO_29 = QK_MACRO_0 + 29,
    MACRO_30 = QK_MACRO_0 + 30,
    MACRO_31 = QK_MACRO_0 + 31,
    MACRO_32 = QK_MACRO_0 + 32,
    MACRO_33 = QK_MACRO_0 + 33,
    MACRO_34 = QK_MACRO_0 + 34,
    MACRO_35 = QK_MACRO_0 + 35,
    MACRO_36 = QK_MACRO_0 + 36,
    MACRO_37 = QK_MACRO_0 + 37,
    MACRO_38 = QK_MACRO_0 + 38,
    MACRO_39 = QK_MACRO_0 + 39,
    MACRO_40 = QK_MACRO_0 + 40,
    MACRO_41 = QK_MACRO_0 + 41,
    MACRO_42 = QK_MACRO_0 + 42,
    MACRO_43 = QK_MACRO_0 + 43,
    MACRO_44 = QK_MACRO_0 + 44,
    MACRO_45 = QK_MACRO_0 + 45,
    MACRO_46 = QK_MACRO_0 + 46,
    MACRO_47 = QK_MACRO_0 + 47,
    MACRO_48 = QK_MACRO_0 + 48,
    MACRO_49 = QK_MACRO_0 + 49,
    MACRO_50 = QK_MACRO_0 + 50,
    MACRO_51 = QK_MACRO_0 + 51,
    MACRO_52 = QK_MACRO_0 + 52,
    MACRO_53 = QK_MACRO_0 + 53,
    MACRO_54 = QK_MACRO_0 + 54,
    MACRO_55 = QK_MACRO_0 + 55,
    MACRO_56 = QK_MACRO_0 + 56,
    MACRO_57 = QK_MACRO_0 + 57,
    MACRO_58 = QK_MACRO_0 + 58,
    MACRO_59 = QK_MACRO_0 + 59,
    MACRO_60 = QK_MACRO_0 + 60,
    MACRO_61 = QK_MACRO_0 + 61,
    MACRO_62 = QK_MACRO_0 + 62,
    MACRO_63 = QK_MACRO_0 + 63,
    MACRO_64 = QK_MACRO_0 + 64,
    MACRO_65 = QK_MACRO_0 + 65,
    MACRO_66 = QK_MACRO_0 + 66,
    MACRO_67 = QK_MACRO_0 + 67,
    MACRO_68 = QK_MACRO_0 + 68,
    MACRO_69 = QK_MACRO_0 + 69,
    MACRO_70 = QK_MACRO_0 + 70,
    MACRO_71 = QK_MACRO_0 + 71,
    MACRO_72 = QK_MACRO_0 + 72,
    MACRO_73 = QK_MACRO_0 + 73,
    MACRO_74 = QK_MACRO_0 + 74,
    MACRO_75 = QK_MACRO_0 + 75,
    MACRO_76 = QK_MACRO_0 + 76,
    MACRO_77 = QK_MACRO_0 + 77,
    MACRO_78 = QK_MACRO_0 + 78,
    MACRO_79 = QK_MACRO_0 + 79,
    MACRO_80 = QK_MACRO_0 + 80,
    MACRO_81 = QK_MACRO_0 + 81,
    MACRO_82 = QK_MACRO_0 + 82,
    MACRO_83 = QK_MACRO_0 + 83,
    MACRO_84 = QK_MACRO_0 + 84,
    MACRO_85 = QK_MACRO_0 + 85,
    MACRO_86 = QK_MACRO_0 + 86,
    MACRO_87 = QK_MACRO_0 + 87,
    MACRO_88 = QK_MACRO_0 + 88,
    MACRO_89 = QK_MACRO_0 + 89,
    MACRO_90 = QK_MACRO_0 + 90,
    MACRO_91 = QK_MACRO_0 + 91,
    MACRO_92 = QK_MACRO_0 + 92,
    MACRO_93 = QK_MACRO_0 + 93,
    MACRO_94 = QK_MACRO_0 + 94,
    MACRO_95 = QK_MACRO_0 + 95,
    MACRO_96 = QK_MACRO_0 + 96,
    MACRO_97 = QK_MACRO_0 + 97,
    MACRO_98 = QK_MACRO_0 + 98,
    MACRO_99 = QK_MACRO_0 + 99,
    MACRO_100 = QK_MACRO_0 + 100,
    MACRO_101 = QK_MACRO_0 + 101,
    MACRO_102 = QK_MACRO_0 + 102,
    MACRO_103 = QK_MACRO_0 + 103,
    MACRO_104 = QK_MACRO_0 + 104,
    MACRO_105 = QK_MACRO_0 + 105,
    MACRO_106 = QK_MACRO_0 + 106,
    MACRO_107 = QK_MACRO_0 + 107,
    MACRO_108 = QK_MACRO_0 + 108,
    MACRO_109 = QK_MACRO_0 + 109,
    MACRO_110 = QK_MACRO_0 + 110,
    MACRO_111 = QK_MACRO_0 + 111,
    MACRO_112 = QK_MACRO_0 + 112,
    MACRO_113 = QK_MACRO_0 + 113,
    MACRO_114 = QK_MACRO_0 + 114,
    MACRO_115 = QK_MACRO_0 + 115,
    MACRO_116 = QK_MACRO_0 + 116,
    MACRO_117 = QK_MACRO_0 + 117,
    MACRO_118 = QK_MACRO_0 + 118,
    MACRO_119 = QK_MACRO_0 + 119,
    MACRO_120 = QK_MACRO_0 + 120,
    MACRO_121 = QK_MACRO_0 + 121,
    MACRO_122 = QK_MACRO_0 + 122,
    MACRO_123 = QK_MACRO_0 + 123,
    MACRO_124 = QK_MACRO_0 + 124,
    MACRO_125 = QK_MACRO_0 + 125,
    MACRO_126 = QK_MACRO_0 + 126,
    MACRO_127 = QK_MACRO_0 + 127,
};

_Static_assert((QK_MACRO_0 + MACRO_SLOT_COUNT - 1) <= QK_MACRO_MAX, "Macro slot count exceeds QMK macro keycode range");

// ─── Userspace Keycodes ─────────────────────────────────────────────────────
//
// QMK's user range starts at SAFE_RANGE (0x7e40). Each family owns one fixed,
// aligned block, reserved beyond what is supported today, so adding pointing
// modes or layers never moves another keycode:
//
//   0x7e40–0x7e7f  retired (custom keys 0–63 before D-F14): inert
//   0x7e80–0x7e9f  PD_SLOT_n            hold pointing slot n    (32, all used)
//   0x7ea0–0x7ebf  PD_SLOT_n_LOCK       toggle pointing slot n  (32, all used)
//   0x7ec0–0x7edf  LOCK_LAYER(n)        toggle layer n's lock   (32 reserved)
//   0x7ee0–0x7eff  unassigned: inert, refused as a step or combo output
//   0x7f00–0x7f7f  CUSTOM_KEY_0–127     named keys: their shortcut and behaviour
//   0x7f80–0x7fff  CUSTOM_KEY_LAYER_TAP(n)  runtime only: what custom key
//                  n's own tap does to its layers; inert on a key, refused as
//                  a step or combo output
//
// Custom keys moved to their own block in D-F14: 128 do not fit beside the
// pointing holds. Their old codes are never reread as anything else: they
// stay inert, and nothing translates an old layout to the new block (D-F10).
//
// A custom key does what its shortcut and its key_behaviors[] row say (D-F26):
// without either it does nothing. Place it on a layer or as a combo output. Its
// name and shortcut come from CUSTOM_KEYS in keymap.c until Charybdis Ark
// changes them. A behaviour step may send one; it sends only its shortcut,
// layers included.
// PD_SLOT_n holds a configurable pointing slot; add a key_behaviors[] row for
// explicit tap, hold, long-hold or multi-tap behavior on top of that default.
// LOCK_LAYER(n) toggles a layer lock from actions authored in key_behaviors[].

#define NOAH_KEYCODE_RETIRED_CUSTOM_KEY_BASE (SAFE_RANGE + 0x00)
#define NOAH_KEYCODE_PD_HOLD_BASE (SAFE_RANGE + 0x40)
#define NOAH_KEYCODE_PD_LOCK_BASE (SAFE_RANGE + 0x60)
#define NOAH_KEYCODE_LAYER_LOCK_BASE (SAFE_RANGE + 0x80)
#define NOAH_KEYCODE_USERSPACE_END (SAFE_RANGE + 0xa0)
#define NOAH_KEYCODE_CUSTOM_KEY_BASE (SAFE_RANGE + 0xc0)
#define NOAH_KEYCODE_CUSTOM_KEY_LAYER_TAP_BASE (SAFE_RANGE + 0x140)
#define NOAH_KEYCODE_RETIRED_CUSTOM_KEY_RESERVED 64
#define NOAH_KEYCODE_CUSTOM_KEY_RESERVED 128
#define NOAH_KEYCODE_PD_RESERVED 32
#define NOAH_KEYCODE_LAYER_LOCK_RESERVED 32
#define CUSTOM_KEY_SLOT_COUNT 128

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
    CUSTOM_KEY_64 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 64,
    CUSTOM_KEY_65 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 65,
    CUSTOM_KEY_66 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 66,
    CUSTOM_KEY_67 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 67,
    CUSTOM_KEY_68 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 68,
    CUSTOM_KEY_69 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 69,
    CUSTOM_KEY_70 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 70,
    CUSTOM_KEY_71 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 71,
    CUSTOM_KEY_72 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 72,
    CUSTOM_KEY_73 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 73,
    CUSTOM_KEY_74 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 74,
    CUSTOM_KEY_75 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 75,
    CUSTOM_KEY_76 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 76,
    CUSTOM_KEY_77 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 77,
    CUSTOM_KEY_78 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 78,
    CUSTOM_KEY_79 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 79,
    CUSTOM_KEY_80 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 80,
    CUSTOM_KEY_81 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 81,
    CUSTOM_KEY_82 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 82,
    CUSTOM_KEY_83 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 83,
    CUSTOM_KEY_84 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 84,
    CUSTOM_KEY_85 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 85,
    CUSTOM_KEY_86 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 86,
    CUSTOM_KEY_87 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 87,
    CUSTOM_KEY_88 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 88,
    CUSTOM_KEY_89 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 89,
    CUSTOM_KEY_90 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 90,
    CUSTOM_KEY_91 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 91,
    CUSTOM_KEY_92 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 92,
    CUSTOM_KEY_93 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 93,
    CUSTOM_KEY_94 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 94,
    CUSTOM_KEY_95 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 95,
    CUSTOM_KEY_96 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 96,
    CUSTOM_KEY_97 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 97,
    CUSTOM_KEY_98 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 98,
    CUSTOM_KEY_99 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 99,
    CUSTOM_KEY_100 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 100,
    CUSTOM_KEY_101 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 101,
    CUSTOM_KEY_102 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 102,
    CUSTOM_KEY_103 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 103,
    CUSTOM_KEY_104 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 104,
    CUSTOM_KEY_105 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 105,
    CUSTOM_KEY_106 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 106,
    CUSTOM_KEY_107 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 107,
    CUSTOM_KEY_108 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 108,
    CUSTOM_KEY_109 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 109,
    CUSTOM_KEY_110 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 110,
    CUSTOM_KEY_111 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 111,
    CUSTOM_KEY_112 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 112,
    CUSTOM_KEY_113 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 113,
    CUSTOM_KEY_114 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 114,
    CUSTOM_KEY_115 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 115,
    CUSTOM_KEY_116 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 116,
    CUSTOM_KEY_117 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 117,
    CUSTOM_KEY_118 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 118,
    CUSTOM_KEY_119 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 119,
    CUSTOM_KEY_120 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 120,
    CUSTOM_KEY_121 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 121,
    CUSTOM_KEY_122 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 122,
    CUSTOM_KEY_123 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 123,
    CUSTOM_KEY_124 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 124,
    CUSTOM_KEY_125 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 125,
    CUSTOM_KEY_126 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 126,
    CUSTOM_KEY_127 = NOAH_KEYCODE_CUSTOM_KEY_BASE + 127,
};

enum {
    NOAH_KEYCODE_PD_HOLD_BEFORE = NOAH_KEYCODE_PD_HOLD_BASE - 1,
#define NOAH_POINTING_MODE_KEYCODE(name, keycode) keycode,
    NOAH_POINTING_MODE_LIST(NOAH_POINTING_MODE_KEYCODE)
#undef NOAH_POINTING_MODE_KEYCODE
};
enum {
    NOAH_KEYCODE_PD_LOCK_BEFORE = NOAH_KEYCODE_PD_LOCK_BASE - 1,
#define NOAH_POINTING_MODE_LOCK_KEYCODE(name, keycode) keycode##_LOCK,
    NOAH_POINTING_MODE_LIST(NOAH_POINTING_MODE_LOCK_KEYCODE)
#undef NOAH_POINTING_MODE_LOCK_KEYCODE
};
#define LAYER_LOCK_BASE NOAH_KEYCODE_LAYER_LOCK_BASE

_Static_assert(NOAH_KEYCODE_RETIRED_CUSTOM_KEY_BASE == 0x7e40 && NOAH_KEYCODE_PD_HOLD_BASE == 0x7e80 && NOAH_KEYCODE_PD_LOCK_BASE == 0x7ea0 && NOAH_KEYCODE_LAYER_LOCK_BASE == 0x7ec0 && NOAH_KEYCODE_CUSTOM_KEY_BASE == 0x7f00, "userspace keycode blocks are the action ABI");
_Static_assert(NOAH_KEYCODE_RETIRED_CUSTOM_KEY_BASE + NOAH_KEYCODE_RETIRED_CUSTOM_KEY_RESERVED == NOAH_KEYCODE_PD_HOLD_BASE, "the retired custom-key block ends where pointing holds begin");
_Static_assert(CUSTOM_KEY_SLOT_COUNT <= NOAH_KEYCODE_CUSTOM_KEY_RESERVED && CUSTOM_KEY_127 == 0x7f7f && NOAH_KEYCODE_CUSTOM_KEY_BASE + NOAH_KEYCODE_CUSTOM_KEY_RESERVED - 1 <= QK_USER_MAX, "custom keys fill their block inside QMK's user range");
_Static_assert(PD_SLOT_0 == 0x7e80 && PD_SLOT_31 == 0x7e9f && PD_SLOT_31 - PD_SLOT_0 + 1 <= NOAH_KEYCODE_PD_RESERVED && NOAH_KEYCODE_PD_HOLD_BASE + NOAH_KEYCODE_PD_RESERVED == NOAH_KEYCODE_PD_LOCK_BASE, "pointing holds stay inside their block");
_Static_assert(PD_SLOT_0_LOCK == 0x7ea0 && PD_SLOT_31_LOCK == 0x7ebf && PD_SLOT_31_LOCK - PD_SLOT_0_LOCK + 1 <= NOAH_KEYCODE_PD_RESERVED && NOAH_KEYCODE_PD_LOCK_BASE + NOAH_KEYCODE_PD_RESERVED == NOAH_KEYCODE_LAYER_LOCK_BASE, "pointing locks stay inside their block");
_Static_assert(LAYER_COUNT <= NOAH_KEYCODE_LAYER_LOCK_RESERVED && NOAH_KEYCODE_LAYER_LOCK_BASE + NOAH_KEYCODE_LAYER_LOCK_RESERVED == NOAH_KEYCODE_USERSPACE_END, "layer locks stay inside their block");
_Static_assert(NOAH_KEYCODE_USERSPACE_END - 1 <= QK_USER_MAX, "userspace keycodes stay in QMK's user range");
_Static_assert(NOAH_KEYCODE_CUSTOM_KEY_LAYER_TAP_BASE == CUSTOM_KEY_127 + 1 && NOAH_KEYCODE_CUSTOM_KEY_LAYER_TAP_BASE + CUSTOM_KEY_SLOT_COUNT - 1 == QK_USER_MAX, "custom keys' runtime layer taps fill the last block of QMK's user range");

#define POINTING_MODE_KEYCODE_COUNT POINTING_MODE_COUNT
#define POINTING_MODE_LOCK_KEYCODE_COUNT POINTING_MODE_COUNT
#define LOCK_LAYER(layer_) (LAYER_LOCK_BASE + (layer_))
#define NOAH_KEYCODE_IS_CUSTOM_KEY(keycode_) ((keycode_) >= CUSTOM_KEY_0 && (keycode_) <= CUSTOM_KEY_127)
// The runtime's own action for what custom key `slot_`'s own tap does to its
// layers when the key held its shortcut from the press (D-F26): a One-shot
// shortcut's tap arms them, as OSL()'s tap does, and a Tap-toggle shortcut's
// last tap locks them, as TT()'s sends LOCK_LAYER(n). Never stored: a profile
// refuses it everywhere.
#define CUSTOM_KEY_LAYER_TAP(slot_) (NOAH_KEYCODE_CUSTOM_KEY_LAYER_TAP_BASE + (slot_))
#define NOAH_KEYCODE_IS_CUSTOM_KEY_LAYER_TAP(keycode_) ((keycode_) >= NOAH_KEYCODE_CUSTOM_KEY_LAYER_TAP_BASE && (keycode_) <= NOAH_KEYCODE_CUSTOM_KEY_LAYER_TAP_BASE + CUSTOM_KEY_SLOT_COUNT - 1)

extern const char *const macro_payloads[MACRO_SLOT_COUNT];
// The same macros already in the bank encoding, generated at build time, so
// writing the compiled defaults copies bytes instead of compiling macros.
extern const uint8_t  authored_macro_bank[];
extern const uint16_t authored_macro_bank_size;
extern const char        macro_names[MACRO_SLOT_COUNT][NOAH_MACRO_NAME_SIZE];
extern const char        custom_key_names[CUSTOM_KEY_SLOT_COUNT][NOAH_MACRO_NAME_SIZE];
// Each custom key's authored shortcut, as settings v10 stores it: the report
// modifier mask, the key, the layer mask, low byte first, then the layer kind;
// all zero for none.
extern const uint8_t custom_key_shortcuts[CUSTOM_KEY_SLOT_COUNT][NOAH_CUSTOM_KEY_SHORTCUT_SIZE];
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
