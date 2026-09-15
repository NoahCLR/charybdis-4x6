// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Durable Split Reconciliation
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_split_sync.h"

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)

#    include <stddef.h>
#    include <string.h>

#    include "atomic_util.h"
#    include "via.h"
#    include "../macro/via_macro_defaults.h"
#    include "../macro/via_macro_provider.h"
#    include "../rgb/core/rgb_runtime.h"
#    include "qmk_via_storage_contract.h"
#    include "qmk_via_storage_regions.h"
#    include "qmk_via_sync_metadata.h"
#    include "qmk_via_sync_protocol.h"
#    include "qmk_via_sync_state.h"
#    include "transactions.h" // QMK

#    ifndef VIA_SPLIT_SYNC_RETRY_INITIAL_MS
#        define VIA_SPLIT_SYNC_RETRY_INITIAL_MS 50u
#    endif
#    ifndef VIA_SPLIT_SYNC_RETRY_MAX_MS
#        define VIA_SPLIT_SYNC_RETRY_MAX_MS 1000u
#    endif
#    ifndef VIA_SPLIT_SYNC_SESSION_REFRESH_MS
#        define VIA_SPLIT_SYNC_SESSION_REFRESH_MS 1000u
#    endif

_Static_assert(NOAH_QMK_VIA_SYNC_FRAME_SIZE <= RPC_M2S_BUFFER_SIZE, "VIA sync request frame exceeds QMK RPC buffer");
_Static_assert(NOAH_QMK_VIA_SYNC_FRAME_SIZE <= RPC_S2M_BUFFER_SIZE, "VIA sync response frame exceeds QMK RPC buffer");
_Static_assert(VIA_SPLIT_SYNC_RETRY_INITIAL_MS > 0u, "VIA sync retry delay must be nonzero");
_Static_assert(VIA_SPLIT_SYNC_RETRY_INITIAL_MS <= VIA_SPLIT_SYNC_RETRY_MAX_MS, "VIA sync retry range is invalid");

typedef enum {
    NOAH_QMK_VIA_TX_METADATA = 0,
    NOAH_QMK_VIA_TX_PUSH_BEGIN,
    NOAH_QMK_VIA_TX_PUSH_CHUNK,
    NOAH_QMK_VIA_TX_PUSH_COMMIT,
    NOAH_QMK_VIA_TX_PULL_CHUNK,
    NOAH_QMK_VIA_TX_PULL_VERIFY,
} noah_qmk_via_tx_phase_t;

typedef struct {
    bool                       active;
    bool                       all_received;
    bool                       verifying;
    bool                       finalizing;
    bool                       committed;
    bool                       logical_staging;
    bool                       staged;
    uint32_t                   generation;
    uint32_t                   digest;
    noah_qmk_via_sync_region_t region;
    uint16_t                   offset;
    noah_qmk_via_sync_region_t last_region;
    uint16_t                   last_offset;
    uint8_t                    last_length;
} noah_qmk_via_receiver_t;

typedef struct {
    uint8_t  pending_effects;
    bool     digest_active;
    bool     digest_valid;
    bool     replication_pending;
    bool     recovery_cache_invalidation_pending;
    uint8_t  digest_epoch;
    uint32_t digest;
} noah_qmk_via_shared_state_t;

typedef struct {
    uint8_t request[NOAH_QMK_VIA_SYNC_FRAME_SIZE];
    uint8_t cached_response[NOAH_QMK_VIA_SYNC_FRAME_SIZE];
    bool    pending;
    bool    processing;
    bool    cached_response_valid;
} noah_qmk_via_slave_mailbox_t;

typedef struct {
    noah_qmk_via_sync_frame_t request;
    noah_qmk_via_logical_status_t status;
} noah_qmk_via_logical_transaction_t;

static noah_qmk_via_shared_state_t          noah_qmk_via_shared_state;
static noah_qmk_via_storage_digest_cursor_t noah_qmk_via_local_digest_cursor;
static noah_qmk_via_slave_mailbox_t          noah_qmk_via_slave_mailbox;
static noah_qmk_via_logical_transaction_t    noah_qmk_via_logical;

static noah_qmk_via_tx_phase_t    noah_qmk_via_tx_phase;
static noah_qmk_via_sync_region_t noah_qmk_via_tx_region;
static uint16_t                   noah_qmk_via_tx_offset;
static uint32_t                   noah_qmk_via_tx_generation;
static uint32_t                   noah_qmk_via_tx_digest;
static uint32_t                   noah_qmk_via_peer_generation;
static uint32_t                   noah_qmk_via_peer_digest;
static uint32_t                   noah_qmk_via_last_peer_ack_generation;
static uint32_t                   noah_qmk_via_last_peer_ack_digest;
static uint32_t                   noah_qmk_via_next_attempt_at;
static uint16_t                   noah_qmk_via_retry_ms;
static bool                       noah_qmk_via_role_known;
static bool                       noah_qmk_via_was_master;
static uint16_t                   noah_qmk_via_retry_count;
static uint8_t                    noah_qmk_via_session_reject_count;
static uint16_t                   noah_qmk_via_rejected_frame_count;
static uint16_t                   noah_qmk_via_conflict_count;
static noah_qmk_via_sync_status_t noah_qmk_via_last_error;
static bool                       noah_qmk_via_boot_authority_known;
static bool                       noah_qmk_via_boot_recovery_active;
static uint32_t                   noah_qmk_via_boot_generation;
static uint32_t                   noah_qmk_via_boot_digest;

static noah_qmk_via_receiver_t              noah_qmk_via_receiver;
static uint8_t                              noah_qmk_via_receiver_epoch;
static uint8_t                              noah_qmk_via_receiver_verify_epoch;
static bool                                 noah_qmk_via_receiver_verify_active;
static noah_qmk_via_storage_digest_cursor_t noah_qmk_via_receiver_verify_cursor;
static uint32_t                             noah_qmk_via_receiver_verify_digest;

static noah_qmk_via_shared_state_t noah_qmk_via_shared_snapshot(void) {
    noah_qmk_via_shared_state_t snapshot;

    ATOMIC_BLOCK_RESTORESTATE {
        snapshot = noah_qmk_via_shared_state;
    }
    return snapshot;
}

static void noah_qmk_via_slave_cache_invalidate(void) {
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_slave_mailbox.cached_response_valid = false;
    }
}

static noah_qmk_via_sync_region_t noah_qmk_via_first_transfer_region(void) {
    return NOAH_QMK_VIA_SYNC_REGION_KEYMAP;
}

static noah_qmk_via_sync_region_t noah_qmk_via_next_transfer_region(noah_qmk_via_sync_region_t region) {
    switch (region) {
        case NOAH_QMK_VIA_SYNC_REGION_KEYMAP:
            return NOAH_QMK_VIA_SYNC_REGION_ENCODER;
        case NOAH_QMK_VIA_SYNC_REGION_ENCODER:
            return NOAH_QMK_VIA_SYNC_REGION_MACRO;
        case NOAH_QMK_VIA_SYNC_REGION_MACRO:
            return NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG;
        default:
            return NOAH_QMK_VIA_SYNC_REGION_NONE;
    }
}

static void noah_qmk_via_skip_empty_transfer_regions(noah_qmk_via_sync_region_t *region, uint16_t *offset) {
    while (*region != NOAH_QMK_VIA_SYNC_REGION_NONE && noah_qmk_via_storage_region_size(*region) == 0u) {
        *region = noah_qmk_via_next_transfer_region(*region);
        *offset = 0u;
    }
}

static bool noah_qmk_via_time_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

static void noah_qmk_via_schedule_retry(uint32_t now) {
    ATOMIC_BLOCK_RESTORESTATE {
        if (noah_qmk_via_retry_count < UINT16_MAX) {
            noah_qmk_via_retry_count++;
        }
    }
    noah_qmk_via_next_attempt_at = now + noah_qmk_via_retry_ms;
    if (noah_qmk_via_retry_ms < VIA_SPLIT_SYNC_RETRY_MAX_MS) {
        uint32_t doubled      = (uint32_t)noah_qmk_via_retry_ms * 2u;
        noah_qmk_via_retry_ms = doubled > VIA_SPLIT_SYNC_RETRY_MAX_MS ? VIA_SPLIT_SYNC_RETRY_MAX_MS : (uint16_t)doubled;
    }
}

static void noah_qmk_via_schedule_progress(uint32_t now) {
    noah_qmk_via_retry_ms             = VIA_SPLIT_SYNC_RETRY_INITIAL_MS;
    noah_qmk_via_next_attempt_at      = now;
    noah_qmk_via_session_reject_count = 0u;
}

