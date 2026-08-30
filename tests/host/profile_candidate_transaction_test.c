#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/protocol/profile_candidate_v1.h"
#include "users/noah/lib/profile/storage/profile_candidate_transaction.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

static const char *fixture_path;

typedef struct {
    uint8_t  staged[NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE];
    uint16_t declared_length;
    uint16_t validation_offset;
    uint32_t validation_crc;
    uint32_t validation_digest;
    uint8_t  max_validation_budget;
    unsigned begin_calls;
    unsigned write_calls;
    unsigned read_calls;
    unsigned validation_begin_calls;
    unsigned validation_step_calls;
    unsigned commit_begin_calls;
    unsigned commit_step_calls;
    unsigned activation_begin_calls;
    unsigned activation_step_calls;
    unsigned abort_calls;
    noah_profile_candidate_backend_result_t begin_result;
    noah_profile_candidate_backend_result_t write_result;
    noah_profile_candidate_backend_result_t read_result;
    noah_profile_candidate_backend_result_t validation_begin_result;
    noah_profile_candidate_backend_result_t commit_begin_result;
    noah_profile_candidate_backend_result_t commit_terminal_result;
    noah_profile_candidate_backend_result_t activation_begin_result;
    noah_profile_candidate_backend_result_t activation_terminal_result;
    noah_profile_candidate_backend_result_t abort_result;
    unsigned commit_steps_remaining;
    unsigned activation_steps_remaining;
    bool semantic_reject;
    bool validation_step_returns_ok;
    noah_profile_candidate_v1_metadata_t metadata;
} fake_backend_t;

static uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') {
        return (uint8_t)(value - '0');
    }
    value = (char)tolower((unsigned char)value);
    assert(value >= 'a' && value <= 'f');
    return (uint8_t)(value - 'a' + 10);
}

static void load_fixture(const char *label, uint8_t expected[NOAH_PROFILE_WIRE_V1_REPORT_SIZE]) {
    FILE  *fixture = fopen(fixture_path, "r");
    char   line[192];
    size_t label_length = strlen(label);
    bool   found        = false;

    assert(fixture != NULL);
    while (fgets(line, sizeof(line), fixture)) {
        const char *hex;
        uint8_t     index;

        if (strncmp(line, label, label_length) != 0 || line[label_length] != '=') {
            continue;
        }
        hex = &line[label_length + 1u];
        for (index = 0u; index < NOAH_PROFILE_WIRE_V1_REPORT_SIZE; index++) {
            assert(isxdigit((unsigned char)hex[index * 2u]));
            assert(isxdigit((unsigned char)hex[index * 2u + 1u]));
            expected[index] = (uint8_t)((hex_nibble(hex[index * 2u]) << 4u) | hex_nibble(hex[index * 2u + 1u]));
        }
        assert(hex[NOAH_PROFILE_WIRE_V1_REPORT_SIZE * 2u] == '\n' || hex[NOAH_PROFILE_WIRE_V1_REPORT_SIZE * 2u] == '\0');
        found = true;
        break;
    }
    fclose(fixture);
    assert(found);
}

static void assert_golden(const char *label, const uint8_t actual[NOAH_PROFILE_WIRE_V1_REPORT_SIZE]) {
    uint8_t expected[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];
    load_fixture(label, expected);
    assert(memcmp(actual, expected, sizeof(expected)) == 0);
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

static noah_profile_candidate_v1_metadata_t metadata_for(const uint8_t *bytes, uint16_t length) {
    noah_profile_candidate_v1_metadata_t metadata = {
        .schema_major      = 1u,
        .schema_minor      = 0u,
        .requested_domains = 0u,
        .flags             = 0u,
        .payload_length    = length,
        .crc32             = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length)),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, bytes, length),
        .action_abi_digest = UINT32_C(0xCCBBAA99),
    };
    return metadata;
}

static void begin_frame(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], uint16_t transaction_id, const noah_profile_candidate_v1_metadata_t *metadata) {
    memset(frame, 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE);
    frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN;
    write_u16(&frame[3], transaction_id);
    frame[5] = metadata->schema_major;
    frame[6] = metadata->schema_minor;
    frame[7] = metadata->requested_domains;
    frame[8] = metadata->flags;
    write_u16(&frame[9], metadata->payload_length);
    write_u32(&frame[11], metadata->crc32);
    write_u32(&frame[15], metadata->digest);
    write_u32(&frame[19], metadata->action_abi_digest);
}

static void chunk_frame(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], uint16_t transaction_id, uint16_t offset, const uint8_t *bytes, uint8_t length) {
    memset(frame, 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE);
    frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK;
    write_u16(&frame[3], transaction_id);
    write_u16(&frame[5], offset);
    frame[7] = length;
    if (length <= NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX) {
        memcpy(&frame[8], bytes, length);
    }
}

