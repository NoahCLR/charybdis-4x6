#include "../schema/profile_versions.h"
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
    NOAH_PROFILE_CANDIDATE_V1_VALUE_REUSE         = 0x1Bu,
    NOAH_PROFILE_CANDIDATE_V1_VALUE_STREAM_CHUNK  = 0x1Cu,
    NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX           = 20u,
    NOAH_PROFILE_CANDIDATE_V1_REUSE_MAX           = 1024u,
    NOAH_PROFILE_CANDIDATE_V1_MIN_BLOB_SIZE       = 8u,
    NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE       = NOAH_PROFILE_PAYLOAD_MAX,
    NOAH_PROFILE_CANDIDATE_V1_DOMAIN_RGB          = NOAH_PROFILE_DOMAIN_MASK_RGB,
    NOAH_PROFILE_CANDIDATE_V1_DOMAIN_KEY_BEHAVIOR = NOAH_PROFILE_DOMAIN_MASK_KEY_BEHAVIORS,
    NOAH_PROFILE_CANDIDATE_V1_DOMAIN_COMBOS       = NOAH_PROFILE_DOMAIN_MASK_COMBOS,
    NOAH_PROFILE_CANDIDATE_V1_DOMAIN_SETTINGS     = NOAH_PROFILE_DOMAIN_MASK_SETTINGS,
    NOAH_PROFILE_CANDIDATE_V1_DOMAIN_PD = NOAH_PROFILE_ENABLED_PD_MASK,
    NOAH_PROFILE_CANDIDATE_V1_KNOWN_DOMAINS = NOAH_PROFILE_ENABLED_DOMAIN_MASK,
    NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8    = 0xFFu,
    NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16   = 0xFFFFu,
    // Status page 0 carries progress; page 3 the rejected byte and its
    // location (D-F28), since 32-bit offsets no longer fit one page.
    NOAH_PROFILE_CANDIDATE_V1_STATUS_ERROR_PAGE   = 3u,
};
#define NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U32 UINT32_MAX

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
    // The copy to the other half stopped for good before any commit (after
    // its storage retries); candidate status page 1 says what the peer
    // answered last. Nothing was saved on either half.
    NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_TRANSFER_FAILED,
} noah_profile_candidate_v1_error_id_t;

typedef enum {
    NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED = 0u,
    NOAH_PROFILE_CANDIDATE_V1_ADMISSION_MALFORMED,
    NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY,
    NOAH_PROFILE_CANDIDATE_V1_ADMISSION_UNSUPPORTED,
    NOAH_PROFILE_CANDIDATE_V1_ADMISSION_REJECTED,
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
    NOAH_PROFILE_CANDIDATE_V1_OPERATION_REUSE,
} noah_profile_candidate_v1_operation_t;

enum {
    NOAH_PROFILE_CANDIDATE_V1_STATUS_MAILBOX_PENDING = 1u << 0,
    NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED        = 1u << 1,
};

typedef struct {
    uint8_t  schema_major;
    uint8_t  schema_minor;
    uint8_t  requested_domains;
    // Not on the wire since D-F28: always zero.
    uint8_t  flags;
    uint32_t payload_length;
    uint32_t crc32;
    uint32_t digest;
    uint32_t action_abi_digest;
    // Not on the wire since D-F28: the base's version implies store format
    // 6, which the decoder fills in. The report tail that bound a candidate
    // to the VIA store must be zero since D-F31.
    uint8_t  store_format_version;
} noah_profile_candidate_v1_metadata_t;

// Two candidate declarations are the same. The one home of this comparison:
// the store-free transaction and the store backend both ask it.
static inline bool noah_profile_candidate_v1_metadata_equal(const noah_profile_candidate_v1_metadata_t *left, const noah_profile_candidate_v1_metadata_t *right) {
    return left && right && left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->requested_domains == right->requested_domains && left->flags == right->flags && left->payload_length == right->payload_length && left->crc32 == right->crc32 && left->digest == right->digest && left->action_abi_digest == right->action_abi_digest && left->store_format_version == right->store_format_version;
}

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
    uint32_t                             byte_offset;
} noah_profile_candidate_v1_error_t;

typedef struct {
    uint32_t generation;
    uint32_t digest;
    uint32_t crc32;
    uint8_t  kind; // compiled (0), committed (1) or the working copy (2, D-F32).
    uint8_t  origin;
} noah_profile_candidate_v1_source_t;

typedef struct {
    noah_profile_candidate_v1_operation_t operation;
    uint16_t                              transaction_id;
    union {
        noah_profile_candidate_v1_metadata_t begin;
        struct {
            uint32_t offset;
            uint8_t  length;
            uint8_t  bytes[NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX];
        } chunk;
        struct {
            noah_profile_candidate_v1_source_t source;
            uint32_t offset;
            uint32_t source_offset;
            uint16_t length; // at most NOAH_PROFILE_CANDIDATE_V1_REUSE_MAX
            uint16_t copied; // Scan-owned progress, never supplied by the host.
        } reuse;
    } payload;
} noah_profile_candidate_v1_command_t;

