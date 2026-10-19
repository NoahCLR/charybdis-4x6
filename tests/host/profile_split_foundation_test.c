#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_validator_v1.h"
#include "users/noah/lib/profile/split/profile_split_authority.h"
#include "users/noah/lib/profile/split/profile_split_protocol_v1.h"
#include "users/noah/lib/profile/storage/profile_store.h"

static noah_profile_split_descriptor_t compiled_descriptor(void) {
    return (noah_profile_split_descriptor_t){
        .compiled_default_digest = UINT32_C(0x11223344),
        .action_abi_digest       = UINT32_C(0x55667788),
        .schema_major            = NOAH_PROFILE_SCHEMA_MAJOR,
        .schema_minor            = 0u,
        .readable                = true,
    };
}

static noah_profile_split_descriptor_t committed_descriptor(uint32_t generation, uint8_t origin, uint32_t digest) {
    noah_profile_split_descriptor_t descriptor = compiled_descriptor();

    descriptor.generation     = generation;
    descriptor.payload_crc32  = UINT32_C(0xA1B2C3D4) ^ digest;
    descriptor.payload_digest = digest;
    descriptor.payload_length = 1089u;
    descriptor.domain_mask    = (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);
    descriptor.profile_flags  = 1u;
    descriptor.origin_half    = origin;
    descriptor.has_profile    = true;
    descriptor.logical        = true;
    return descriptor;
}

static uint8_t test_crc8(const uint8_t *bytes, size_t length) {
    uint8_t crc = 0u;

    for (size_t index = 0u; index < length; index++) {
        crc ^= bytes[index];
        for (uint8_t bit = 0u; bit < 8u; bit++) {
            crc = (crc & 0x80u) != 0u ? (uint8_t)((crc << 1u) ^ 0x07u) : (uint8_t)(crc << 1u);
        }
    }
    return crc;
}

static void refresh_crc_length(uint8_t *wire, uint8_t length) {
    wire[length - 1u] = test_crc8(wire, length - 1u);
}

static void refresh_crc(uint8_t wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    refresh_crc_length(wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
}

static void assert_descriptors_equal(const noah_profile_split_descriptor_t *actual, const noah_profile_split_descriptor_t *expected) {
    assert(actual->generation == expected->generation);
    assert(actual->payload_crc32 == expected->payload_crc32);
    assert(actual->payload_digest == expected->payload_digest);
    assert(actual->compiled_default_digest == expected->compiled_default_digest);
    assert(actual->action_abi_digest == expected->action_abi_digest);
    assert(actual->payload_length == expected->payload_length);
    assert(actual->schema_major == expected->schema_major);
    assert(actual->schema_minor == expected->schema_minor);
    assert(actual->domain_mask == expected->domain_mask);
    assert(actual->profile_flags == expected->profile_flags);
    assert(actual->origin_half == expected->origin_half);
    assert(actual->readable == expected->readable);
    assert(actual->has_profile == expected->has_profile);
    assert(actual->logical == expected->logical);
}

static void test_authority_decision_table(void) {
    noah_profile_split_descriptor_t absent     = compiled_descriptor();
    noah_profile_split_descriptor_t local      = committed_descriptor(7u, 0u, UINT32_C(0x10203040));
    noah_profile_split_descriptor_t peer       = local;
    noah_profile_split_descriptor_t unreadable = {0};

    assert(noah_profile_split_authority_compare(&unreadable, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_UNREADABLE);
    assert(noah_profile_split_authority_compare(&absent, &unreadable) == NOAH_PROFILE_SPLIT_AUTHORITY_PEER_UNREADABLE);
    assert(noah_profile_split_authority_compare(&absent, &absent) == NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED);
    assert(noah_profile_split_authority_compare(&local, &absent) == NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_NEWER);
    assert(noah_profile_split_authority_compare(&absent, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER);
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED);

    peer.generation = 8u;
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER);
    peer.generation = 6u;
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_NEWER);

    peer             = local;
    peer.origin_half = 1u;
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_CONCURRENT_COMMIT);
    peer = local;
    peer.payload_digest ^= 1u;
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_CORRUPT_SAME_TUPLE);
    peer = local;
    peer.payload_crc32 ^= 1u;
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_CORRUPT_SAME_TUPLE);
    peer = local;
    peer.domain_mask ^= NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_CORRUPT_SAME_TUPLE);
    peer = local;
    peer.compiled_default_digest ^= 1u;
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_INCOMPATIBLE);
    peer = local;
    peer.action_abi_digest ^= 1u;
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_INCOMPATIBLE);

    peer             = local;
    peer.domain_mask = 0x80u;
    assert(!noah_profile_split_descriptor_valid(&peer));
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_INVALID_METADATA);

    peer               = local;
    local.schema_major = 1u;
    peer.schema_major  = 1u;
    assert(!noah_profile_split_descriptor_valid(&local));
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_INVALID_METADATA);

    local         = committed_descriptor(7u, 0u, UINT32_C(0x10203040));
    local.logical = false;
    assert(!noah_profile_split_descriptor_valid(&local));

    local            = committed_descriptor(7u, 0u, UINT32_C(0x10203040));
    peer             = local;
    peer.has_profile = false;
    assert(!noah_profile_split_descriptor_valid(&peer));
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_INVALID_METADATA);
}

