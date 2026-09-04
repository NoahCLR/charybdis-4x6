// ──────────────────────────────────────────────────────────────────────────
// Single Live-Profile Runtime Owner
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effective_key_behavior_runtime.h"
#include "effective_profile_provider.h"
#include "effective_rgb_runtime.h"
#include "profile_activation_policy.h"
#include "../schema/profile_compiled_defaults_v1.h"
#include "../split/profile_split_reconciler.h"
#include "../storage/profile_candidate_store_backend.h"
#include "../storage/profile_candidate_transaction.h"
#include "../storage/profile_peer_store_backend.h"
#include "../storage/profile_store.h"

enum {
    // Engineering-state regression policy for one 32-bit keyboard half. This
    // is not an RP2040 physical-SRAM limit; linked target gates remain the
    // authority for the concrete firmware artifact.
    NOAH_PROFILE_OWNER_STATE_BUDGET_32BIT = 4096u,
    // Ordinary host staging expires after 15 seconds without host-owned work.
    // Split preparation has a separate no-progress window: a maximum 4,064-byte
    // profile needs at least 292 request/retry intervals at the 50 ms split
    // floor, before scan overhead or a transient retry is included.
    NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS                = 15000u,
    NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS   = 60000u,
};

typedef enum {
    NOAH_PROFILE_OWNER_UNINITIALIZED = 0u,
    NOAH_PROFILE_OWNER_VALIDATING_COMPILED,
    NOAH_PROFILE_OWNER_DISCOVERING,
    NOAH_PROFILE_OWNER_ADOPTING_COMMITTED,
    NOAH_PROFILE_OWNER_RECONCILING_COMMITTED,
    NOAH_PROFILE_OWNER_ACTIVATING_COMMITTED,
    NOAH_PROFILE_OWNER_READY_COMPILED,
    NOAH_PROFILE_OWNER_READY_VALIDATED,
    NOAH_PROFILE_OWNER_GENERATION_CONFLICT,
    NOAH_PROFILE_OWNER_DURABILITY_UNKNOWN,
    NOAH_PROFILE_OWNER_COMPILED_ERROR,
    NOAH_PROFILE_OWNER_STORAGE_ERROR,
    NOAH_PROFILE_OWNER_INTEGRATION_ERROR,
    NOAH_PROFILE_OWNER_POSTCOMMIT_AUTHORITY_LOST,
    NOAH_PROFILE_OWNER_CONCURRENT_COMMIT,
} noah_profile_owner_state_t;

typedef struct {
    noah_profile_store_io_t             store_io;
    noah_profile_split_exchange_fn      split_exchange;
    void                               *split_transport_context;
    uint8_t                             origin_half;
    bool                                peer_required;
} noah_profile_owner_config_t;

// The compiled validator is needed only before the candidate backend exists,
// so both lifetimes deliberately share one stable union. The backend address
// never changes after it is constructed.
typedef union {
    noah_profile_validator_v1_t              compiled_validator;
    noah_profile_candidate_store_backend_t   candidate_backend;
} noah_profile_owner_staging_t;

typedef struct {
    noah_profile_owner_config_t                 config;
    noah_profile_compiled_v1_t                  compiled;
    noah_profile_reader_t                       compiled_reader;
    noah_profile_validator_v1_compatibility_t  compatibility;
    noah_profile_validator_v1_declaration_t    compiled_declaration;
    noah_profile_validator_v1_profile_t        compiled_profile;
    noah_effective_profile_snapshot_t          compiled_snapshot;
    noah_profile_owner_staging_t                staging;
    noah_profile_store_t                        store;
    noah_effective_profile_provider_t           provider;
    noah_profile_activation_policy_t            activation_policy;
    noah_effective_key_behavior_runtime_t       key_behaviors;
    noah_effective_rgb_runtime_t                rgb;
    noah_profile_candidate_transaction_t        host_transaction;
    noah_profile_peer_store_backend_t           peer_store;
    noah_profile_split_reconciler_t             reconciler;
    noah_profile_split_descriptor_t             committed_descriptor;
    noah_profile_split_descriptor_t             host_barrier_descriptor;
    noah_profile_candidate_v1_error_t            adoption_error;
    noah_profile_candidate_v1_error_id_t         host_cancel_reason;
    noah_profile_store_result_t                  discovery_result;
    noah_profile_owner_state_t                   state;
    uint32_t                                     host_last_activity_at;
    uint16_t                                     host_barrier_progress_offset;
    uint8_t                                      scheduler_cursor;
    bool                                         boot_activation_started;
    bool                                         peer_activation_started;
    bool                                         host_activity_known;
    bool                                         descriptor_readable;
    bool                                         split_initialized;
    bool                                         runtimes_installed;
    bool                                         host_barrier_descriptor_known;
    bool                                         host_barrier_started;
    bool                                         host_barrier_local_published;
    bool                                         host_barrier_peer_commit_authorized;
    bool                                         host_cancel_pending;
} noah_profile_owner_t;

// Protocol-neutral, caller-owned observation of the complete live-profile
// graph. The VIA adapter maps this snapshot into its frozen two-page status;
// no mutable owner pointer crosses the runtime boundary.
typedef struct {
    noah_profile_owner_state_t             owner_state;
    noah_effective_profile_identity_t      active;
    noah_effective_profile_identity_t      pending;
    noah_profile_split_descriptor_t        committed;
    noah_profile_split_descriptor_t        peer;
    noah_profile_candidate_v1_status_t     candidate;
    noah_profile_split_authority_state_t   authority_state;
    uint32_t                               compiled_default_digest;
    uint32_t                               action_abi_digest;
    uint32_t                               safe_boundary_reason_mask;
    uint16_t                               last_committed_transaction_id;
    uint8_t                                supported_domain_mask;
    bool                                   provider_known;
    bool                                   has_pending;
    bool                                   has_committed;
    bool                                   candidate_pending;
    bool                                   peer_known;
    bool                                   peer_converged;
    bool                                   transfer_pending;
} noah_profile_owner_status_t;

// Initializes metadata and begins incremental validation of the compiled
// canonical profile. It performs no EEPROM or split-transport I/O.
bool noah_profile_owner_init(noah_profile_owner_t *owner, const noah_profile_owner_config_t *config);

// One scan grant performs at most one validator/store/provider operation or
// one split exchange. `master` is current transport role; `now_ms` is the
// caller's wrapping millisecond clock.
bool noah_profile_owner_scan(noah_profile_owner_t *owner, bool master, uint32_t now_ms);

// Callback-side candidate admission remains a bounded decode/mailbox copy.
// Advertising or routing this surface is a separate capability decision.
bool noah_profile_owner_receive(noah_profile_owner_t *owner, uint8_t *frame, size_t length);

noah_profile_owner_state_t             noah_profile_owner_state(const noah_profile_owner_t *owner);
noah_profile_store_result_t            noah_profile_owner_discovery_result(const noah_profile_owner_t *owner);
const noah_profile_store_record_t      *noah_profile_owner_committed(const noah_profile_owner_t *owner);
noah_profile_candidate_transaction_t  *noah_profile_owner_host_transaction(noah_profile_owner_t *owner);
noah_profile_split_reconciler_t        *noah_profile_owner_split_reconciler(noah_profile_owner_t *owner);
bool                                    noah_profile_owner_status(const noah_profile_owner_t *owner, noah_profile_owner_status_t *status);

#ifdef NOAH_PROFILE_OWNER_TEST_DIAGNOSTICS
uint32_t noah_profile_owner_test_ready_refresh_count(void);
#endif
