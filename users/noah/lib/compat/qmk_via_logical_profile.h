// ────────────────────────────────────────────────────────────────────────────
// Logical Profile VIA Staging Channel
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "qmk_via_split_sync.h"

enum {
    NOAH_QMK_VIA_LOGICAL_VALUE_BEGIN  = 0x15u,
    NOAH_QMK_VIA_LOGICAL_VALUE_CHUNK  = 0x16u,
    NOAH_QMK_VIA_LOGICAL_VALUE_VERIFY = 0x17u,
    NOAH_QMK_VIA_LOGICAL_VALUE_STATUS = 0x19u,
    NOAH_QMK_VIA_LOGICAL_VALUE_ABORT  = 0x1Au,
    NOAH_QMK_VIA_LOGICAL_CHUNK_MAX    = 12u,
};

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)
// Handles the logical VIA staging values only. Mutation requests are admitted
// into the split-sync mailbox and do no EEPROM or split I/O in callback context.
bool noah_qmk_via_logical_profile_handle(uint8_t *data, uint8_t length);
bool noah_qmk_via_logical_profile_accept(uint16_t transaction_id, uint32_t generation, uint32_t digest);
bool noah_qmk_via_logical_profile_abort(uint16_t transaction_id, uint32_t generation, uint32_t digest);
#else
static inline bool noah_qmk_via_logical_profile_handle(uint8_t *data, uint8_t length) {
    (void)data;
    (void)length;
    return false;
}
static inline bool noah_qmk_via_logical_profile_accept(uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)transaction_id;
    (void)generation;
    (void)digest;
    return false;
}
static inline bool noah_qmk_via_logical_profile_abort(uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)transaction_id; (void)generation; (void)digest; return false;
}
#endif
