// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Split-Sync Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Mirrors keymap-mutating VIA commands onto the non-master half so any
// split-local RGB rendering that reads the dynamic keymap storage sees the
// same authored data on both MCUs.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdint.h>

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)
void noah_qmk_via_split_sync_init(void);
void noah_qmk_via_split_sync_command(const uint8_t *data, uint8_t length);
#else
static inline void noah_qmk_via_split_sync_init(void) {}

static inline void noah_qmk_via_split_sync_command(const uint8_t *data, uint8_t length) {
    (void)data;
    (void)length;
}
#endif
