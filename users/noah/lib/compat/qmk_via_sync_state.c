// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Reconciliation Persistent State
// ───────────────────────────────────────────────────────────────────────────

#include "qmk_via_sync_state.h"

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)

#    include "atomic_util.h"

static noah_qmk_via_sync_state_snapshot_t noah_qmk_via_sync_state;

static void noah_qmk_via_sync_state_persist(noah_qmk_via_sync_metadata_t metadata, bool recovery_required) {
    eeconfig_update_user(noah_qmk_via_sync_metadata_encode(metadata));
    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_sync_state.metadata          = metadata;
        noah_qmk_via_sync_state.initialized       = true;
        noah_qmk_via_sync_state.recovery_required = recovery_required;
    }
}

void noah_qmk_via_sync_state_init(void) {
    noah_qmk_via_sync_metadata_t metadata;

    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_sync_state = (noah_qmk_via_sync_state_snapshot_t){0};
    }
    if (!noah_qmk_via_sync_metadata_decode(eeconfig_read_user(), &metadata)) {
        ATOMIC_BLOCK_RESTORESTATE {
            noah_qmk_via_sync_state.recovery_required = true;
        }
        return;
    }

    ATOMIC_BLOCK_RESTORESTATE {
        noah_qmk_via_sync_state.metadata          = metadata;
        noah_qmk_via_sync_state.initialized       = true;
        noah_qmk_via_sync_state.recovery_required = metadata.dirty;
    }
}

void noah_qmk_via_sync_state_reset_after_defaults(bool defaults_committed) {
    noah_qmk_via_sync_state_snapshot_t state                = noah_qmk_via_sync_state_snapshot();
    bool                               mutation_in_progress = state.initialized && state.metadata.dirty && !state.recovery_required;
    noah_qmk_via_sync_metadata_t       metadata             = {
        .generation = 1u,
        .dirty      = !defaults_committed || mutation_in_progress,
    };

    noah_qmk_via_sync_state_persist(metadata, !defaults_committed);
}

bool noah_qmk_via_sync_state_begin_mutation(void) {
    noah_qmk_via_sync_state_snapshot_t state    = noah_qmk_via_sync_state_snapshot();
    noah_qmk_via_sync_metadata_t       metadata = state.metadata;

    if (!state.initialized) {
        metadata.generation     = 1u;
        state.recovery_required = true;
    }
    if (metadata.dirty) {
        return true;
    }

    metadata.dirty = true;
    noah_qmk_via_sync_state_persist(metadata, state.recovery_required);
    return true;
}

bool noah_qmk_via_sync_state_complete_mutation(bool canonical_readback_valid) {
    noah_qmk_via_sync_state_snapshot_t state    = noah_qmk_via_sync_state_snapshot();
    noah_qmk_via_sync_metadata_t       metadata = state.metadata;

    if (!canonical_readback_valid || !state.initialized || !metadata.dirty || state.recovery_required) {
        return false;
    }

    metadata.generation = noah_qmk_via_sync_generation_next(metadata.generation);
    metadata.dirty      = false;
    noah_qmk_via_sync_state_persist(metadata, false);
    return true;
}

bool noah_qmk_via_sync_state_begin_remote_apply(uint32_t generation) {
    noah_qmk_via_sync_metadata_t metadata = {
        .generation = generation,
        .dirty      = true,
    };

    if (generation == 0u || generation > NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK) {
        return false;
    }
    noah_qmk_via_sync_state_persist(metadata, true);
    return true;
}

bool noah_qmk_via_sync_state_accept_remote(uint32_t generation) {
    noah_qmk_via_sync_metadata_t metadata = {
        .generation = generation,
        .dirty      = false,
    };

    if (generation == 0u || generation > NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK) {
        return false;
    }
    noah_qmk_via_sync_state_persist(metadata, false);
    return true;
}

noah_qmk_via_sync_state_snapshot_t noah_qmk_via_sync_state_snapshot(void) {
    noah_qmk_via_sync_state_snapshot_t snapshot;

    ATOMIC_BLOCK_RESTORESTATE {
        snapshot = noah_qmk_via_sync_state;
    }
    return snapshot;
}

#endif
