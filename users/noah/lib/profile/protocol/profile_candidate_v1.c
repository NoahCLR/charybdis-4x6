#include "../schema/profile_versions.h"
// ───────────────────────────────────────────────────────────────────────────
// Live Profile Candidate Wire V1
// ───────────────────────────────────────────────────────────────────────────

#include "profile_candidate_v1.h"

#include <string.h>

enum {
    FRAME_COMMAND          = 0u,
    FRAME_CHANNEL          = 1u,
    FRAME_VALUE            = 2u,
    FRAME_TRANSACTION      = 3u,
    FRAME_BODY             = 5u,
    FRAME_ACK_ADMISSION    = 5u,
    FRAME_ACK_ERROR        = 6u,
    FRAME_ACK_OFFSET       = 7u,
    FRAME_READ_REQUEST     = 3u,
    FRAME_READ_PAGE        = 4u,
    FRAME_READ_STATUS      = 5u,
    FRAME_READ_LENGTH      = 6u,
    FRAME_READ_PAYLOAD     = 7u,
    OPERATION_PAYLOAD_SIZE = 25u,
};

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << 8u));
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

static noah_profile_candidate_v1_decode_result_t fail(noah_profile_candidate_v1_frame_error_t *error, noah_profile_candidate_v1_decode_result_t result, uint8_t offset) {
    if (error) {
        error->code         = NOAH_PROFILE_CANDIDATE_V1_ERROR_MALFORMED_FRAME;
        error->frame_offset = offset;
    }
    return result;
}

static bool operation_is_supported(uint8_t command, uint8_t value) {
    if (command == NOAH_PROFILE_CANDIDATE_V1_COMMAND_SAVE) {
        return value == NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT;
    }
    return command == NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET && (value == NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN || value == NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK || value == NOAH_PROFILE_CANDIDATE_V1_VALUE_STREAM_CHUNK || value == NOAH_PROFILE_CANDIDATE_V1_VALUE_REUSE || value == NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE || value == NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT);
}

static noah_profile_candidate_v1_operation_t decode_operation(uint8_t value) {
    switch (value) {
        case NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN:
            return NOAH_PROFILE_CANDIDATE_V1_OPERATION_BEGIN;
        case NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK:
        case NOAH_PROFILE_CANDIDATE_V1_VALUE_STREAM_CHUNK:
            return NOAH_PROFILE_CANDIDATE_V1_OPERATION_CHUNK;
        case NOAH_PROFILE_CANDIDATE_V1_VALUE_REUSE:
            return NOAH_PROFILE_CANDIDATE_V1_OPERATION_REUSE;
        case NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE:
            return NOAH_PROFILE_CANDIDATE_V1_OPERATION_VALIDATE;
        case NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT:
            return NOAH_PROFILE_CANDIDATE_V1_OPERATION_COMMIT;
        case NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT:
            return NOAH_PROFILE_CANDIDATE_V1_OPERATION_ABORT;
        default:
            return NOAH_PROFILE_CANDIDATE_V1_OPERATION_NONE;
    }
}

static noah_profile_candidate_v1_decode_result_t require_zero(const uint8_t *frame, uint8_t first, uint8_t end, noah_profile_candidate_v1_frame_error_t *error) {
    uint8_t index;

    for (index = first; index < end; index++) {
        if (frame[index] != 0u) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, index);
        }
    }
    return NOAH_PROFILE_CANDIDATE_V1_DECODE_OK;
}

