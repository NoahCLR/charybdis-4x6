// ───────────────────────────────────────────────────────────────────────────
// Scan-Owned Live Profile Candidate Transaction
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../protocol/profile_candidate_v1.h"

enum {
    // The whole-profile validator guarantees one reader operation and at most
    // this many newly observed bytes per scan call. Checksum reads also honor
    // a smaller nonzero budget supplied by the owner.
    NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET = NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX,
};

typedef enum {
    NOAH_PROFILE_CANDIDATE_BACKEND_OK = 0u,
    NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS,
    NOAH_PROFILE_CANDIDATE_BACKEND_VALID,
    NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED,
    NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR,
    NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN,
    NOAH_PROFILE_CANDIDATE_BACKEND_BUSY,
} noah_profile_candidate_backend_result_t;

typedef struct {
    void *context;
    noah_profile_candidate_backend_result_t (*begin)(void *context, const noah_profile_candidate_v1_metadata_t *metadata);
    noah_profile_candidate_backend_result_t (*write)(void *context, uint16_t offset, const uint8_t *bytes, uint8_t length);
    noah_profile_candidate_backend_result_t (*read)(void *context, uint16_t offset, uint8_t *bytes, uint8_t length);
    noah_profile_candidate_backend_result_t (*validation_begin)(void *context, const noah_profile_candidate_v1_metadata_t *metadata, noah_profile_candidate_v1_error_t *error);
    // Each step must return IN_PROGRESS, VALID, REJECTED, or IO_ERROR and obey
    // the per-step reader-work contract above. OK is deliberately not a
    // terminal validation result and is treated as an invalid backend
    // response/storage failure by the owner.
    noah_profile_candidate_backend_result_t (*validation_step)(void *context, uint8_t byte_budget, noah_profile_candidate_v1_error_t *error);
    // Durable commit and activation advance only from scan context. Each
    // commit_step obeys the same one-operation / <=20-byte work ceiling.
    noah_profile_candidate_backend_result_t (*commit_begin)(void *context);
    noah_profile_candidate_backend_result_t (*commit_step)(void *context, uint8_t byte_budget);
    noah_profile_candidate_backend_result_t (*activation_begin)(void *context);
    noah_profile_candidate_backend_result_t (*activation_step)(void *context);
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
    bool                                   split_commit_authorization_required;
    uint16_t                               last_aborted_transaction_id;
    uint16_t                               last_committed_transaction_id;
} noah_profile_candidate_transaction_t;

typedef enum {
    NOAH_PROFILE_CANDIDATE_EXPIRE_NOTHING = 0u,
    NOAH_PROFILE_CANDIDATE_EXPIRE_DONE,
    NOAH_PROFILE_CANDIDATE_EXPIRE_MAILBOX_BUSY,
    NOAH_PROFILE_CANDIDATE_EXPIRE_DURABLE_PHASE,
    NOAH_PROFILE_CANDIDATE_EXPIRE_BACKEND_ERROR,
} noah_profile_candidate_expire_result_t;

void noah_profile_candidate_transaction_init(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_backend_t *backend, const noah_profile_candidate_compatibility_t *compatibility);

// Selects the split-authority commit barrier while the transaction is idle.
// Coordinated commits stop before durability and again before activation until
// the owner explicitly authorizes each boundary. The setting persists across
// completed candidates. Returns false if a candidate or mailbox is active.
bool noah_profile_candidate_transaction_require_split_authorization(noah_profile_candidate_transaction_t *transaction, bool required);

// Owner-side split-authority boundary. PREPARING_PEER begins marker-last
// durability and becomes COMMITTING. A synchronously completed commit becomes
// CONVERGING_PEER. Calls after that boundary are idempotent and do not restart
// persistence. Returns false outside coordinated commit states.
bool noah_profile_candidate_transaction_authorize_commit(noah_profile_candidate_transaction_t *transaction);

// Owner-side split-authority boundary. Only CONVERGING_PEER begins provider
// activation and becomes ACTIVATING. Repeated calls while ACTIVATING are
// idempotent and never restart activation.
bool noah_profile_candidate_transaction_authorize_activation(noah_profile_candidate_transaction_t *transaction);

// USB/callback side: exact decode plus one bounded mailbox copy only. A handled
// frame is replaced in place with its immediate admission acknowledgement.
bool noah_profile_candidate_transaction_receive(noah_profile_candidate_transaction_t *transaction, uint8_t *frame, size_t length);

// Scan side: processes at most one mailbox item, validation step, commit step,
// or activation step. Validation and commit obey the bounded-work contracts
// above; provider activation remains a separate safe-boundary poll.
bool noah_profile_candidate_transaction_scan(noah_profile_candidate_transaction_t *transaction);

// Owner-requested inactivity cleanup. Only precommit states with an admitted
// candidate may be aborted. A queued mailbox, marker-last commit, activation,
// or durability-unknown terminal state can never be expired through this API.
noah_profile_candidate_expire_result_t noah_profile_candidate_transaction_expire_precommit(noah_profile_candidate_transaction_t *transaction);

// Split-authority cleanup for a host candidate that lost to an already newer
// peer before marker-last commit began. Unlike inactivity expiry, this scan-
// owned operation may discard one acknowledged but unprocessed mailbox item so
// a queued commit cannot cross the newly observed durable-authority boundary.
noah_profile_candidate_expire_result_t noah_profile_candidate_transaction_supersede_precommit(noah_profile_candidate_transaction_t *transaction);
// Deterministic simultaneous-host arbitration. The losing physical origin
// aborts only its still-provisional candidate and reports a distinct status.
noah_profile_candidate_expire_result_t noah_profile_candidate_transaction_yield_precommit(noah_profile_candidate_transaction_t *transaction);

// Fail closed after local marker-last durability. This retains the candidate
// and backend lease/backing so a two-slot runtime cannot overwrite the active
// slot while authority is unresolved. Only the two postcommit split errors
// are accepted.
bool noah_profile_candidate_transaction_fail_postcommit(noah_profile_candidate_transaction_t *transaction, noah_profile_candidate_v1_error_id_t reason);

// Releases a candidate whose local commit failed before marker durability was
// possible while retaining its terminal STORAGE_FAILURE status for the host.
// The split owner calls this only after confirming that any provisional peer
// copy has been aborted. Durable/ambiguous states are refused.
noah_profile_candidate_expire_result_t noah_profile_candidate_transaction_cleanup_failed_precommit(noah_profile_candidate_transaction_t *transaction);

void noah_profile_candidate_transaction_status(const noah_profile_candidate_transaction_t *transaction, noah_profile_candidate_v1_status_t *status);
