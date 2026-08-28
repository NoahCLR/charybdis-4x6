// ──────────────────────────────────────────────────────────────────────────
// Live-Profile Split Protocol v1
// ──────────────────────────────────────────────────────────────────────────

#include "profile_split_protocol_v1.h"

#include <stddef.h>
#include <string.h>

#include "../schema/profile_blob_v1.h"

enum {
    WIRE_VERSION          = 0u,
    WIRE_KIND             = 1u,
    WIRE_STATUS           = 2u,
    WIRE_FLAGS            = 3u,
    WIRE_SCHEMA_MAJOR     = 4u,
    WIRE_SCHEMA_MINOR     = 5u,
    WIRE_PROFILE_FLAGS    = 6u,
    WIRE_ORIGIN_HALF      = 7u,
    WIRE_PAYLOAD_LENGTH   = 8u,
    WIRE_GENERATION       = 10u,
    WIRE_PAYLOAD_CRC32    = 14u,
    WIRE_PAYLOAD_DIGEST   = 18u,
    WIRE_COMPILED_DIGEST  = 22u,
    WIRE_ACTION_ABI       = 26u,
    WIRE_DOMAIN_MASK      = 30u,
    WIRE_TRANSFER_GEN     = 4u,
    WIRE_TRANSFER_DIGEST  = 8u,
    WIRE_TRANSFER_OFFSET  = 12u,
    WIRE_TRANSFER_LENGTH  = 14u,
    WIRE_CHUNK_LENGTH     = 16u,
    WIRE_CHUNK            = 17u,
    WIRE_CRC              = 31u,
    WIRE_FLAG_HAS_PROFILE = 1u << 0,
    WIRE_FLAG_READABLE    = 1u << 1,
    WIRE_DESCRIPTOR_FLAGS = WIRE_FLAG_HAS_PROFILE | WIRE_FLAG_READABLE,
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

static void write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8u);
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
    return kind == NOAH_PROFILE_SPLIT_V1_METADATA || kind == NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN || kind == NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT || kind == NOAH_PROFILE_SPLIT_V1_ABORT;
}

static bool descriptor_zero(const noah_profile_split_descriptor_t *descriptor) {
    return descriptor && descriptor->generation == 0u && descriptor->payload_crc32 == 0u && descriptor->payload_digest == 0u && descriptor->compiled_default_digest == 0u && descriptor->action_abi_digest == 0u && descriptor->payload_length == 0u && descriptor->schema_major == 0u && descriptor->schema_minor == 0u && descriptor->domain_mask == 0u && descriptor->profile_flags == 0u && descriptor->origin_half == 0u && !descriptor->readable && !descriptor->has_profile;
}

static bool transfer_shape_valid(const noah_profile_split_v1_frame_t *frame) {
    if (!descriptor_zero(&frame->descriptor) || frame->payload_length > NOAH_PROFILE_BLOB_V1_MAX_SIZE || frame->offset > frame->payload_length) {
        return false;
    }
    if (frame->kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK) {
        return frame->status == NOAH_PROFILE_SPLIT_V1_STATUS_OK && frame->generation != 0u && frame->payload_length >= NOAH_PROFILE_BLOB_V1_HEADER_SIZE && frame->chunk_length != 0u && frame->chunk_length <= NOAH_PROFILE_SPLIT_V1_CHUNK_MAX && frame->chunk_length <= frame->payload_length - frame->offset;
    }
    if (frame->chunk_length != 0u || !bytes_zero(frame->chunk, sizeof(frame->chunk))) {
        return false;
    }
    if (frame->kind == NOAH_PROFILE_SPLIT_V1_ACK) {
        return frame->status == NOAH_PROFILE_SPLIT_V1_STATUS_OK || frame->status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
    }
    return frame->kind == NOAH_PROFILE_SPLIT_V1_ERROR && frame->status != NOAH_PROFILE_SPLIT_V1_STATUS_OK && frame->status != NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
}

