// ──────────────────────────────────────────────────────────────────────────
// Live-Profile Split Protocol v1
// ──────────────────────────────────────────────────────────────────────────

#include "profile_split_protocol_v1.h"

#include <stddef.h>
#include <string.h>

#include "../schema/profile_blob_v1.h"
#include "../storage/profile_store.h"

enum {
    WIRE_VERSION               = 0u,
    WIRE_KIND                  = 1u,
    WIRE_STATUS                = 2u,
    WIRE_FLAGS                 = 3u,
    WIRE_SCHEMA_MAJOR          = 4u,
    WIRE_SCHEMA_MINOR          = 5u,
    WIRE_DOMAIN_MASK           = 6u,
    WIRE_PAYLOAD_LENGTH        = 7u,
    WIRE_GENERATION            = 11u,
    WIRE_PAYLOAD_CRC32         = 15u,
    WIRE_PAYLOAD_DIGEST        = 19u,
    WIRE_COMPILED_DIGEST       = 23u,
    WIRE_ACTION_ABI            = 27u,
    WIRE_TRANSFER_GEN          = 4u,
    WIRE_TRANSFER_DIGEST       = 8u,
    WIRE_TRANSFER_OFFSET       = 12u,
    WIRE_TRANSFER_LENGTH       = 16u,
    WIRE_CHUNK_LENGTH          = 20u,
    WIRE_CHUNK                 = 21u,
    WIRE_BUSY_REASON           = 21u, // ACK/BUSY reuses the unused chunk bytes
    WIRE_BUSY_STORE_STATE      = 22u,
    WIRE_BUSY_OWNER            = 23u,
    WIRE_BUSY_ADMISSION        = 24u,
    WIRE_REUSE_LENGTH          = 20u,
    WIRE_REUSE_SOURCE_OFF      = 24u,
    WIRE_REUSE_SOURCE_GEN      = 28u,
    WIRE_REUSE_SOURCE_DIG      = 32u,
    WIRE_REUSE_SOURCE_CRC      = 36u,
    WIRE_REUSE_SOURCE_KIND     = 40u,
    WIRE_REUSE_SOURCE_ORIGIN   = 41u,
    WIRE_REUSE_RESERVED        = 42u,
    WIRE_REUSE_KIND_COMPILED   = 0u,
    WIRE_REUSE_KIND_COMMITTED  = 1u,
    // The working copy (D-F32): over a committed base or the compiled one.
    WIRE_REUSE_KIND_WORKING    = 2u,
    WIRE_REUSE_ORIGIN_COMPILED = 255u,
    WIRE_FLAG_HAS_PROFILE      = 1u << 0,
    WIRE_FLAG_READABLE         = 1u << 1,
    // Bit 2 marked a record bound to the VIA store; retired with VIA (D-F31)
    // and refused when set.
    // A descriptor's origin half and its one profile flag (override) share
    // the flags byte since version 3, leaving room for a 32-bit length.
    WIRE_FLAG_ORIGIN_HALF      = 1u << 3,
    WIRE_FLAG_PROFILE_OVERRIDE = 1u << 4,
    WIRE_DESCRIPTOR_FLAGS      = WIRE_FLAG_HAS_PROFILE | WIRE_FLAG_READABLE | WIRE_FLAG_ORIGIN_HALF | WIRE_FLAG_PROFILE_OVERRIDE,
};

static uint8_t crc8(const uint8_t *bytes, size_t length) {
    uint8_t crc = 0u;

    for (size_t index = 0u; index < length; index++) {
        crc ^= bytes[index];
        for (uint8_t bit = 0u; bit < 8u; bit++) {
            crc = (crc & 0x80u) != 0u ? (uint8_t)((crc << 1u) ^ 0x07u) : (uint8_t)(crc << 1u);
        }
    }
    return crc;
}

static void write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static uint32_t read_u32(const uint8_t *source) {
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8u) | ((uint32_t)source[2] << 16u) | ((uint32_t)source[3] << 24u);
}

static bool bytes_zero(const uint8_t *bytes, size_t length) {
    for (size_t index = 0u; index < length; index++) {
        if (bytes[index] != 0u) {
            return false;
        }
    }
    return true;
}

static bool descriptor_kind(noah_profile_split_v1_kind_t kind) {
    return kind == NOAH_PROFILE_SPLIT_V1_METADATA || kind == NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN || kind == NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE || kind == NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT || kind == NOAH_PROFILE_SPLIT_V1_ABORT;
}