static void simple_frame(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], uint8_t value, uint16_t transaction_id) {
    memset(frame, 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE);
    frame[0] = value == NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT ? NOAH_PROFILE_CANDIDATE_V1_COMMAND_SAVE : NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = value;
    write_u16(&frame[3], transaction_id);
}

static noah_profile_candidate_backend_result_t fake_begin(void *context, const noah_profile_candidate_v1_metadata_t *metadata) {
    fake_backend_t *fake = context;
    fake->begin_calls++;
    fake->metadata        = *metadata;
    fake->declared_length = metadata->payload_length;
    memset(fake->staged, 0xA5, metadata->payload_length);
    return fake->begin_result;
}

static noah_profile_candidate_backend_result_t fake_write(void *context, uint16_t offset, const uint8_t *bytes, uint8_t length) {
    fake_backend_t *fake = context;
    fake->write_calls++;
    if (fake->write_result == NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        assert((uint32_t)offset + length <= fake->declared_length);
        memcpy(&fake->staged[offset], bytes, length);
    }
    return fake->write_result;
}

static noah_profile_candidate_backend_result_t fake_read(void *context, uint16_t offset, uint8_t *bytes, uint8_t length) {
    fake_backend_t *fake = context;
    fake->read_calls++;
    if (fake->read_result == NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        assert((uint32_t)offset + length <= fake->declared_length);
        memcpy(bytes, &fake->staged[offset], length);
    }
    return fake->read_result;
}

static noah_profile_candidate_backend_result_t fake_validation_begin(void *context, const noah_profile_candidate_v1_metadata_t *metadata, noah_profile_candidate_v1_error_t *error) {
    fake_backend_t *fake = context;
    (void)error;
    fake->validation_begin_calls++;
    assert(metadata->payload_length == fake->declared_length);
    fake->validation_offset = 0u;
    fake->validation_crc    = NOAH_PROFILE_CRC32_INITIAL;
    fake->validation_digest = NOAH_PROFILE_FNV1A_INITIAL;
    return fake->validation_begin_result;
}

