// ───────────────────────────────────────────────────────────────────────────
// Scan-Owned Live Profile Candidate Transaction
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../protocol/profile_candidate_v1.h"

enum {
    // Checksum reads honor this per-call byte budget. Domain decoders currently
    // bound individual reads, not total work; production scan routing remains
    // disabled until the work-budget gate is replaced by an accepted
    // incrementally bounded implementation.
    NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET = NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX,
};

typedef enum {
    NOAH_PROFILE_CANDIDATE_BACKEND_OK = 0u,
    NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS,
    NOAH_PROFILE_CANDIDATE_BACKEND_VALID,
    NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED,
    NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR,
} noah_profile_candidate_backend_result_t;

typedef struct {
    void *context;
    noah_profile_candidate_backend_result_t (*begin)(void *context, const noah_profile_candidate_v1_metadata_t *metadata);
    noah_profile_candidate_backend_result_t (*write)(void *context, uint16_t offset, const uint8_t *bytes, uint8_t length);
    noah_profile_candidate_backend_result_t (*read)(void *context, uint16_t offset, uint8_t *bytes, uint8_t length);
    noah_profile_candidate_backend_result_t (*validation_begin)(void *context, const noah_profile_candidate_v1_metadata_t *metadata, noah_profile_candidate_v1_error_t *error);
    // Each step must return IN_PROGRESS, VALID, REJECTED, or IO_ERROR. The
    // byte_budget currently constrains checksum reads only; it is not yet a
    // total domain-work guarantee. OK is deliberately not a terminal
    // validation result and is treated as an invalid backend response/storage
    // failure by the owner.
    noah_profile_candidate_backend_result_t (*validation_step)(void *context, uint8_t byte_budget, noah_profile_candidate_v1_error_t *error);
    noah_profile_candidate_backend_result_t (*abort)(void *context);
} noah_profile_candidate_backend_t;

typedef struct {
    uint8_t  schema_major;
    uint8_t  schema_minor;
    uint8_t  supported_domain_mask;
    uint16_t max_payload_length;
    uint32_t action_abi_digest;
} noah_profile_candidate_compatibility_t;

typedef struct {
    bool                                pending;
    noah_profile_candidate_v1_command_t command;
} noah_profile_candidate_mailbox_t;

typedef struct {
    noah_profile_candidate_backend_t       backend;
    noah_profile_candidate_compatibility_t compatibility;
    noah_profile_candidate_mailbox_t       mailbox;
    noah_profile_candidate_v1_metadata_t   metadata;
    noah_profile_candidate_v1_status_t     status;
    bool                                   has_candidate;
    bool                                   poisoned;
    uint16_t                               last_aborted_transaction_id;
} noah_profile_candidate_transaction_t;

void noah_profile_candidate_transaction_init(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_backend_t *backend, const noah_profile_candidate_compatibility_t *compatibility);

// USB/callback side: exact decode plus one bounded mailbox copy only. A handled
// frame is replaced in place with its immediate admission acknowledgement.
bool noah_profile_candidate_transaction_receive(noah_profile_candidate_transaction_t *transaction, uint8_t *frame, size_t length);

// Scan side: processes at most one mailbox item or one validator phase. The
// checksum phase is byte-bounded; total domain work is not yet scan-safe.
bool noah_profile_candidate_transaction_scan(noah_profile_candidate_transaction_t *transaction);

void noah_profile_candidate_transaction_status(const noah_profile_candidate_transaction_t *transaction, noah_profile_candidate_v1_status_t *status);
