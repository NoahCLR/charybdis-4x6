#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_validator_v1.h"
#include "users/noah/lib/profile/split/profile_split_authority.h"
#include "users/noah/lib/profile/split/profile_split_protocol_v1.h"

static noah_profile_split_descriptor_t compiled_descriptor(void) {
    return (noah_profile_split_descriptor_t){
        .compiled_default_digest = UINT32_C(0x11223344),
        .action_abi_digest       = UINT32_C(0x55667788),
        .schema_major            = 1u,
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
    descriptor.domain_mask    = NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS;
    descriptor.profile_flags  = 1u;
    descriptor.origin_half    = origin;
    descriptor.has_profile    = true;
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

static void refresh_crc(uint8_t wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u] = test_crc8(wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u);
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
    local.schema_major = 2u;
    peer.schema_major  = 2u;
    assert(!noah_profile_split_descriptor_valid(&local));
    assert(noah_profile_split_authority_compare(&local, &peer) == NOAH_PROFILE_SPLIT_AUTHORITY_INVALID_METADATA);

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
    uint8_t                       wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    noah_profile_split_v1_frame_t decoded;

    assert(noah_profile_split_v1_frame_encode(expected, wire));
    assert(noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    assert(decoded.kind == expected->kind);
    assert(decoded.status == expected->status);
    assert_descriptors_equal(&decoded.descriptor, &expected->descriptor);
    assert(decoded.generation == expected->generation);
    assert(decoded.payload_digest == expected->payload_digest);
    assert(decoded.offset == expected->offset);
    assert(decoded.payload_length == expected->payload_length);
    assert(decoded.chunk_length == expected->chunk_length);
    assert(memcmp(decoded.chunk, expected->chunk, sizeof(decoded.chunk)) == 0);
}

static void test_protocol_golden_frames(void) {
    static const uint8_t metadata_golden[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {
        0x01, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55, 0x00, 0xFC,
    };
    static const uint8_t begin_golden[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {
        0x01, 0x02, 0x00, 0x03, 0x01, 0x00, 0x01, 0x01, 0x41, 0x04, 0x07, 0x00, 0x00, 0x00, 0xD4, 0xC3, 0xB2, 0xA1, 0x40, 0x30, 0x20, 0x10, 0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55, 0x03, 0xFD,
    };
    static const uint8_t chunk_golden[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {
        0x01, 0x03, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x40, 0x30, 0x20, 0x10, 0x0E, 0x00, 0x41, 0x04, 0x0E, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x45,
    };
    static const uint8_t request_golden[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {
        0x01, 0x08, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x40, 0x30, 0x20, 0x10, 0x0E, 0x00, 0x41, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBB,
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
        .chunk_length   = 14u,
        .chunk          = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u},
    };
    noah_profile_split_v1_frame_t request = {
        .kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST,
        .status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation     = 7u,
        .payload_digest = UINT32_C(0x10203040),
        .offset         = 14u,
        .payload_length = 1089u,
    };
    uint8_t wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

    // The fixed begin CRC uses the exact descriptor CRC, not the helper's
    // digest-derived fixture value.
    begin.descriptor.payload_crc32 = UINT32_C(0xA1B2C3D4);
    assert(noah_profile_split_v1_frame_encode(&metadata, wire));
    assert(memcmp(wire, metadata_golden, sizeof(wire)) == 0);
    assert(noah_profile_split_v1_frame_encode(&begin, wire));
    assert(memcmp(wire, begin_golden, sizeof(wire)) == 0);
    assert(noah_profile_split_v1_frame_encode(&chunk, wire));
    assert(memcmp(wire, chunk_golden, sizeof(wire)) == 0);
    assert(noah_profile_split_v1_frame_encode(&request, wire));
    assert(memcmp(wire, request_golden, sizeof(wire)) == 0);

    assert_round_trip(&metadata);
    assert_round_trip(&begin);
    assert_round_trip(&chunk);
    assert_round_trip(&request);
    assert_round_trip(&(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_ACK, .status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, .generation = 7u, .payload_digest = UINT32_C(0x10203040), .offset = 28u, .payload_length = 1089u});
    assert_round_trip(&(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_ERROR, .status = NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT, .generation = 7u, .payload_digest = UINT32_C(0x10203040)});
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

    assert(noah_profile_split_v1_frame_encode(&frame, wire));
    for (uint8_t length = 0u; length < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE; length++) {
        assert(!noah_profile_split_v1_frame_decode(wire, length, &decoded));
    }
    wire[31] ^= 1u;
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));

    assert(noah_profile_split_v1_frame_encode(&frame, wire));
    wire[3] = 1u;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    assert(noah_profile_split_v1_frame_encode(&frame, wire));
    wire[30] = 1u;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    assert(noah_profile_split_v1_frame_encode(&frame, wire));
    wire[16] = 15u;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    assert(noah_profile_split_v1_frame_encode(&frame, wire));
    wire[1] = 0xFFu;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));
    assert(noah_profile_split_v1_frame_encode(&frame, wire));
    wire[2] = 0xFFu;
    refresh_crc(wire);
    assert(!noah_profile_split_v1_frame_decode(wire, sizeof(wire), &decoded));

    frame.offset       = 20u;
    frame.chunk_length = 1u;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire));
    frame.offset       = 0u;
    frame.chunk_length = 0u;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire));
    frame.kind   = NOAH_PROFILE_SPLIT_V1_ACK;
    frame.status = NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire));
    frame.kind   = NOAH_PROFILE_SPLIT_V1_ERROR;
    frame.status = NOAH_PROFILE_SPLIT_V1_STATUS_OK;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire));
    frame.kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST;
    frame.status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK;
    frame.generation     = 3u;
    frame.payload_length = 20u;
    frame.offset         = 20u;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire));
    frame.offset       = 0u;
    frame.chunk_length = 1u;
    frame.chunk[0]     = 0xA5u;
    assert(!noah_profile_split_v1_frame_encode(&frame, wire));
}

int main(void) {
    test_authority_decision_table();
    test_authority_publication_and_activation_observer();
    test_protocol_golden_frames();
    test_protocol_rejects_malformed_frames();
    puts("profile split foundation host tests passed");
    return 0;
}