typedef struct {
    noah_profile_candidate_v1_state_t     state;
    noah_profile_candidate_v1_operation_t last_operation;
    uint8_t                               flags;
    uint16_t                              transaction_id;
    uint32_t                              next_offset;
    uint32_t                              payload_length;
    uint32_t                              digest;
    noah_profile_candidate_v1_error_t     error;
    uint16_t                              operation_sequence;
    // Advances whenever `error` changes, including asynchronous validation
    // results that no mailbox operation precedes; pages 0 and 3 both carry it.
    uint16_t                              error_sequence;
} noah_profile_candidate_v1_status_t;

noah_profile_candidate_v1_decode_result_t noah_profile_candidate_v1_decode(const uint8_t *frame, size_t length, noah_profile_candidate_v1_command_t *command, noah_profile_candidate_v1_frame_error_t *error);

// Preserves bytes 0..4 from a complete request and emits canonical zero
// padding. The request and response may be the same 32-byte buffer.
void noah_profile_candidate_v1_encode_ack(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], noah_profile_candidate_v1_admission_t admission, noah_profile_candidate_v1_error_id_t error, uint8_t frame_offset);

// Handles value 0x18 pages 0 (progress) and 3 (the error's location).
bool noah_profile_candidate_v1_handle_status_get(const noah_profile_candidate_v1_status_t *status, uint8_t *frame, size_t length);

// Status page 1: how far a save's copy to the other half has got, and the
// other half's last answer. Page 0 cannot show it, so without this the host
// waits out PREPARING_PEER blind. The phase is a stable wire enum, mapped
// from the split reconciler's internal states by the owner.
typedef enum {
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_IDLE = 0u,
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_BINDING,    // retired with VIA (D-F31), never reported
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_BEGINNING,  // PREPARE_BEGIN not yet admitted
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_SENDING,    // payload bytes in flight
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_PREPARING,  // peer validating and writing its prepared copy
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_PREPARED,   // peer holds a complete inactive copy
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_COMMITTING, // peer commit authorized, converging
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_ABORTING,   // cancelling the peer's copy
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_STOPPED,    // reconciler stopped; last_status says why
    NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_WAITING,    // retrying the link between steps
} noah_profile_candidate_v1_peer_phase_t;

enum {
    NOAH_PROFILE_CANDIDATE_V1_PEER_FLAG_CLEANUP_PENDING = 1u << 0,
    NOAH_PROFILE_CANDIDATE_V1_PEER_FLAG_MASTER          = 1u << 1,
    // The commit decision, or the ACCEPT after it, waits for held keys,
    // locked layers, one-shots or macros to finish on this half.
    NOAH_PROFILE_CANDIDATE_V1_PEER_FLAG_WAITING_SAFE_BOUNDARY = 1u << 2,
    NOAH_PROFILE_CANDIDATE_V1_PEER_FLAGS_KNOWN          = 0x07u,
};

typedef struct {
    noah_profile_candidate_v1_peer_phase_t phase;
    uint8_t                                last_status; // noah_profile_split_v1_status_t of the last peer answer
    uint8_t                                flags;
    uint32_t                               transfer_offset;
    uint32_t                               transfer_length;
    uint32_t                               retry_count;             // cumulative since boot
    uint32_t                               transport_failure_count; // cumulative since boot
    // The peer's last ACK/BUSY: BUSY replies in a row, why
    // (noah_profile_split_v1_busy_reason_t), and its store state and transfer
    // owner then. Diagnostics for a copy that is not moving.
    uint16_t                               busy_streak;
    uint8_t                                busy_reason;
    uint8_t                                busy_store_state;
    uint8_t                                busy_owner;
    uint8_t                                busy_admission; // noah_profile_storage_admission_owner_t
    // Page 2: the current/last prepared push; page 1 stays byte-identical.
    uint32_t                               reused_bytes;
    uint16_t                               reuse_exchanges;
    uint16_t                               source_unavailable_replies;
    uint32_t                               prepared_scans;
    uint32_t                               prepared_exchanges;
    bool                                   reuse_unavailable;
} noah_profile_candidate_v1_peer_status_t;

// Handles value 0x18 pages 1 and 2, and returns false for anything else so the
// caller falls through to the page-0 codec.
bool noah_profile_candidate_v1_handle_peer_status_get(const noah_profile_candidate_v1_peer_status_t *peer, uint8_t *frame, size_t length);

noah_profile_candidate_v1_error_t noah_profile_candidate_v1_no_error(void);