static void test_authority_publication_and_activation_observer(void) {
    noah_profile_split_authority_t        authority;
    noah_profile_split_authority_status_t status;
    noah_profile_split_descriptor_t       absent     = compiled_descriptor();
    noah_profile_split_descriptor_t       local      = committed_descriptor(9u, 1u, UINT32_C(0x99887766));
    noah_profile_split_descriptor_t       peer       = local;
    uint8_t                               unresolved = 99u;

    noah_profile_split_authority_init(&authority);
    assert(noah_profile_split_authority_peer_observer(&authority, &unresolved));
    assert(unresolved == 1u);

    assert(noah_profile_split_authority_publish(&authority, absent, absent, false));
    assert(noah_profile_split_authority_peer_observer(&authority, &unresolved));
    assert(unresolved == 0u);
    assert(noah_profile_split_authority_publish(&authority, local, peer, true));
    assert(noah_profile_split_authority_peer_observer(&authority, &unresolved));
    assert(unresolved == 1u);
    assert(noah_profile_split_authority_publish(&authority, local, peer, false));
    assert(noah_profile_split_authority_peer_observer(&authority, &unresolved));
    assert(unresolved == 0u);

    peer.schema_major = 0u;
    assert(!noah_profile_split_authority_publish(&authority, local, peer, false));
    assert(noah_profile_split_authority_status(&authority, &status));
    assert(status.state == NOAH_PROFILE_SPLIT_AUTHORITY_INVALID_METADATA);
    assert(status.publication_count == 4u);
    assert(noah_profile_split_authority_peer_observer(&authority, &unresolved));
    assert(unresolved == 1u);

    authority.status.publication_count = UINT32_MAX;
    assert(noah_profile_split_authority_publish(&authority, absent, absent, false));
    assert(authority.status.publication_count == UINT32_MAX);

    authority.publication_sequence = 1u;
    status.state                   = NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED;
    assert(!noah_profile_split_authority_status(&authority, &status));
    assert(status.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED);
    assert(!noah_profile_split_authority_peer_observer(&authority, &unresolved));
}

static void assert_round_trip(const noah_profile_split_v1_frame_t *expected) {
    uint8_t                       wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX];
    noah_profile_split_v1_frame_t decoded;
    uint8_t                       length = noah_profile_split_v1_frame_encode(expected, wire, sizeof(wire));

    assert(length != 0u && length == noah_profile_split_v1_frame_length(expected));
    assert(noah_profile_split_v1_frame_decode(wire, length, &decoded));
    assert(decoded.kind == expected->kind);
    assert(decoded.status == expected->status);
    assert_descriptors_equal(&decoded.descriptor, &expected->descriptor);
    assert(decoded.generation == expected->generation);
    assert(decoded.payload_digest == expected->payload_digest);
    assert(decoded.offset == expected->offset);
    assert(decoded.payload_length == expected->payload_length);
    assert(decoded.chunk_length == expected->chunk_length);
    assert(memcmp(decoded.chunk, expected->chunk, sizeof(decoded.chunk)) == 0);
    assert(decoded.store_format_version == expected->store_format_version);
    assert(decoded.via_generation == expected->via_generation);
    assert(decoded.via_digest == expected->via_digest);
    assert(decoded.reuse_length == expected->reuse_length);
    assert(decoded.reuse_source_offset == expected->reuse_source_offset);
    assert(decoded.reuse_source.generation == expected->reuse_source.generation);
    assert(decoded.reuse_source.digest == expected->reuse_source.digest);
    assert(decoded.reuse_source.crc32 == expected->reuse_source.crc32);
    assert(decoded.reuse_source.kind == expected->reuse_source.kind);
    assert(decoded.reuse_source.origin == expected->reuse_source.origin);
    assert(decoded.busy_reason == expected->busy_reason);
}

