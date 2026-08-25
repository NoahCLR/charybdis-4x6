// ───────────────────────────────────────────────────────────────────────────
// Incremental Whole-Profile Validator — Profile Wire v1.0
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "key_behavior_domain_v1.h"
#include "profile_rgb_v1.h"

enum {
    NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB          = 1u << 0,
    NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS = 1u << 1,
    NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS       = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS,
    NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX  = 20u,
    NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8    = 0xffu,
    NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U16   = 0xffffu,
};

typedef enum {
    NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS = 0u,
    NOAH_PROFILE_VALIDATOR_V1_VALID,
    NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT,
    NOAH_PROFILE_VALIDATOR_V1_READ_ERROR,
    NOAH_PROFILE_VALIDATOR_V1_INCOMPATIBLE_SCHEMA,
    NOAH_PROFILE_VALIDATOR_V1_INCOMPATIBLE_ACTION_ABI,
    NOAH_PROFILE_VALIDATOR_V1_CAPACITY_EXCEEDED,
    NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_MISMATCH,
    NOAH_PROFILE_VALIDATOR_V1_INVALID_BLOB,
    NOAH_PROFILE_VALIDATOR_V1_UNSUPPORTED_DOMAIN,
    NOAH_PROFILE_VALIDATOR_V1_MISSING_DOMAIN,
    NOAH_PROFILE_VALIDATOR_V1_DOMAIN_MASK_MISMATCH,
    NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN,
    NOAH_PROFILE_VALIDATOR_V1_INVALID_REFERENCE,
} noah_profile_validator_v1_result_t;

typedef enum {
    NOAH_PROFILE_VALIDATOR_V1_DETAIL_NONE = 0u,
    NOAH_PROFILE_VALIDATOR_V1_DETAIL_BLOB,
    NOAH_PROFILE_VALIDATOR_V1_DETAIL_KEY_BEHAVIOR,
    NOAH_PROFILE_VALIDATOR_V1_DETAIL_RGB,
} noah_profile_validator_v1_detail_t;

// Exposed for deterministic scan-owner tests and diagnostics. A step either
// remains in CHECKSUM after consuming one bounded chunk or advances one phase.
typedef enum {
    NOAH_PROFILE_VALIDATOR_V1_PHASE_UNINITIALIZED = 0u,
    NOAH_PROFILE_VALIDATOR_V1_PHASE_CHECKSUM,
    NOAH_PROFILE_VALIDATOR_V1_PHASE_BLOB_HEADER,
    NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_HEADER,
    NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_DECODE,
    NOAH_PROFILE_VALIDATOR_V1_PHASE_CROSS_REFERENCE_ROW,
    NOAH_PROFILE_VALIDATOR_V1_PHASE_CROSS_REFERENCE_STEP,
    NOAH_PROFILE_VALIDATOR_V1_PHASE_VALID,
    NOAH_PROFILE_VALIDATOR_V1_PHASE_REJECTED,
} noah_profile_validator_v1_phase_t;

typedef struct {
    noah_profile_validator_v1_result_t code;
    size_t                             byte_offset;
    uint8_t                            domain_index;
    uint8_t                            domain_id;
    uint8_t                            table_id;
    uint16_t                           row_index;
    uint8_t                            step_index;
    uint8_t                            field_id;
    noah_profile_validator_v1_detail_t detail_kind;
    uint16_t                           detail_code;
} noah_profile_validator_v1_error_t;

// Candidate metadata is deliberately independent of Raw HID framing and the
// storage slot header. The caller supplies the identities declared at begin.
typedef struct {
    uint8_t  schema_major;
    uint8_t  schema_minor;
    uint8_t  domain_mask;
    uint8_t  flags;
    uint16_t byte_length;
    uint32_t crc32;
    uint32_t digest;
    uint32_t action_abi_digest;
} noah_profile_validator_v1_declaration_t;

