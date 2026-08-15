// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Reconciliation Metadata
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    NOAH_QMK_VIA_SYNC_METADATA_SCHEMA          = 1u,
    NOAH_QMK_VIA_SYNC_METADATA_GENERATION_BITS = 27u,
};

#define NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK UINT32_C(0x07FFFFFF)
#define NOAH_QMK_VIA_SYNC_METADATA_DIRTY_MASK UINT32_C(0x08000000)
#define NOAH_QMK_VIA_SYNC_METADATA_SCHEMA_MASK UINT32_C(0xF0000000)
#define NOAH_QMK_VIA_SYNC_METADATA_SERIAL_HALF UINT32_C(0x04000000)

typedef struct {
    uint32_t generation;
    bool     dirty;
} noah_qmk_via_sync_metadata_t;

typedef enum {
    NOAH_QMK_VIA_SYNC_SERIAL_OLDER     = -1,
    NOAH_QMK_VIA_SYNC_SERIAL_EQUAL     = 0,
    NOAH_QMK_VIA_SYNC_SERIAL_NEWER     = 1,
    NOAH_QMK_VIA_SYNC_SERIAL_AMBIGUOUS = 2,
} noah_qmk_via_sync_serial_order_t;

bool                             noah_qmk_via_sync_metadata_decode(uint32_t word, noah_qmk_via_sync_metadata_t *out);
uint32_t                         noah_qmk_via_sync_metadata_encode(noah_qmk_via_sync_metadata_t metadata);
uint32_t                         noah_qmk_via_sync_generation_next(uint32_t generation);
noah_qmk_via_sync_serial_order_t noah_qmk_via_sync_generation_compare(uint32_t candidate, uint32_t reference);