static noah_profile_candidate_backend_result_t fake_validation_step(void *context, uint8_t byte_budget, noah_profile_candidate_v1_error_t *error) {
    fake_backend_t *fake = context;
    uint16_t        remaining;
    uint8_t         consumed;

    fake->validation_step_calls++;
    if (byte_budget > fake->max_validation_budget) {
        fake->max_validation_budget = byte_budget;
    }
    assert(byte_budget <= NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET);
    if (fake->validation_step_returns_ok) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    }

    remaining = (uint16_t)(fake->declared_length - fake->validation_offset);
    consumed  = remaining < byte_budget ? (uint8_t)remaining : byte_budget;
    fake->validation_crc = noah_profile_crc32_update(fake->validation_crc, &fake->staged[fake->validation_offset], consumed);
    fake->validation_digest = noah_profile_fnv1a_update(fake->validation_digest, &fake->staged[fake->validation_offset], consumed);
    fake->validation_offset = (uint16_t)(fake->validation_offset + consumed);
    if (fake->validation_offset < fake->declared_length) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    if (noah_profile_crc32_finish(fake->validation_crc) != fake->metadata.crc32 || fake->validation_digest != fake->metadata.digest) {
        error->code        = NOAH_PROFILE_CANDIDATE_V1_ERROR_CHECKSUM_MISMATCH;
        error->byte_offset = fake->declared_length;
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    if (fake->semantic_reject) {
        error->code        = NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED;
        error->domain_id   = 0x20u;
        error->table_id    = 2u;
        error->row_index   = 5u;
        error->tap_index   = 3u;
        error->field_id    = 4u;
        error->byte_offset = 6u;
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    return NOAH_PROFILE_CANDIDATE_BACKEND_VALID;
}

static noah_profile_candidate_backend_result_t fake_abort(void *context) {
    fake_backend_t *fake = context;
    fake->abort_calls++;
    return fake->abort_result;
}

static noah_profile_candidate_backend_result_t fake_commit_begin(void *context) {
    fake_backend_t *fake = context;
    fake->commit_begin_calls++;
    return fake->commit_begin_result;
}

static noah_profile_candidate_backend_result_t fake_commit_step(void *context, uint8_t byte_budget) {
    fake_backend_t *fake = context;
    fake->commit_step_calls++;
    assert(byte_budget <= NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET);
    if (fake->commit_steps_remaining != 0u) {
        fake->commit_steps_remaining--;
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    return fake->commit_terminal_result;
}

static noah_profile_candidate_backend_result_t fake_activation_begin(void *context) {
    fake_backend_t *fake = context;
    fake->activation_begin_calls++;
    return fake->activation_begin_result;
}

static noah_profile_candidate_backend_result_t fake_activation_step(void *context) {
    fake_backend_t *fake = context;
    fake->activation_step_calls++;
    if (fake->activation_steps_remaining != 0u) {
        fake->activation_steps_remaining--;
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    return fake->activation_terminal_result;
}

static void fake_init(fake_backend_t *fake) {
    memset(fake, 0, sizeof(*fake));
    fake->begin_result            = NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    fake->write_result            = NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    fake->read_result             = NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    fake->validation_begin_result = NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    fake->commit_begin_result     = NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    fake->commit_terminal_result  = NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    fake->activation_begin_result = NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    fake->activation_terminal_result = NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    fake->abort_result            = NOAH_PROFILE_CANDIDATE_BACKEND_OK;
}

static noah_profile_candidate_backend_t backend_for(fake_backend_t *fake) {
    noah_profile_candidate_backend_t backend = {
        .context          = fake,
        .begin            = fake_begin,
        .write            = fake_write,
        .read             = fake_read,
        .validation_begin = fake_validation_begin,
        .validation_step  = fake_validation_step,
        .commit_begin     = fake_commit_begin,
        .commit_step      = fake_commit_step,
        .activation_begin = fake_activation_begin,
        .activation_step  = fake_activation_step,
        .abort            = fake_abort,
    };
    return backend;
}

static noah_profile_candidate_compatibility_t compatibility(void) {
    noah_profile_candidate_compatibility_t compatibility = {
        .schema_major          = 1u,
        .schema_minor          = 0u,
        .supported_domain_mask = NOAH_PROFILE_CANDIDATE_V1_KNOWN_DOMAINS,
        .max_payload_length    = NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE,
        .action_abi_digest     = UINT32_C(0xCCBBAA99),
    };
    return compatibility;
}

static unsigned backend_call_count(const fake_backend_t *fake) {
    return fake->begin_calls + fake->write_calls + fake->read_calls + fake->validation_begin_calls + fake->validation_step_calls + fake->commit_begin_calls + fake->commit_step_calls + fake->activation_begin_calls + fake->activation_step_calls + fake->abort_calls;
}

static void queue(noah_profile_candidate_transaction_t *transaction, fake_backend_t *fake, uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE]) {
    unsigned calls = backend_call_count(fake);
    assert(noah_profile_candidate_transaction_receive(transaction, frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE));
    assert(frame[5] == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED);
    assert(frame[6] == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
    assert(frame[7] == NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8);
    assert(backend_call_count(fake) == calls);
}

static void queue_and_scan(noah_profile_candidate_transaction_t *transaction, fake_backend_t *fake, uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE]) {
    queue(transaction, fake, frame);
    assert(noah_profile_candidate_transaction_scan(transaction));
}

static noah_profile_candidate_v1_status_t status_of(const noah_profile_candidate_transaction_t *transaction) {
    noah_profile_candidate_v1_status_t status;
    noah_profile_candidate_transaction_status(transaction, &status);
    return status;
}

static void test_exact_frame_codecs(void) {
    noah_profile_candidate_v1_command_t     command;
    noah_profile_candidate_v1_frame_error_t error;
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    load_fixture("begin-request", frame);
    assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_OK);
    assert(command.operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_BEGIN);
    assert(command.transaction_id == UINT16_C(0x1234));
    assert(command.payload.begin.payload_length == 8u);
    assert(command.payload.begin.crc32 == UINT32_C(0x11223344));
    assert(command.payload.begin.digest == UINT32_C(0x88776655));
    assert(command.payload.begin.action_abi_digest == UINT32_C(0xCCBBAA99));
    noah_profile_candidate_v1_encode_ack(frame, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8);
    assert_golden("begin-queued-ack", frame);

    load_fixture("chunk-request", frame);
    assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_OK);
    assert(command.operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_CHUNK);
    assert(command.payload.chunk.offset == 0u && command.payload.chunk.length == 8u);
    assert(memcmp(command.payload.chunk.bytes, "NLP1\x01\x00\x00\x01", 8u) == 0);

    load_fixture("validate-request", frame);
    assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_OK);
    assert(command.operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_VALIDATE);
    load_fixture("commit-request", frame);
    assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_OK);
    assert(command.operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_COMMIT);
    noah_profile_candidate_v1_encode_ack(frame, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8);
    assert_golden("commit-queued-ack", frame);
    load_fixture("abort-request", frame);
    assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_OK);
    assert(command.operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_ABORT);

    load_fixture("begin-request", frame);
    noah_profile_candidate_v1_encode_ack(frame, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_MALFORMED, NOAH_PROFILE_CANDIDATE_V1_ERROR_MALFORMED_FRAME, 8u);
    assert_golden("begin-malformed-ack", frame);
}

