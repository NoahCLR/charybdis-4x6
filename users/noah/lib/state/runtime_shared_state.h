// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────
//
// Syncs split-visible runtime state across halves so non-owning modules such
// as RGB can render the correct remote state.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t automouse_progress;
    uint8_t  pd_mode_flags;
    uint8_t  pd_mode_locked_flags;
    uint8_t  key_feedback_flags;
} runtime_shared_state_packet_t;

#if defined(SPLIT_TRANSACTION_IDS_USER)

extern runtime_shared_state_packet_t runtime_shared_state_remote;

void runtime_shared_state_init(void);
void runtime_shared_state_sync_tick(void);
void runtime_shared_state_sync_elapsed(uint16_t raw_elapsed);
void runtime_shared_state_sync(void);

#else

static const runtime_shared_state_packet_t runtime_shared_state_remote = {0};

static inline void runtime_shared_state_init(void) {}
static inline void runtime_shared_state_sync_tick(void) {}
static inline void runtime_shared_state_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
static inline void runtime_shared_state_sync(void) {}

#endif // defined(SPLIT_TRANSACTION_IDS_USER)
