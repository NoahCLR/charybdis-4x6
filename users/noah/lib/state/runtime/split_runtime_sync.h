// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync
// ────────────────────────────────────────────────────────────────────────────
//
// Owns the custom split RPC sync for runtime-visible state such as pd modes,
// auto-mouse progress, and key-feedback flags. The master periodically
// re-sends the full snapshot as a heartbeat so a rebooted or rejoined half can
// recover even if no state changed meanwhile.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

#include "../../pointing/defs/pd_mode_flags.h"

typedef struct __attribute__((packed)) {
    uint16_t     automouse_progress;
    pd_mode_id_t active_mode_id;
    pd_mode_id_t locked_mode_id;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    uint8_t      pd_mode_owner_half;
#endif
    uint8_t      key_feedback_flags;
    uint8_t      key_feedback_key;
    uint8_t      key_preview_layer;
} split_runtime_sync_packet_t;

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
#define SPLIT_RUNTIME_SYNC_PACKET_EMPTY_INIT   \
    {                                          \
        .automouse_progress = 0,               \
        .active_mode_id     = PD_MODE_ID_NONE, \
        .locked_mode_id     = PD_MODE_ID_NONE, \
        .pd_mode_owner_half = SPLIT_HALF_NONE, \
        .key_feedback_flags = 0,               \
        .key_feedback_key   = UINT8_MAX,       \
        .key_preview_layer  = UINT8_MAX,       \
    }
#else
#    define SPLIT_RUNTIME_SYNC_PACKET_EMPTY_INIT \
        {                                        \
            .automouse_progress = 0,             \
            .active_mode_id     = PD_MODE_ID_NONE, \
            .locked_mode_id     = PD_MODE_ID_NONE, \
            .key_feedback_flags = 0,             \
            .key_feedback_key   = UINT8_MAX,     \
            .key_preview_layer  = UINT8_MAX,     \
        }
#endif

_Static_assert(sizeof(split_runtime_sync_packet_t) <= UINT8_MAX, "split_runtime_sync_packet_t must fit in the QMK RPC length field");

#if defined(SPLIT_TRANSACTION_IDS_USER)

extern split_runtime_sync_packet_t split_runtime_sync_remote;

void split_runtime_sync_init(void);
void split_runtime_sync_tick(void);
void split_runtime_sync_elapsed(uint16_t raw_elapsed);
void split_runtime_sync_request(void);
void split_runtime_sync(void);

#else

static const split_runtime_sync_packet_t split_runtime_sync_remote = SPLIT_RUNTIME_SYNC_PACKET_EMPTY_INIT;

static inline void split_runtime_sync_init(void) {}
static inline void split_runtime_sync_tick(void) {}
static inline void split_runtime_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
static inline void split_runtime_sync_request(void) {}
static inline void split_runtime_sync(void) {}

#endif // defined(SPLIT_TRANSACTION_IDS_USER)
