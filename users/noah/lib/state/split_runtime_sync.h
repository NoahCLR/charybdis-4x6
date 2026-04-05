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

#include "../pointing/pd_mode_flags.h"

typedef struct __attribute__((packed)) {
    uint16_t automouse_progress;
    pd_mode_mask_t pd_mode_flags;
    pd_mode_mask_t pd_mode_locked_flags;
    uint8_t  key_feedback_flags;
} split_runtime_sync_packet_t;

#if defined(SPLIT_TRANSACTION_IDS_USER)

extern split_runtime_sync_packet_t split_runtime_sync_remote;

void split_runtime_sync_init(void);
void split_runtime_sync_tick(void);
void split_runtime_sync_elapsed(uint16_t raw_elapsed);
void split_runtime_sync(void);

#else

static const split_runtime_sync_packet_t split_runtime_sync_remote = {0};

static inline void split_runtime_sync_init(void) {}
static inline void split_runtime_sync_tick(void) {}
static inline void split_runtime_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
static inline void split_runtime_sync(void) {}

#endif // defined(SPLIT_TRANSACTION_IDS_USER)