// SNAPSHOT_REQUIRED covers two very different situations, and treating them
// alike breaks one of them.
//
// Transient: the peer is briefly unable to serve this frame -- its digest is
// still being computed (noah_qmk_via_local_state_is_clean), a commit arrived
// before the last chunk registered, or its own state is momentarily dirty.
// Retrying the same frame clears these, and restarting the session instead
// re-enters metadata, gets rejected again for the same reason, and livelocks.
//
// Terminal: the peer reset and forgot the session entirely. No amount of
// retrying the same frame can clear that, because only a new SNAPSHOT_BEGIN
// reopens a session.
//
// The two are indistinguishable in a single response, so retry first and only
// renegotiate once a peer has rejected repeatedly. That keeps the self-healing
// behavior for transient rejections and still escapes a genuinely lost session.
#    ifndef VIA_SPLIT_SYNC_SESSION_REJECT_LIMIT
#        define VIA_SPLIT_SYNC_SESSION_REJECT_LIMIT 3u
#    endif

_Static_assert(VIA_SPLIT_SYNC_SESSION_REJECT_LIMIT > 0u, "VIA sync session reject limit must allow at least one retry before renegotiating");

static bool noah_qmk_via_response_requires_new_session(const noah_qmk_via_sync_frame_t *response) {
    return response && response->status == NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED;
}

static void noah_qmk_via_note_session_reject(uint32_t now) {
    if (noah_qmk_via_session_reject_count < UINT8_MAX) {
        noah_qmk_via_session_reject_count++;
    }

    if (noah_qmk_via_session_reject_count >= VIA_SPLIT_SYNC_SESSION_REJECT_LIMIT) {
        noah_qmk_via_session_reject_count = 0u;
        noah_qmk_via_tx_phase             = NOAH_QMK_VIA_TX_METADATA;
    }

    noah_qmk_via_schedule_retry(now);
}

static void noah_qmk_via_schedule_session_refresh(uint32_t now) {
    noah_qmk_via_retry_ms        = VIA_SPLIT_SYNC_RETRY_INITIAL_MS;
    noah_qmk_via_next_attempt_at = now + VIA_SPLIT_SYNC_SESSION_REFRESH_MS;
    noah_qmk_via_tx_phase        = NOAH_QMK_VIA_TX_METADATA;
}

static void noah_qmk_via_local_digest_start(void) {
    noah_qmk_via_storage_digest_init(&noah_qmk_via_local_digest_cursor);
    noah_qmk_via_slave_cache_invalidate();
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_shared_state.digest_epoch++;
        noah_qmk_via_shared_state.digest_active = true;
        noah_qmk_via_shared_state.digest_valid  = false;
    }
}

static void noah_qmk_via_recover_local_defaults(void) {
    bool defaults_committed;

    eeconfig_init_via();
    defaults_committed = noah_via_macro_defaults_reseed_for_recovery();
    noah_qmk_via_sync_state_reset_after_defaults(defaults_committed);
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_shared_state.pending_effects                     = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
        noah_qmk_via_shared_state.replication_pending                 = false;
        noah_qmk_via_shared_state.recovery_cache_invalidation_pending = defaults_committed;
    }
    noah_qmk_via_local_digest_start();
}

static bool noah_qmk_via_local_state_is_clean(noah_qmk_via_sync_state_snapshot_t state, noah_qmk_via_shared_state_t shared) {
    return state.initialized && !state.recovery_required && !state.metadata.dirty && shared.digest_valid;
}

static uint32_t noah_qmk_via_frame_generation(noah_qmk_via_sync_state_snapshot_t state) {
    return state.initialized && state.metadata.generation != 0u ? state.metadata.generation : 1u;
}

static noah_qmk_via_sync_status_t noah_qmk_via_metadata_status(noah_qmk_via_sync_state_snapshot_t state, noah_qmk_via_shared_state_t shared) {
    if (!state.initialized) {
        return NOAH_QMK_VIA_SYNC_STATUS_SCHEMA_MISMATCH;
    }
    if (state.recovery_required || state.metadata.dirty) {
        return NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED;
    }
    if (!shared.digest_valid) {
        return NOAH_QMK_VIA_SYNC_STATUS_BUSY;
    }
    return NOAH_QMK_VIA_SYNC_STATUS_OK;
}

static bool noah_qmk_via_encode_response(const noah_qmk_via_sync_frame_t *frame, uint8_t target_size, void *target) {
    return target && target_size == NOAH_QMK_VIA_SYNC_FRAME_SIZE && noah_qmk_via_sync_frame_encode(frame, (uint8_t *)target);
}

static void noah_qmk_via_respond_status(noah_qmk_via_sync_message_kind_t kind, noah_qmk_via_sync_status_t status, uint32_t generation, uint32_t digest, noah_qmk_via_sync_region_t region, uint16_t offset, uint16_t region_length, uint8_t target_size, void *target) {
    noah_qmk_via_sync_frame_t response = {
        .kind          = kind,
        .status        = status,
        .region        = region,
        .generation    = generation != 0u ? generation : 1u,
        .offset        = offset,
        .region_length = region_length,
        .digest        = digest,
    };

    (void)noah_qmk_via_encode_response(&response, target_size, target);
}

static bool noah_qmk_via_receiver_duplicate_matches(const noah_qmk_via_sync_frame_t *request) {
    uint8_t stored[NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX];

    if (request->region != noah_qmk_via_receiver.last_region || request->offset != noah_qmk_via_receiver.last_offset || request->payload_length != noah_qmk_via_receiver.last_length) {
        return false;
    }
    return noah_qmk_via_storage_region_read(request->region, request->offset, stored, request->payload_length) && memcmp(stored, request->payload, request->payload_length) == 0;
}

static void noah_qmk_via_receiver_advance(uint8_t length) {
    uint16_t capacity = noah_qmk_via_storage_region_size(noah_qmk_via_receiver.region);

    noah_qmk_via_receiver.offset += length;
    if (noah_qmk_via_receiver.offset == capacity) {
        noah_qmk_via_receiver.region = noah_qmk_via_next_transfer_region(noah_qmk_via_receiver.region);
        noah_qmk_via_receiver.offset = 0u;
        noah_qmk_via_skip_empty_transfer_regions(&noah_qmk_via_receiver.region, &noah_qmk_via_receiver.offset);
        noah_qmk_via_receiver.all_received = noah_qmk_via_receiver.region == NOAH_QMK_VIA_SYNC_REGION_NONE;
    }
}

static void noah_qmk_via_handle_metadata(uint8_t target_size, void *target) {
    noah_qmk_via_sync_state_snapshot_t state    = noah_qmk_via_sync_state_snapshot();
    noah_qmk_via_shared_state_t        shared   = noah_qmk_via_shared_snapshot();
    noah_qmk_via_sync_frame_t          response = {
        .kind       = NOAH_QMK_VIA_SYNC_MESSAGE_METADATA,
        .status     = noah_qmk_via_metadata_status(state, shared),
        .generation = noah_qmk_via_frame_generation(state),
        .digest     = shared.digest_valid ? shared.digest : 0u,
    };

    (void)noah_qmk_via_encode_response(&response, target_size, target);
}

static void noah_qmk_via_handle_snapshot_begin(const noah_qmk_via_sync_frame_t *request, bool logical_staging, uint8_t target_size, void *target) {
    bool                       finalizing;
    noah_qmk_via_sync_region_t first_region = noah_qmk_via_first_transfer_region();
    uint16_t                   first_offset = 0u;

    noah_qmk_via_skip_empty_transfer_regions(&first_region, &first_offset);

    ATOMIC_BLOCK_RESTORESTATE {
        finalizing = noah_qmk_via_receiver.finalizing;
        if (!finalizing) {
            noah_qmk_via_receiver_epoch++;
            noah_qmk_via_receiver = (noah_qmk_via_receiver_t){
                .finalizing = true,
                .logical_staging = logical_staging,
                .generation = request->generation,
                .digest     = request->digest,
                .region     = first_region,
                .offset     = first_offset,
            };
        }
    }
    if (finalizing) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_BUSY, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }
    if (!noah_qmk_via_sync_state_begin_remote_apply(request->generation)) {
        ATOMIC_BLOCK_RESTORESTATE {
            noah_qmk_via_receiver.finalizing = false;
        }
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_STORAGE_ERROR, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }

    via_eeprom_set_valid(false);
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_shared_state.pending_effects                     = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
        noah_qmk_via_shared_state.digest_active                       = false;
        noah_qmk_via_shared_state.digest_valid                        = false;
        noah_qmk_via_shared_state.replication_pending                 = false;
        noah_qmk_via_shared_state.recovery_cache_invalidation_pending = false;
        noah_qmk_via_shared_state.digest_epoch++;
    }
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_receiver.active     = true;
        noah_qmk_via_receiver.finalizing = false;
    }
    noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request->generation, request->digest, noah_qmk_via_receiver.region, noah_qmk_via_receiver.offset, noah_qmk_via_storage_region_size(noah_qmk_via_receiver.region), target_size, target);
}

