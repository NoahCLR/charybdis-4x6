// ────────────────────────────────────────────────────────────────────────────
// Logical Profile VIA Staging Channel
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_logical_profile.h"

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)

#    include <string.h>

#    include "../profile/protocol/profile_candidate_v1.h"
#    include "../profile/protocol/profile_wire_v1.h"
#    include "qmk_via_storage_regions.h"

enum {
    FRAME_COMMAND = 0u,
    FRAME_CHANNEL = 1u,
    FRAME_VALUE = 2u,
    FRAME_CORRELATION = 3u,
    FRAME_BODY = 5u,
    FRAME_ACK_ADMISSION = 5u,
    FRAME_ACK_ERROR = 6u,
    FRAME_ACK_OFFSET = 7u,
    FRAME_STATUS = 5u,
    FRAME_LENGTH = 6u,
    FRAME_PAYLOAD = 7u,
    LOGICAL_STATUS_PAYLOAD_SIZE = 18u,
};

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8u);
}

static uint32_t read_u32(const uint8_t *source) {
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8u) | ((uint32_t)source[2] << 16u) | ((uint32_t)source[3] << 24u);
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

static bool all_zero(const uint8_t *data, uint8_t first, uint8_t end) {
    for (uint8_t index = first; index < end; index++) {
        if (data[index] != 0u) {
            return false;
        }
    }
    return true;
}

static bool logical_value(uint8_t value) {
    return value == NOAH_QMK_VIA_LOGICAL_VALUE_BEGIN || value == NOAH_QMK_VIA_LOGICAL_VALUE_CHUNK || value == NOAH_QMK_VIA_LOGICAL_VALUE_VERIFY || value == NOAH_QMK_VIA_LOGICAL_VALUE_STATUS || value == NOAH_QMK_VIA_LOGICAL_VALUE_ABORT;
}

static void acknowledge(uint8_t *data, noah_profile_candidate_v1_admission_t admission, noah_profile_candidate_v1_error_id_t error, uint8_t offset) {
    memset(&data[FRAME_ACK_ADMISSION], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - FRAME_ACK_ADMISSION);
    data[FRAME_ACK_ADMISSION] = (uint8_t)admission;
    data[FRAME_ACK_ERROR] = (uint8_t)error;
    data[FRAME_ACK_OFFSET] = offset;
}

static bool handle_status(uint8_t *data) {
    noah_qmk_via_logical_status_t status;
    uint8_t *payload;

    if (data[FRAME_COMMAND] != NOAH_PROFILE_WIRE_V1_COMMAND_GET || data[FRAME_CORRELATION] == 0u || data[4] != 0u || !all_zero(data, FRAME_BODY, NOAH_PROFILE_WIRE_V1_REPORT_SIZE)) {
        memset(&data[FRAME_STATUS], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - FRAME_STATUS);
        data[FRAME_STATUS] = NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED;
        return true;
    }
    if (!noah_qmk_via_logical_status(&status)) {
        memset(&data[FRAME_STATUS], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - FRAME_STATUS);
        data[FRAME_STATUS] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
        return true;
    }
    memset(&data[FRAME_STATUS], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - FRAME_STATUS);
    data[FRAME_STATUS] = NOAH_PROFILE_WIRE_V1_STATUS_OK;
    data[FRAME_LENGTH] = LOGICAL_STATUS_PAYLOAD_SIZE;
    payload = &data[FRAME_PAYLOAD];
    payload[0] = 1u;
    payload[1] = (uint8_t)status.state;
    payload[2] = (uint8_t)status.last_status;
    payload[3] = status.pending ? 1u : 0u;
    write_u16(&payload[4], status.transaction_id);
    write_u16(&payload[6], status.operation_sequence);
    write_u32(&payload[8], status.generation);
    write_u32(&payload[12], status.digest);
    return true;
}

