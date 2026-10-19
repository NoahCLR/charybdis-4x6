#include QMK_KEYBOARD_H
#include "noah_keymap_ids.h"
#include "../profile/runtime/effective_keymap_macros_runtime.h"
#include "../profile/schema/profile_keymap_v1.h"

// BK deliberately makes this hook weak. All QMK and combo-origin reads use
// it; the RGB coverage map uses it too. Never read VIA's unused keymap copy.
uint16_t keymap_key_to_keycode(uint8_t layer, keypos_t key) {
    if (layer >= LAYER_COUNT || key.row >= MATRIX_ROWS || key.col >= MATRIX_COLS) return KC_NO;
    if (!noah_effective_keymap_is_live()) return pgm_read_word(&keymaps[layer][key.row][key.col]);
    uint8_t bytes[2];
    uint32_t offset = noah_profile_keymap_v1_offset(layer, key.row * MATRIX_COLS + key.col, MATRIX_ROWS * MATRIX_COLS);
    return noah_effective_keymap_read(offset, bytes) ? (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8u) : KC_NO;
}