static void noah_qmk_via_handle_logical_stage_verify(const noah_qmk_via_sync_frame_t *request, uint8_t target_size, void *target) {
    bool staged;
    bool active;
    bool verifying;
    bool logical_staging;
    uint32_t generation;
    uint32_t digest;

    ATOMIC_BLOCK_RESTORESTATE {
        staged          = noah_qmk_via_receiver.staged;
        active          = noah_qmk_via_receiver.active;
        verifying       = noah_qmk_via_receiver.verifying;
        logical_staging = noah_qmk_via_receiver.logical_staging;
        generation      = noah_qmk_via_receiver.generation;
        digest          = noah_qmk_via_receiver.digest;
    }
    if (staged && logical_staging && request->generation == generation && request->digest == digest) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, generation, digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }
    if (!logical_staging || !active || request->generation != generation || request->digest != digest) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }
    if (!verifying) {
        ATOMIC_BLOCK_RESTORESTATE {
            noah_qmk_via_receiver.verifying = true;
        }
    }
    noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_BUSY, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
}

static void noah_qmk_via_handle_logical_stage_accept(const noah_qmk_via_sync_frame_t *request, uint8_t target_size, void *target) {
    bool matches;
    noah_qmk_via_sync_state_snapshot_t state;
    noah_qmk_via_shared_state_t shared;

    ATOMIC_BLOCK_RESTORESTATE {
        matches = noah_qmk_via_receiver.logical_staging && noah_qmk_via_receiver.staged && request->generation == noah_qmk_via_receiver.generation && request->digest == noah_qmk_via_receiver.digest;
    }
    if (!matches) {
        state   = noah_qmk_via_sync_state_snapshot();
        shared  = noah_qmk_via_shared_snapshot();
        matches = state.initialized && state.metadata.generation == request->generation && shared.digest_valid && shared.digest == request->digest;
    }
    if (!matches) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }
    state = noah_qmk_via_sync_state_snapshot();
    if ((state.metadata.dirty || state.recovery_required) && !noah_qmk_via_sync_state_accept_remote(request->generation)) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_STORAGE_ERROR, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_receiver.logical_staging = false;
        noah_qmk_via_receiver.staged          = false;
        noah_qmk_via_receiver.committed       = true;
        noah_qmk_via_shared_state.replication_pending = false;
        noah_qmk_via_boot_authority_known = true;
        noah_qmk_via_boot_recovery_active = false;
    }
    via_macro_provider_invalidate_all();
    noah_rgb_runtime_invalidate_layer_maps();
    noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
}

static void noah_qmk_via_handle_logical_stage_abort(const noah_qmk_via_sync_frame_t *request, uint8_t target_size, void *target) {
    bool matches;

    ATOMIC_BLOCK_RESTORESTATE {
        matches = noah_qmk_via_receiver.logical_staging && request->generation == noah_qmk_via_receiver.generation && request->digest == noah_qmk_via_receiver.digest;
        if (matches) {
            noah_qmk_via_receiver_epoch++;
            noah_qmk_via_receiver = (noah_qmk_via_receiver_t){0};
        }
    }
    noah_qmk_via_receiver_verify_active = false;
    noah_qmk_via_respond_status(matches ? NOAH_QMK_VIA_SYNC_MESSAGE_ACK : NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, matches ? NOAH_QMK_VIA_SYNC_STATUS_OK : NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
}

static void noah_qmk_via_handle_push_chunk(const noah_qmk_via_sync_frame_t *request, uint8_t target_size, void *target) {
    uint16_t capacity;

    if (!noah_qmk_via_receiver.active || request->generation != noah_qmk_via_receiver.generation || request->digest != noah_qmk_via_receiver.digest) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }
    capacity = noah_qmk_via_storage_region_size(request->region);
    if (noah_qmk_via_receiver.logical_staging) {
        if (request->region == NOAH_QMK_VIA_SYNC_REGION_NONE || request->region_length != capacity || request->payload_length == 0u || request->offset >= capacity || request->payload_length > capacity - request->offset || !noah_qmk_via_storage_region_write(request->region, request->offset, request->payload, request->payload_length)) {
            ATOMIC_BLOCK_RESTORESTATE {
                if (noah_qmk_via_rejected_frame_count < UINT16_MAX) {
                    noah_qmk_via_rejected_frame_count++;
                }
                noah_qmk_via_last_error = NOAH_QMK_VIA_SYNC_STATUS_RANGE_ERROR;
            }
            noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_RANGE_ERROR, request->generation, request->digest, request->region, request->offset, capacity, target_size, target);
            return;
        }
        noah_qmk_via_receiver.last_region = request->region;
        noah_qmk_via_receiver.last_offset = request->offset;
        noah_qmk_via_receiver.last_length = request->payload_length;
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request->generation, request->digest, request->region, (uint16_t)(request->offset + request->payload_length), capacity, target_size, target);
        return;
    }
    if (noah_qmk_via_receiver_duplicate_matches(request)) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request->generation, request->digest, noah_qmk_via_receiver.region, noah_qmk_via_receiver.offset, noah_qmk_via_storage_region_size(noah_qmk_via_receiver.region), target_size, target);
        return;
    }
    if (request->region != noah_qmk_via_receiver.region || request->offset != noah_qmk_via_receiver.offset || request->region_length != noah_qmk_via_storage_region_size(request->region) || !noah_qmk_via_storage_region_write(request->region, request->offset, request->payload, request->payload_length)) {
        ATOMIC_BLOCK_RESTORESTATE {
            if (noah_qmk_via_rejected_frame_count < UINT16_MAX) {
                noah_qmk_via_rejected_frame_count++;
            }
            noah_qmk_via_last_error = NOAH_QMK_VIA_SYNC_STATUS_RANGE_ERROR;
        }
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_RANGE_ERROR, request->generation, request->digest, noah_qmk_via_receiver.region, noah_qmk_via_receiver.offset, noah_qmk_via_storage_region_size(noah_qmk_via_receiver.region), target_size, target);
        return;
    }

    noah_qmk_via_receiver.last_region = request->region;
    noah_qmk_via_receiver.last_offset = request->offset;
    noah_qmk_via_receiver.last_length = request->payload_length;
    noah_qmk_via_receiver_advance(request->payload_length);
    noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request->generation, request->digest, noah_qmk_via_receiver.region, noah_qmk_via_receiver.offset, noah_qmk_via_storage_region_size(noah_qmk_via_receiver.region), target_size, target);
}

static void noah_qmk_via_handle_pull_chunk(const noah_qmk_via_sync_frame_t *request, uint8_t target_size, void *target) {
    noah_qmk_via_sync_state_snapshot_t state  = noah_qmk_via_sync_state_snapshot();
    noah_qmk_via_shared_state_t        shared = noah_qmk_via_shared_snapshot();
    noah_qmk_via_sync_frame_t          response;
    uint16_t                           capacity;
    uint16_t                           remaining;

    if (!noah_qmk_via_local_state_is_clean(state, shared) || request->generation != state.metadata.generation || request->digest != shared.digest) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, noah_qmk_via_frame_generation(state), shared.digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }

    capacity = noah_qmk_via_storage_region_size(request->region);
    if (request->region_length != capacity || request->offset >= capacity) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_RANGE_ERROR, request->generation, request->digest, request->region, request->offset, capacity, target_size, target);
        return;
    }
    remaining = capacity - request->offset;
    response  = (noah_qmk_via_sync_frame_t){
        .kind           = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK,
        .status         = NOAH_QMK_VIA_SYNC_STATUS_OK,
        .region         = request->region,
        .generation     = request->generation,
        .offset         = request->offset,
        .region_length  = capacity,
        .digest         = request->digest,
        .payload_length = remaining < NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX ? (uint8_t)remaining : NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX,
    };
    if (!noah_qmk_via_storage_region_read(response.region, response.offset, response.payload, response.payload_length)) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_STORAGE_ERROR, request->generation, request->digest, request->region, request->offset, capacity, target_size, target);
        return;
    }
    (void)noah_qmk_via_encode_response(&response, target_size, target);
}

