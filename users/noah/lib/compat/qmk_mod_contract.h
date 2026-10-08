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

void register_mods(uint8_t mods);
void unregister_mods(uint8_t mods);
