// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Split-Sync Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Reconciles committed VIA-owned storage between halves. Persistent dirty and
// generation metadata, canonical snapshot digests, application acknowledgments,
// and bounded retry keep either USB role from promoting partial or stale data.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "qmk_via_sync_protocol.h"

typedef struct {
    uint32_t local_generation;
    uint32_t local_digest;
    uint32_t peer_generation;
    uint32_t peer_digest;
    uint32_t last_peer_ack_generation;
    uint32_t last_peer_ack_digest;
    uint16_t retry_count;
    uint16_t rejected_frame_count;
    uint16_t conflict_count;
    uint8_t  tx_phase;
    uint8_t  last_error;
    bool     local_dirty;
    bool     recovery_required;
    bool     digest_valid;
    bool     replication_pending;
    bool     receiver_active;
} noah_qmk_via_split_sync_debug_snapshot_t;

typedef enum {
    NOAH_QMK_VIA_LOGICAL_IDLE = 0u,
    NOAH_QMK_VIA_LOGICAL_STAGING,
    NOAH_QMK_VIA_LOGICAL_STAGED,
    NOAH_QMK_VIA_LOGICAL_ACCEPTED,
    NOAH_QMK_VIA_LOGICAL_ABORTED,
    NOAH_QMK_VIA_LOGICAL_ERROR,
} noah_qmk_via_logical_state_t;

typedef struct {
    noah_qmk_via_logical_state_t state;
    noah_qmk_via_sync_status_t   last_status;
    uint16_t                     transaction_id;
    uint16_t                     operation_sequence;
    uint32_t                     generation;
    uint32_t                     digest;
    bool                         pending;
} noah_qmk_via_logical_status_t;

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)
void noah_qmk_via_split_sync_init(void);
void noah_qmk_via_split_sync_matrix_scan(void);
// Performs at most one queued slave request, digest/storage step, or master
// exchange. Returns true when this subsystem consumed the scan's durable-I/O
// budget.
bool noah_qmk_via_split_sync_matrix_scan_step(void);
void noah_qmk_via_split_sync_note_mutation(uint8_t effects);
// Storage changed outside this layer, via the write-through mirror. Recompute
// the local digest so nothing advertises a stale one.
void                                     noah_qmk_via_split_sync_note_local_storage_changed(void);
noah_qmk_via_split_sync_debug_snapshot_t noah_qmk_via_split_sync_debug_snapshot(void);
// Callback-safe admission of one logical VIA staging frame. The actual split
// RPC runs from matrix-scan context. A queued frame must reach a terminal
// operation_sequence before the next frame is submitted.
bool noah_qmk_via_logical_submit(uint16_t transaction_id, const noah_qmk_via_sync_frame_t *request);
bool noah_qmk_via_logical_status(noah_qmk_via_logical_status_t *status);
bool noah_qmk_via_logical_staged(uint16_t transaction_id, uint32_t generation, uint32_t digest);
bool noah_qmk_via_logical_converged(uint32_t generation, uint32_t digest);
// Boot remains fenced until the profile owner identifies whether the selected
// custom record binds a logical VIA generation. Recovery accepts an already
// staged dirty bank only after its full canonical digest matches.
void noah_qmk_via_logical_boot_release(void);
bool noah_qmk_via_logical_boot_recover(uint32_t generation, uint32_t digest);
#else
static inline void noah_qmk_via_split_sync_init(void) {}
static inline void noah_qmk_via_split_sync_matrix_scan(void) {}
static inline bool noah_qmk_via_split_sync_matrix_scan_step(void) {
    return false;
}

static inline void noah_qmk_via_split_sync_note_mutation(uint8_t effects) {
    (void)effects;
}
static inline void                                     noah_qmk_via_split_sync_note_local_storage_changed(void) {}
static inline noah_qmk_via_split_sync_debug_snapshot_t noah_qmk_via_split_sync_debug_snapshot(void) {
    return (noah_qmk_via_split_sync_debug_snapshot_t){0};
}
static inline bool noah_qmk_via_logical_submit(uint16_t transaction_id, const noah_qmk_via_sync_frame_t *request) {
    (void)transaction_id;
    (void)request;
    return false;
}
static inline bool noah_qmk_via_logical_status(noah_qmk_via_logical_status_t *status) {
    (void)status;
    return false;
}
static inline bool noah_qmk_via_logical_staged(uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)transaction_id;
    (void)generation;
    (void)digest;
    return false;
}
static inline bool noah_qmk_via_logical_converged(uint32_t generation, uint32_t digest) {
    (void)generation;
    (void)digest;
    return false;
}
static inline void noah_qmk_via_logical_boot_release(void) {}
static inline bool noah_qmk_via_logical_boot_recover(uint32_t generation, uint32_t digest) {
    (void)generation;
    (void)digest;
    return false;
}
#endif