static void noah_qmk_via_handle_snapshot_commit(const noah_qmk_via_sync_frame_t *request, uint8_t target_size, void *target) {
    bool     committed;
    bool     active;
    bool     all_received;
    bool     verifying;
    uint32_t generation;
    uint32_t digest;

    ATOMIC_BLOCK_RESTORESTATE {
        committed    = noah_qmk_via_receiver.committed;
        active       = noah_qmk_via_receiver.active;
        all_received = noah_qmk_via_receiver.all_received;
        verifying    = noah_qmk_via_receiver.verifying;
        generation   = noah_qmk_via_receiver.generation;
        digest       = noah_qmk_via_receiver.digest;
    }
    if (committed && request->generation == generation && request->digest == digest) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }
    if (!active || !all_received || request->generation != generation || request->digest != digest) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
        return;
    }
    if (!verifying) {
        ATOMIC_BLOCK_RESTORESTATE {
            if (!noah_qmk_via_receiver.verifying) {
                noah_qmk_via_receiver.verifying = true;
            }
        }
    }
    noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_BUSY, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, target_size, target);
}

static void noah_qmk_via_process_slave_request(const noah_qmk_via_sync_frame_t *request, uint8_t response_size, void *response_data) {
    if (!request) {
        return;
    }
    if (!noah_qmk_via_boot_authority_known && request->kind != NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ACCEPT && request->kind != NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ABORT) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_BUSY, request->generation, request->digest, request->region, request->offset, request->region_length, response_size, response_data);
        return;
    }

    switch (request->kind) {
        case NOAH_QMK_VIA_SYNC_MESSAGE_METADATA:
            noah_qmk_via_handle_metadata(response_size, response_data);
            break;
        case NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN:
            noah_qmk_via_handle_snapshot_begin(request, false, response_size, response_data);
            break;
        case NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_BEGIN:
            noah_qmk_via_handle_snapshot_begin(request, true, response_size, response_data);
            break;
        case NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK:
            noah_qmk_via_handle_push_chunk(request, response_size, response_data);
            break;
        case NOAH_QMK_VIA_SYNC_MESSAGE_PULL_CHUNK:
            noah_qmk_via_handle_pull_chunk(request, response_size, response_data);
            break;
        case NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT:
            noah_qmk_via_handle_snapshot_commit(request, response_size, response_data);
            break;
        case NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_VERIFY:
            noah_qmk_via_handle_logical_stage_verify(request, response_size, response_data);
            break;
        case NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ACCEPT:
            noah_qmk_via_handle_logical_stage_accept(request, response_size, response_data);
            break;
        case NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ABORT:
            noah_qmk_via_handle_logical_stage_abort(request, response_size, response_data);
            break;
        default:
            noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_INVALID_FRAME, request->generation, request->digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, response_size, response_data);
            break;
    }
}

static void noah_qmk_via_respond_callback_busy(const noah_qmk_via_sync_frame_t *request, uint8_t response_size, void *response_data) {
    noah_qmk_via_sync_message_kind_t kind;

    if (!request) {
        return;
    }
    kind = request->kind == NOAH_QMK_VIA_SYNC_MESSAGE_METADATA ? NOAH_QMK_VIA_SYNC_MESSAGE_METADATA : NOAH_QMK_VIA_SYNC_MESSAGE_ACK;
    noah_qmk_via_respond_status(kind, NOAH_QMK_VIA_SYNC_STATUS_BUSY, request->generation, request->digest, request->region, request->offset, request->region_length, response_size, response_data);
}

static void noah_qmk_via_split_sync_rpc(uint8_t request_size, const void *request_data, uint8_t response_size, void *response_data) {
    noah_qmk_via_sync_frame_t request;
    bool                      cached = false;

    if (!noah_qmk_via_sync_frame_decode((const uint8_t *)request_data, request_size, &request)) {
        noah_qmk_via_sync_state_snapshot_t state  = noah_qmk_via_sync_state_snapshot();
        noah_qmk_via_shared_state_t        shared = noah_qmk_via_shared_snapshot();
        ATOMIC_BLOCK_RESTORESTATE {
            if (noah_qmk_via_rejected_frame_count < UINT16_MAX) {
                noah_qmk_via_rejected_frame_count++;
            }
            noah_qmk_via_last_error = NOAH_QMK_VIA_SYNC_STATUS_INVALID_FRAME;
        }
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_INVALID_FRAME, noah_qmk_via_frame_generation(state), shared.digest, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, response_size, response_data);
        return;
    }
    if (!noah_qmk_via_boot_authority_known && request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_METADATA) {
        noah_qmk_via_respond_callback_busy(&request, response_size, response_data);
        return;
    }

    ATOMIC_BLOCK_RESTORESTATE {
        if (response_data && response_size == NOAH_QMK_VIA_SYNC_FRAME_SIZE && noah_qmk_via_slave_mailbox.cached_response_valid && memcmp(noah_qmk_via_slave_mailbox.request, request_data, NOAH_QMK_VIA_SYNC_FRAME_SIZE) == 0) {
            memcpy(response_data, noah_qmk_via_slave_mailbox.cached_response, NOAH_QMK_VIA_SYNC_FRAME_SIZE);
            cached = true;
        } else if (!noah_qmk_via_slave_mailbox.pending && !noah_qmk_via_slave_mailbox.processing) {
            memcpy(noah_qmk_via_slave_mailbox.request, request_data, NOAH_QMK_VIA_SYNC_FRAME_SIZE);
            noah_qmk_via_slave_mailbox.pending               = true;
            noah_qmk_via_slave_mailbox.cached_response_valid = false;
        }
    }
    if (!cached) {
        noah_qmk_via_respond_callback_busy(&request, response_size, response_data);
    }
}

static bool noah_qmk_via_slave_mailbox_tick(void) {
    uint8_t                   request_wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE];
    uint8_t                   response_wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE] = {0};
    noah_qmk_via_sync_frame_t request;
    noah_qmk_via_sync_frame_t response;
    bool                      available = false;
    bool                      terminal;

    ATOMIC_BLOCK_RESTORESTATE {
        if (noah_qmk_via_slave_mailbox.pending && !noah_qmk_via_slave_mailbox.processing) {
            memcpy(request_wire, noah_qmk_via_slave_mailbox.request, sizeof(request_wire));
            noah_qmk_via_slave_mailbox.pending    = false;
            noah_qmk_via_slave_mailbox.processing = true;
            available                             = true;
        }
    }
    if (!available) {
        return false;
    }

    if (!noah_qmk_via_sync_frame_decode(request_wire, sizeof(request_wire), &request)) {
        noah_qmk_via_respond_status(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_INVALID_FRAME, 1u, 0u, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, 0u, sizeof(response_wire), response_wire);
    } else {
        noah_qmk_via_process_slave_request(&request, sizeof(response_wire), response_wire);
    }
    terminal = noah_qmk_via_sync_frame_decode(response_wire, sizeof(response_wire), &response) && response.status != NOAH_QMK_VIA_SYNC_STATUS_BUSY;

    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_slave_mailbox.processing = false;
        if (terminal) {
            memcpy(noah_qmk_via_slave_mailbox.cached_response, response_wire, sizeof(response_wire));
            noah_qmk_via_slave_mailbox.cached_response_valid = true;
        }
    }
    return true;
}

static bool noah_qmk_via_rpc_exchange(const noah_qmk_via_sync_frame_t *request, noah_qmk_via_sync_frame_t *response) {
    uint8_t request_wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE];
    uint8_t response_wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE] = {0};

    if (!noah_qmk_via_sync_frame_encode(request, request_wire) || !transaction_rpc_exec(PUT_VIA_KEYMAP_SYNC, sizeof(request_wire), request_wire, sizeof(response_wire), response_wire)) {
        return false;
    }
    return noah_qmk_via_sync_frame_decode(response_wire, sizeof(response_wire), response);
}

static void noah_qmk_via_start_push(noah_qmk_via_sync_state_snapshot_t state) {
    noah_qmk_via_shared_state_t shared = noah_qmk_via_shared_snapshot();

    noah_qmk_via_tx_generation = state.metadata.generation;
    noah_qmk_via_tx_digest     = shared.digest;
    noah_qmk_via_tx_phase      = NOAH_QMK_VIA_TX_PUSH_BEGIN;
    noah_qmk_via_tx_region     = noah_qmk_via_first_transfer_region();
    noah_qmk_via_tx_offset     = 0u;
    noah_qmk_via_skip_empty_transfer_regions(&noah_qmk_via_tx_region, &noah_qmk_via_tx_offset);
}

static void noah_qmk_via_start_pull(uint32_t generation, uint32_t digest) {
    noah_qmk_via_slave_cache_invalidate();
    if (!noah_qmk_via_sync_state_begin_remote_apply(generation)) {
        return;
    }
    via_eeprom_set_valid(false);
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_shared_state.pending_effects = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
    }
    noah_qmk_via_tx_generation = generation;
    noah_qmk_via_tx_digest     = digest;
    noah_qmk_via_tx_phase      = NOAH_QMK_VIA_TX_PULL_CHUNK;
    noah_qmk_via_tx_region     = noah_qmk_via_first_transfer_region();
    noah_qmk_via_tx_offset     = 0u;
    noah_qmk_via_skip_empty_transfer_regions(&noah_qmk_via_tx_region, &noah_qmk_via_tx_offset);
}