static bool descriptor_zero(const noah_profile_split_descriptor_t *descriptor) {
    return descriptor && descriptor->generation == 0u && descriptor->payload_crc32 == 0u && descriptor->payload_digest == 0u && descriptor->compiled_default_digest == 0u && descriptor->action_abi_digest == 0u && descriptor->payload_length == 0u && descriptor->schema_major == 0u && descriptor->schema_minor == 0u && descriptor->domain_mask == 0u && descriptor->profile_flags == 0u && descriptor->origin_half == 0u && !descriptor->readable && !descriptor->has_profile;
}

static bool reuse_zero(const noah_profile_split_v1_frame_t *frame) {
    return frame->reuse_length == 0u && frame->reuse_source_offset == 0u && frame->reuse_source.generation == 0u && frame->reuse_source.digest == 0u && frame->reuse_source.crc32 == 0u && frame->reuse_source.kind == 0u && frame->reuse_source.origin == 0u;
}

// A chunk frame is at least the 32 bytes every other frame has. A pulled
// chunk fills the reply buffer its PAYLOAD_REQUEST asked for exactly.
static uint8_t chunk_frame_length(uint8_t chunk_length) {
    uint16_t length = (uint16_t)NOAH_PROFILE_SPLIT_V1_CHUNK_OVERHEAD + chunk_length;
    return length < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE ? NOAH_PROFILE_SPLIT_V1_FRAME_SIZE : (uint8_t)length;
}

// The frame length a frame's first bytes declare, or zero.
static uint8_t declared_frame_length(const uint8_t *wire) {
    if (wire[WIRE_KIND] == NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK) {
        return wire[WIRE_CHUNK_LENGTH] > NOAH_PROFILE_SPLIT_V1_CHUNK_MAX ? 0u : chunk_frame_length(wire[WIRE_CHUNK_LENGTH]);
    }
    return wire[WIRE_KIND] == NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE ? NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE : NOAH_PROFILE_SPLIT_V1_FRAME_SIZE;
}

static bool busy_frame(const noah_profile_split_v1_frame_t *frame) {
    return frame->kind == NOAH_PROFILE_SPLIT_V1_ACK && frame->status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
}

static bool transfer_shape_valid(const noah_profile_split_v1_frame_t *frame) {
    if (!descriptor_zero(&frame->descriptor) || frame->payload_length > NOAH_PROFILE_BLOB_V1_MAX_SIZE || frame->offset > frame->payload_length) {
        return false;
    }
    if (busy_frame(frame) ? frame->busy_reason > NOAH_PROFILE_SPLIT_V1_BUSY_REASON_MAX : (frame->busy_reason || frame->busy_store_state || frame->busy_owner || frame->busy_admission)) {
        return false;
    }
    if (frame->kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK) {
        return frame->status == NOAH_PROFILE_SPLIT_V1_STATUS_OK && frame->generation != 0u && frame->payload_length >= NOAH_PROFILE_BLOB_V1_HEADER_SIZE && frame->chunk_length != 0u && frame->chunk_length <= NOAH_PROFILE_SPLIT_V1_CHUNK_MAX && frame->chunk_length <= frame->payload_length - frame->offset;
    }
    if (frame->chunk_length != 0u || !bytes_zero(frame->chunk, sizeof(frame->chunk))) {
        return false;
    }
    if (frame->kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE) {
        bool committed_base = frame->reuse_source.generation != 0u && frame->reuse_source.origin <= 1u;
        bool compiled_base  = frame->reuse_source.generation == 0u && frame->reuse_source.origin == WIRE_REUSE_ORIGIN_COMPILED;
        bool source_valid   = frame->reuse_source.kind == WIRE_REUSE_KIND_COMMITTED ? committed_base : frame->reuse_source.kind == WIRE_REUSE_KIND_COMPILED ? compiled_base : frame->reuse_source.kind == WIRE_REUSE_KIND_WORKING && (committed_base || compiled_base);

        return frame->status == NOAH_PROFILE_SPLIT_V1_STATUS_OK && frame->generation != 0u && frame->payload_length >= NOAH_PROFILE_BLOB_V1_HEADER_SIZE && frame->reuse_length != 0u && frame->reuse_length <= frame->payload_length - frame->offset && frame->reuse_source_offset <= NOAH_PROFILE_BLOB_V1_MAX_SIZE - frame->reuse_length && source_valid;
    }
    if (!reuse_zero(frame)) {
        return false;
    }
    if (frame->kind == NOAH_PROFILE_SPLIT_V1_ACK) {
        return frame->status == NOAH_PROFILE_SPLIT_V1_STATUS_OK || frame->status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
    }
    if (frame->kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST) {
        return frame->status == NOAH_PROFILE_SPLIT_V1_STATUS_OK && frame->generation != 0u && frame->payload_length >= NOAH_PROFILE_BLOB_V1_HEADER_SIZE && frame->offset < frame->payload_length;
    }
    return frame->kind == NOAH_PROFILE_SPLIT_V1_ERROR && frame->status != NOAH_PROFILE_SPLIT_V1_STATUS_OK && frame->status != NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
}

