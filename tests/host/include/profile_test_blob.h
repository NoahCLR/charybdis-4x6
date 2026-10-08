#pragma once

// A current one-domain blob around a test payload, so code that finds a
// domain by walking the envelope sees exactly what a validated profile holds.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_blob_v1.h"

enum {
    NOAH_PROFILE_TEST_BLOB_PAYLOAD_OFFSET = NOAH_PROFILE_BLOB_V1_HEADER_SIZE + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE,
};

// One byte of the blob holding `length` payload bytes of domain `id`.
static inline uint8_t noah_profile_test_blob_byte(uint8_t id, const uint8_t *payload, uint16_t length, size_t offset) {
    const uint8_t header[NOAH_PROFILE_TEST_BLOB_PAYLOAD_OFFSET] = {
        'N', 'L', 'P', '1', NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR, NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR, 1u, NOAH_PROFILE_BLOB_V1_CANONICAL_FLAG,
        id, noah_profile_domain_find(id) ? noah_profile_domain_find(id)->version : 0u, (uint8_t)length, (uint8_t)(length >> 8u),
    };
    return offset < sizeof(header) ? header[offset] : payload[offset - sizeof(header)];
}

// Copies the blob into `out`; returns its length, or 0 when it does not fit.
static inline size_t noah_profile_test_blob_wrap(uint8_t *out, size_t capacity, uint8_t id, const uint8_t *payload, uint16_t length) {
    size_t total = (size_t)NOAH_PROFILE_TEST_BLOB_PAYLOAD_OFFSET + length;
    if (!out || total > capacity) return 0u;
    for (size_t offset = 0; offset < total; offset++)
        out[offset] = noah_profile_test_blob_byte(id, payload, length, offset);
    return total;
}