static void noah_qmk_via_advance_tx_region(uint8_t length) {
    uint16_t capacity = noah_qmk_via_storage_region_size(noah_qmk_via_tx_region);

    noah_qmk_via_tx_offset += length;
    if (noah_qmk_via_tx_offset == capacity) {
        noah_qmk_via_tx_region = noah_qmk_via_next_transfer_region(noah_qmk_via_tx_region);
        noah_qmk_via_tx_offset = 0u;
        noah_qmk_via_skip_empty_transfer_regions(&noah_qmk_via_tx_region, &noah_qmk_via_tx_offset);
    }
}

static void noah_qmk_via_process_metadata_response(const noah_qmk_via_sync_frame_t *response, uint32_t now) {
    noah_qmk_via_sync_state_snapshot_t local  = noah_qmk_via_sync_state_snapshot();
    noah_qmk_via_shared_state_t        shared = noah_qmk_via_shared_snapshot();
    bool                               local_clean;
    bool                               peer_clean;

    if (response->kind != NOAH_QMK_VIA_SYNC_MESSAGE_METADATA || response->status == NOAH_QMK_VIA_SYNC_STATUS_BUSY) {
        noah_qmk_via_schedule_retry(now);
        return;
    }
    noah_qmk_via_peer_generation = response->generation;
    noah_qmk_via_peer_digest     = response->digest;
    local_clean                  = noah_qmk_via_local_state_is_clean(local, shared);
    peer_clean                   = response->status == NOAH_QMK_VIA_SYNC_STATUS_OK;

    if (local_clean && !peer_clean) {
        noah_qmk_via_start_push(local);
        noah_qmk_via_schedule_progress(now);
        return;
    }
    if (!local_clean && peer_clean) {
        noah_qmk_via_start_pull(response->generation, response->digest);
        noah_qmk_via_schedule_progress(now);
        return;
    }
    if (!local_clean && !peer_clean) {
        noah_qmk_via_recover_local_defaults();
        noah_qmk_via_schedule_retry(now);
        return;
    }

    switch (noah_qmk_via_sync_generation_compare(local.metadata.generation, response->generation)) {
        case NOAH_QMK_VIA_SYNC_SERIAL_NEWER:
            noah_qmk_via_start_push(local);
            noah_qmk_via_schedule_progress(now);
            return;
        case NOAH_QMK_VIA_SYNC_SERIAL_OLDER:
            noah_qmk_via_start_pull(response->generation, response->digest);
            noah_qmk_via_schedule_progress(now);
            return;
        case NOAH_QMK_VIA_SYNC_SERIAL_EQUAL:
            if (shared.digest == response->digest) {
                ATOMIC_BLOCK_RESTORESTATE {
                    noah_qmk_via_shared_state.replication_pending = false;
                    noah_qmk_via_last_peer_ack_generation         = local.metadata.generation;
                    noah_qmk_via_last_peer_ack_digest             = shared.digest;
                }
                noah_qmk_via_schedule_session_refresh(now);
                return;
            }
            break;
        case NOAH_QMK_VIA_SYNC_SERIAL_AMBIGUOUS:
            break;
    }

    if (noah_qmk_via_sync_state_begin_mutation() && noah_qmk_via_sync_state_complete_mutation(true)) {
        ATOMIC_BLOCK_RESTORESTATE {
            if (noah_qmk_via_conflict_count < UINT16_MAX) {
                noah_qmk_via_conflict_count++;
            }
        }
        local = noah_qmk_via_sync_state_snapshot();
        ATOMIC_BLOCK_RESTORESTATE {
            noah_qmk_via_shared_state.replication_pending = true;
        }
        noah_qmk_via_start_push(local);
        noah_qmk_via_schedule_progress(now);
    } else {
        noah_qmk_via_schedule_retry(now);
    }
}

static void noah_qmk_via_master_metadata_tick(uint32_t now) {
    noah_qmk_via_sync_state_snapshot_t state   = noah_qmk_via_sync_state_snapshot();
    noah_qmk_via_shared_state_t        shared  = noah_qmk_via_shared_snapshot();
    noah_qmk_via_sync_frame_t          request = {
        .kind       = NOAH_QMK_VIA_SYNC_MESSAGE_METADATA,
        .status     = noah_qmk_via_metadata_status(state, shared),
        .generation = noah_qmk_via_frame_generation(state),
        .digest     = shared.digest_valid ? shared.digest : 0u,
    };
    noah_qmk_via_sync_frame_t response;

    if (!noah_qmk_via_rpc_exchange(&request, &response)) {
        noah_qmk_via_schedule_retry(now);
        return;
    }
    noah_qmk_via_process_metadata_response(&response, now);
}

static void noah_qmk_via_master_push_begin_tick(uint32_t now) {
    noah_qmk_via_sync_frame_t request = {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN, .generation = noah_qmk_via_tx_generation, .digest = noah_qmk_via_tx_digest};
    noah_qmk_via_sync_frame_t response;

    if (!noah_qmk_via_rpc_exchange(&request, &response) || response.kind != NOAH_QMK_VIA_SYNC_MESSAGE_ACK || response.status != NOAH_QMK_VIA_SYNC_STATUS_OK || response.generation != request.generation || response.digest != request.digest) {
        noah_qmk_via_schedule_retry(now);
        return;
    }
    noah_qmk_via_tx_phase = NOAH_QMK_VIA_TX_PUSH_CHUNK;
    noah_qmk_via_schedule_progress(now);
}

static void noah_qmk_via_master_push_chunk_tick(uint32_t now) {
    uint16_t                  capacity = noah_qmk_via_storage_region_size(noah_qmk_via_tx_region);
    uint16_t                  remaining;
    noah_qmk_via_sync_frame_t request;
    noah_qmk_via_sync_frame_t response;

    if (noah_qmk_via_tx_region == NOAH_QMK_VIA_SYNC_REGION_NONE) {
        noah_qmk_via_tx_phase = NOAH_QMK_VIA_TX_PUSH_COMMIT;
        noah_qmk_via_schedule_progress(now);
        return;
    }
    remaining = capacity - noah_qmk_via_tx_offset;
    request   = (noah_qmk_via_sync_frame_t){
        .kind           = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK,
        .region         = noah_qmk_via_tx_region,
        .generation     = noah_qmk_via_tx_generation,
        .offset         = noah_qmk_via_tx_offset,
        .region_length  = capacity,
        .digest         = noah_qmk_via_tx_digest,
        .payload_length = remaining < NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX ? (uint8_t)remaining : NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX,
    };
    if (!noah_qmk_via_storage_region_read(request.region, request.offset, request.payload, request.payload_length) || !noah_qmk_via_rpc_exchange(&request, &response)) {
        noah_qmk_via_schedule_retry(now);
        return;
    }
    if (response.kind != NOAH_QMK_VIA_SYNC_MESSAGE_ACK || response.status != NOAH_QMK_VIA_SYNC_STATUS_OK || response.generation != request.generation || response.digest != request.digest) {
        if (noah_qmk_via_response_requires_new_session(&response)) {
            noah_qmk_via_note_session_reject(now);
            return;
        }
        noah_qmk_via_schedule_retry(now);
        return;
    }
    noah_qmk_via_advance_tx_region(request.payload_length);
    noah_qmk_via_schedule_progress(now);
}

static void noah_qmk_via_master_push_commit_tick(uint32_t now) {
    noah_qmk_via_sync_frame_t request = {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT, .generation = noah_qmk_via_tx_generation, .digest = noah_qmk_via_tx_digest};
    noah_qmk_via_sync_frame_t response;

    if (!noah_qmk_via_rpc_exchange(&request, &response)) {
        noah_qmk_via_schedule_retry(now);
        return;
    }
    if (response.kind == NOAH_QMK_VIA_SYNC_MESSAGE_ACK && response.status == NOAH_QMK_VIA_SYNC_STATUS_BUSY) {
        noah_qmk_via_next_attempt_at = now + VIA_SPLIT_SYNC_RETRY_INITIAL_MS;
        return;
    }
    if (response.kind != NOAH_QMK_VIA_SYNC_MESSAGE_ACK || response.status != NOAH_QMK_VIA_SYNC_STATUS_OK || response.generation != request.generation || response.digest != request.digest) {
        if (noah_qmk_via_response_requires_new_session(&response)) {
            noah_qmk_via_note_session_reject(now);
            return;
        }
        noah_qmk_via_schedule_retry(now);
        return;
    }
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_shared_state.replication_pending = false;
        noah_qmk_via_last_peer_ack_generation         = noah_qmk_via_tx_generation;
        noah_qmk_via_last_peer_ack_digest             = noah_qmk_via_tx_digest;
    }
    noah_qmk_via_schedule_session_refresh(now);
}