static void test_malformed_frame_matrix(void) {
    const uint8_t values[] = {
        NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN,
        NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK,
        NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE,
        NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT,
        NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT,
    };
    const uint8_t bytes[20] = {0u};
    noah_profile_candidate_v1_metadata_t metadata = metadata_for(bytes, 8u);
    noah_profile_candidate_v1_command_t command;
    noah_profile_candidate_v1_frame_error_t error;
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE + 1u];
    size_t value_index;

    for (value_index = 0u; value_index < sizeof(values); value_index++) {
        uint8_t length;
        simple_frame(frame, values[value_index], UINT16_C(0x1234));
        if (values[value_index] == NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN) {
            begin_frame(frame, UINT16_C(0x1234), &metadata);
        } else if (values[value_index] == NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK) {
            chunk_frame(frame, UINT16_C(0x1234), 0u, bytes, 1u);
        }
        for (length = 0u; length < NOAH_PROFILE_WIRE_V1_REPORT_SIZE; length++) {
            assert(noah_profile_candidate_v1_decode(frame, length, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_LENGTH);
        }
        assert(noah_profile_candidate_v1_decode(frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE + 1u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_LENGTH);
    }

    begin_frame(frame, 0u, &metadata);
    assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == 3u);
    begin_frame(frame, 1u, &metadata);
    frame[7] = 0x80u;
    assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == 7u);
    begin_frame(frame, 1u, &metadata);
    frame[8] = 1u;
    assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == 8u);
    metadata.payload_length = 7u;
    begin_frame(frame, 1u, &metadata);
    assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == 9u);
    metadata.payload_length = 8u;
    for (uint8_t offset = 23u; offset < 32u; offset++) {
        begin_frame(frame, 1u, &metadata);
        frame[offset] = 1u;
        assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == offset);
    }

    chunk_frame(frame, 1u, 0u, bytes, 1u);
    frame[7] = 0u;
    assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == 7u);
    chunk_frame(frame, 1u, 0u, bytes, 1u);
    frame[7] = 21u;
    assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == 7u);
    chunk_frame(frame, 1u, 4063u, bytes, 2u);
    assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == 5u);
    for (uint8_t offset = 9u; offset < 32u; offset++) {
        chunk_frame(frame, 1u, 0u, bytes, 1u);
        frame[offset] = 1u;
        assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == offset);
    }
    for (size_t value_index_inner = 2u; value_index_inner < sizeof(values); value_index_inner++) {
        for (uint8_t offset = 5u; offset < 32u; offset++) {
            simple_frame(frame, values[value_index_inner], 1u);
            frame[offset] = 1u;
            assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED && error.frame_offset == offset);
        }
    }

    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 1u);
    frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_NOT_HANDLED);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN, 1u);
    frame[0] = NOAH_PROFILE_WIRE_V1_COMMAND_GET;
    assert(noah_profile_candidate_v1_decode(frame, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_NOT_HANDLED);
    assert(noah_profile_candidate_v1_decode(NULL, 32u, &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_ARGUMENT);
    assert(noah_profile_candidate_v1_decode(frame, 32u, NULL, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_ARGUMENT);
}

static void test_operation_status_codec(void) {
    noah_profile_candidate_v1_status_t status = {
        .state              = NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED,
        .last_operation     = NOAH_PROFILE_CANDIDATE_V1_OPERATION_VALIDATE,
        .flags              = NOAH_PROFILE_CANDIDATE_V1_STATUS_MAILBOX_PENDING | NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED,
        .transaction_id     = UINT16_C(0x1234),
        .next_offset        = 8u,
        .payload_length     = 8u,
        .digest             = UINT32_C(0x88776655),
        .error = {
            .code        = NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED,
            .domain_id   = 0x20u,
            .table_id    = 2u,
            .row_index   = 5u,
            .tap_index   = 3u,
            .field_id    = 4u,
            .byte_offset = UINT16_C(0x1122),
        },
        .operation_sequence = UINT16_C(0x3344),
    };
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    load_fixture("operation-status-request", frame);
    assert(noah_profile_candidate_v1_handle_status_get(&status, frame, sizeof(frame)));
    assert_golden("operation-status-response", frame);

    status.state              = NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING;
    status.last_operation     = NOAH_PROFILE_CANDIDATE_V1_OPERATION_COMMIT;
    status.flags              = 0u;
    status.error              = noah_profile_candidate_v1_no_error();
    status.operation_sequence = UINT16_C(0x3345);
    load_fixture("operation-status-request", frame);
    assert(noah_profile_candidate_v1_handle_status_get(&status, frame, sizeof(frame)));
    assert_golden("commit-status-response", frame);

    load_fixture("operation-status-request", frame);
    frame[3] = 0u;
    assert(noah_profile_candidate_v1_handle_status_get(&status, frame, sizeof(frame)));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED && frame[6] == 0u);
    load_fixture("operation-status-request", frame);
    frame[31] = 1u;
    assert(noah_profile_candidate_v1_handle_status_get(&status, frame, sizeof(frame)));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED && frame[31] == 0u);
    load_fixture("operation-status-request", frame);
    frame[4] = 1u;
    assert(noah_profile_candidate_v1_handle_status_get(&status, frame, sizeof(frame)));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE);
    load_fixture("operation-status-request", frame);
    assert(!noah_profile_candidate_v1_handle_status_get(&status, frame, 31u));
}

