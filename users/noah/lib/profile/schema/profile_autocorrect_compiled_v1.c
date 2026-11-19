// ────────────────────────────────────────────────────────────────────────────
// Compiled Autocorrect List — the authored list as domain 0x80
// ────────────────────────────────────────────────────────────────────────────

#include "profile_compiled_writer.h"

#include "profile_autocorrect_v1.h"
#include "noah_keymap_ids.h"

// The keymap folder's autocorrect.txt, already in the domain's encoding
// (authored_autocorrect_list.h, D-F68). Real-profile validation refuses a
// stale copy, so writing a profile copies bytes. An empty list is still its
// header, so every build's compiled defaults hold the domain and admit it.
noah_profile_compiled_v1_result_t noah_profile_autocorrect_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    if (authored_autocorrect_list_size > NOAH_PROFILE_AUTOCORRECT_V1_MAX_SIZE) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    emit(writer, authored_autocorrect_list, authored_autocorrect_list_size);
    return writer->result;
}
