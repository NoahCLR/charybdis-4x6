// ────────────────────────────────────────────────────────────────────────────
// Compiled Macros — the authored VIA_MACROS() payloads as domain 0x70
// ────────────────────────────────────────────────────────────────────────────

#include "profile_compiled_writer.h"

#include "profile_macros_v1.h"
#include "noah_keymap_ids.h"

_Static_assert(VIA_MACRO_SLOT_COUNT == NOAH_PROFILE_MACROS_V1_SLOTS, "the macro domain holds every macro slot");

// The authored macros already in the bank encoding, each ending with its
// terminator: what VIA's bank is seeded with. Real-profile validation rebuilds
// authored_macro_bank with the firmware encoder, so writing a profile copies
// bytes instead of compiling macros on the save path.
noah_profile_compiled_v1_result_t noah_profile_macros_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    if (authored_macro_bank_size > NOAH_PROFILE_MACROS_V1_MAX_SIZE) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    for (uint16_t index = 0; index < authored_macro_bank_size; index++) {
        if (!emit_u8(writer, authored_macro_bank[index])) return writer->result;
    }
    return writer->result;
}