static void noah_qmk_via_master_pull_chunk_tick(uint32_t now) {
    uint16_t                  capacity = noah_qmk_via_storage_region_size(noah_qmk_via_tx_region);
    noah_qmk_via_sync_frame_t request;
    noah_qmk_via_sync_frame_t response;

    if (noah_qmk_via_tx_region == NOAH_QMK_VIA_SYNC_REGION_NONE) {
        noah_qmk_via_local_digest_start();
        noah_qmk_via_tx_phase = NOAH_QMK_VIA_TX_PULL_VERIFY;
        return;
    }
    request = (noah_qmk_via_sync_frame_t){
        .kind          = NOAH_QMK_VIA_SYNC_MESSAGE_PULL_CHUNK,
        .region        = noah_qmk_via_tx_region,
        .generation    = noah_qmk_via_tx_generation,
        .offset        = noah_qmk_via_tx_offset,
        .region_length = capacity,
        .digest        = noah_qmk_via_tx_digest,
    };
    if (!noah_qmk_via_rpc_exchange(&request, &response)) {
        noah_qmk_via_schedule_retry(now);
        return;
    }
    if (response.kind != NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK || response.status != NOAH_QMK_VIA_SYNC_STATUS_OK || response.region != request.region || response.generation != request.generation || response.offset != request.offset || response.region_length != capacity || response.digest != request.digest || !noah_qmk_via_storage_region_write(response.region, response.offset, response.payload, response.payload_length)) {
        if (noah_qmk_via_response_requires_new_session(&response)) {
            noah_qmk_via_note_session_reject(now);
            return;
        }
        noah_qmk_via_schedule_retry(now);
        return;
    }
    noah_qmk_via_advance_tx_region(response.payload_length);
    noah_qmk_via_schedule_progress(now);
}

static bool noah_qmk_via_receiver_verify_tick(void) {
    bool     verifying;
    bool     finalizing;
    bool     committed;
    uint8_t  epoch;
    uint32_t expected_digest;
    uint32_t generation;
    bool     logical_staging;

    ATOMIC_BLOCK_RESTORESTATE {
        verifying       = noah_qmk_via_receiver.verifying;
        finalizing      = noah_qmk_via_receiver.finalizing;
        committed       = noah_qmk_via_receiver.committed;
        epoch           = noah_qmk_via_receiver_epoch;
        expected_digest = noah_qmk_via_receiver.digest;
        generation      = noah_qmk_via_receiver.generation;
        logical_staging = noah_qmk_via_receiver.logical_staging;
    }
    if (!verifying || finalizing || committed) {
        noah_qmk_via_receiver_verify_active = false;
        return false;
    }
    if (!noah_qmk_via_receiver_verify_active || noah_qmk_via_receiver_verify_epoch != epoch) {
        noah_qmk_via_storage_digest_init(&noah_qmk_via_receiver_verify_cursor);
        noah_qmk_via_receiver_verify_digest = 0u;
        noah_qmk_via_receiver_verify_epoch  = epoch;
        noah_qmk_via_receiver_verify_active = true;
    }
    if (!noah_qmk_via_storage_digest_step(&noah_qmk_via_receiver_verify_cursor, NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX, &noah_qmk_via_receiver_verify_digest)) {
        ATOMIC_BLOCK_RESTORESTATE {
            if (noah_qmk_via_receiver_epoch == epoch) {
                noah_qmk_via_receiver.active    = false;
                noah_qmk_via_receiver.verifying = false;
                noah_qmk_via_last_error         = NOAH_QMK_VIA_SYNC_STATUS_STORAGE_ERROR;
            }
        }
        noah_qmk_via_receiver_verify_active = false;
        return true;
    }
    ATOMIC_BLOCK_RESTORESTATE {
        verifying = noah_qmk_via_receiver.verifying && noah_qmk_via_receiver_epoch == epoch;
    }
    if (!verifying) {
        noah_qmk_via_receiver_verify_active = false;
        return true;
    }
    if (!noah_qmk_via_receiver_verify_cursor.complete) {
        return true;
    }
    if (noah_qmk_via_receiver_verify_digest != expected_digest) {
        ATOMIC_BLOCK_RESTORESTATE {
            if (noah_qmk_via_receiver_epoch == epoch) {
                noah_qmk_via_receiver.active    = false;
                noah_qmk_via_receiver.verifying = false;
                noah_qmk_via_last_error         = NOAH_QMK_VIA_SYNC_STATUS_DIGEST_MISMATCH;
            }
        }
        noah_qmk_via_receiver_verify_active = false;
        return true;
    }

    if (logical_staging) {
        ATOMIC_BLOCK_RESTORESTATE {
            if (noah_qmk_via_receiver_epoch == epoch && noah_qmk_via_receiver.verifying && noah_qmk_via_receiver.logical_staging) {
                noah_qmk_via_receiver.active     = false;
                noah_qmk_via_receiver.verifying  = false;
                noah_qmk_via_receiver.staged     = true;
                noah_qmk_via_shared_state.digest = noah_qmk_via_receiver_verify_digest;
                noah_qmk_via_shared_state.digest_valid = true;
                noah_qmk_via_shared_state.digest_active = false;
                noah_qmk_via_shared_state.pending_effects = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
                noah_qmk_via_shared_state.replication_pending = false;
                noah_qmk_via_shared_state.digest_epoch++;
            }
        }
        noah_qmk_via_receiver_verify_active = false;
        return true;
    }

    ATOMIC_BLOCK_RESTORESTATE {
        if (noah_qmk_via_receiver_epoch == epoch && noah_qmk_via_receiver.verifying) {
            noah_qmk_via_receiver.finalizing = true;
        } else {
            verifying = false;
        }
    }
    if (!verifying) {
        noah_qmk_via_receiver_verify_active = false;
        return true;
    }
    if (!noah_qmk_via_sync_state_accept_remote(generation)) {
        ATOMIC_BLOCK_RESTORESTATE {
            if (noah_qmk_via_receiver_epoch == epoch) {
                noah_qmk_via_receiver.active     = false;
                noah_qmk_via_receiver.verifying  = false;
                noah_qmk_via_receiver.finalizing = false;
                noah_qmk_via_last_error          = NOAH_QMK_VIA_SYNC_STATUS_STORAGE_ERROR;
            }
        }
        noah_qmk_via_receiver_verify_active = false;
        return true;
    }

    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_receiver.active                  = false;
        noah_qmk_via_receiver.verifying               = false;
        noah_qmk_via_receiver.finalizing              = false;
        noah_qmk_via_receiver.committed               = true;
        noah_qmk_via_shared_state.digest              = noah_qmk_via_receiver_verify_digest;
        noah_qmk_via_shared_state.digest_valid        = true;
        noah_qmk_via_shared_state.digest_active       = false;
        noah_qmk_via_shared_state.pending_effects     = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
        noah_qmk_via_shared_state.replication_pending = false;
        noah_qmk_via_shared_state.digest_epoch++;
    }
    noah_qmk_via_receiver_verify_active = false;
    via_macro_provider_invalidate_all();
    noah_rgb_runtime_invalidate_layer_maps();
    return true;
}

static bool noah_qmk_via_local_digest_tick(void) {
    noah_qmk_via_shared_state_t shared                     = noah_qmk_via_shared_snapshot();
    bool                        invalidate_recovery_caches = false;
    bool                        mutation_completed         = false;
    bool                        epoch_matches;
    uint32_t                    digest;

    if (!shared.digest_active) {
        return false;
    }
    if ((shared.pending_effects & NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS) && !noah_via_macro_defaults_last_seed_succeeded()) {
        return false;
    }
    digest = shared.digest;
    if (!noah_qmk_via_storage_digest_step(&noah_qmk_via_local_digest_cursor, NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX, &digest)) {
        return true;
    }
    if (!noah_qmk_via_local_digest_cursor.complete) {
        return true;
    }

    ATOMIC_BLOCK_RESTORESTATE {
        epoch_matches = noah_qmk_via_shared_state.digest_epoch == shared.digest_epoch;
        if (epoch_matches) {
            noah_qmk_via_shared_state.digest_active                       = false;
            noah_qmk_via_shared_state.digest_valid                        = true;
            noah_qmk_via_shared_state.digest                              = digest;
            invalidate_recovery_caches                                    = noah_qmk_via_shared_state.recovery_cache_invalidation_pending;
            noah_qmk_via_shared_state.recovery_cache_invalidation_pending = false;
        }
    }
    if (!epoch_matches) {
        return true;
    }
    if (invalidate_recovery_caches) {
        via_macro_provider_invalidate_all();
        noah_rgb_runtime_invalidate_layer_maps();
    }
    if (shared.pending_effects != NOAH_QMK_VIA_COMMAND_EFFECT_NONE) {
        mutation_completed = noah_qmk_via_sync_state_complete_mutation(true);
        ATOMIC_BLOCK_RESTORESTATE {
            if (noah_qmk_via_shared_state.digest_epoch == shared.digest_epoch) {
                noah_qmk_via_shared_state.pending_effects     = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
                noah_qmk_via_shared_state.replication_pending = mutation_completed;
            }
        }
        noah_qmk_via_tx_phase        = NOAH_QMK_VIA_TX_METADATA;
        noah_qmk_via_next_attempt_at = 0u;
    }
    return true;
}

