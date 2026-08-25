// ───────────────────────────────────────────────────────────────────────────
// Candidate Transaction -> Dual-Slot Store Backend
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../runtime/effective_profile_provider.h"
#include "../schema/profile_validator_v1.h"
#include "profile_candidate_transaction.h"
#include "profile_store.h"

typedef struct {
    noah_profile_store_t                      *store;
    noah_effective_profile_provider_t         *provider;
    noah_profile_validator_v1_compatibility_t compatibility;
    noah_profile_validator_v1_t               validator;
    noah_profile_validator_v1_profile_t        validated_profile;
    noah_profile_candidate_v1_metadata_t       metadata;
    noah_profile_reader_t                      staged_reader;
    noah_effective_profile_snapshot_t          activation_snapshot;
    noah_profile_store_record_t                committed_record;
    uint32_t                                   compiled_default_digest;
    uint8_t                                    origin_half;
    bool                                       validation_complete;
    bool                                       committed_available;
    bool                                       reuse_guard_installed;
} noah_profile_candidate_store_backend_t;

// backend and store must remain at stable addresses for their shared lifetime:
// the store's destructive-write guard borrows backend as its callback context.
void noah_profile_candidate_store_backend_init(noah_profile_candidate_store_backend_t *backend, noah_profile_store_t *store, noah_effective_profile_provider_t *provider, const noah_profile_validator_v1_compatibility_t *compatibility, uint32_t compiled_default_digest, uint8_t origin_half);

// Returns callbacks for the scan-owned candidate transaction. The returned
// interface borrows backend for its entire lifetime.
noah_profile_candidate_backend_t noah_profile_candidate_store_backend_interface(noah_profile_candidate_store_backend_t *backend);

// Commit remains a separate scan-owner operation. Merely wiring the candidate
// backend cannot make a validated payload durable or active.
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_commit(noah_profile_candidate_store_backend_t *backend, noah_profile_store_record_t *committed);

// After durable commit, request publication through the effective provider.
// This never publishes directly; the scan owner must poll the provider and the
// provider's safe-boundary predicate remains authoritative.
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_request_activation(noah_profile_candidate_store_backend_t *backend);

const noah_profile_validator_v1_profile_t *noah_profile_candidate_store_backend_validated_profile(const noah_profile_candidate_store_backend_t *backend);
