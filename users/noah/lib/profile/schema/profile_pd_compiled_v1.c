// ────────────────────────────────────────────────────────────────────────────
// Compiled Pointing Slots — the authored noah_pd_defaults[] as domain 0x50
// ────────────────────────────────────────────────────────────────────────────

#include "profile_compiled_writer.h"

#include "profile_pd_v1.h"

#ifdef NOAH_PD_PROFILE_ENABLE
// The sparse version-2 domain stores only the slots a profile uses.
static uint16_t pd_payload_size(void) {
    return (uint16_t)(NOAH_PROFILE_PD_V1_HEADER_SIZE + (size_t)noah_profile_pd_v1_default_record_count(noah_pd_defaults) * NOAH_PROFILE_PD_V1_RECORD_SIZE);
}

noah_profile_compiled_v1_result_t noah_profile_pd_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    const uint8_t               pd_header[8] = {NOAH_PROFILE_PD_V1_VERSION, NOAH_PROFILE_PD_V1_SLOT_COUNT, NOAH_PROFILE_PD_V1_RECORD_SIZE, noah_profile_pd_v1_default_record_count(noah_pd_defaults), 0, 0, 0, 0};
    noah_profile_pd_v1_cursor_t cursor;
    if (noah_profile_pd_v1_cursor_begin(&cursor, pd_header, pd_payload_size(), NULL) != NOAH_PROFILE_PD_V1_OK) return NOAH_PROFILE_COMPILED_V1_INVALID_ACTION;
    if (!emit(writer, pd_header, sizeof(pd_header))) return writer->result;
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) {
        uint8_t record[NOAH_PROFILE_PD_V1_RECORD_SIZE];
        noah_profile_pd_v1_encode_record(&noah_pd_defaults[slot], record);
        if (noah_profile_pd_v1_validate_record(record, sizeof(record), slot, NULL) != NOAH_PROFILE_PD_V1_OK) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, slot, UINT8_MAX);
        if (!noah_profile_pd_v1_record_present(record)) continue;
        if (noah_profile_pd_v1_cursor_next(&cursor, record, NULL) != NOAH_PROFILE_PD_V1_OK) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, slot, UINT8_MAX);
        for (uint8_t offset = 0; offset < sizeof(record); offset++)
            if (!emit_u8(writer, record[offset])) return writer->result;
    }
    return writer->result;
}
#else
// Without pointing profiles the compiled profile has no pointing domain.
noah_profile_compiled_v1_result_t noah_profile_pd_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    (void)writer;
    (void)error;
    return NOAH_PROFILE_COMPILED_V1_OK;
}
#endif
