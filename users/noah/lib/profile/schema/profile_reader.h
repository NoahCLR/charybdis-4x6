// ─────────────────────────────────────────────────────────────────────────
// Bounded Profile Schema Reader
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*noah_profile_reader_read_fn)(void *context, size_t offset, uint8_t *target, size_t length);

typedef struct {
    noah_profile_reader_read_fn read;
    void                       *context;
    size_t                      length;
} noah_profile_reader_t;

// The returned reader borrows bytes for its entire lifetime. It stores the
// byte pointer directly, so copying the reader is safe and needs no auxiliary
// context object.
noah_profile_reader_t noah_profile_reader_from_memory(const uint8_t *bytes, size_t length);

// Performs overflow-safe bounds checks before invoking the injected reader.
// A zero-byte read is valid and does not require a target pointer.
bool noah_profile_reader_read(const noah_profile_reader_t *reader, size_t offset, uint8_t *target, size_t length);