static void test_mailbox_and_begin_retries(void) {
    static const uint8_t profile[8] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
    fake_backend_t fake;
    noah_profile_candidate_backend_t backend;
    noah_profile_candidate_compatibility_t compatible = compatibility();
    noah_profile_candidate_transaction_t transaction;
    noah_profile_candidate_v1_metadata_t metadata = metadata_for(profile, sizeof(profile));
    noah_profile_candidate_v1_status_t status;
    uint8_t frame[32];
    uint8_t second[32];

    fake_init(&fake);
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_frame(frame, UINT16_C(0x1234), &metadata);
    queue(&transaction, &fake, frame);
    status = status_of(&transaction);
    assert(status.flags == NOAH_PROFILE_CANDIDATE_V1_STATUS_MAILBOX_PENDING);

    chunk_frame(second, UINT16_C(0x1234), 0u, profile, sizeof(profile));
    assert(noah_profile_candidate_transaction_receive(&transaction, second, sizeof(second)));
    assert(second[5] == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY);
    assert(second[6] == NOAH_PROFILE_CANDIDATE_V1_ERROR_MAILBOX_BUSY);
    assert(fake.begin_calls == 0u && fake.write_calls == 0u);
    assert(noah_profile_candidate_transaction_scan(&transaction));
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
    assert(status.transaction_id == UINT16_C(0x1234));
    assert(status.operation_sequence == 1u);
    assert(fake.begin_calls == 1u);

    begin_frame(frame, UINT16_C(0x5678), &metadata);
    queue_and_scan(&transaction, &fake, frame);
    status = status_of(&transaction);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_WRONG_TRANSACTION);
    assert(status.transaction_id == UINT16_C(0x1234));
    assert(status.payload_length == sizeof(profile) && status.digest == metadata.digest);
    assert(fake.begin_calls == 1u);

    begin_frame(frame, UINT16_C(0x1234), &metadata);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
    assert(fake.begin_calls == 1u);

    metadata.digest++;
    begin_frame(frame, UINT16_C(0x1234), &metadata);
    queue_and_scan(&transaction, &fake, frame);
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(status.flags == NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_CONFLICTING_RETRY);

    metadata.digest--;
    begin_frame(frame, UINT16_C(0x1234), &metadata);
    queue_and_scan(&transaction, &fake, frame);
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(status.flags == NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_POISONED);
}

static void begin_transaction(noah_profile_candidate_transaction_t *transaction, fake_backend_t *fake, uint16_t transaction_id, const noah_profile_candidate_v1_metadata_t *metadata) {
    uint8_t frame[32];
    begin_frame(frame, transaction_id, metadata);
    queue_and_scan(transaction, fake, frame);
    assert(status_of(transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
}

static void write_chunk(noah_profile_candidate_transaction_t *transaction, fake_backend_t *fake, uint16_t transaction_id, uint16_t offset, const uint8_t *bytes, uint8_t length) {
    uint8_t frame[32];
    chunk_frame(frame, transaction_id, offset, bytes, length);
    queue_and_scan(transaction, fake, frame);
}

static void test_sequential_chunks_and_duplicate_rules(void) {
    uint8_t profile[40];
    fake_backend_t fake;
    noah_profile_candidate_backend_t backend;
    noah_profile_candidate_compatibility_t compatible = compatibility();
    noah_profile_candidate_transaction_t transaction;
    noah_profile_candidate_v1_metadata_t metadata;
    noah_profile_candidate_v1_status_t status;

    for (uint8_t index = 0u; index < sizeof(profile); index++) {
        profile[index] = index;
    }
    metadata = metadata_for(profile, sizeof(profile));
    fake_init(&fake);
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_transaction(&transaction, &fake, 7u, &metadata);
    write_chunk(&transaction, &fake, 7u, 0u, profile, 20u);
    assert(status_of(&transaction).next_offset == 20u);

    write_chunk(&transaction, &fake, 7u, 21u, &profile[21], 1u);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_OUT_OF_ORDER);
    assert(status_of(&transaction).next_offset == 20u);
    write_chunk(&transaction, &fake, 7u, 15u, &profile[15], 10u);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_OUT_OF_ORDER);
    write_chunk(&transaction, &fake, 7u, 35u, &profile[30], 10u);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_CAPACITY_EXCEEDED);
    write_chunk(&transaction, &fake, 8u, 20u, &profile[20], 20u);
    status = status_of(&transaction);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_WRONG_TRANSACTION);
    assert(status.transaction_id == 7u && status.next_offset == 20u);

    write_chunk(&transaction, &fake, 7u, 0u, profile, 20u);
    assert(fake.read_calls == 1u && fake.write_calls == 1u);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
    write_chunk(&transaction, &fake, 7u, 20u, &profile[20], 20u);
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE);
    write_chunk(&transaction, &fake, 7u, 20u, &profile[20], 20u);
    assert(fake.read_calls == 2u && fake.write_calls == 2u);

    profile[20] ^= 0xFFu;
    write_chunk(&transaction, &fake, 7u, 20u, &profile[20], 20u);
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_CONFLICTING_RETRY);
    assert((status.flags & NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED) != 0u);
}