noah_profile_candidate_v1_decode_result_t noah_profile_candidate_v1_decode(const uint8_t *frame, size_t length, noah_profile_candidate_v1_command_t *command, noah_profile_candidate_v1_frame_error_t *error) {
    uint8_t value;

    if (error) {
        error->code         = NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE;
        error->frame_offset = NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8;
    }
    if (!frame || !command) {
        return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_ARGUMENT, 0u);
    }
    if (length != NOAH_PROFILE_WIRE_V1_REPORT_SIZE) {
        uint8_t offset = length < NOAH_PROFILE_WIRE_V1_REPORT_SIZE ? (uint8_t)length : NOAH_PROFILE_WIRE_V1_REPORT_SIZE;
        return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_LENGTH, offset);
    }
    if ((frame[FRAME_COMMAND] != NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET && frame[FRAME_COMMAND] != NOAH_PROFILE_CANDIDATE_V1_COMMAND_SAVE) || frame[FRAME_CHANNEL] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL) {
        return NOAH_PROFILE_CANDIDATE_V1_DECODE_NOT_HANDLED;
    }

    value = frame[FRAME_VALUE];
    if (!operation_is_supported(frame[FRAME_COMMAND], value)) {
        return NOAH_PROFILE_CANDIDATE_V1_DECODE_NOT_HANDLED;
    }

    memset(command, 0, sizeof(*command));
    command->operation      = decode_operation(value);
    command->transaction_id = read_u16(&frame[FRAME_TRANSACTION]);
    if (command->transaction_id == 0u) {
        return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, FRAME_TRANSACTION);
    }

    if (command->operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_BEGIN) {
        noah_profile_candidate_v1_metadata_t *metadata = &command->payload.begin;

        // The base's version implies the store format and leaves no flags.
        metadata->schema_major         = frame[5];
        metadata->schema_minor         = frame[6];
        metadata->requested_domains    = frame[7];
        metadata->flags                = 0u;
        metadata->payload_length       = read_u32(&frame[8]);
        metadata->crc32                = read_u32(&frame[12]);
        metadata->digest               = read_u32(&frame[16]);
        metadata->action_abi_digest    = read_u32(&frame[20]);
        metadata->store_format_version = NOAH_PROFILE_LOGICAL_STORE_VERSION;
        if ((metadata->requested_domains & (uint8_t)~NOAH_PROFILE_CANDIDATE_V1_KNOWN_DOMAINS) != 0u) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, 7u);
        }
        if (metadata->payload_length < NOAH_PROFILE_CANDIDATE_V1_MIN_BLOB_SIZE || metadata->payload_length > NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, 8u);
        }
        metadata->via_generation = read_u32(&frame[24]);
        metadata->via_digest     = read_u32(&frame[28]);
        if (metadata->via_generation == 0u || metadata->via_digest == 0u) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, metadata->via_generation == 0u ? 24u : 28u);
        }
        return NOAH_PROFILE_CANDIDATE_V1_DECODE_OK;
    }

    if (command->operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_REUSE) {
        uint32_t length = read_u32(&frame[13]);

        command->payload.reuse.offset = read_u32(&frame[5]);
        command->payload.reuse.source_offset = read_u32(&frame[9]);
        command->payload.reuse.source.generation = read_u32(&frame[17]);
        command->payload.reuse.source.digest = read_u32(&frame[21]);
        command->payload.reuse.source.crc32 = read_u32(&frame[25]);
        command->payload.reuse.source.kind = frame[29];
        command->payload.reuse.source.origin = frame[30];
        if (length == 0u || length > NOAH_PROFILE_CANDIDATE_V1_REUSE_MAX) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, 13u);
        }
        command->payload.reuse.length = (uint16_t)length;
        if (command->payload.reuse.offset > NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE - length) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, 5u);
        }
        if (command->payload.reuse.source_offset > NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE - length) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, 9u);
        }
        if (frame[29] > 1u || (frame[29] == 0u && (command->payload.reuse.source.generation != 0u || frame[30] != UINT8_MAX)) || (frame[29] == 1u && (command->payload.reuse.source.generation == 0u || frame[30] > 1u))) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, 29u);
        }
        return require_zero(frame, 31u, NOAH_PROFILE_WIRE_V1_REPORT_SIZE, error);
    }

    if (command->operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_CHUNK) {
        uint8_t chunk_length = frame[9];

        command->payload.chunk.offset = read_u32(&frame[5]);
        command->payload.chunk.length = chunk_length;
        if (chunk_length == 0u || chunk_length > NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, 9u);
        }
        if (command->payload.chunk.offset > NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE - chunk_length) {
            return fail(error, NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED, 5u);
        }
        memcpy(command->payload.chunk.bytes, &frame[10], chunk_length);
        // Unused chunk bytes, then the two reserved bytes, are zero.
        return require_zero(frame, (uint8_t)(10u + chunk_length), NOAH_PROFILE_WIRE_V1_REPORT_SIZE, error);
    }

    return require_zero(frame, FRAME_BODY, NOAH_PROFILE_WIRE_V1_REPORT_SIZE, error);
}