void noah_qmk_via_split_sync_init(void) {
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_shared_state = (noah_qmk_via_shared_state_t){0};
        noah_qmk_via_slave_mailbox = (noah_qmk_via_slave_mailbox_t){0};
        noah_qmk_via_logical = (noah_qmk_via_logical_transaction_t){0};
    }
    noah_qmk_via_receiver                 = (noah_qmk_via_receiver_t){0};
    noah_qmk_via_receiver_epoch           = 0u;
    noah_qmk_via_receiver_verify_epoch    = 0u;
    noah_qmk_via_receiver_verify_active   = false;
    noah_qmk_via_receiver_verify_digest   = 0u;
    noah_qmk_via_tx_phase                 = NOAH_QMK_VIA_TX_METADATA;
    noah_qmk_via_next_attempt_at          = 0u;
    noah_qmk_via_retry_ms                 = VIA_SPLIT_SYNC_RETRY_INITIAL_MS;
    noah_qmk_via_role_known               = false;
    noah_qmk_via_retry_count              = 0u;
    noah_qmk_via_session_reject_count     = 0u;
    noah_qmk_via_rejected_frame_count     = 0u;
    noah_qmk_via_conflict_count           = 0u;
    noah_qmk_via_last_error               = NOAH_QMK_VIA_SYNC_STATUS_OK;
    noah_qmk_via_boot_authority_known     = false;
    noah_qmk_via_boot_recovery_active     = false;
    noah_qmk_via_boot_generation          = 0u;
    noah_qmk_via_boot_digest              = 0u;
    noah_qmk_via_peer_generation          = 0u;
    noah_qmk_via_peer_digest              = 0u;
    noah_qmk_via_last_peer_ack_generation = 0u;
    noah_qmk_via_last_peer_ack_digest     = 0u;
    noah_qmk_via_sync_state_init();
    noah_qmk_via_local_digest_start();
    transaction_register_rpc(PUT_VIA_KEYMAP_SYNC, noah_qmk_via_split_sync_rpc);
}

void noah_qmk_via_split_sync_note_local_storage_changed(void) {
    noah_qmk_via_local_digest_start();
}

void noah_qmk_via_split_sync_note_mutation(uint8_t effects) {
    if (effects == NOAH_QMK_VIA_COMMAND_EFFECT_NONE || !noah_qmk_via_sync_state_begin_mutation()) {
        return;
    }
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_shared_state.pending_effects |= effects;
    }
    noah_qmk_via_local_digest_start();
}

void noah_qmk_via_logical_boot_release(void) {
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_boot_recovery_active = false;
        noah_qmk_via_boot_generation      = 0u;
        noah_qmk_via_boot_digest          = 0u;
        noah_qmk_via_boot_authority_known = true;
    }
}

bool noah_qmk_via_logical_boot_recover(uint32_t generation, uint32_t digest) {
    if (generation == 0u || generation > NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK || digest == 0u) {
        return false;
    }
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_boot_authority_known = false;
        noah_qmk_via_boot_recovery_active = true;
        noah_qmk_via_boot_generation      = generation;
        noah_qmk_via_boot_digest          = digest;
    }
    return true;
}

static bool noah_qmk_via_logical_boot_recovery_tick(bool master) {
    noah_qmk_via_sync_state_snapshot_t state;
    noah_qmk_via_shared_state_t shared;
    noah_qmk_via_sync_frame_t request;
    noah_qmk_via_sync_frame_t response;
    bool active;
    uint32_t generation;
    uint32_t digest;

    ATOMIC_BLOCK_RESTORESTATE {
        active     = noah_qmk_via_boot_recovery_active;
        generation = noah_qmk_via_boot_generation;
        digest     = noah_qmk_via_boot_digest;
    }
    if (!active) {
        return false;
    }
    state  = noah_qmk_via_sync_state_snapshot();
    shared = noah_qmk_via_shared_snapshot();
    if (!shared.digest_valid) {
        return false;
    }
    if (state.initialized && state.metadata.generation == generation && shared.digest == digest) {
        if ((state.metadata.dirty || state.recovery_required) && !noah_qmk_via_sync_state_accept_remote(generation)) {
            return true;
        }
        ATOMIC_BLOCK_RESTORESTATE {
            noah_qmk_via_boot_authority_known = true;
            noah_qmk_via_boot_recovery_active = false;
            noah_qmk_via_shared_state.replication_pending = master;
        }
        via_macro_provider_invalidate_all();
        noah_rgb_runtime_invalidate_layer_maps();
        noah_qmk_via_tx_phase        = NOAH_QMK_VIA_TX_METADATA;
        noah_qmk_via_next_attempt_at = 0u;
        return true;
    }
    if (!master) {
        return false;
    }
    request = (noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ACCEPT, .generation = generation, .digest = digest};
    if (!noah_qmk_via_rpc_exchange(&request, &response) || response.kind != NOAH_QMK_VIA_SYNC_MESSAGE_ACK || response.status != NOAH_QMK_VIA_SYNC_STATUS_OK || response.generation != generation || response.digest != digest) {
        return true;
    }
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_boot_authority_known = true;
        noah_qmk_via_boot_recovery_active = false;
    }
    noah_qmk_via_tx_phase        = NOAH_QMK_VIA_TX_METADATA;
    noah_qmk_via_next_attempt_at = 0u;
    return true;
}

