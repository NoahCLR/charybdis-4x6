// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Reconciliation Metadata
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_sync_metadata.h"

_Static_assert(NOAH_QMK_VIA_SYNC_METADATA_GENERATION_BITS == 27u, "VIA sync metadata layout changed");
_Static_assert((NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK & NOAH_QMK_VIA_SYNC_METADATA_DIRTY_MASK) == 0u, "VIA sync metadata fields overlap");
_Static_assert((NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK | NOAH_QMK_VIA_SYNC_METADATA_DIRTY_MASK | NOAH_QMK_VIA_SYNC_METADATA_SCHEMA_MASK) == UINT32_MAX, "VIA sync metadata must account for the complete word");

bool noah_qmk_via_sync_metadata_decode(uint32_t word, noah_qmk_via_sync_metadata_t *out) {
    uint8_t  schema     = (uint8_t)((word & NOAH_QMK_VIA_SYNC_METADATA_SCHEMA_MASK) >> 28u);
    uint32_t generation = word & NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK;

    if (!out || schema != NOAH_QMK_VIA_SYNC_METADATA_SCHEMA || generation == 0u) {
        return false;
    }

    *out = (noah_qmk_via_sync_metadata_t){
        .generation = generation,
        .dirty      = (word & NOAH_QMK_VIA_SYNC_METADATA_DIRTY_MASK) != 0u,
    };
    return true;
}

uint32_t noah_qmk_via_sync_metadata_encode(noah_qmk_via_sync_metadata_t metadata) {
    uint32_t generation = metadata.generation & NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK;

    if (generation == 0u) {
        return 0u;
    }

    return ((uint32_t)NOAH_QMK_VIA_SYNC_METADATA_SCHEMA << 28u) | (metadata.dirty ? NOAH_QMK_VIA_SYNC_METADATA_DIRTY_MASK : 0u) | generation;
}

uint32_t noah_qmk_via_sync_generation_next(uint32_t generation) {
    uint32_t next = (generation + 1u) & NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK;

    return next == 0u ? 1u : next;
}

noah_qmk_via_sync_serial_order_t noah_qmk_via_sync_generation_compare(uint32_t candidate, uint32_t reference) {
    uint32_t candidate_generation = candidate & NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK;
    uint32_t reference_generation = reference & NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK;
    uint32_t distance             = (candidate_generation - reference_generation) & NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK;

    if (candidate_generation == 0u || reference_generation == 0u || distance == NOAH_QMK_VIA_SYNC_METADATA_SERIAL_HALF) {
        return NOAH_QMK_VIA_SYNC_SERIAL_AMBIGUOUS;
    }
    if (distance == 0u) {
        return NOAH_QMK_VIA_SYNC_SERIAL_EQUAL;
    }
    return distance < NOAH_QMK_VIA_SYNC_METADATA_SERIAL_HALF ? NOAH_QMK_VIA_SYNC_SERIAL_NEWER : NOAH_QMK_VIA_SYNC_SERIAL_OLDER;
}
