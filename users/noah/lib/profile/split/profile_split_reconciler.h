// ──────────────────────────────────────────────────────────────────────────
// Scan-Owned Live-Profile Split Reconciler
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile_split_authority.h"
#include "profile_split_protocol_v1.h"
#include "../storage/profile_peer_store_backend.h"

enum {
    NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS = 50u,
    NOAH_PROFILE_SPLIT_RETRY_MAX_MS     = 1000u,
    // Mutating RPC callbacks acknowledge mailbox admission with BUSY; the
    // receiver's next matrix scan publishes the actual result. Collect that
    // expected acknowledgement promptly without weakening the exponential
    // backoff used when the peer remains busy or transport fails.
    NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS = 5u,
    NOAH_PROFILE_SPLIT_POLL_MS            = 1000u,
    // A passive half cannot actively probe the link. Missing three normal
    // master polls invalidates its peer observation and therefore activation.
    NOAH_PROFILE_SPLIT_PEER_TIMEOUT_MS = 3000u,
    // A receiver holds staged peer storage only while the sender remains
    // active. This is a pre-marker lease; expiry never interrupts COMMITTING
    // or a durability-unknown recovery state.
    NOAH_PROFILE_SPLIT_PREPARE_LEASE_MS = 3000u,
    // Firmware-state regression policy, not an RP2040 SRAM-capacity claim.
    NOAH_PROFILE_SPLIT_RECONCILER_STATE_BUDGET_32BIT = 768u,
};

typedef enum {
    NOAH_PROFILE_SPLIT_RECONCILER_UNINITIALIZED = 0u,
    NOAH_PROFILE_SPLIT_RECONCILER_PASSIVE,
    NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER,
    NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BIND,
    NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN,
    NOAH_PROFILE_SPLIT_RECONCILER_PUSH_READ,
    NOAH_PROFILE_SPLIT_RECONCILER_PUSH_SEND,
    NOAH_PROFILE_SPLIT_RECONCILER_PUSH_DURABLE,
    NOAH_PROFILE_SPLIT_RECONCILER_PUSH_PREPARED,
    NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT,
    NOAH_PROFILE_SPLIT_RECONCILER_PUSH_ABORT,
    NOAH_PROFILE_SPLIT_RECONCILER_PULL_BEGIN,
    NOAH_PROFILE_SPLIT_RECONCILER_PULL_BIND,
    NOAH_PROFILE_SPLIT_RECONCILER_PULL_REQUEST,
    NOAH_PROFILE_SPLIT_RECONCILER_PULL_WRITE,
    NOAH_PROFILE_SPLIT_RECONCILER_PULL_COMMIT_BEGIN,
    NOAH_PROFILE_SPLIT_RECONCILER_PULL_COMMIT_STEP,
    NOAH_PROFILE_SPLIT_RECONCILER_VERIFY,
    NOAH_PROFILE_SPLIT_RECONCILER_CONVERGED,
    NOAH_PROFILE_SPLIT_RECONCILER_STOPPED,
} noah_profile_split_reconciler_state_t;

typedef bool (*noah_profile_split_local_descriptor_fn)(void *context, noah_profile_split_descriptor_t *descriptor);
typedef bool (*noah_profile_split_local_read_fn)(void *context, const noah_profile_split_descriptor_t *descriptor, uint16_t offset, uint8_t *bytes, uint8_t length);
typedef bool (*noah_profile_split_local_binding_fn)(void *context, const noah_profile_split_descriptor_t *descriptor, uint32_t *via_generation, uint32_t *via_digest);
typedef bool (*noah_profile_split_exchange_fn)(void *context, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]);

typedef struct {
    void                                  *local_context;
    noah_profile_split_local_descriptor_fn local_descriptor;
    noah_profile_split_local_read_fn       local_read;
    noah_profile_split_local_binding_fn    local_binding;
    void                                  *transport_context;
    noah_profile_split_exchange_fn         exchange;
    noah_profile_peer_store_backend_t     *peer_store;
} noah_profile_split_reconciler_config_t;

typedef struct {
    noah_profile_split_reconciler_state_t state;
    noah_profile_split_v1_status_t        last_status;
    uint16_t                              transfer_offset;
    uint16_t                              transfer_length;
    uint32_t                              transport_failure_count;
    uint32_t                              retry_count;
    bool                                  role_known;
    bool                                  master;
    bool                                  mailbox_pending;
    bool                                  transfer_pending;
} noah_profile_split_reconciler_status_t;

typedef enum {
    NOAH_PROFILE_SPLIT_TRANSFER_NONE = 0u,
    NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH,
    NOAH_PROFILE_SPLIT_TRANSFER_LOCAL_PULL,
} noah_profile_split_transfer_owner_t;

typedef enum {
    // Normal arbitration may import a newer peer profile.
    NOAH_PROFILE_SPLIT_RECONCILE_FULL = 0u,
    // Used while HOST owns the shared candidate backend. Metadata exchange,
    // serving local payload, and pushing the host's durable record continue;
    // starting or advancing a local peer import is refused with BUSY.
    NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY,
} noah_profile_split_reconcile_mode_t;