static void stage_complete(noah_profile_candidate_transaction_t *transaction, fake_backend_t *fake, uint16_t transaction_id, const uint8_t *profile, uint16_t length, const noah_profile_candidate_v1_metadata_t *metadata) {
    uint16_t offset;

    begin_transaction(transaction, fake, transaction_id, metadata);
    for (offset = 0u; offset < length;) {
        uint16_t remaining = (uint16_t)(length - offset);
        uint8_t chunk_length = remaining > 20u ? 20u : (uint8_t)remaining;
        write_chunk(transaction, fake, transaction_id, offset, &profile[offset], chunk_length);
        offset = (uint16_t)(offset + chunk_length);
    }
    assert(status_of(transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE);
}

static void request_validation(noah_profile_candidate_transaction_t *transaction, fake_backend_t *fake, uint16_t transaction_id) {
    uint8_t frame[32];
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, transaction_id);
    queue_and_scan(transaction, fake, frame);
}

static void finish_validation(noah_profile_candidate_transaction_t *transaction) {
    unsigned guard = 0u;
    while (status_of(transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING) {
        assert(noah_profile_candidate_transaction_scan(transaction));
        assert(++guard < 300u);
    }
}

static void test_bounded_validation_and_error_locations(void) {
    uint8_t profile[48];
    fake_backend_t fake;
    noah_profile_candidate_backend_t backend;
    noah_profile_candidate_compatibility_t compatible = compatibility();
    noah_profile_candidate_transaction_t transaction;
    noah_profile_candidate_v1_metadata_t metadata;
    noah_profile_candidate_v1_status_t status;

    for (uint8_t index = 0u; index < sizeof(profile); index++) {
        profile[index] = (uint8_t)(0x80u + index);
    }
    metadata = metadata_for(profile, sizeof(profile));
    fake_init(&fake);
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_transaction(&transaction, &fake, 9u, &metadata);
    write_chunk(&transaction, &fake, 9u, 0u, profile, 20u);
    request_validation(&transaction, &fake, 9u);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE);
    write_chunk(&transaction, &fake, 9u, 20u, &profile[20], 20u);
    write_chunk(&transaction, &fake, 9u, 40u, &profile[40], 8u);

    request_validation(&transaction, &fake, 9u);
    assert(fake.validation_begin_calls == 1u && fake.validation_step_calls == 0u);
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING);
    finish_validation(&transaction);
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
    assert(fake.validation_step_calls == 3u);
    assert(fake.max_validation_budget == NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET);

    fake_init(&fake);
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    metadata.digest ^= 1u;
    stage_complete(&transaction, &fake, 10u, profile, sizeof(profile), &metadata);
    request_validation(&transaction, &fake, 10u);
    finish_validation(&transaction);
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_CHECKSUM_MISMATCH);
    assert(status.error.byte_offset == sizeof(profile));

    fake_init(&fake);
    fake.semantic_reject = true;
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    metadata = metadata_for(profile, sizeof(profile));
    stage_complete(&transaction, &fake, 11u, profile, sizeof(profile), &metadata);
    request_validation(&transaction, &fake, 11u);
    finish_validation(&transaction);
    status = status_of(&transaction);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED);
    assert(status.error.domain_id == 0x20u && status.error.table_id == 2u);
    assert(status.error.row_index == 5u && status.error.tap_index == 3u && status.error.field_id == 4u && status.error.byte_offset == 6u);

    fake_init(&fake);
    fake.validation_step_returns_ok = true;
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    stage_complete(&transaction, &fake, 12u, profile, sizeof(profile), &metadata);
    request_validation(&transaction, &fake, 12u);
    assert(noah_profile_candidate_transaction_scan(&transaction));
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE);
}

