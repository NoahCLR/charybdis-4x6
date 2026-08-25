// ─────────────────────────────────────────────────────────────────────────
// Bounded Profile Schema Reader
// ───────────────────────────────────────────────────────────────────────────

#include "profile_reader.h"

#include <string.h>

static bool memory_read(void *context, size_t offset, uint8_t *target, size_t length) {
    const uint8_t *bytes = context;

    if (length != 0u) {
        memcpy(target, &bytes[offset], length);
    }
    return true;
}

noah_profile_reader_t noah_profile_reader_from_memory(const uint8_t *bytes, size_t length) {
    noah_profile_reader_t reader = {
        .read    = bytes || length == 0u ? memory_read : NULL,
        .context = (void *)bytes,
        .length  = length,
    };
    return reader;
}

bool noah_profile_reader_read(const noah_profile_reader_t *reader, size_t offset, uint8_t *target, size_t length) {
    if (!reader || !reader->read || (!target && length != 0u) || offset > reader->length || length > reader->length - offset) {
        return false;
    }
    if (length == 0u) {
        return true;
    }
    return reader->read(reader->context, offset, target, length);
}