void noah_profile_candidate_v1_encode_ack(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], noah_profile_candidate_v1_admission_t admission, noah_profile_candidate_v1_error_id_t error, uint8_t frame_offset) {
    if (!frame) {
        return;
    }
    memset(&frame[FRAME_ACK_ADMISSION], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - FRAME_ACK_ADMISSION);
    frame[FRAME_ACK_ADMISSION] = (uint8_t)admission;
    frame[FRAME_ACK_ERROR]     = (uint8_t)error;
    frame[FRAME_ACK_OFFSET]    = frame_offset;
}

noah_profile_candidate_v1_error_t noah_profile_candidate_v1_no_error(void) {
    noah_profile_candidate_v1_error_t error = {
        .code        = NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE,
        .domain_id   = NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8,
        .table_id    = NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8,
        .row_index   = NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16,
        .tap_index   = NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8,
        .field_id    = NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8,
        .byte_offset = NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U32,
    };
    return error;
}

static bool status_request_reserved_bytes_are_zero(const uint8_t *frame) {
    uint8_t index;

    for (index = FRAME_READ_STATUS; index < NOAH_PROFILE_WIRE_V1_REPORT_SIZE; index++) {
        if (frame[index] != 0u) {
            return false;
        }
    }
    return true;
}

static void begin_status_response(uint8_t *frame, noah_profile_wire_v1_status_code_t response_status, uint8_t payload_length) {
    memset(&frame[FRAME_READ_STATUS], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - FRAME_READ_STATUS);
    frame[FRAME_READ_STATUS] = (uint8_t)response_status;
    frame[FRAME_READ_LENGTH] = payload_length;
}

bool noah_profile_candidate_v1_handle_status_get(const noah_profile_candidate_v1_status_t *status, uint8_t *frame, size_t length) {
    uint8_t *payload;

    if (!status || !frame || length != NOAH_PROFILE_WIRE_V1_REPORT_SIZE) {
        return false;
    }
    if (frame[FRAME_COMMAND] != NOAH_PROFILE_WIRE_V1_COMMAND_GET || frame[FRAME_CHANNEL] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL || frame[FRAME_VALUE] != NOAH_PROFILE_CANDIDATE_V1_VALUE_STATUS) {
        return false;
    }
    if (frame[FRAME_READ_REQUEST] == 0u || !status_request_reserved_bytes_are_zero(frame)) {
        begin_status_response(frame, NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED, 0u);
        return true;
    }
    if (frame[FRAME_READ_PAGE] != 0u && frame[FRAME_READ_PAGE] != NOAH_PROFILE_CANDIDATE_V1_STATUS_ERROR_PAGE) {
        begin_status_response(frame, NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE, 0u);
        return true;
    }

    uint8_t page = frame[FRAME_READ_PAGE];
    begin_status_response(frame, NOAH_PROFILE_WIRE_V1_STATUS_OK, OPERATION_PAYLOAD_SIZE);
    payload = &frame[FRAME_READ_PAYLOAD];
    if (page == NOAH_PROFILE_CANDIDATE_V1_STATUS_ERROR_PAGE) {
        // The error page repeats the transaction, operation sequence and
        // error sequence so a host can tie it to the page-0 read it follows.
        payload[0] = 1u;
        payload[1] = (uint8_t)status->error.code;
        payload[2] = status->error.domain_id;
        payload[3] = status->error.table_id;
        write_u16(&payload[4], status->error.row_index);
        payload[6] = status->error.tap_index;
        payload[7] = status->error.field_id;
        write_u32(&payload[8], status->error.byte_offset);
        write_u16(&payload[12], status->transaction_id);
        write_u16(&payload[14], status->operation_sequence);
        write_u16(&payload[16], status->error_sequence);
        return true; // Bytes 18..24 remain reserved zero.
    }
    payload[0] = 2u;
    payload[1] = (uint8_t)status->state;
    payload[2] = (uint8_t)status->last_operation;
    payload[3] = status->flags;
    write_u16(&payload[4], status->transaction_id);
    write_u32(&payload[6], status->next_offset);
    write_u32(&payload[10], status->payload_length);
    write_u32(&payload[14], status->digest);
    payload[18] = (uint8_t)status->error.code;
    write_u16(&payload[19], status->operation_sequence);
    write_u16(&payload[21], status->error_sequence);
    return true; // Bytes 23..24 remain reserved zero.
}

