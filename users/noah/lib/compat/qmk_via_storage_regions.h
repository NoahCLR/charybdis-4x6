// ───────────────────────────────────────────────────────────────────────────
// QMK VIA Canonical Storage Regions
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "qmk_via_sync_protocol.h"

typedef struct {
    uint32_t                   hash;
    noah_qmk_via_sync_region_t region;
    uint16_t                   offset;
    bool                       complete;
} noah_qmk_via_storage_digest_cursor_t;

#ifdef VIA_ENABLE
uint16_t noah_qmk_via_storage_region_size(noah_qmk_via_sync_region_t region);
bool     noah_qmk_via_storage_region_read(noah_qmk_via_sync_region_t region, uint16_t offset, uint8_t *data, uint8_t length);
bool     noah_qmk_via_storage_region_write(noah_qmk_via_sync_region_t region, uint16_t offset, const uint8_t *data, uint8_t length);
void     noah_qmk_via_storage_digest_init(noah_qmk_via_storage_digest_cursor_t *cursor);
bool     noah_qmk_via_storage_digest_step(noah_qmk_via_storage_digest_cursor_t *cursor, uint8_t byte_budget, uint32_t *out_digest);
#else
static inline uint16_t noah_qmk_via_storage_region_size(noah_qmk_via_sync_region_t region) {
    (void)region;
    return 0u;
}

static inline bool noah_qmk_via_storage_region_read(noah_qmk_via_sync_region_t region, uint16_t offset, uint8_t *data, uint8_t length) {
    (void)region;
    (void)offset;
    (void)data;
    (void)length;
    return false;
}

static inline bool noah_qmk_via_storage_region_write(noah_qmk_via_sync_region_t region, uint16_t offset, const uint8_t *data, uint8_t length) {
    (void)region;
    (void)offset;
    (void)data;
    (void)length;
    return false;
}

static inline void noah_qmk_via_storage_digest_init(noah_qmk_via_storage_digest_cursor_t *cursor) {
    if (cursor) {
        *cursor = (noah_qmk_via_storage_digest_cursor_t){0};
    }
}

static inline bool noah_qmk_via_storage_digest_step(noah_qmk_via_storage_digest_cursor_t *cursor, uint8_t byte_budget, uint32_t *out_digest) {
    (void)cursor;
    (void)byte_budget;
    (void)out_digest;
    return false;
}
#endif
