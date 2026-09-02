// ───────────────────────────────────────────────────────────────────────────
// Live Profile Candidate Wire V1
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile_wire_v1.h"

enum {
    NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET         = 0x07u,
    NOAH_PROFILE_CANDIDATE_V1_COMMAND_SAVE        = 0x09u,
    NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN         = 0x10u,
    NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK         = 0x11u,
    NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE      = 0x12u,
    NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT        = 0x13u,
    NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT         = 0x14u,
    NOAH_PROFILE_CANDIDATE_V1_VALUE_STATUS        = 0x18u,
    NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX           = 20u,
    NOAH_PROFILE_CANDIDATE_V1_MIN_BLOB_SIZE       = 8u,
    NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE       = 4064u,
    NOAH_PROFILE_CANDIDATE_V1_DOMAIN_RGB          = 1u << 0,
    NOAH_PROFILE_CANDIDATE_V1_DOMAIN_KEY_BEHAVIOR = 1u << 1,
    NOAH_PROFILE_CANDIDATE_V1_KNOWN_DOMAINS       = NOAH_PROFILE_CANDIDATE_V1_DOMAIN_RGB | NOAH_PROFILE_CANDIDATE_V1_DOMAIN_KEY_BEHAVIOR,
    NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8    = 0xFFu,
    NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16   = 0xFFFFu,
};

typedef enum {
    NOAH_PROFILE_CANDIDATE_V1_DECODE_OK = 0u,
    NOAH_PROFILE_CANDIDATE_V1_DECODE_NOT_HANDLED,
    NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_ARGUMENT,
    NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_LENGTH,
    NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED,
} noah_profile_candidate_v1_decode_result_t;

typedef enum {
    NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE = 0u,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_MALFORMED_FRAME,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_MAILBOX_BUSY,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_TRANSACTION,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_WRONG_TRANSACTION,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_INCOMPATIBLE_SCHEMA,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_UNSUPPORTED_DOMAIN,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_INCOMPATIBLE_ACTION_ABI,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_CAPACITY_EXCEEDED,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_OUT_OF_ORDER,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_CONFLICTING_RETRY,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_CHECKSUM_MISMATCH,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_UNSUPPORTED_OPERATION,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_POISONED,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_ACTIVATION_FAILED,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_DURABILITY_UNKNOWN,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_PREPARE_YIELDED,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_POSTCOMMIT_AUTHORITY_LOST,
    NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_COMMIT_CONFLICT,
} noah_profile_candidate_v1_error_id_t;

typedef enum {
    NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED = 0u,
    NOAH_PROFILE_CANDIDATE_V1_ADMISSION_MALFORMED,
    NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY,
    NOAH_PROFILE_CANDIDATE_V1_ADMISSION_UNSUPPORTED,
} noah_profile_candidate_v1_admission_t;

typedef enum {
    NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE = 0u,
    NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING,
    NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE,
    NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING,
    NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED,
    NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED,
    NOAH_PROFILE_CANDIDATE_V1_STATE_COMMITTING,
    NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING,
    NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER,
    NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER,
    NOAH_PROFILE_CANDIDATE_V1_STATE_AUTHORITY_FAILED,
} noah_profile_candidate_v1_state_t;

typedef enum {
    NOAH_PROFILE_CANDIDATE_V1_OPERATION_NONE = 0u,
    NOAH_PROFILE_CANDIDATE_V1_OPERATION_BEGIN,
    NOAH_PROFILE_CANDIDATE_V1_OPERATION_CHUNK,
    NOAH_PROFILE_CANDIDATE_V1_OPERATION_VALIDATE,
    NOAH_PROFILE_CANDIDATE_V1_OPERATION_ABORT,
    NOAH_PROFILE_CANDIDATE_V1_OPERATION_COMMIT,
} noah_profile_candidate_v1_operation_t;

enum {
    NOAH_PROFILE_CANDIDATE_V1_STATUS_MAILBOX_PENDING = 1u << 0,
    NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED        = 1u << 1,
};

typedef struct {
    uint8_t schema_major;
    uint8_t schema_minor;
    uint8_t requested_domains;
    uint8_t flags;
    uint16_t payload_length;
    uint32_t crc32;
    uint32_t digest;
    uint32_t action_abi_digest;
} noah_profile_candidate_v1_metadata_t;

typedef struct {
    noah_profile_candidate_v1_error_id_t code;
    uint8_t                              frame_offset;
} noah_profile_candidate_v1_frame_error_t;

typedef struct {
    noah_profile_candidate_v1_error_id_t code;
    uint8_t                              domain_id;
    uint8_t                              table_id;
    uint16_t                             row_index;
    uint8_t                              tap_index;
    uint8_t                              field_id;
    uint16_t                             byte_offset;
} noah_profile_candidate_v1_error_t;

typedef struct {
    noah_profile_candidate_v1_operation_t operation;
    uint16_t                              transaction_id;
    union {
        noah_profile_candidate_v1_metadata_t begin;
        struct {
            uint16_t offset;
            uint8_t  length;
            uint8_t  bytes[NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX];
        } chunk;
    } payload;
} noah_profile_candidate_v1_command_t;

typedef struct {
    noah_profile_candidate_v1_state_t     state;
    noah_profile_candidate_v1_operation_t last_operation;
    uint8_t                               flags;
    uint16_t                              transaction_id;
    uint16_t                              next_offset;
    uint16_t                              payload_length;
    uint32_t                              digest;
    noah_profile_candidate_v1_error_t     error;
    uint16_t                              operation_sequence;
} noah_profile_candidate_v1_status_t;

noah_profile_candidate_v1_decode_result_t noah_profile_candidate_v1_decode(const uint8_t *frame, size_t length, noah_profile_candidate_v1_command_t *command, noah_profile_candidate_v1_frame_error_t *error);

// Preserves bytes 0..4 from a complete request and emits canonical zero
// padding. The request and response may be the same 32-byte buffer.
void noah_profile_candidate_v1_encode_ack(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], noah_profile_candidate_v1_admission_t admission, noah_profile_candidate_v1_error_id_t error, uint8_t frame_offset);

// Handles only value 0x18. This standalone status codec is intentionally not
// wired into the production VIA hook until candidate-write support is complete.
bool noah_profile_candidate_v1_handle_status_get(const noah_profile_candidate_v1_status_t *status, uint8_t *frame, size_t length);

noah_profile_candidate_v1_error_t noah_profile_candidate_v1_no_error(void);
