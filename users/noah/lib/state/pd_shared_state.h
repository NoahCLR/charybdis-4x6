// ────────────────────────────────────────────────────────────────────────────
// Pointing Shared State
// ────────────────────────────────────────────────────────────────────────────
//
// Syncs pointing-related runtime state across split halves so non-owning
// modules such as RGB can render the correct remote state.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t automouse_progress;
    uint8_t  pd_mode_flags;
    uint8_t  pd_mode_locked_flags;
} pd_shared_state_packet_t;

#if defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

extern pd_shared_state_packet_t pd_shared_state_remote;

void pd_shared_state_init(void);
void pd_shared_state_sync_tick(void);
void pd_shared_state_sync_elapsed(uint16_t raw_elapsed);
void pd_shared_state_sync(void);

#else

static const pd_shared_state_packet_t pd_shared_state_remote = {0};

static inline void pd_shared_state_init(void) {}
static inline void pd_shared_state_sync_tick(void) {}
static inline void pd_shared_state_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
static inline void pd_shared_state_sync(void) {}

#endif // defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
