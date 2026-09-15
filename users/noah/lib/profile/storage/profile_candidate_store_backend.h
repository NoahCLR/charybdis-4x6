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

typedef enum {
    NOAH_PROFILE_STORAGE_ADMISSION_NONE = 0u,
    NOAH_PROFILE_STORAGE_ADMISSION_HOST,
    NOAH_PROFILE_STORAGE_ADMISSION_PEER,
} noah_profile_storage_admission_owner_t;

typedef struct {
    noah_profile_store_t                     *store;
    noah_effective_profile_provider_t        *provider;
    noah_profile_validator_v1_compatibility_t compatibility;
    noah_profile_validator_v1_t               validator;
    noah_profile_validator_v1_profile_t       validated_profile;
    noah_profile_candidate_v1_metadata_t      metadata;
    noah_profile_reader_t                     staged_reader;
    noah_effective_profile_snapshot_t         activation_snapshot;
    noah_profile_store_record_t               committed_record;
    uint32_t                                  compiled_default_digest;
    uint8_t                                   origin_half;
    bool                                      validation_complete;
    bool                                      committed_available;
    bool                                      activation_requested;
    bool                                      reuse_guard_installed;
    bool                                      validating_committed_record;
    noah_profile_storage_admission_owner_t    admission_owner;
} noah_profile_candidate_store_backend_t;

// backend and store must remain at stable addresses for their shared lifetime:
// the store's destructive-write guard borrows backend as its callback context.
void noah_profile_candidate_store_backend_init(noah_profile_candidate_store_backend_t *backend, noah_profile_store_t *store, noah_effective_profile_provider_t *provider, const noah_profile_validator_v1_compatibility_t *compatibility, uint32_t compiled_default_digest, uint8_t origin_half);

// Returns callbacks for the scan-owned candidate transaction. The returned
// interface borrows backend for its entire lifetime.
noah_profile_candidate_backend_t noah_profile_candidate_store_backend_interface(noah_profile_candidate_store_backend_t *backend);

// Split reconciliation uses the same storage/validator/provider owner as host
// candidates but supplies the sender's complete durable identity. This path
// never manufactures a local generation or origin. metadata supplies the
// derived canonical domain mask used by whole-profile validation.
noah_profile_store_result_t noah_profile_candidate_store_backend_begin_exact(noah_profile_candidate_store_backend_t *backend, const noah_profile_candidate_v1_metadata_t *metadata, const noah_profile_store_candidate_t *candidate);

// Reboot path for a record already selected by profile_store_boot_select().
// It performs no write and does not manufacture a new durable identity. Begin
// and every step are scan-owned and obey the same bounded whole-profile
// validator contract as an uploaded or peer-supplied candidate.
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_adopt_committed_begin(noah_profile_candidate_store_backend_t *backend, const noah_profile_store_record_t *record, noah_profile_candidate_v1_error_t *error);
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_adopt_committed_step(noah_profile_candidate_store_backend_t *backend, uint8_t byte_budget, noah_profile_candidate_v1_error_t *error);

// Host and peer staging share one backend. Admission remains held across
// validation, durability, convergence, and activation so neither path can
// overwrite the other's retained validated view. Abort or a successful
// activation releases it; durability-unknown deliberately retains it.
noah_profile_storage_admission_owner_t noah_profile_candidate_store_backend_admission_owner(const noah_profile_candidate_store_backend_t *backend);
bool                                   noah_profile_candidate_store_backend_release_admission(noah_profile_candidate_store_backend_t *backend, noah_profile_storage_admission_owner_t owner);

// While a validated HOST candidate is still precommit, the split owner may
// stream its exact bytes to the sibling's inactive slot. These observations
// never imply durability: the returned identity is the provisional store
// candidate, and both calls fail once local marker-last commit begins.
bool noah_profile_candidate_store_backend_staged_candidate(const noah_profile_candidate_store_backend_t *backend, noah_profile_store_candidate_t *candidate);
bool noah_profile_candidate_store_backend_staged_read(void *context, const noah_profile_store_candidate_t *candidate, uint16_t offset, uint8_t *bytes, uint8_t length);

// Split logical transactions persist intent and the decision in separate
// phases. These calls retain the validated profile and admission lease across
// the prepared boundary.
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_prepare_durable_begin(noah_profile_candidate_store_backend_t *backend);
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_prepare_durable_step(noah_profile_candidate_store_backend_t *backend, uint8_t byte_budget);
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_prepared_commit_begin(noah_profile_candidate_store_backend_t *backend);
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_prepared_commit_step(noah_profile_candidate_store_backend_t *backend, uint8_t byte_budget);

// Cold/test convenience wrapper around the interface's bounded commit steps.
// Staging and validation never call it; production durability still requires
// an explicit commit operation owned by scan context.
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_commit(noah_profile_candidate_store_backend_t *backend, noah_profile_store_record_t *committed);

// After durable commit, request publication through the effective provider.
// This never publishes directly; the scan owner must poll the provider and the
// provider's safe-boundary predicate remains authoritative.
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_request_activation(noah_profile_candidate_store_backend_t *backend);

// Owner-facing activation lifecycle for boot adoption and exact peer commits.
// expected_owner prevents a top-level scheduler from polling a lease that
// changed underneath it. NONE is valid only for boot adoption, which does not
// acquire a host/peer staging lease.
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_activation_begin(noah_profile_candidate_store_backend_t *backend, noah_profile_storage_admission_owner_t expected_owner);
noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_activation_step(noah_profile_candidate_store_backend_t *backend, noah_profile_storage_admission_owner_t expected_owner);

// Returns only the exact semantically validated durable record retained by
// this backend. Callers must copy it before a later staging admission reuses
// the backend's validated view.
bool noah_profile_candidate_store_backend_committed(const noah_profile_candidate_store_backend_t *backend, noah_profile_store_record_t *record);

const noah_profile_validator_v1_profile_t *noah_profile_candidate_store_backend_validated_profile(const noah_profile_candidate_store_backend_t *backend);