static bool frame_shape_valid(const noah_profile_split_v1_frame_t *frame) {
    if (!frame || frame->kind < NOAH_PROFILE_SPLIT_V1_METADATA || frame->kind > NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE || frame->status > NOAH_PROFILE_SPLIT_V1_STATUS_SOURCE_UNAVAILABLE) {
        return false;
    }
    if (frame->kind != NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE && !reuse_zero(frame)) {
        return false;
    }
    if (frame->kind == 9u || frame->kind == 11u) {
        return false; // retired VIA binding kinds (D-F31)
    }
    if (frame->kind != NOAH_PROFILE_SPLIT_V1_ACK && (frame->busy_reason || frame->busy_store_state || frame->busy_owner || frame->busy_admission)) {
        return false;
    }
    if (descriptor_kind(frame->kind)) {
        if (!noah_profile_split_descriptor_valid(&frame->descriptor) || frame->generation != 0u || frame->payload_digest != 0u || frame->offset != 0u || frame->payload_length != 0u || frame->chunk_length != 0u || !bytes_zero(frame->chunk, sizeof(frame->chunk))) {
            return false;
        }
        if (frame->kind != NOAH_PROFILE_SPLIT_V1_METADATA && (!frame->descriptor.readable || !frame->descriptor.has_profile || frame->status != NOAH_PROFILE_SPLIT_V1_STATUS_OK)) {
            return false;
        }
        return true;
    }
    return transfer_shape_valid(frame);
}

uint8_t noah_profile_split_v1_frame_length(const noah_profile_split_v1_frame_t *frame) {
    if (!frame_shape_valid(frame)) {
        return 0u;
    }
    if (frame->kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK) {
        return chunk_frame_length(frame->chunk_length);
    }
    return frame->kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE ? NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE : NOAH_PROFILE_SPLIT_V1_FRAME_SIZE;
}

uint8_t noah_profile_split_v1_frame_encode(const noah_profile_split_v1_frame_t *frame, uint8_t *out, uint8_t capacity) {
    uint8_t length = noah_profile_split_v1_frame_length(frame);

    if (!out || length == 0u || length > capacity) {
        return 0u;
    }
    memset(out, 0, length);
    out[WIRE_VERSION] = NOAH_PROFILE_SPLIT_PROTOCOL_VERSION;
    out[WIRE_KIND]    = (uint8_t)frame->kind;
    out[WIRE_STATUS]  = (uint8_t)frame->status;
    if (descriptor_kind(frame->kind)) {
        out[WIRE_FLAGS]        = (frame->descriptor.has_profile ? WIRE_FLAG_HAS_PROFILE : 0u) | (frame->descriptor.readable ? WIRE_FLAG_READABLE : 0u) | (frame->descriptor.origin_half ? WIRE_FLAG_ORIGIN_HALF : 0u) | (frame->descriptor.profile_flags ? WIRE_FLAG_PROFILE_OVERRIDE : 0u);
        out[WIRE_SCHEMA_MAJOR] = frame->descriptor.schema_major;
        out[WIRE_SCHEMA_MINOR] = frame->descriptor.schema_minor;
        write_u32(&out[WIRE_PAYLOAD_LENGTH], frame->descriptor.payload_length);
        write_u32(&out[WIRE_GENERATION], frame->descriptor.generation);
        write_u32(&out[WIRE_PAYLOAD_CRC32], frame->descriptor.payload_crc32);
        write_u32(&out[WIRE_PAYLOAD_DIGEST], frame->descriptor.payload_digest);
        write_u32(&out[WIRE_COMPILED_DIGEST], frame->descriptor.compiled_default_digest);
        write_u32(&out[WIRE_ACTION_ABI], frame->descriptor.action_abi_digest);
        out[WIRE_DOMAIN_MASK] = frame->descriptor.domain_mask;
    } else if (frame->kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE) {
        write_u32(&out[WIRE_TRANSFER_GEN], frame->generation);
        write_u32(&out[WIRE_TRANSFER_DIGEST], frame->payload_digest);
        write_u32(&out[WIRE_TRANSFER_OFFSET], frame->offset);
        write_u32(&out[WIRE_TRANSFER_LENGTH], frame->payload_length);
        write_u32(&out[WIRE_REUSE_LENGTH], frame->reuse_length);
        write_u32(&out[WIRE_REUSE_SOURCE_OFF], frame->reuse_source_offset);
        write_u32(&out[WIRE_REUSE_SOURCE_GEN], frame->reuse_source.generation);
        write_u32(&out[WIRE_REUSE_SOURCE_DIG], frame->reuse_source.digest);
        write_u32(&out[WIRE_REUSE_SOURCE_CRC], frame->reuse_source.crc32);
        out[WIRE_REUSE_SOURCE_KIND]   = frame->reuse_source.kind;
        out[WIRE_REUSE_SOURCE_ORIGIN] = frame->reuse_source.origin;
    } else {
        write_u32(&out[WIRE_TRANSFER_GEN], frame->generation);
        write_u32(&out[WIRE_TRANSFER_DIGEST], frame->payload_digest);
        write_u32(&out[WIRE_TRANSFER_OFFSET], frame->offset);
        write_u32(&out[WIRE_TRANSFER_LENGTH], frame->payload_length);
        out[WIRE_CHUNK_LENGTH] = frame->chunk_length;
        memcpy(&out[WIRE_CHUNK], frame->chunk, frame->chunk_length);
        if (busy_frame(frame)) {
            out[WIRE_BUSY_REASON]      = frame->busy_reason;
            out[WIRE_BUSY_STORE_STATE] = frame->busy_store_state;
            out[WIRE_BUSY_OWNER]       = frame->busy_owner;
            out[WIRE_BUSY_ADMISSION]   = frame->busy_admission;
        }
    }
    out[length - 1u] = crc8(out, (size_t)length - 1u);
    return length;
}