static void test_protocol_golden_frames(void) {
    static const uint8_t metadata_golden[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {
        0x03, 0x01, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55, 0x9B,
    };
    static const uint8_t begin_golden[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {
        0x03, 0x02, 0x00, 0x1F, 0x04, 0x00, 0x03, 0x41, 0x04, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0xD4, 0xC3, 0xB2, 0xA1, 0x40, 0x30, 0x20, 0x10, 0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55, 0x02,
    };
    static const uint8_t chunk_golden[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {
        0x03, 0x03, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x40, 0x30, 0x20, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x41, 0x04, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x2C,
    };
    // Version 3 (D-F28): 32-bit offsets and lengths; PAYLOAD_REUSE is 44 bytes.
    static const uint8_t reuse_golden[NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE] = {
        0x03, 0x0C, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x40, 0x30, 0x20, 0x10, 0x00, 0x01, 0x00, 0x00, 0x41, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0xBE, 0xBA, 0xFE, 0xCA, 0x0D, 0xF0, 0xAD, 0x0B, 0x01, 0x00, 0x00, 0x1E,
    };
    static const uint8_t request_golden[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {
        0x03, 0x08, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x40, 0x30, 0x20, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x41, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45,
    };
    noah_profile_split_v1_frame_t metadata = {.kind = NOAH_PROFILE_SPLIT_V1_METADATA, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = compiled_descriptor()};
    noah_profile_split_v1_frame_t begin    = {.kind = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = committed_descriptor(7u, 1u, UINT32_C(0x10203040))};
    noah_profile_split_v1_frame_t chunk    = {
        .kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK,
        .status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation     = 7u,
        .payload_digest = UINT32_C(0x10203040),
        .offset         = 14u,
        .payload_length = 1089u,
        .chunk_length   = 10u,
        .chunk          = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u},
    };
    noah_profile_split_v1_frame_t request = {
        .kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST,
        .status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation     = 7u,
        .payload_digest = UINT32_C(0x10203040),
        .offset         = 14u,
        .payload_length = 1089u,
    };
    noah_profile_split_v1_frame_t reuse = {
        .kind                = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE,
        .status              = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation          = 7u,
        .payload_digest      = UINT32_C(0x10203040),
        .offset              = 0x0100u,
        .payload_length      = 1089u,
        .reuse_length        = 0x0200u,
        .reuse_source_offset = 0x0080u,
        .reuse_source        = {.generation = 5u, .digest = UINT32_C(0xCAFEBABE), .crc32 = UINT32_C(0x0BADF00D), .kind = 1u, .origin = 0u},
    };
    noah_profile_split_v1_frame_t full_chunk = chunk;
    uint8_t                       wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX];

    // The fixed begin CRC uses the exact descriptor CRC, not the helper's
    // digest-derived fixture value.
    begin.descriptor.payload_crc32 = UINT32_C(0xA1B2C3D4);
    assert(noah_profile_split_v1_frame_encode(&metadata, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    assert(memcmp(wire, metadata_golden, sizeof(metadata_golden)) == 0);
    assert(noah_profile_split_v1_frame_encode(&begin, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    assert(memcmp(wire, begin_golden, sizeof(begin_golden)) == 0);
    assert(noah_profile_split_v1_frame_encode(&chunk, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    assert(memcmp(wire, chunk_golden, sizeof(chunk_golden)) == 0);
    assert(noah_profile_split_v1_frame_encode(&request, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    assert(memcmp(wire, request_golden, sizeof(request_golden)) == 0);
    assert(noah_profile_split_v1_frame_encode(&reuse, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE);
    assert(memcmp(wire, reuse_golden, sizeof(reuse_golden)) == 0);
    // A request's capacity bounds the encoding.
    assert(noah_profile_split_v1_frame_encode(&reuse, wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE) == 0u);

    // A full chunk fills the largest request: its header, 106 bytes and the
    // CRC, which follows the chunk directly.
    full_chunk.chunk_length = NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;
    for (uint8_t index = 0u; index < NOAH_PROFILE_SPLIT_V1_CHUNK_MAX; index++) {
        full_chunk.chunk[index] = (uint8_t)(index * 3u + 1u);
    }
    assert(noah_profile_split_v1_frame_encode(&full_chunk, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_MAX);
    assert(wire[20] == NOAH_PROFILE_SPLIT_V1_CHUNK_MAX && memcmp(&wire[21], full_chunk.chunk, NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) == 0);
    assert(wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX - 1u] == test_crc8(wire, NOAH_PROFILE_SPLIT_V1_FRAME_MAX - 1u));
    assert_round_trip(&full_chunk);
    full_chunk.chunk_length = 11u;
    memset(&full_chunk.chunk[11], 0, sizeof(full_chunk.chunk) - 11u);
    assert(noah_profile_split_v1_frame_length(&full_chunk) == 33u);
    assert_round_trip(&full_chunk);

    assert_round_trip(&metadata);
    assert_round_trip(&begin);
    assert_round_trip(&chunk);
    assert_round_trip(&request);
    assert_round_trip(&reuse);
    reuse.reuse_source = (noah_profile_split_v1_source_t){.digest = UINT32_C(0xCAFEBABE), .crc32 = UINT32_C(0x0BADF00D), .kind = 0u, .origin = 255u};
    assert_round_trip(&reuse);
    assert_round_trip(&(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_ERROR, .status = NOAH_PROFILE_SPLIT_V1_STATUS_SOURCE_UNAVAILABLE, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .offset = 0x0180u, .payload_length = 1089u});
    assert_round_trip(&(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_ACK, .status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .offset = 0x0180u, .payload_length = 1089u, .busy_reason = NOAH_PROFILE_SPLIT_V1_BUSY_COPYING});
    assert_round_trip(&(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .store_format_version = NOAH_PROFILE_STORE_FORMAT_VERSION, .via_generation = 9u, .via_digest = UINT32_C(0x55667788)});
    assert_round_trip(&(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND_REQUEST, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .generation = 7u, .payload_digest = UINT32_C(0x10203040)});
    assert_round_trip(&(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = begin.descriptor});
    assert_round_trip(&(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_ACK, .status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .offset = 28u, .payload_length = 1089u});
    assert_round_trip(&(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_ERROR, .status = NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT, .generation = 7u, .payload_digest = UINT32_C(0x10203040)});
}

// Version 2's sized chunks and reuse frames: each kind has exactly one length
// and every field outside its layout stays zero.
static void test_protocol_rejects_malformed_sized_frames(void) {
    noah_profile_split_v1_frame_t chunk = {
        .kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK,
        .status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation     = 3u,
        .payload_digest = 4u,
        .payload_length = 400u,
        .chunk_length   = 60u,
    };
    noah_profile_split_v1_frame_t reuse = {
        .kind                = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE,
        .status              = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation          = 3u,
        .payload_digest      = 4u,
        .offset              = 100u,
        .payload_length      = 400u,
        .reuse_length        = 300u,
        .reuse_source_offset = 20u,
        .reuse_source        = {.generation = 2u, .digest = 5u, .crc32 = 6u, .kind = 1u, .origin = 1u},
    };
    noah_profile_split_v1_frame_t probe;
    noah_profile_split_v1_frame_t decoded;
    uint8_t                       wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX];
    uint8_t                       length;

    length = noah_profile_split_v1_frame_encode(&chunk, wire, sizeof(wire));
    assert(length == 82u);
    assert(noah_profile_split_v1_frame_decode(wire, length, &decoded));
    assert(!noah_profile_split_v1_frame_decode(wire, length - 1u, &decoded));
    assert(!noah_profile_split_v1_frame_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, &decoded));
    // Beyond the largest request, whatever the frame claims.
    wire[20] = NOAH_PROFILE_SPLIT_V1_CHUNK_MAX + 1u;
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    probe              = chunk;
    probe.chunk_length = NOAH_PROFILE_SPLIT_V1_CHUNK_MAX + 1u;
    assert(noah_profile_split_v1_frame_length(&probe) == 0u);
    probe              = chunk;
    probe.reuse_length = 1u;
    assert(noah_profile_split_v1_frame_encode(&probe, wire, sizeof(wire)) == 0u);
    // A 32-byte frame of another kind has room for 10 chunk bytes at most.
    probe = (noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_ACK, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .generation = 3u, .payload_digest = 4u, .offset = 0u, .payload_length = 400u};
    assert(noah_profile_split_v1_frame_encode(&probe, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    wire[20] = 11u;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, &decoded));

    length = noah_profile_split_v1_frame_encode(&reuse, wire, sizeof(wire));
    assert(length == NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE);
    assert(noah_profile_split_v1_frame_decode(wire, length, &decoded));
    assert(!noah_profile_split_v1_frame_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, &decoded));
    wire[42] = 1u;
    refresh_crc_length(wire, length);
    assert(!noah_profile_split_v1_frame_decode(wire, length, &decoded));
    (void)noah_profile_split_v1_frame_encode(&reuse, wire, sizeof(wire));
    wire[40] = 2u;
    refresh_crc_length(wire, length);
    assert(!noah_profile_split_v1_frame_decode(wire, length, &decoded));

    probe              = reuse;
    probe.reuse_length = 0u;
    assert(noah_profile_split_v1_frame_length(&probe) == 0u);
    probe              = reuse;
    probe.reuse_length = 301u;
    assert(noah_profile_split_v1_frame_length(&probe) == 0u);
    probe        = reuse;
    probe.status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
    assert(noah_profile_split_v1_frame_length(&probe) == 0u);
    probe                   = reuse;
    probe.reuse_source.kind = 0u;
    assert(noah_profile_split_v1_frame_length(&probe) == 0u);
    probe                     = reuse;
    probe.reuse_source.origin = 2u;
    assert(noah_profile_split_v1_frame_length(&probe) == 0u);
    probe              = reuse;
    probe.chunk_length = 1u;
    probe.chunk[0]     = 1u;
    assert(noah_profile_split_v1_frame_length(&probe) == 0u);
    probe              = reuse;
    probe.reuse_source = (noah_profile_split_v1_source_t){.generation = 1u, .kind = 0u, .origin = 255u};
    assert(noah_profile_split_v1_frame_length(&probe) == 0u);

    // Earlier framing versions are refused.
    for (uint8_t version = 1u; version < NOAH_PROFILE_SPLIT_PROTOCOL_VERSION; version++) {
        (void)noah_profile_split_v1_frame_encode(&reuse, wire, sizeof(wire));
        wire[0] = version;
        refresh_crc_length(wire, NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE);
        assert(!noah_profile_split_v1_frame_decode(wire, NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE, &decoded));
    }
}

static void test_protocol_rejects_malformed_frames(void) {
    noah_profile_split_v1_frame_t frame = {
        .kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK,
        .status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation     = 3u,
        .payload_digest = 4u,
        .payload_length = 20u,
        .chunk_length   = 1u,
        .chunk          = {0xA5u},
    };
    noah_profile_split_v1_frame_t decoded;
    uint8_t                       wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

    assert(noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    for (uint8_t length = 0u; length < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE; length++) {
        assert(!noah_profile_split_v1_frame_decode(wire, length, &decoded));
    }
    wire[31] ^= 1u;
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));

    assert(noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    wire[3] = 1u;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    assert(noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    wire[30] = 1u;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    assert(noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    // A chunk length the frame's length does not hold.
    wire[20] = 11u;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    assert(noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    wire[1] = 0xFFu;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    assert(noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    wire[2] = 0xFFu;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));

    frame.offset       = 20u;
    frame.chunk_length = 1u;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    frame.offset       = 0u;
    frame.chunk_length = 0u;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    frame.kind   = NOAH_PROFILE_SPLIT_V1_ACK;
    frame.status = NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    frame.kind   = NOAH_PROFILE_SPLIT_V1_ERROR;
    frame.status = NOAH_PROFILE_SPLIT_V1_STATUS_OK;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    frame.kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST;
    frame.status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK;
    frame.generation     = 3u;
    frame.payload_length = 20u;
    frame.offset         = 20u;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    frame.offset       = 0u;
    frame.chunk_length = 1u;
    frame.chunk[0]     = 0xA5u;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)));
    test_protocol_rejects_malformed_sized_frames();
}

// Version 3 packs a descriptor's origin half and override flag into its flags
// byte (bits 3 and 4) beside a 32-bit length; other flag bits are refused.
static void test_protocol_descriptor_flags(void) {
    noah_profile_split_v1_frame_t frame = {.kind = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK};
    noah_profile_split_v1_frame_t decoded;
    uint8_t                       wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

    for (uint8_t origin = 0u; origin < 2u; origin++) {
        for (uint8_t flags = 0u; flags < 2u; flags++) {
            frame.descriptor               = committed_descriptor(9u, origin, UINT32_C(0x01020304));
            frame.descriptor.profile_flags = flags;
            assert(noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
            assert(wire[3] == (uint8_t)(0x07u | (origin << 3u) | (flags << 4u)));
            assert(wire[7] == (uint8_t)1089u && wire[8] == (uint8_t)(1089u >> 8u) && wire[9] == 0u && wire[10] == 0u);
            assert_round_trip(&frame);
        }
    }
    for (uint8_t bit = 5u; bit < 8u; bit++) {
        assert(noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
        wire[3] = (uint8_t)(wire[3] | (1u << bit));
        refresh_crc(wire);
        assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    }
    // A length differing only in its high bytes is that length, refused here
    // as past the schema's ceiling rather than read as its low 16 bits.
    assert(noah_profile_split_v1_frame_encode(&frame, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    wire[10] = 1u;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
}

// A pulled chunk travels in a reply as large as a pushed one (D-F28): the
// requester sizes the reply buffer from its PAYLOAD_REQUEST, and the peer's
// reply starts it, the rest zero.
static void test_protocol_reply_buffers(void) {
    noah_profile_split_v1_frame_t request = {.kind = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .offset = 0u, .payload_length = 1089u};
    noah_profile_split_v1_frame_t chunk   = {.kind = NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .offset = 0u, .payload_length = 1089u};
    noah_profile_split_v1_frame_t error   = {.kind = NOAH_PROFILE_SPLIT_V1_ERROR, .status = NOAH_PROFILE_SPLIT_V1_STATUS_STALE, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .offset = 0u, .payload_length = 1089u};
    noah_profile_split_v1_frame_t ack     = {.kind = NOAH_PROFILE_SPLIT_V1_ACK, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .payload_length = 1089u};
    noah_profile_split_v1_frame_t decoded;
    uint8_t                       wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX];

    assert(noah_profile_split_v1_pulled_chunk_length(0u, 1089u) == NOAH_PROFILE_SPLIT_V1_CHUNK_MAX);
    assert(noah_profile_split_v1_pulled_chunk_length(1089u - 106u, 1089u) == 106u);
    assert(noah_profile_split_v1_pulled_chunk_length(1080u, 1089u) == 9u);
    assert(noah_profile_split_v1_pulled_chunk_length(1089u, 1089u) == 0u);
    assert(noah_profile_split_v1_reply_capacity(&request) == NOAH_PROFILE_SPLIT_V1_FRAME_MAX);
    request.offset = 1080u;
    assert(noah_profile_split_v1_reply_capacity(&request) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    request.offset = 1089u - 20u;
    assert(noah_profile_split_v1_reply_capacity(&request) == NOAH_PROFILE_SPLIT_V1_CHUNK_OVERHEAD + 20u);
    assert(noah_profile_split_v1_reply_capacity(&ack) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    request.offset = 1089u;
    assert(noah_profile_split_v1_reply_capacity(&request) == 0u);
    request.offset = 0u;

    // A full chunk fills the largest buffer exactly.
    chunk.chunk_length = NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;
    for (uint8_t index = 0u; index < NOAH_PROFILE_SPLIT_V1_CHUNK_MAX; index++) {
        chunk.chunk[index] = (uint8_t)(index + 5u);
    }
    assert(noah_profile_split_v1_frame_encode(&chunk, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_MAX);
    assert(noah_profile_split_v1_reply_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_MAX, &decoded));
    assert(decoded.kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK && decoded.chunk_length == NOAH_PROFILE_SPLIT_V1_CHUNK_MAX && memcmp(decoded.chunk, chunk.chunk, NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) == 0);
    // ...and never fits a smaller one.
    assert(!noah_profile_split_v1_reply_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_MAX - 1u, &decoded));
    assert(!noah_profile_split_v1_reply_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, &decoded));

    // An ERROR or BUSY answering a pull is a 32-byte frame in its buffer.
    memset(wire, 0, sizeof(wire));
    assert(noah_profile_split_v1_frame_encode(&error, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    assert(noah_profile_split_v1_reply_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_MAX, &decoded));
    assert(decoded.kind == NOAH_PROFILE_SPLIT_V1_ERROR && decoded.status == NOAH_PROFILE_SPLIT_V1_STATUS_STALE);
    assert(noah_profile_split_v1_reply_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, &decoded));
    // Anything left past the frame is a torn or stale reply.
    wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX - 1u] = 1u;
    assert(!noah_profile_split_v1_reply_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_MAX, &decoded));
    wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX - 1u] = 0u;
    wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]     = 1u;
    assert(!noah_profile_split_v1_reply_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_MAX, &decoded));
    wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = 0u;
    // Buffers outside 32..128 bytes are refused, as is a frame whose CRC fails.
    assert(!noah_profile_split_v1_reply_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u, &decoded));
    assert(!noah_profile_split_v1_reply_decode(NULL, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, &decoded));
    wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u] ^= 1u;
    assert(!noah_profile_split_v1_reply_decode(wire, NOAH_PROFILE_SPLIT_V1_FRAME_MAX, &decoded));
}

#ifdef NOAH_PROFILE_TEST_WIDE_SLOTS
// The split copy past 65,535 bytes (D-F28): a descriptor, chunk, pull request
// and reuse range of a 100,000-byte copy round-trip with every field whole.
static void test_protocol_over_64_kib(void) {
    noah_profile_split_v1_frame_t begin = {.kind = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = committed_descriptor(7u, 0u, UINT32_C(0x10203040))};
    noah_profile_split_v1_frame_t chunk = {.kind = NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .offset = 70000u, .payload_length = 100000u, .chunk_length = 3u, .chunk = {1u, 2u, 3u}};
    noah_profile_split_v1_frame_t request = {.kind = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .offset = 99999u, .payload_length = 100000u};
    noah_profile_split_v1_frame_t reuse = {
        .kind                = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE,
        .status              = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation          = 7u,
        .payload_digest      = UINT32_C(0x10203040),
        .offset              = 66000u,
        .payload_length      = 100000u,
        .reuse_length        = 20000u,
        .reuse_source_offset = 67000u,
        .reuse_source        = {.generation = 5u, .digest = 6u, .crc32 = 7u, .kind = 1u, .origin = 1u},
    };
    uint8_t wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX];

    begin.descriptor.payload_length = 100000u;
    assert_round_trip(&begin);
    assert_round_trip(&chunk);
    assert(noah_profile_split_v1_frame_encode(&chunk, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    assert(wire[12] == (uint8_t)70000u && wire[13] == (uint8_t)(70000u >> 8u) && wire[14] == 1u && wire[15] == 0u);
    assert(wire[16] == (uint8_t)100000u && wire[17] == (uint8_t)(100000u >> 8u) && wire[18] == 1u && wire[19] == 0u);
    assert_round_trip(&request);
    assert_round_trip(&reuse);
    assert(noah_profile_split_v1_frame_encode(&reuse, wire, sizeof(wire)) == NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE);
    assert(wire[24] == (uint8_t)67000u && wire[25] == (uint8_t)(67000u >> 8u) && wire[26] == 1u);
    // A range past the copy, or one reaching past the source's ceiling, is
    // refused.
    reuse.reuse_length = 34001u;
    assert(noah_profile_split_v1_frame_length(&reuse) == 0u);
    reuse.reuse_length        = 20000u;
    reuse.reuse_source_offset = NOAH_PROFILE_BLOB_V1_MAX_SIZE - 19999u;
    assert(noah_profile_split_v1_frame_length(&reuse) == 0u);
}
#endif

int main(void) {
#ifdef NOAH_PROFILE_TEST_WIDE_SLOTS
    test_protocol_over_64_kib();
#endif
    test_protocol_descriptor_flags();
    test_authority_decision_table();
    test_authority_publication_and_activation_observer();
    test_protocol_golden_frames();
    test_protocol_rejects_malformed_frames();
    test_protocol_reply_buffers();
    puts("profile split foundation host tests passed");
    return 0;
}