static bool decode_mutation(const uint8_t *data, uint16_t *transaction_id, noah_qmk_via_sync_frame_t *request, uint8_t *error_offset) {
    uint8_t value = data[FRAME_VALUE];

    *transaction_id = read_u16(&data[FRAME_CORRELATION]);
    *request = (noah_qmk_via_sync_frame_t){0};
    *error_offset = 0xffu;
    if (data[FRAME_COMMAND] != NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET || *transaction_id == 0u) {
        *error_offset = data[FRAME_COMMAND] != NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET ? FRAME_COMMAND : FRAME_CORRELATION;
        return false;
    }
    if (value == NOAH_QMK_VIA_LOGICAL_VALUE_BEGIN || value == NOAH_QMK_VIA_LOGICAL_VALUE_VERIFY || value == NOAH_QMK_VIA_LOGICAL_VALUE_ABORT) {
        request->kind = value == NOAH_QMK_VIA_LOGICAL_VALUE_BEGIN ? NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_BEGIN : value == NOAH_QMK_VIA_LOGICAL_VALUE_VERIFY ? NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_VERIFY : NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ABORT;
        request->generation = read_u32(&data[5]);
        request->digest = read_u32(&data[9]);
        if (request->generation == 0u || request->digest == 0u || !all_zero(data, 13u, NOAH_PROFILE_WIRE_V1_REPORT_SIZE)) {
            *error_offset = request->generation == 0u ? 5u : request->digest == 0u ? 9u : 13u;
            return false;
        }
        return true;
    }
    if (value == NOAH_QMK_VIA_LOGICAL_VALUE_CHUNK) {
        request->kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK;
        request->region = (noah_qmk_via_sync_region_t)data[5];
        request->offset = read_u16(&data[6]);
        request->region_length = read_u16(&data[8]);
        request->payload_length = data[10];
        request->generation = read_u32(&data[23]);
        request->digest = read_u32(&data[27]);
        if (request->region < NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG || request->region > NOAH_QMK_VIA_SYNC_REGION_MACRO || request->region_length != noah_qmk_via_storage_region_size(request->region) || request->payload_length == 0u || request->payload_length > NOAH_QMK_VIA_LOGICAL_CHUNK_MAX || request->offset >= request->region_length || request->payload_length > request->region_length - request->offset) {
            *error_offset = 5u;
            return false;
        }
        memcpy(request->payload, &data[11], request->payload_length);
        if (!all_zero(data, (uint8_t)(11u + request->payload_length), 23u) || data[31] != 0u || request->generation == 0u || request->digest == 0u) {
            *error_offset = (uint8_t)(11u + request->payload_length);
            return false;
        }
        return true;
    }
    *error_offset = FRAME_VALUE;
    return false;
}

bool noah_qmk_via_logical_profile_handle(uint8_t *data, uint8_t length) {
    noah_qmk_via_sync_frame_t request;
    uint16_t transaction_id;
    uint8_t error_offset;

    if (!data || length != NOAH_PROFILE_WIRE_V1_REPORT_SIZE || data[FRAME_CHANNEL] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL || !logical_value(data[FRAME_VALUE])) {
        return false;
    }
    if (data[FRAME_VALUE] == NOAH_QMK_VIA_LOGICAL_VALUE_STATUS) {
        return handle_status(data);
    }
    if (!decode_mutation(data, &transaction_id, &request, &error_offset)) {
        acknowledge(data, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_MALFORMED, NOAH_PROFILE_CANDIDATE_V1_ERROR_MALFORMED_FRAME, error_offset);
        return true;
    }
    if (!noah_qmk_via_logical_submit(transaction_id, &request)) {
        acknowledge(data, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY, NOAH_PROFILE_CANDIDATE_V1_ERROR_MAILBOX_BUSY, 0xffu);
        return true;
    }
    acknowledge(data, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE, 0xffu);
    return true;
}

bool noah_qmk_via_logical_profile_accept(uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    noah_qmk_via_sync_frame_t request = {
        .kind = NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ACCEPT,
        .generation = generation,
        .digest = digest,
    };
    return noah_qmk_via_logical_submit(transaction_id, &request);
}

bool noah_qmk_via_logical_profile_abort(uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    noah_qmk_via_sync_frame_t request = {
        .kind = NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ABORT,
        .generation = generation,
        .digest = digest,
    };
    return noah_qmk_via_logical_submit(transaction_id, &request);
}

#endif