bool noah_profile_split_v1_frame_decode(const uint8_t *wire, uint8_t length, noah_profile_split_v1_frame_t *frame) {
    noah_profile_split_v1_frame_t decoded = {0};
    uint8_t                       expected;

    if (!wire || !frame || length < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE || length > NOAH_PROFILE_SPLIT_V1_FRAME_MAX || wire[WIRE_VERSION] != NOAH_PROFILE_SPLIT_PROTOCOL_VERSION) {
        return false;
    }
    expected = declared_frame_length(wire);
    if (expected == 0u || length != expected || wire[length - 1u] != crc8(wire, (size_t)length - 1u)) {
        return false;
    }
    decoded.kind   = (noah_profile_split_v1_kind_t)wire[WIRE_KIND];
    decoded.status = (noah_profile_split_v1_status_t)wire[WIRE_STATUS];
    if (descriptor_kind(decoded.kind)) {
        if ((wire[WIRE_FLAGS] & (uint8_t)~WIRE_DESCRIPTOR_FLAGS) != 0u) {
            return false;
        }
        decoded.descriptor = (noah_profile_split_descriptor_t){
            .generation              = read_u32(&wire[WIRE_GENERATION]),
            .payload_crc32           = read_u32(&wire[WIRE_PAYLOAD_CRC32]),
            .payload_digest          = read_u32(&wire[WIRE_PAYLOAD_DIGEST]),
            .compiled_default_digest = read_u32(&wire[WIRE_COMPILED_DIGEST]),
            .action_abi_digest       = read_u32(&wire[WIRE_ACTION_ABI]),
            .payload_length          = read_u32(&wire[WIRE_PAYLOAD_LENGTH]),
            .schema_major            = wire[WIRE_SCHEMA_MAJOR],
            .schema_minor            = wire[WIRE_SCHEMA_MINOR],
            .domain_mask             = wire[WIRE_DOMAIN_MASK],
            .profile_flags           = (wire[WIRE_FLAGS] & WIRE_FLAG_PROFILE_OVERRIDE) != 0u ? NOAH_PROFILE_STORE_FLAG_OVERRIDE : 0u,
            .origin_half             = (wire[WIRE_FLAGS] & WIRE_FLAG_ORIGIN_HALF) != 0u ? 1u : 0u,
            .readable                = (wire[WIRE_FLAGS] & WIRE_FLAG_READABLE) != 0u,
            .has_profile             = (wire[WIRE_FLAGS] & WIRE_FLAG_HAS_PROFILE) != 0u,
        };
    } else if (decoded.kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE) {
        if (wire[WIRE_FLAGS] != 0u || wire[WIRE_REUSE_RESERVED] != 0u) {
            return false;
        }
        decoded.generation          = read_u32(&wire[WIRE_TRANSFER_GEN]);
        decoded.payload_digest      = read_u32(&wire[WIRE_TRANSFER_DIGEST]);
        decoded.offset              = read_u32(&wire[WIRE_TRANSFER_OFFSET]);
        decoded.payload_length      = read_u32(&wire[WIRE_TRANSFER_LENGTH]);
        decoded.reuse_length        = read_u32(&wire[WIRE_REUSE_LENGTH]);
        decoded.reuse_source_offset = read_u32(&wire[WIRE_REUSE_SOURCE_OFF]);
        decoded.reuse_source        = (noah_profile_split_v1_source_t){
            .generation = read_u32(&wire[WIRE_REUSE_SOURCE_GEN]),
            .digest     = read_u32(&wire[WIRE_REUSE_SOURCE_DIG]),
            .crc32      = read_u32(&wire[WIRE_REUSE_SOURCE_CRC]),
            .kind       = wire[WIRE_REUSE_SOURCE_KIND],
            .origin     = wire[WIRE_REUSE_SOURCE_ORIGIN],
        };
    } else {
        bool    busy = decoded.kind == NOAH_PROFILE_SPLIT_V1_ACK && decoded.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
        uint8_t used = busy ? 4u : wire[WIRE_CHUNK_LENGTH];

        if (wire[WIRE_FLAGS] != 0u || wire[WIRE_CHUNK_LENGTH] > (length - 1u) - WIRE_CHUNK || (busy && wire[WIRE_CHUNK_LENGTH] != 0u) || !bytes_zero(&wire[WIRE_CHUNK + used], (size_t)(length - 1u) - (WIRE_CHUNK + used))) {
            return false;
        }
        if (busy) {
            decoded.busy_reason      = wire[WIRE_BUSY_REASON];
            decoded.busy_store_state = wire[WIRE_BUSY_STORE_STATE];
            decoded.busy_owner       = wire[WIRE_BUSY_OWNER];
            decoded.busy_admission   = wire[WIRE_BUSY_ADMISSION];
        }
        decoded.generation     = read_u32(&wire[WIRE_TRANSFER_GEN]);
        decoded.payload_digest = read_u32(&wire[WIRE_TRANSFER_DIGEST]);
        decoded.offset         = read_u32(&wire[WIRE_TRANSFER_OFFSET]);
        decoded.payload_length = read_u32(&wire[WIRE_TRANSFER_LENGTH]);
        decoded.chunk_length   = wire[WIRE_CHUNK_LENGTH];
        memcpy(decoded.chunk, &wire[WIRE_CHUNK], decoded.chunk_length);
    }
    if (!frame_shape_valid(&decoded)) {
        return false;
    }
    *frame = decoded;
    return true;
}

