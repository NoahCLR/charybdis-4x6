// ──────────────────────────────────────────────────────────────────────────
// Exact Peer Profile -> Shared Candidate/Store Backend
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../split/profile_split_protocol_v1.h"
#include "profile_candidate_store_backend.h"

enum {
    // Firmware-state regression policy, not an RP2040 SRAM-capacity claim.
    NOAH_PROFILE_PEER_STORE_STATE_BUDGET_32BIT = 256u,
};

typedef enum {
    NOAH_PROFILE_PEER_STORE_UNINITIALIZED = 0u,
    NOAH_PROFILE_PEER_STORE_IDLE,
    NOAH_PROFILE_PEER_STORE_RECEIVING,
    NOAH_PROFILE_PEER_STORE_VALIDATING,
    NOAH_PROFILE_PEER_STORE_PREPARING,
    NOAH_PROFILE_PEER_STORE_PREPARED,
    NOAH_PROFILE_PEER_STORE_COMMITTING,
    NOAH_PROFILE_PEER_STORE_COMMITTED,
    NOAH_PROFILE_PEER_STORE_REJECTED,
    NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED,
} noah_profile_peer_store_state_t;

typedef enum {
    NOAH_PROFILE_PEER_STORE_OK = 0u,
    NOAH_PROFILE_PEER_STORE_IN_PROGRESS,
    NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED,
    NOAH_PROFILE_PEER_STORE_BUSY,
    NOAH_PROFILE_PEER_STORE_STALE,
    NOAH_PROFILE_PEER_STORE_CONFLICT,
    NOAH_PROFILE_PEER_STORE_CORRUPT,
    NOAH_PROFILE_PEER_STORE_INCOMPATIBLE,
    NOAH_PROFILE_PEER_STORE_INVALID_METADATA,
    NOAH_PROFILE_PEER_STORE_RANGE_ERROR,
    NOAH_PROFILE_PEER_STORE_CHUNK_CONFLICT,
    NOAH_PROFILE_PEER_STORE_STORAGE_ERROR,
    NOAH_PROFILE_PEER_STORE_VALIDATION_ERROR,
    NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN,
} noah_profile_peer_store_result_t;

typedef struct {
    noah_profile_candidate_store_backend_t *backend;
    noah_profile_candidate_backend_t        interface;
    noah_profile_split_descriptor_t         descriptor;
    noah_profile_candidate_v1_metadata_t    metadata;
    noah_profile_candidate_v1_error_t       validation_error;
    noah_profile_peer_store_state_t         state;
    noah_profile_peer_store_result_t        result;
    uint16_t                                next_offset;
    bool                                    auto_commit;
} noah_profile_peer_store_backend_t;

// The peer receiver borrows the one candidate backend already shared with the
// host transaction. It never installs another store reuse guard or provider.
void noah_profile_peer_store_backend_init(noah_profile_peer_store_backend_t *peer, noah_profile_candidate_store_backend_t *backend);

// Opens exact sender-owned staging. Generation, physical origin, persistent
// flags, and all payload identities are preserved unchanged.
noah_profile_peer_store_result_t noah_profile_peer_store_backend_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);
noah_profile_peer_store_result_t noah_profile_peer_store_backend_begin_logical(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor, uint32_t via_generation, uint32_t via_digest);

// Accepts only sequential chunks. A fully repeated chunk is idempotent when
// its staged bytes match; gaps, partial overlaps, and conflicting repeats
// poison and abort the prepare.
noah_profile_peer_store_result_t noah_profile_peer_store_backend_write(noah_profile_peer_store_backend_t *peer, uint32_t generation, uint32_t payload_digest, uint16_t offset, const uint8_t *bytes, uint8_t length);

// Starts whole-profile validation after every declared byte arrived. step()
// performs at most one bounded validator/store operation per call.
noah_profile_peer_store_result_t noah_profile_peer_store_backend_commit_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);
noah_profile_peer_store_result_t noah_profile_peer_store_backend_prepare_durable_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);
noah_profile_peer_store_result_t noah_profile_peer_store_backend_prepared_commit_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);
noah_profile_peer_store_result_t noah_profile_peer_store_backend_step(noah_profile_peer_store_backend_t *peer, uint8_t byte_budget);

// Abort is allowed only before marker-last commit begins. Once durability can
// be ambiguous the caller must finish or reboot-scan the store.
noah_profile_peer_store_result_t noah_profile_peer_store_backend_abort(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);

noah_profile_peer_store_state_t          noah_profile_peer_store_backend_state(const noah_profile_peer_store_backend_t *peer);
noah_profile_peer_store_result_t         noah_profile_peer_store_backend_result(const noah_profile_peer_store_backend_t *peer);
uint16_t                                 noah_profile_peer_store_backend_next_offset(const noah_profile_peer_store_backend_t *peer);
const noah_profile_candidate_v1_error_t *noah_profile_peer_store_backend_validation_error(const noah_profile_peer_store_backend_t *peer);
