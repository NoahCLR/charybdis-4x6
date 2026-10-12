// ────────────────────────────────────────────────────────────────────────────
// QMK Modifier Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Declares this userspace's intentional override of QMK's register_mods() and
// unregister_mods() symbols so modifier ownership remains centralized.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

// QMK encodes Ctrl/Shift/Alt/GUI in the low nibble; bit 4 selects the
// right-hand bank. MT()/QK_MODS fields use this encoding, not report bits.
// Interpret the encoding only: keymap/Magic remapping remains QMK policy.
static inline uint8_t noah_qmk_mods_to_report_mask(uint8_t mods) {
    return (mods & 0x10u) ? (uint8_t)((mods & 0x0Fu) << 4u) : (uint8_t)(mods & 0x0Fu);
}

// Native modifier-only ACT_LMODS/ACT_RMODS actions use real mods: a plain
// modifier, KC_MEH/KC_HYPR (KC_NO base), or a modifier wrapped in QK_MODS.
// A modified ordinary key uses weak mods instead and owns no real modifier.
static inline uint8_t noah_qmk_real_mods_for_keycode(uint16_t keycode) {
    if (IS_MODIFIER_KEYCODE(keycode)) return MOD_BIT(keycode);
    if (!IS_QK_MODS(keycode)) return 0u;
    uint8_t basic = QK_MODS_GET_BASIC_KEYCODE(keycode);
    if (basic != KC_NO && !IS_MODIFIER_KEYCODE(basic)) return 0u;
    return noah_qmk_mods_to_report_mask(QK_MODS_GET_MODS(keycode)) | (IS_MODIFIER_KEYCODE(basic) ? MOD_BIT(basic) : 0u);
}

void register_mods(uint8_t mods);
void unregister_mods(uint8_t mods);
