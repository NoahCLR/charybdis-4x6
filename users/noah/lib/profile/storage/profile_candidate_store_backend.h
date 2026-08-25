// ───────────────────────────────────────────────────────────────────────────
// Candidate Transaction -> Dual-Slot Store Backend
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../schema/profile_validator_v1.h"
#include "profile_candidate_transaction.h"
#include "profile_store.h"

typedef struct {
    noah_profile_store_t                      *store;
    noah_profile_validator_v1_compatibility_t compatibility;
    noah_profile_validator_v1_t               validator;
    noah_profile_validator_v1_profile_t        validated_profile;
    noah_profile_candidate_v1_metadata_t       metadata;
    noah_profile_reader_t                      staged_reader;
    uint32_t                                   compiled_default_digest;
    uint8_t                                    origin_half;
    bool                                       validation_complete;
} noah_profile_candidate_store_backend_t;

void noah_profile_candidate_store_backend_init(noah_profile_candidate_store_backend_t *backend, noah_profile_store_t *store, const noah_profile_validator_v1_compatibility_t *compatibility, uint32_t compiled_default_digest, uint8_t origin_half);

// Returns callbacks for the scan-owned candidate transaction. The returned
// interface borrows backend for its entire lifetime.
noah_profile_candidate_backend_t noah_profile_candidate_store_backend_interface(noah_profile_candidate_store_backend_t *backend);

// Commit remains a separate scan-owner operation. Merely wiring the candidate
// backend cannot make a validated payload durable or active.
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_commit(noah_profile_candidate_store_backend_t *backend, noah_profile_store_record_t *committed);

const noah_profile_validator_v1_profile_t *noah_profile_candidate_store_backend_validated_profile(const noah_profile_candidate_store_backend_t *backend);
