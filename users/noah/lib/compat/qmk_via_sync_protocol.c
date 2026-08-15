// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Reconciliation Wire Protocol
// ───────────────────────────────────────────────────────────────────────────

#include "qmk_via_sync_protocol.h"
#include "qmk_via_sync_metadata.h"

#include <stddef.h>
#include <string.h>

enum {
    NOAH_QMK_VIA_SYNC_WIRE_VERSION        = 0,
    NOAH_QMK_VIA_SYNC_WIRE_KIND           = 1,
    NOAH_QMK_VIA_SYNC_WIRE_STATUS         = 2,
    NOAH_QMK_VIA_SYNC_WIRE_REGION         = 3,
    NOAH_QMK_VIA_SYNC_WIRE_GENERATION     = 4,
    NOAH_QMK_VIA_SYNC_WIRE_OFFSET         = 8,
    NOAH_QMK_VIA_SYNC_WIRE_REGION_LENGTH  = 10,
    NOAH_QMK_VIA_SYNC_WIRE_DIGEST         = 12,
    NOAH_QMK_VIA_SYNC_WIRE_PAYLOAD_LENGTH = 16,
    NOAH_QMK_VIA_SYNC_WIRE_PAYLOAD        = 17,
    NOAH_QMK_VIA_SYNC_WIRE_CRC            = 31,
};

static uint8_t noah_qmk_via_sync_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0u;

    for (size_t index = 0u; index < length; index++) {
        crc ^= data[index];
        for (uint8_t bit = 0u; bit < 8u; bit++) {
            crc = (crc & 0x80u) != 0u ? (uint8_t)((crc << 1u) ^ 0x07u) : (uint8_t)(crc << 1u);
        }
    }
    return crc;
}

static void noah_qmk_via_sync_write_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8u);
}

static void noah_qmk_via_sync_write_u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8u);
    out[2] = (uint8_t)(value >> 16u);
    out[3] = (uint8_t)(value >> 24u);
}

static uint16_t noah_qmk_via_sync_read_u16(const uint8_t *in) {
    return (uint16_t)in[0] | ((uint16_t)in[1] << 8u);
}

static uint32_t noah_qmk_via_sync_read_u32(const uint8_t *in) {
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8u) | ((uint32_t)in[2] << 16u) | ((uint32_t)in[3] << 24u);
}

static bool noah_qmk_via_sync_frame_shape_is_valid(const noah_qmk_via_sync_frame_t *frame) {
    bool region_valid;

    if (!frame || frame->generation == 0u || frame->generation > NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK || frame->payload_length > NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX || frame->status > NOAH_QMK_VIA_SYNC_STATUS_BUSY) {
        return false;
    }

    region_valid = frame->region >= NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG && frame->region <= NOAH_QMK_VIA_SYNC_REGION_MACRO;
    switch (frame->kind) {
        case NOAH_QMK_VIA_SYNC_MESSAGE_METADATA:
        case NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN:
        case NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT:
            return frame->region == NOAH_QMK_VIA_SYNC_REGION_NONE && frame->offset == 0u && frame->region_length == 0u && frame->payload_length == 0u;
        case NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK:
            return region_valid && frame->payload_length != 0u && frame->region_length != 0u && frame->offset <= frame->region_length && frame->payload_length <= frame->region_length - frame->offset;
        case NOAH_QMK_VIA_SYNC_MESSAGE_PULL_CHUNK:
            return region_valid && frame->payload_length == 0u && frame->region_length != 0u && frame->offset < frame->region_length;
        case NOAH_QMK_VIA_SYNC_MESSAGE_ACK:
        case NOAH_QMK_VIA_SYNC_MESSAGE_ERROR:
            if (frame->payload_length != 0u || (frame->region != NOAH_QMK_VIA_SYNC_REGION_NONE && !region_valid)) {
                return false;
            }
            return frame->region == NOAH_QMK_VIA_SYNC_REGION_NONE ? frame->offset == 0u && frame->region_length == 0u : frame->offset <= frame->region_length;
        default:
            return false;
    }
}

