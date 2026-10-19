// ────────────────────────────────────────────────────────────────────────────
// Compiled Keymap — the authored keymaps[][][] as domain 0x60
// ────────────────────────────────────────────────────────────────────────────

#include "profile_compiled_writer.h"

#include "profile_keymap_v1.h"
#include "noah_keymap_ids.h"

_Static_assert((unsigned)LAYER_COUNT <= (unsigned)NOAH_PROFILE_KEYMAP_V1_MAX_LAYERS, "the keymap domain holds at most sixteen layers");
_Static_assert(MATRIX_ROWS * MATRIX_COLS <= UINT8_MAX, "a keymap position fits one byte");

noah_profile_compiled_v1_result_t noah_profile_keymap_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    (void)error;
    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++)
        for (uint8_t row = 0; row < MATRIX_ROWS; row++)
            for (uint8_t col = 0; col < MATRIX_COLS; col++)
                if (!emit_u16(writer, pgm_read_word(&keymaps[layer][row][col]))) return writer->result;
    return writer->result;
}