// Caller-owned and payload-independent. It contains only protocol frames,
// descriptors, retry state, and references to the shared store/provider owner.
typedef struct {
    noah_profile_split_reconciler_config_t config;
    noah_profile_split_authority_t         authority;
    noah_profile_split_descriptor_t        local_descriptor;
    noah_profile_split_descriptor_t        peer_descriptor;
    noah_profile_split_descriptor_t        provisional_peer_descriptor;
    noah_profile_split_descriptor_t        transfer_descriptor;
    noah_profile_split_descriptor_t        stopped_local_descriptor;
    noah_profile_split_reconciler_state_t  state;
    noah_profile_split_transfer_owner_t    transfer_owner;
    noah_profile_split_v1_status_t         last_status;
    void                                  *prepared_source_context;
    noah_profile_split_local_read_fn       prepared_source_read;
    uint32_t                               next_attempt_at;
    uint32_t                               retry_ms;
    uint32_t                               last_peer_activity_at;
    uint32_t                               transport_failure_count;
    uint32_t                               retry_count;
    uint32_t                               prepared_via_generation;
    uint32_t                               prepared_via_digest;
    uint32_t                               incoming_via_generation;
    uint32_t                               incoming_via_digest;
    uint32_t                               incoming_profile_generation;
    uint32_t                               incoming_profile_digest;
    uint16_t                               transfer_offset;
    uint8_t                                outbound_chunk_length;
    uint8_t                                outbound_chunk[NOAH_PROFILE_SPLIT_V1_CHUNK_MAX];
    uint8_t                                mailbox_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t                                cached_request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t                                cached_response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t                                metadata_response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    noah_runtime_publication_generation_t  mailbox_sequence;
    noah_runtime_publication_generation_t  response_sequence;
    noah_runtime_publication_generation_t  metadata_sequence;
    volatile uint8_t                       peer_activity_sequence;
    uint8_t                                mailbox_consumed_sequence;
    uint8_t                                observed_peer_activity_sequence;
    bool                                   initialized;
    bool                                   role_known;
    bool                                   master;
    bool                                   cached_response_valid;
    bool                                   metadata_response_valid;
    bool                                   peer_activity_known;
    bool                                   provisional_peer_descriptor_known;
    bool                                   prepared_push_active;
    bool                                   prepared_remote_started;
    bool                                   prepared_commit_authorized;
    bool                                   prepared_cancel_refused;
    bool                                   prepared_logical;
    bool                                   incoming_logical_binding;
    bool                                   attempt_immediate;
} noah_profile_split_reconciler_t;

void noah_profile_split_reconciler_init(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_reconciler_config_t *config);

// QMK RPC/callback side. It performs strict frame decode, at most one bounded
// mailbox copy, and a cached response copy. EEPROM, validation, and provider
// operations are deliberately absent from this path.
bool noah_profile_split_reconciler_receive(noah_profile_split_reconciler_t *reconciler, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length);

// Matrix-scan side. Each call performs at most one RPC, one payload read/write,
// or one validator/commit step. `master` is current transport role only; durable
// authority continues to use the descriptor's stable physical origin.
bool noah_profile_split_reconciler_scan(noah_profile_split_reconciler_t *reconciler, bool master, uint32_t now_ms);
bool noah_profile_split_reconciler_scan_mode(noah_profile_split_reconciler_t *reconciler, bool master, uint32_t now_ms, noah_profile_split_reconcile_mode_t mode);

// Starts a provisional outbound transfer from caller-owned staged storage.
// The reconciler transfers and ACKs every payload byte, then pauses in
// PUSH_PREPARED until the owner explicitly authorizes the peer commit.
bool noah_profile_split_reconciler_prepared_push_begin(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor, void *source_context, noah_profile_split_local_read_fn source_read);
bool noah_profile_split_reconciler_prepared_push_begin_logical(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor, void *source_context, noah_profile_split_local_read_fn source_read, uint32_t via_generation, uint32_t via_digest);
bool noah_profile_split_reconciler_prepared_push_ready(const noah_profile_split_reconciler_t *reconciler, noah_profile_split_descriptor_t *descriptor);
bool noah_profile_split_reconciler_prepared_push_authorize_commit(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor);
// Cancellation is scan-owned: once the peer admitted PREPARE_BEGIN, this
// schedules an idempotent protocol ABORT rather than touching storage here.
bool noah_profile_split_reconciler_prepared_push_cancel(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor);

// PREPARE_BEGIN intent is deliberately separate from durable peer metadata.
// This lets the owner arbitrate simultaneous candidates without publishing a
// provisional record as committed split authority.
bool noah_profile_split_reconciler_provisional_peer_descriptor(const noah_profile_split_reconciler_t *reconciler, noah_profile_split_descriptor_t *descriptor);

// Refresh and publish local durable authority immediately, even while normal
// retry/poll backoff would otherwise defer the next metadata exchange.
bool noah_profile_split_reconciler_refresh_authority(noah_profile_split_reconciler_t *reconciler);

const noah_profile_split_authority_t *noah_profile_split_reconciler_authority(const noah_profile_split_reconciler_t *reconciler);
bool                                  noah_profile_split_reconciler_status(const noah_profile_split_reconciler_t *reconciler, noah_profile_split_reconciler_status_t *status);
