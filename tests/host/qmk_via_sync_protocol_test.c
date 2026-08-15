#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_via_sync_metadata.h"
#include "users/noah/lib/compat/qmk_via_sync_protocol.h"

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

static void check_round_trip(const noah_qmk_via_sync_frame_t *source) {
    uint8_t                   wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE];
    noah_qmk_via_sync_frame_t decoded;

    CHECK(noah_qmk_via_sync_frame_encode(source, wire));
    CHECK(noah_qmk_via_sync_frame_decode(wire, sizeof(wire), &decoded));
    CHECK(decoded.kind == source->kind);
    CHECK(decoded.status == source->status);
    CHECK(decoded.region == source->region);
    CHECK(decoded.generation == source->generation);
    CHECK(decoded.offset == source->offset);
    CHECK(decoded.region_length == source->region_length);
    CHECK(decoded.digest == source->digest);
    CHECK(decoded.payload_length == source->payload_length);
    CHECK(memcmp(decoded.payload, source->payload, source->payload_length) == 0);
}

static void test_all_message_shapes_round_trip(void) {
    static const noah_qmk_via_sync_frame_t frames[] = {
        {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_METADATA, .generation = 1u, .digest = UINT32_C(0x12345678)}, {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN, .generation = 2u, .digest = UINT32_C(0x87654321)}, {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_KEYMAP, .generation = 3u, .offset = 14u, .region_length = 30u, .digest = 9u, .payload_length = 14u, .payload = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}}, {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PULL_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_MACRO, .generation = 4u, .offset = 28u, .region_length = 30u, .digest = 10u}, {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT, .generation = 5u, .digest = 11u}, {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_ACK, .generation = 6u, .digest = 12u}, {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, .status = NOAH_QMK_VIA_SYNC_STATUS_DIGEST_MISMATCH, .generation = 7u, .digest = 13u},
    };

    for (size_t index = 0u; index < sizeof(frames) / sizeof(frames[0]); index++) {
        check_round_trip(&frames[index]);
    }
}

static void test_corruption_truncation_and_unknown_schema_are_rejected(void) {
    noah_qmk_via_sync_frame_t source = {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_METADATA, .generation = 1u};
    noah_qmk_via_sync_frame_t decoded;
    uint8_t                   wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE];

    CHECK(noah_qmk_via_sync_frame_encode(&source, wire));
    CHECK(!noah_qmk_via_sync_frame_decode(wire, NOAH_QMK_VIA_SYNC_FRAME_SIZE - 1u, &decoded));
    CHECK(!noah_qmk_via_sync_frame_decode(wire, sizeof(wire), NULL));

    wire[8] ^= 0x80u;
    CHECK(!noah_qmk_via_sync_frame_decode(wire, sizeof(wire), &decoded));
    wire[8] ^= 0x80u;

    wire[0] = NOAH_QMK_VIA_SYNC_PROTOCOL_VERSION + 1u;
    CHECK(!noah_qmk_via_sync_frame_decode(wire, sizeof(wire), &decoded));
}

static void test_invalid_shapes_fail_before_wire_effects(void) {
    noah_qmk_via_sync_frame_t frame = {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_KEYMAP, .generation = 1u, .region_length = 10u, .payload_length = 1u, .payload = {1u}};
    uint8_t                   wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE];
    uint8_t                   sentinel[NOAH_QMK_VIA_SYNC_FRAME_SIZE];

    memset(wire, 0xA5, sizeof(wire));
    memcpy(sentinel, wire, sizeof(wire));

    frame.generation = 0u;
    CHECK(!noah_qmk_via_sync_frame_encode(&frame, wire));
    CHECK(memcmp(wire, sentinel, sizeof(wire)) == 0);
    frame.generation = NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK + 1u;
    CHECK(!noah_qmk_via_sync_frame_encode(&frame, wire));
    frame.generation     = 1u;
    frame.payload_length = NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX + 1u;
    CHECK(!noah_qmk_via_sync_frame_encode(&frame, wire));
    frame.payload_length = 1u;
    frame.offset         = 10u;
    CHECK(!noah_qmk_via_sync_frame_encode(&frame, wire));
    frame.offset = 9u;
    frame.region = NOAH_QMK_VIA_SYNC_REGION_NONE;
    CHECK(!noah_qmk_via_sync_frame_encode(&frame, wire));
    frame.region = NOAH_QMK_VIA_SYNC_REGION_KEYMAP;
    frame.kind   = (noah_qmk_via_sync_message_kind_t)0x7Fu;
    CHECK(!noah_qmk_via_sync_frame_encode(&frame, wire));
}

static void test_decode_revalidates_authenticated_shape(void) {
    noah_qmk_via_sync_frame_t source = {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_METADATA, .generation = 1u};
    noah_qmk_via_sync_frame_t decoded;
    uint8_t                   wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE];

    CHECK(noah_qmk_via_sync_frame_encode(&source, wire));
    wire[1] = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK;
    /* Re-encoding is the supported way to obtain a valid CRC for a new shape. */
    source.kind           = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK;
    source.region         = NOAH_QMK_VIA_SYNC_REGION_KEYMAP;
    source.region_length  = 1u;
    source.payload_length = 1u;
    source.payload[0]     = 1u;
    CHECK(noah_qmk_via_sync_frame_encode(&source, wire));
    wire[16] = 0u;
    /* A stale CRC is rejected before malformed shape interpretation. */
    CHECK(!noah_qmk_via_sync_frame_decode(wire, sizeof(wire), &decoded));
}

int main(void) {
    test_all_message_shapes_round_trip();
    test_corruption_truncation_and_unknown_schema_are_rejected();
    test_invalid_shapes_fail_before_wire_effects();
    test_decode_revalidates_authenticated_shape();

    puts("qmk_via_sync_protocol host tests passed");
    return 0;
}
