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

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)
void noah_qmk_via_split_sync_init(void);
void noah_qmk_via_split_sync_matrix_scan(void);
void noah_qmk_via_split_sync_note_mutation(uint8_t effects);
// Storage changed outside this layer, via the write-through mirror. Recompute
// the local digest so nothing advertises a stale one.
void                                     noah_qmk_via_split_sync_note_local_storage_changed(void);
noah_qmk_via_split_sync_debug_snapshot_t noah_qmk_via_split_sync_debug_snapshot(void);
#else
static inline void noah_qmk_via_split_sync_init(void) {}
static inline void noah_qmk_via_split_sync_matrix_scan(void) {}

static inline void noah_qmk_via_split_sync_note_mutation(uint8_t effects) {
    (void)effects;
}
static inline void                                     noah_qmk_via_split_sync_note_local_storage_changed(void) {}
static inline noah_qmk_via_split_sync_debug_snapshot_t noah_qmk_via_split_sync_debug_snapshot(void) {
    return (noah_qmk_via_split_sync_debug_snapshot_t){0};
}
#endif