bool noah_profile_candidate_v1_handle_peer_status_get(const noah_profile_candidate_v1_peer_status_t *peer, uint8_t *frame, size_t length) {
    uint8_t *payload;

    if (!peer || !frame || length != NOAH_PROFILE_WIRE_V1_REPORT_SIZE) {
        return false;
    }
    if (frame[FRAME_COMMAND] != NOAH_PROFILE_WIRE_V1_COMMAND_GET || frame[FRAME_CHANNEL] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL || frame[FRAME_VALUE] != NOAH_PROFILE_CANDIDATE_V1_VALUE_STATUS || (frame[FRAME_READ_PAGE] != 1u && frame[FRAME_READ_PAGE] != 2u)) {
        return false;
    }
    if (frame[FRAME_READ_REQUEST] == 0u || !status_request_reserved_bytes_are_zero(frame)) {
        begin_status_response(frame, NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED, 0u);
        return true;
    }

    uint8_t page = frame[FRAME_READ_PAGE];
    begin_status_response(frame, NOAH_PROFILE_WIRE_V1_STATUS_OK, OPERATION_PAYLOAD_SIZE);
    payload    = &frame[FRAME_READ_PAYLOAD];
    payload[0] = 2u;
    if (page == 2u) {
        // The transfer length is page 1's; this page keeps the offset only.
        payload[1] = peer->reuse_unavailable ? 1u : 0u;
        write_u32(&payload[2], peer->reused_bytes);
        write_u16(&payload[6], peer->reuse_exchanges);
        write_u16(&payload[8], peer->source_unavailable_replies);
        write_u32(&payload[10], peer->prepared_scans);
        write_u32(&payload[14], peer->prepared_exchanges);
        write_u32(&payload[18], peer->transfer_offset);
        return true; // Bytes 22..24 remain reserved zero.
    }
    payload[1] = (uint8_t)peer->phase;
    payload[2] = peer->last_status;
    payload[3] = (uint8_t)(peer->flags & NOAH_PROFILE_CANDIDATE_V1_PEER_FLAGS_KNOWN);
    write_u32(&payload[4], peer->transfer_offset);
    write_u32(&payload[8], peer->transfer_length);
    write_u32(&payload[12], peer->retry_count);
    write_u32(&payload[16], peer->transport_failure_count);
    write_u16(&payload[20], peer->busy_streak);
    payload[22] = peer->busy_reason;
    payload[23] = peer->busy_store_state;
    // Transfer owner (0..2) in the low nibble, storage admission (0..2) in
    // the high one.
    payload[24] = (uint8_t)((peer->busy_owner & 0x0Fu) | (uint8_t)(peer->busy_admission << 4u));
    return true;
}

_Static_assert(FRAME_READ_PAYLOAD + OPERATION_PAYLOAD_SIZE == NOAH_PROFILE_WIRE_V1_REPORT_SIZE, "Candidate status must fill one 32-byte report exactly");