static void test_commit_activation_and_idempotent_retry(void) {
    static const uint8_t profile[8] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
    fake_backend_t fake;
    noah_profile_candidate_backend_t backend;
    noah_profile_candidate_compatibility_t compatible = compatibility();
    noah_profile_candidate_transaction_t transaction;
    noah_profile_candidate_v1_metadata_t metadata = metadata_for(profile, sizeof(profile));
    noah_profile_candidate_v1_status_t status;
    uint8_t frame[32];

    fake_init(&fake);
    fake.commit_steps_remaining     = 1u;
    fake.activation_steps_remaining = 1u;
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    stage_complete(&transaction, &fake, 50u, profile, sizeof(profile), &metadata);

    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 50u);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE);
    assert(fake.commit_begin_calls == 0u);

    request_validation(&transaction, &fake, 50u);
    finish_validation(&transaction);
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);

    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 50u);
    queue_and_scan(&transaction, &fake, frame);
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMMITTING);
    assert(status.last_operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_COMMIT);
    assert(fake.commit_begin_calls == 1u && fake.commit_step_calls == 0u);

    // A lost acknowledgment can be retried without restarting persistence.
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 50u);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMMITTING);
    assert(fake.commit_begin_calls == 1u && fake.commit_step_calls == 0u);

    assert(noah_profile_candidate_transaction_scan(&transaction));
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMMITTING);
    assert(noah_profile_candidate_transaction_scan(&transaction));
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING);
    assert(fake.commit_step_calls == 2u && fake.activation_begin_calls == 1u);
    assert(noah_profile_candidate_transaction_scan(&transaction));
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING);
    assert(noah_profile_candidate_transaction_scan(&transaction));
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(status.transaction_id == 50u && status.digest == metadata.digest);
    assert(transaction.last_committed_transaction_id == 50u);

    // Retrying after success is also a no-op and preserves correlation.
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 50u);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(fake.commit_begin_calls == 1u && fake.commit_step_calls == 2u && fake.activation_begin_calls == 1u && fake.activation_step_calls == 2u);
}

static void test_activation_failure_is_durable_and_requires_status_clear(void) {
    static const uint8_t profile[8] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
    fake_backend_t fake;
    noah_profile_candidate_backend_t backend;
    noah_profile_candidate_compatibility_t compatible = compatibility();
    noah_profile_candidate_transaction_t transaction;
    noah_profile_candidate_v1_metadata_t metadata = metadata_for(profile, sizeof(profile));
    uint8_t frame[32];

    fake_init(&fake);
    fake.activation_terminal_result = NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    stage_complete(&transaction, &fake, 51u, profile, sizeof(profile), &metadata);
    request_validation(&transaction, &fake, 51u);
    finish_validation(&transaction);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 51u);
    queue_and_scan(&transaction, &fake, frame);
    assert(noah_profile_candidate_transaction_scan(&transaction));
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING);

    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 51u);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE);
    assert(fake.abort_calls == 0u);

    assert(noah_profile_candidate_transaction_scan(&transaction));
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_ACTIVATION_FAILED);
    assert(!transaction.has_candidate && transaction.poisoned);
    assert(transaction.last_committed_transaction_id == 51u);

    // The durable record cannot be aborted, but an idempotent abort clears the
    // operation error so a later transaction can reconcile or redeploy.
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 51u);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(!transaction.poisoned && fake.abort_calls == 0u);
}

static void test_unknown_marker_durability_is_not_reported_as_safe_failure(void) {
    static const uint8_t profile[8] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
    fake_backend_t fake;
    noah_profile_candidate_backend_t backend;
    noah_profile_candidate_compatibility_t compatible = compatibility();
    noah_profile_candidate_transaction_t transaction;
    noah_profile_candidate_v1_metadata_t metadata = metadata_for(profile, sizeof(profile));
    uint8_t frame[32];

    fake_init(&fake);
    fake.commit_terminal_result = NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN;
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    stage_complete(&transaction, &fake, 52u, profile, sizeof(profile), &metadata);
    request_validation(&transaction, &fake, 52u);
    finish_validation(&transaction);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 52u);
    queue_and_scan(&transaction, &fake, frame);
    assert(noah_profile_candidate_transaction_scan(&transaction));
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_DURABILITY_UNKNOWN);
    assert(!transaction.has_candidate && transaction.poisoned);
    assert(fake.activation_begin_calls == 0u);
}

static void test_abort_reset_and_no_timeout(void) {
    static const uint8_t profile[8] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
    fake_backend_t fake;
    noah_profile_candidate_backend_t backend;
    noah_profile_candidate_compatibility_t compatible = compatibility();
    noah_profile_candidate_transaction_t transaction;
    noah_profile_candidate_v1_metadata_t metadata = metadata_for(profile, sizeof(profile));
    noah_profile_candidate_v1_status_t status;
    uint8_t frame[32];

    fake_init(&fake);
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_transaction(&transaction, &fake, 20u, &metadata);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 21u);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_WRONG_TRANSACTION);
    assert(fake.abort_calls == 0u && status_of(&transaction).transaction_id == 20u);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 20u);
    queue_and_scan(&transaction, &fake, frame);
    assert(fake.abort_calls == 1u && status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 20u);
    queue_and_scan(&transaction, &fake, frame);
    assert(fake.abort_calls == 1u && status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);

    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_frame(frame, 30u, &metadata);
    queue(&transaction, &fake, frame);
    assert(status_of(&transaction).flags == NOAH_PROFILE_CANDIDATE_V1_STATUS_MAILBOX_PENDING);
    {
        unsigned calls = backend_call_count(&fake);
        noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
        assert(backend_call_count(&fake) == calls);
        assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
        assert(!noah_profile_candidate_transaction_scan(&transaction));
    }

    begin_transaction(&transaction, &fake, 31u, &metadata);
    write_chunk(&transaction, &fake, 31u, 0u, profile, sizeof(profile));
    assert(fake.staged[0] == 'N');
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    assert(status_of(&transaction).state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(!noah_profile_candidate_transaction_scan(&transaction));

    begin_transaction(&transaction, &fake, 32u, &metadata);
    for (unsigned scan = 0u; scan < 1000u; scan++) {
        assert(!noah_profile_candidate_transaction_scan(&transaction));
    }
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
    assert(status.transaction_id == 32u);

    fake_init(&fake);
    fake.begin_result = NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_frame(frame, 40u, &metadata);
    queue_and_scan(&transaction, &fake, frame);
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE);
    assert((status.flags & NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED) != 0u);
}

