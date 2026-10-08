#pragma once
#include "profile_compiled_defaults_v1.h"
#include <string.h>
typedef struct {
    noah_profile_compiled_v1_write_fn write;
    void                             *context;
    size_t                            offset;
    noah_profile_compiled_v1_result_t result;
    bool                              allow_early_stop;
    bool                              stopped;
} compiled_writer_t;

static inline noah_profile_compiled_v1_error_t no_error(void) {
    return (noah_profile_compiled_v1_error_t){
        .code    = NOAH_PROFILE_COMPILED_V1_OK,
        .surface = NOAH_PROFILE_COMPILED_V1_SURFACE_NONE,
        .row     = UINT8_MAX,
        .step    = UINT8_MAX,
    };
}

static inline noah_profile_compiled_v1_result_t fail(noah_profile_compiled_v1_error_t *error, noah_profile_compiled_v1_result_t code, noah_profile_compiled_v1_surface_t surface, uint8_t row, uint8_t step) {
    if (error) {
        *error = (noah_profile_compiled_v1_error_t){.code = code, .surface = surface, .row = row, .step = step};
    }
    return code;
}

static inline bool emit(compiled_writer_t *writer, const uint8_t *bytes, size_t length) {
    if (writer->result != NOAH_PROFILE_COMPILED_V1_OK || writer->stopped) {
        return false;
    }
    if (length != 0u && !writer->write(writer->context, bytes, length)) {
        if (writer->allow_early_stop) {
            writer->stopped = true;
            return false;
        }
        writer->result = NOAH_PROFILE_COMPILED_V1_WRITE_ERROR;
        return false;
    }
    writer->offset += length;
    return true;
}

static inline bool emit_u8(compiled_writer_t *writer, uint8_t value) {
    return emit(writer, &value, 1u);
}

static inline bool emit_u16(compiled_writer_t *writer, uint16_t value) {
    uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    return emit(writer, bytes, sizeof(bytes));
}

static inline bool emit_u32(compiled_writer_t *writer, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u), (uint8_t)(value >> 16u), (uint8_t)(value >> 24u)};
    return emit(writer, bytes, sizeof(bytes));
}

static inline bool emit_action(compiled_writer_t *writer, const noah_profile_action_v1_t *action) {
    uint8_t bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE] = {action->kind, action->flags, (uint8_t)action->operand, (uint8_t)(action->operand >> 8u)};
    return emit(writer, bytes, sizeof(bytes));
}

// Each registry row's module encodes that domain from authored data, named
// noah_profile_<module>_compiled_v1_write. A build without the domain writes
// nothing, and the compiled profile omits it.
#define NOAH_DOMAIN_COMPILED_WRITER(NAME, module, id, version) noah_profile_compiled_v1_result_t noah_profile_##module##_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error);
NOAH_PROFILE_DOMAIN_ROWS(NOAH_DOMAIN_COMPILED_WRITER)
#undef NOAH_DOMAIN_COMPILED_WRITER

#if defined(RGB_MATRIX_ENABLE)
uint16_t noah_profile_rgb_compiled_v1_stage_mask(void);
#endif