bool noah_qmk_via_sync_frame_encode(const noah_qmk_via_sync_frame_t *frame, uint8_t out[NOAH_QMK_VIA_SYNC_FRAME_SIZE]) {
    if (!out || !noah_qmk_via_sync_frame_shape_is_valid(frame)) {
        return false;
    }

    memset(out, 0, NOAH_QMK_VIA_SYNC_FRAME_SIZE);
    out[NOAH_QMK_VIA_SYNC_WIRE_VERSION] = NOAH_QMK_VIA_SYNC_PROTOCOL_VERSION;
    out[NOAH_QMK_VIA_SYNC_WIRE_KIND]    = (uint8_t)frame->kind;
    out[NOAH_QMK_VIA_SYNC_WIRE_STATUS]  = (uint8_t)frame->status;
    out[NOAH_QMK_VIA_SYNC_WIRE_REGION]  = (uint8_t)frame->region;
    noah_qmk_via_sync_write_u32(&out[NOAH_QMK_VIA_SYNC_WIRE_GENERATION], frame->generation);
    noah_qmk_via_sync_write_u16(&out[NOAH_QMK_VIA_SYNC_WIRE_OFFSET], frame->offset);
    noah_qmk_via_sync_write_u16(&out[NOAH_QMK_VIA_SYNC_WIRE_REGION_LENGTH], frame->region_length);
    noah_qmk_via_sync_write_u32(&out[NOAH_QMK_VIA_SYNC_WIRE_DIGEST], frame->digest);
    out[NOAH_QMK_VIA_SYNC_WIRE_PAYLOAD_LENGTH] = frame->payload_length;
    memcpy(&out[NOAH_QMK_VIA_SYNC_WIRE_PAYLOAD], frame->payload, frame->payload_length);
    out[NOAH_QMK_VIA_SYNC_WIRE_CRC] = noah_qmk_via_sync_crc8(out, NOAH_QMK_VIA_SYNC_WIRE_CRC);
    return true;
}

bool noah_qmk_via_sync_frame_decode(const uint8_t *wire, uint8_t length, noah_qmk_via_sync_frame_t *out) {
    noah_qmk_via_sync_frame_t frame;

    if (!wire || !out || length != NOAH_QMK_VIA_SYNC_FRAME_SIZE || wire[NOAH_QMK_VIA_SYNC_WIRE_VERSION] != NOAH_QMK_VIA_SYNC_PROTOCOL_VERSION || wire[NOAH_QMK_VIA_SYNC_WIRE_CRC] != noah_qmk_via_sync_crc8(wire, NOAH_QMK_VIA_SYNC_WIRE_CRC)) {
        return false;
    }

    frame = (noah_qmk_via_sync_frame_t){
        .kind           = (noah_qmk_via_sync_message_kind_t)wire[NOAH_QMK_VIA_SYNC_WIRE_KIND],
        .status         = (noah_qmk_via_sync_status_t)wire[NOAH_QMK_VIA_SYNC_WIRE_STATUS],
        .region         = (noah_qmk_via_sync_region_t)wire[NOAH_QMK_VIA_SYNC_WIRE_REGION],
        .generation     = noah_qmk_via_sync_read_u32(&wire[NOAH_QMK_VIA_SYNC_WIRE_GENERATION]),
        .offset         = noah_qmk_via_sync_read_u16(&wire[NOAH_QMK_VIA_SYNC_WIRE_OFFSET]),
        .region_length  = noah_qmk_via_sync_read_u16(&wire[NOAH_QMK_VIA_SYNC_WIRE_REGION_LENGTH]),
        .digest         = noah_qmk_via_sync_read_u32(&wire[NOAH_QMK_VIA_SYNC_WIRE_DIGEST]),
        .payload_length = wire[NOAH_QMK_VIA_SYNC_WIRE_PAYLOAD_LENGTH],
    };
    if (frame.payload_length <= NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX) {
        memcpy(frame.payload, &wire[NOAH_QMK_VIA_SYNC_WIRE_PAYLOAD], frame.payload_length);
    }
    if (!noah_qmk_via_sync_frame_shape_is_valid(&frame)) {
        return false;
    }

    *out = frame;
    return true;
}