uint8_t noah_profile_split_v1_pulled_chunk_length(uint32_t offset, uint32_t payload_length) {
    uint32_t remaining = offset < payload_length ? payload_length - offset : 0u;

    return remaining < NOAH_PROFILE_SPLIT_V1_CHUNK_MAX ? (uint8_t)remaining : NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;
}

uint8_t noah_profile_split_v1_reply_capacity(const noah_profile_split_v1_frame_t *request) {
    if (noah_profile_split_v1_frame_length(request) == 0u) {
        return 0u;
    }
    return request->kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST ? chunk_frame_length(noah_profile_split_v1_pulled_chunk_length(request->offset, request->payload_length)) : NOAH_PROFILE_SPLIT_V1_FRAME_SIZE;
}

bool noah_profile_split_v1_reply_decode(const uint8_t *wire, uint8_t capacity, noah_profile_split_v1_frame_t *frame) {
    uint8_t length;

    if (!wire || capacity < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE || capacity > NOAH_PROFILE_SPLIT_V1_FRAME_MAX) {
        return false;
    }
    length = declared_frame_length(wire);
    return length != 0u && length <= capacity && bytes_zero(&wire[length], (size_t)(capacity - length)) && noah_profile_split_v1_frame_decode(wire, length, frame);
}

_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_SIZE == 32u, "fixed profile split frames are one reviewed 32-byte QMK RPC frame");
_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_MAX <= UINT8_MAX, "profile split frames are sized by QMK's 8-bit RPC lengths");
_Static_assert(NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE <= NOAH_PROFILE_SPLIT_V1_FRAME_MAX, "a reuse frame must fit the largest request");
_Static_assert(NOAH_PROFILE_SPLIT_V1_CHUNK_MAX == 106u, "pushed and pulled chunks share one 128-byte frame");
_Static_assert(WIRE_ACTION_ABI + 4u + 1u == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, "a descriptor frame fills one 32-byte frame exactly");
_Static_assert(WIRE_REUSE_RESERVED + 2u == NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE, "a reuse frame is its fields, one reserved byte and the CRC");