typedef struct {
    uint8_t  required_domain_mask;
    uint8_t  allowed_domain_mask;
    uint16_t max_blob_size;
    uint32_t action_abi_digest;

    // Exact compiled identities used by behavior cross-reference validation.
    uint8_t logical_layer_count;
    uint8_t supported_pd_mode_mask;
    uint8_t via_macro_slot_count;
    uint8_t hardcoded_macro_slot_count;

    noah_key_behavior_limits_v1_t  behavior_limits;
    noah_profile_rgb_v1_limits_t   rgb_limits;
} noah_profile_validator_v1_compatibility_t;

typedef struct {
    uint8_t                        domain_mask;
    uint8_t                        domain_count;
    uint16_t                       byte_length;
    uint32_t                       crc32;
    uint32_t                       digest;
    uint32_t                       action_abi_digest;
    noah_profile_rgb_v1_view_t     rgb;
    noah_key_behavior_domain_v1_t  key_behaviors;
} noah_profile_validator_v1_profile_t;

// Caller-owned, payload-independent state. Treat fields after phase as
// private; they are public only so firmware can allocate the object statically.
typedef struct {
    noah_profile_validator_v1_phase_t         phase;
    noah_profile_validator_v1_result_t        terminal_result;
    noah_profile_validator_v1_error_t         terminal_error;
    noah_profile_reader_t                     reader;
    size_t                                    base_offset;
    noah_profile_validator_v1_declaration_t   declaration;
    noah_profile_validator_v1_compatibility_t compatibility;
    noah_profile_validator_v1_profile_t       profile;
    size_t                                    checksum_offset;
    uint32_t                                  crc32_state;
    uint32_t                                  digest_state;
    size_t                                    blob_offset;
    size_t                                    domain_payload_offset;
    uint16_t                                  domain_payload_length;
    uint8_t                                   declared_domain_count;
    uint8_t                                   domain_index;
    uint8_t                                   current_domain_id;
    uint8_t                                   previous_domain_id;
    uint8_t                                   seen_domain_mask;
    uint8_t                                   cross_row_index;
    uint8_t                                   cross_step_index;
    size_t                                    cross_step_offset;
    noah_key_behavior_row_v1_view_t           cross_row;
} noah_profile_validator_v1_t;

noah_profile_validator_v1_compatibility_t noah_profile_validator_v1_default_compatibility(uint32_t action_abi_digest);
noah_profile_validator_v1_error_t noah_profile_validator_v1_no_error(void);

// begin performs compatibility/capacity checks but no reader I/O. A successful
// begin returns IN_PROGRESS. base_offset allows validating a slot-bounded view.
noah_profile_validator_v1_result_t noah_profile_validator_v1_begin(noah_profile_validator_v1_t *validator, const noah_profile_reader_t *reader, size_t base_offset, const noah_profile_validator_v1_declaration_t *declaration, const noah_profile_validator_v1_compatibility_t *compatibility, noah_profile_validator_v1_error_t *error);

// During CHECKSUM, at most min(checksum_byte_budget, 20, remaining) bytes are
// read. Later calls deliberately ignore the byte budget and execute one phase:
// one 8-byte blob header, one 4-byte domain envelope, one existing reader-
// backed domain decoder call, or one behavior row/step accessor. Existing
// domain decoders may issue multiple reads, but no individual read exceeds
// their frozen 16-byte RGB / 12-byte behavior bound.
noah_profile_validator_v1_result_t noah_profile_validator_v1_step(noah_profile_validator_v1_t *validator, uint8_t checksum_byte_budget, noah_profile_validator_v1_error_t *error);

// Available only after step returns VALID. The copied views continue to borrow
// the reader supplied to begin.
noah_profile_validator_v1_result_t noah_profile_validator_v1_profile(const noah_profile_validator_v1_t *validator, noah_profile_validator_v1_profile_t *profile, noah_profile_validator_v1_error_t *error);
