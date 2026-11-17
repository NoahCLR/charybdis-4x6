#pragma once

// The other half while a person flashes one half, then the other: it still
// runs the released firmware, v2026.10.27 (profile schema 4.3), and this half
// runs this build. Tests stand it in with the bytes it sends, built here,
// never with its sources.
//
// The profile split protocol (framing version 4), the working-copy sync and
// both codecs are unchanged since that release, so its frames have this
// build's layout (docs/spec/profile-split-v1.md). Only these differ:
// - schema 4.3: that release's users/noah/lib/profile/schema/profile_versions.h;
// - domain mask 0x7f, seven domains and no replacement list (0x80): its
//   profile_domain_registry.h, and its firmware-contract.json's
//   supportedDomainMask 127;
// - the compiled-default and action-ABI digests: its firmware-contract.json;
// - the base runtime packet, 14 bytes: its users/noah/lib/split/runtime_sync.h
//   with RGB_POINTING_MODE_ACTIVE_HALF_ENABLE, as users/noah/config.h sets it,
//   and an 8-byte key bitmap (60 keys): automouse progress (2), active and
//   locked mode ids (1 each), owner sides (1), owner bitmap (8), preview
//   layer (1).

#include <stdint.h>
#include <string.h>

enum {
    RELEASED_SCHEMA_MAJOR = 4u,
    RELEASED_SCHEMA_MINOR = 3u,
    RELEASED_DOMAIN_MASK  = 0x7fu,

    RELEASED_SPLIT_FRAMING_VERSION = 4u,
    RELEASED_SPLIT_FRAME_SIZE      = 32u,
    // Descriptor flag bits: has a saved profile, readable, origin right,
    // override, and (METADATA requests) no prepared push.
    RELEASED_FLAG_HAS_PROFILE      = 1u << 0,
    RELEASED_FLAG_READABLE         = 1u << 1,
    RELEASED_FLAG_ORIGIN_RIGHT     = 1u << 3,
    RELEASED_FLAG_OVERRIDE         = 1u << 4,
    RELEASED_FLAG_NO_PREPARED_PUSH = 1u << 5,

    RELEASED_BASE_PACKET_SIZE                  = 14u,
    RELEASED_BASE_AUTOMOUSE_PROGRESS_OFFSET    = 0u,
    RELEASED_BASE_ACTIVE_MODE_OFFSET           = 2u,
    RELEASED_BASE_LOCKED_MODE_OFFSET           = 3u,
    RELEASED_BASE_OWNER_SIDES_OFFSET           = 4u,
    RELEASED_BASE_OWNER_BITMAP_OFFSET          = 5u,
    RELEASED_BASE_OWNER_BITMAP_SIZE            = 8u,
    RELEASED_BASE_PREVIEW_LAYER_OFFSET         = 13u,
};

#define RELEASED_COMPILED_DEFAULT_DIGEST UINT32_C(0xc7b2740c)
#define RELEASED_ACTION_ABI_DIGEST       UINT32_C(0x67df11b3)

static inline void released_put_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

// The split frames' CRC8: polynomial 0x07, initial 0.
static inline uint8_t released_crc8(const uint8_t *bytes, uint32_t length) {
    uint8_t crc = 0u;

    for (uint32_t index = 0u; index < length; index++) {
        crc ^= bytes[index];
        for (uint8_t bit = 0u; bit < 8u; bit++) {
            crc = (crc & 0x80u) != 0u ? (uint8_t)((crc << 1u) ^ 0x07u) : (uint8_t)(crc << 1u);
        }
    }
    return crc;
}

// A saved record as the released half describes it. Its store saves every
// domain it knows, so the mask is all seven.
typedef struct {
    uint32_t generation;
    uint32_t payload_length;
    uint32_t payload_crc32;
    uint32_t payload_digest;
    uint8_t  origin_half;
} released_record_t;

// A descriptor frame (METADATA 1, PREPARE_BEGIN 2, PREPARE_COMMIT 4, ABORT 5,
// PREPARE_DURABLE 10) as the released half encodes it, status OK. A null
// record is its compiled defaults: readable, nothing saved.
static inline void released_descriptor_frame(uint8_t frame[RELEASED_SPLIT_FRAME_SIZE], uint8_t kind, const released_record_t *record, uint8_t extra_flags) {
    memset(frame, 0, RELEASED_SPLIT_FRAME_SIZE);
    frame[0] = RELEASED_SPLIT_FRAMING_VERSION;
    frame[1] = kind;
    frame[3] = (uint8_t)(RELEASED_FLAG_READABLE | extra_flags);
    frame[4] = RELEASED_SCHEMA_MAJOR;
    frame[5] = RELEASED_SCHEMA_MINOR;
    if (record) {
        frame[3] |= RELEASED_FLAG_HAS_PROFILE | RELEASED_FLAG_OVERRIDE | (record->origin_half ? RELEASED_FLAG_ORIGIN_RIGHT : 0u);
        frame[6] = RELEASED_DOMAIN_MASK;
        released_put_u32(&frame[7], record->payload_length);
        released_put_u32(&frame[11], record->generation);
        released_put_u32(&frame[15], record->payload_crc32);
        released_put_u32(&frame[19], record->payload_digest);
    }
    released_put_u32(&frame[23], RELEASED_COMPILED_DEFAULT_DIGEST);
    released_put_u32(&frame[27], RELEASED_ACTION_ABI_DIGEST);
    frame[31] = released_crc8(frame, 31u);
}

// A PAYLOAD_CHUNK (3) of the released half's copy of `record`: 22 + n bytes,
// never under 32. Returns its length.
static inline uint8_t released_chunk_frame(uint8_t *frame, const released_record_t *record, uint32_t offset, const uint8_t *bytes, uint8_t count) {
    uint8_t length = (uint8_t)(22u + count) < RELEASED_SPLIT_FRAME_SIZE ? RELEASED_SPLIT_FRAME_SIZE : (uint8_t)(22u + count);

    memset(frame, 0, length);
    frame[0] = RELEASED_SPLIT_FRAMING_VERSION;
    frame[1] = 3u;
    released_put_u32(&frame[4], record->generation);
    released_put_u32(&frame[8], record->payload_digest);
    released_put_u32(&frame[12], offset);
    released_put_u32(&frame[16], record->payload_length);
    frame[20] = count;
    memcpy(&frame[21], bytes, count);
    frame[length - 1u] = released_crc8(frame, (uint32_t)length - 1u);
    return length;
}

// What the released half answers any request it cannot decode, this build's
// METADATA among them (its schema minor is not 3): ERROR (7),
// INVALID_FRAME (1), generation and digest zero.
static inline void released_invalid_frame_reply(uint8_t frame[RELEASED_SPLIT_FRAME_SIZE]) {
    memset(frame, 0, RELEASED_SPLIT_FRAME_SIZE);
    frame[0]  = RELEASED_SPLIT_FRAMING_VERSION;
    frame[1]  = 7u;
    frame[2]  = 1u;
    frame[31] = released_crc8(frame, 31u);
}