bool noah_qmk_via_split_sync_matrix_scan_step(void) {
    bool                        master = is_keyboard_master();
    noah_qmk_via_shared_state_t shared;
    uint32_t                    now;

    if (noah_qmk_via_receiver_verify_tick() || noah_qmk_via_local_digest_tick() || noah_qmk_via_slave_mailbox_tick()) {
        return true;
    }
    shared = noah_qmk_via_shared_snapshot();
    if (shared.pending_effects != NOAH_QMK_VIA_COMMAND_EFFECT_NONE) {
        return false;
    }
    if (!noah_qmk_via_role_known || master != noah_qmk_via_was_master) {
        noah_qmk_via_slave_cache_invalidate();
        noah_qmk_via_role_known      = true;
        noah_qmk_via_was_master      = master;
        noah_qmk_via_tx_phase        = NOAH_QMK_VIA_TX_METADATA;
        noah_qmk_via_next_attempt_at = 0u;
        noah_qmk_via_retry_ms        = VIA_SPLIT_SYNC_RETRY_INITIAL_MS;
    }
    if (noah_qmk_via_logical_boot_recovery_tick(master)) {
        return true;
    }
    if (!noah_qmk_via_boot_authority_known) {
        return false;
    }
    if (master && noah_qmk_via_logical.status.pending) {
        noah_qmk_via_sync_frame_t response;

        if (!noah_qmk_via_rpc_exchange(&noah_qmk_via_logical.request, &response) || response.status == NOAH_QMK_VIA_SYNC_STATUS_BUSY) {
            return true;
        }
        ATOMIC_BLOCK_RESTORESTATE {
            noah_qmk_via_logical.status.pending = false;
            noah_qmk_via_logical.status.last_status = response.status;
            noah_qmk_via_logical.status.operation_sequence++;
            if (response.kind != NOAH_QMK_VIA_SYNC_MESSAGE_ACK || response.status != NOAH_QMK_VIA_SYNC_STATUS_OK) {
                noah_qmk_via_logical.status.state = NOAH_QMK_VIA_LOGICAL_ERROR;
            } else if (noah_qmk_via_logical.request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_BEGIN) {
                noah_qmk_via_logical.status.state = NOAH_QMK_VIA_LOGICAL_STAGING;
            } else if (noah_qmk_via_logical.request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_VERIFY) {
                noah_qmk_via_logical.status.state = NOAH_QMK_VIA_LOGICAL_STAGED;
            } else if (noah_qmk_via_logical.request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ACCEPT) {
                noah_qmk_via_logical.status.state = NOAH_QMK_VIA_LOGICAL_ACCEPTED;
            } else if (noah_qmk_via_logical.request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ABORT) {
                noah_qmk_via_logical.status.state = NOAH_QMK_VIA_LOGICAL_ABORTED;
            }
        }
        return true;
    }
    if (noah_qmk_via_logical.status.state == NOAH_QMK_VIA_LOGICAL_STAGING || noah_qmk_via_logical.status.state == NOAH_QMK_VIA_LOGICAL_STAGED) {
        // The peer is the inactive VIA bank until the custom decision marker
        // authorizes ACCEPT. Ordinary reconciliation must not copy either
        // direction while that transaction is live.
        return false;
    }
    if (noah_qmk_via_logical.status.state == NOAH_QMK_VIA_LOGICAL_ACCEPTED) {
        noah_qmk_via_sync_state_snapshot_t local = noah_qmk_via_sync_state_snapshot();
        shared = noah_qmk_via_shared_snapshot();
        if (!noah_qmk_via_local_state_is_clean(local, shared) || local.metadata.generation != noah_qmk_via_logical.status.generation || shared.digest != noah_qmk_via_logical.status.digest) {
            // The decision marker is durable and the peer owns the recovery
            // copy. Hold ordinary reconciliation while the host applies only
            // its changed VIA ranges to this USB half. A reboot intentionally
            // clears this volatile fence and boot recovery can pull the peer's
            // complete copy if the host disappeared mid-roll-forward.
            return false;
        }
        ATOMIC_BLOCK_RESTORESTATE {
            noah_qmk_via_logical.status.state = NOAH_QMK_VIA_LOGICAL_IDLE;
        }
    }
    if (!master || noah_qmk_via_tx_phase == NOAH_QMK_VIA_TX_PULL_VERIFY) {
        if (noah_qmk_via_tx_phase == NOAH_QMK_VIA_TX_PULL_VERIFY && shared.digest_valid) {
            if (shared.digest == noah_qmk_via_tx_digest && noah_qmk_via_sync_state_accept_remote(noah_qmk_via_tx_generation)) {
                ATOMIC_BLOCK_RESTORESTATE {
                    noah_qmk_via_shared_state.pending_effects     = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
                    noah_qmk_via_shared_state.replication_pending = false;
                }
                via_macro_provider_invalidate_all();
                noah_rgb_runtime_invalidate_layer_maps();
                noah_qmk_via_tx_phase        = NOAH_QMK_VIA_TX_METADATA;
                noah_qmk_via_next_attempt_at = 0u;
                return true;
            } else if (shared.digest != noah_qmk_via_tx_digest) {
                noah_qmk_via_tx_phase = NOAH_QMK_VIA_TX_METADATA;
            }
        }
        return false;
    }

    now = timer_read32();
    if (!noah_qmk_via_time_reached(now, noah_qmk_via_next_attempt_at)) {
        return false;
    }
    switch (noah_qmk_via_tx_phase) {
        case NOAH_QMK_VIA_TX_METADATA:
            noah_qmk_via_master_metadata_tick(now);
            break;
        case NOAH_QMK_VIA_TX_PUSH_BEGIN:
            noah_qmk_via_master_push_begin_tick(now);
            break;
        case NOAH_QMK_VIA_TX_PUSH_CHUNK:
            noah_qmk_via_master_push_chunk_tick(now);
            break;
        case NOAH_QMK_VIA_TX_PUSH_COMMIT:
            noah_qmk_via_master_push_commit_tick(now);
            break;
        case NOAH_QMK_VIA_TX_PULL_CHUNK:
            noah_qmk_via_master_pull_chunk_tick(now);
            break;
        case NOAH_QMK_VIA_TX_PULL_VERIFY:
            break;
    }
    return true;
}

bool noah_qmk_via_logical_submit(uint16_t transaction_id, const noah_qmk_via_sync_frame_t *request) {
    bool begin;
    bool terminal;

    if (transaction_id == 0u || !request || !is_keyboard_master()) {
        return false;
    }
    begin = request->kind == NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_BEGIN;
    terminal = request->kind == NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ACCEPT || request->kind == NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ABORT;
    if (!begin && request->kind != NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK && request->kind != NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_VERIFY && !terminal) {
        return false;
    }
    ATOMIC_BLOCK_RESTORESTATE {
        if (noah_qmk_via_logical.status.pending) {
            transaction_id = 0u;
        } else if (begin) {
            if (noah_qmk_via_logical.status.state == NOAH_QMK_VIA_LOGICAL_STAGING || noah_qmk_via_logical.status.state == NOAH_QMK_VIA_LOGICAL_STAGED) {
                transaction_id = 0u;
            }
        } else if ((noah_qmk_via_logical.status.state != NOAH_QMK_VIA_LOGICAL_STAGING && noah_qmk_via_logical.status.state != NOAH_QMK_VIA_LOGICAL_STAGED) || noah_qmk_via_logical.status.transaction_id != transaction_id || noah_qmk_via_logical.status.generation != request->generation || noah_qmk_via_logical.status.digest != request->digest) {
            transaction_id = 0u;
        }
        if (transaction_id != 0u) {
            noah_qmk_via_logical.request = *request;
            noah_qmk_via_logical.status.pending = true;
            noah_qmk_via_logical.status.last_status = NOAH_QMK_VIA_SYNC_STATUS_BUSY;
            if (begin) {
                noah_qmk_via_logical.status.state          = NOAH_QMK_VIA_LOGICAL_STAGING;
                noah_qmk_via_logical.status.transaction_id = transaction_id;
                noah_qmk_via_logical.status.generation     = request->generation;
                noah_qmk_via_logical.status.digest         = request->digest;
            }
        }
    }
    return transaction_id != 0u;
}

bool noah_qmk_via_logical_status(noah_qmk_via_logical_status_t *status) {
    if (!status) {
        return false;
    }
    ATOMIC_BLOCK_RESTORESTATE {
        *status = noah_qmk_via_logical.status;
    }
    return true;
}

bool noah_qmk_via_logical_staged(uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    noah_qmk_via_logical_status_t status;
    return noah_qmk_via_logical_status(&status) && status.state == NOAH_QMK_VIA_LOGICAL_STAGED && !status.pending && status.transaction_id == transaction_id && status.generation == generation && status.digest == digest;
}

bool noah_qmk_via_logical_converged(uint32_t generation, uint32_t digest) {
    noah_qmk_via_sync_state_snapshot_t state = noah_qmk_via_sync_state_snapshot();
    noah_qmk_via_shared_state_t shared = noah_qmk_via_shared_snapshot();
    if (generation == 0u || digest == 0u || !noah_qmk_via_local_state_is_clean(state, shared) || state.metadata.generation != generation || shared.digest != digest) {
        return false;
    }
    return !is_keyboard_master() || (noah_qmk_via_peer_generation == generation && noah_qmk_via_peer_digest == digest && noah_qmk_via_last_peer_ack_generation == generation && noah_qmk_via_last_peer_ack_digest == digest);
}

void noah_qmk_via_split_sync_matrix_scan(void) {
    (void)noah_qmk_via_split_sync_matrix_scan_step();
}

noah_qmk_via_split_sync_debug_snapshot_t noah_qmk_via_split_sync_debug_snapshot(void) {
    noah_qmk_via_sync_state_snapshot_t       state    = noah_qmk_via_sync_state_snapshot();
    noah_qmk_via_shared_state_t              shared   = noah_qmk_via_shared_snapshot();
    noah_qmk_via_split_sync_debug_snapshot_t snapshot = {
        .local_generation    = state.metadata.generation,
        .local_digest        = shared.digest,
        .peer_generation     = noah_qmk_via_peer_generation,
        .peer_digest         = noah_qmk_via_peer_digest,
        .tx_phase            = (uint8_t)noah_qmk_via_tx_phase,
        .local_dirty         = state.metadata.dirty,
        .recovery_required   = state.recovery_required,
        .digest_valid        = shared.digest_valid,
        .replication_pending = shared.replication_pending,
    };

    ATOMIC_BLOCK_RESTORESTATE {
        snapshot.retry_count              = noah_qmk_via_retry_count;
        snapshot.rejected_frame_count     = noah_qmk_via_rejected_frame_count;
        snapshot.conflict_count           = noah_qmk_via_conflict_count;
        snapshot.last_error               = (uint8_t)noah_qmk_via_last_error;
        snapshot.receiver_active          = noah_qmk_via_receiver.active;
        snapshot.last_peer_ack_generation = noah_qmk_via_last_peer_ack_generation;
        snapshot.last_peer_ack_digest     = noah_qmk_via_last_peer_ack_digest;
    }
    return snapshot;
}

#endif