static void test_begin_compatibility_rejections(void) {
    static const uint8_t profile[8] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
    fake_backend_t fake;
    noah_profile_candidate_backend_t backend;
    noah_profile_candidate_compatibility_t compatible = compatibility();
    noah_profile_candidate_transaction_t transaction;
    noah_profile_candidate_v1_metadata_t metadata = metadata_for(profile, sizeof(profile));
    uint8_t frame[32];

    fake_init(&fake);
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    metadata.schema_minor = 1u;
    begin_frame(frame, 1u, &metadata);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_INCOMPATIBLE_SCHEMA && fake.begin_calls == 0u);
    metadata.schema_minor = 0u;
    metadata.requested_domains = NOAH_PROFILE_CANDIDATE_V1_DOMAIN_RGB;
    compatible.supported_domain_mask = 0u;
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_frame(frame, 2u, &metadata);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_UNSUPPORTED_DOMAIN && fake.begin_calls == 0u);
    metadata.requested_domains = 0u;
    metadata.action_abi_digest++;
    compatible = compatibility();
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_frame(frame, 3u, &metadata);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_INCOMPATIBLE_ACTION_ABI && fake.begin_calls == 0u);
    metadata.action_abi_digest--;
    compatible.max_payload_length = 7u;
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_frame(frame, 4u, &metadata);
    queue_and_scan(&transaction, &fake, frame);
    assert(status_of(&transaction).error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_CAPACITY_EXCEEDED && fake.begin_calls == 0u);
}

static void test_busy_begin_remains_queued_without_poisoning(void) {
    static const uint8_t profile[8] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
    fake_backend_t fake;
    noah_profile_candidate_backend_t backend;
    noah_profile_candidate_compatibility_t compatible = compatibility();
    noah_profile_candidate_transaction_t transaction;
    noah_profile_candidate_v1_metadata_t metadata = metadata_for(profile, sizeof(profile));
    noah_profile_candidate_v1_status_t status;
    uint8_t frame[32];

    fake_init(&fake);
    fake.begin_result = NOAH_PROFILE_CANDIDATE_BACKEND_BUSY;
    backend = backend_for(&fake);
    noah_profile_candidate_transaction_init(&transaction, &backend, &compatible);
    begin_frame(frame, 55u, &metadata);
    queue(&transaction, &fake, frame);

    assert(noah_profile_candidate_transaction_scan(&transaction));
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_MAILBOX_BUSY);
    assert((status.flags & NOAH_PROFILE_CANDIDATE_V1_STATUS_MAILBOX_PENDING) != 0u);
    assert((status.flags & NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED) == 0u);
    assert(fake.begin_calls == 1u);

    assert(noah_profile_candidate_transaction_scan(&transaction));
    assert(fake.begin_calls == 2u);
    fake.begin_result = NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    assert(noah_profile_candidate_transaction_scan(&transaction));
    status = status_of(&transaction);
    assert(status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
    assert((status.flags & NOAH_PROFILE_CANDIDATE_V1_STATUS_MAILBOX_PENDING) == 0u);
    assert((status.flags & NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED) == 0u);
    assert(fake.begin_calls == 3u);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    fixture_path = argv[1];
    assert(sizeof(noah_profile_candidate_transaction_t) <= 256u);
    test_exact_frame_codecs();
    test_malformed_frame_matrix();
    test_operation_status_codec();
    test_mailbox_and_begin_retries();
    test_sequential_chunks_and_duplicate_rules();
    test_bounded_validation_and_error_locations();
    test_commit_activation_and_idempotent_retry();
    test_activation_failure_is_durable_and_requires_status_clear();
    test_unknown_marker_durability_is_not_reported_as_safe_failure();
    test_abort_reset_and_no_timeout();
    test_begin_compatibility_rejections();
    test_busy_begin_remains_queued_without_poisoning();
    puts("profile candidate transaction host tests passed");
    return 0;
}
