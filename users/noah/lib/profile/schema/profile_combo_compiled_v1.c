// ────────────────────────────────────────────────────────────────────────────
// Compiled Combos — the authored key_combos[] as domain 0x30
// ────────────────────────────────────────────────────────────────────────────

#include "profile_compiled_writer.h"

#include "profile_combo_v1.h"
#include "noah_keymap_ids.h"

#ifdef COMBO_ENABLE
noah_profile_compiled_v1_result_t noah_profile_combos_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    if (noah_combo_count > NOAH_PROFILE_COMBO_V1_MAX_ROWS) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    if (!(emit_u8(writer, noah_combo_count) && emit_u8(writer, 0) && emit_u16(writer, 0) && emit_u16(writer, COMBO_TERM) && emit_u16(writer, TAPPING_TERM))) return writer->result;
    for (uint16_t row = 0; row < noah_combo_count; row++) {
        const combo_t *combo = &key_combos[row];
        uint8_t        count = 0, flags = 0;
        while (count <= NOAH_PROFILE_COMBO_V1_MAX_INPUTS && pgm_read_word(&combo->keys[count]) != COMBO_END)
            count++;
        if (count < 2 || count > NOAH_PROFILE_COMBO_V1_MAX_INPUTS) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, (uint8_t)row, UINT8_MAX);
#    ifdef COMBO_MUST_PRESS_IN_ORDER
        flags |= 4u;
#    endif
#    ifdef COMBO_MUST_HOLD_MODS
        uint16_t key = combo->keycode;
        if ((key >= 0xe0u && key <= 0xe7u) || (key >= 0x100u && key <= 0x1fffu && (key & 0xffu) == 0u) || (key >= 0x5220u && key <= 0x523fu)) flags |= 1u;
#    endif
        // An authored combo is enabled on every layer of the bank.
        if (!(emit_u8(writer, count) && emit_u8(writer, flags) && emit_u16(writer, noah_combo_terms[row]) && emit_u32(writer, (uint32_t)((UINT64_C(1) << LAYER_COUNT) - 1u)))) return writer->result;
        // The output, then only the inputs the combo has.
        for (uint8_t member = 0; member <= count; member++) {
            noah_profile_action_v1_t action = {0};
            uint16_t                 native = member == 0 ? combo->keycode : pgm_read_word(&combo->keys[member - 1]);
            if (noah_profile_compiled_v1_action(native, &action) != NOAH_PROFILE_COMPILED_V1_OK || action.kind == NOAH_PROFILE_ACTION_V1_NONE) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, (uint8_t)row, member);
            if (!emit_action(writer, &action)) return writer->result;
        }
    }
    return writer->result;
}
#else
// Without combos the compiled profile has no combo domain.
noah_profile_compiled_v1_result_t noah_profile_combos_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    (void)writer;
    (void)error;
    return NOAH_PROFILE_COMPILED_V1_OK;
}
#endif