static bool frame_shape_valid(const noah_profile_split_v1_frame_t *frame) {
    if (!frame || frame->kind < NOAH_PROFILE_SPLIT_V1_METADATA || frame->kind > NOAH_PROFILE_SPLIT_V1_ERROR || frame->status > NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR) {
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

bool noah_profile_split_v1_frame_encode(const noah_profile_split_v1_frame_t *frame, uint8_t out[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    if (!out || !frame_shape_valid(frame)) {
        return false;
    }
    memset(out, 0, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    out[WIRE_VERSION] = NOAH_PROFILE_SPLIT_V1_PROTOCOL_VERSION;
    out[WIRE_KIND]    = (uint8_t)frame->kind;
    out[WIRE_STATUS]  = (uint8_t)frame->status;
    if (descriptor_kind(frame->kind)) {
        out[WIRE_FLAGS]         = (frame->descriptor.has_profile ? WIRE_FLAG_HAS_PROFILE : 0u) | (frame->descriptor.readable ? WIRE_FLAG_READABLE : 0u);
        out[WIRE_SCHEMA_MAJOR]  = frame->descriptor.schema_major;
        out[WIRE_SCHEMA_MINOR]  = frame->descriptor.schema_minor;
        out[WIRE_PROFILE_FLAGS] = frame->descriptor.profile_flags;
        out[WIRE_ORIGIN_HALF]   = frame->descriptor.origin_half;
        write_u16(&out[WIRE_PAYLOAD_LENGTH], frame->descriptor.payload_length);
        write_u32(&out[WIRE_GENERATION], frame->descriptor.generation);
        write_u32(&out[WIRE_PAYLOAD_CRC32], frame->descriptor.payload_crc32);
        write_u32(&out[WIRE_PAYLOAD_DIGEST], frame->descriptor.payload_digest);
        write_u32(&out[WIRE_COMPILED_DIGEST], frame->descriptor.compiled_default_digest);
        write_u32(&out[WIRE_ACTION_ABI], frame->descriptor.action_abi_digest);
        out[WIRE_DOMAIN_MASK] = frame->descriptor.domain_mask;
    } else {
        write_u32(&out[WIRE_TRANSFER_GEN], frame->generation);
        write_u32(&out[WIRE_TRANSFER_DIGEST], frame->payload_digest);
        write_u16(&out[WIRE_TRANSFER_OFFSET], frame->offset);
        write_u16(&out[WIRE_TRANSFER_LENGTH], frame->payload_length);
        out[WIRE_CHUNK_LENGTH] = frame->chunk_length;
        memcpy(&out[WIRE_CHUNK], frame->chunk, frame->chunk_length);
    }
    out[WIRE_CRC] = crc8(out, WIRE_CRC);
    return true;
}

bool noah_profile_split_v1_frame_decode(const uint8_t *wire, uint8_t length, noah_profile_split_v1_frame_t *frame) {
    noah_profile_split_v1_frame_t decoded = {0};

    if (!wire || !frame || length != NOAH_PROFILE_SPLIT_V1_FRAME_SIZE || wire[WIRE_VERSION] != NOAH_PROFILE_SPLIT_V1_PROTOCOL_VERSION || wire[WIRE_CRC] != crc8(wire, WIRE_CRC)) {
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
            .payload_length          = read_u16(&wire[WIRE_PAYLOAD_LENGTH]),
            .schema_major            = wire[WIRE_SCHEMA_MAJOR],
            .schema_minor            = wire[WIRE_SCHEMA_MINOR],
            .domain_mask             = wire[WIRE_DOMAIN_MASK],
            .profile_flags           = wire[WIRE_PROFILE_FLAGS],
            .origin_half             = wire[WIRE_ORIGIN_HALF],
            .readable                = (wire[WIRE_FLAGS] & WIRE_FLAG_READABLE) != 0u,
            .has_profile             = (wire[WIRE_FLAGS] & WIRE_FLAG_HAS_PROFILE) != 0u,
        };
    } else {
        if (wire[WIRE_FLAGS] != 0u || wire[WIRE_CHUNK_LENGTH] > NOAH_PROFILE_SPLIT_V1_CHUNK_MAX || !bytes_zero(&wire[WIRE_CHUNK + wire[WIRE_CHUNK_LENGTH]], NOAH_PROFILE_SPLIT_V1_CHUNK_MAX - wire[WIRE_CHUNK_LENGTH])) {
            return false;
        }
        decoded.generation     = read_u32(&wire[WIRE_TRANSFER_GEN]);
        decoded.payload_digest = read_u32(&wire[WIRE_TRANSFER_DIGEST]);
        decoded.offset         = read_u16(&wire[WIRE_TRANSFER_OFFSET]);
        decoded.payload_length = read_u16(&wire[WIRE_TRANSFER_LENGTH]);
        decoded.chunk_length   = wire[WIRE_CHUNK_LENGTH];
        memcpy(decoded.chunk, &wire[WIRE_CHUNK], decoded.chunk_length);
    }
    if (!frame_shape_valid(&decoded)) {
        return false;
    }
    *frame = decoded;
    return true;
}

_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_SIZE == 32u, "profile split protocol requires one reviewed QMK RPC frame");
