// ───────────────────────────────────────────────────────────────────────────
// QMK VIA Reconciliation Persistent State
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>

#include "qmk_via_sync_metadata.h"

typedef struct {
    noah_qmk_via_sync_metadata_t metadata;
    bool                         initialized;
    bool                         recovery_required;
} noah_qmk_via_sync_state_snapshot_t;

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)
void                               noah_qmk_via_sync_state_init(void);
void                               noah_qmk_via_sync_state_reset_after_defaults(bool defaults_committed);
bool                               noah_qmk_via_sync_state_begin_mutation(void);
bool                               noah_qmk_via_sync_state_complete_mutation(bool canonical_readback_valid);
bool                               noah_qmk_via_sync_state_begin_remote_apply(uint32_t generation);
bool                               noah_qmk_via_sync_state_accept_remote(uint32_t generation);
noah_qmk_via_sync_state_snapshot_t noah_qmk_via_sync_state_snapshot(void);
#else
static inline void noah_qmk_via_sync_state_init(void) {}

static inline void noah_qmk_via_sync_state_reset_after_defaults(bool defaults_committed) {
    (void)defaults_committed;
}

static inline bool noah_qmk_via_sync_state_begin_mutation(void) {
    return false;
}

static inline bool noah_qmk_via_sync_state_complete_mutation(bool canonical_readback_valid) {
    (void)canonical_readback_valid;
    return false;
}

static inline bool noah_qmk_via_sync_state_begin_remote_apply(uint32_t generation) {
    (void)generation;
    return false;
}

static inline bool noah_qmk_via_sync_state_accept_remote(uint32_t generation) {
    (void)generation;
    return false;
}

static inline noah_qmk_via_sync_state_snapshot_t noah_qmk_via_sync_state_snapshot(void) {
    return (noah_qmk_via_sync_state_snapshot_t){0};
}
#endif
