// ────────────────────────────────────────────────────────────────────────────
// Split State Sync (Master → Slave)
// ────────────────────────────────────────────────────────────────────────────
//
// Public interface for syncing auto-mouse elapsed time and pointing-device
// mode state across halves. Implementation lives in split_sync.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

typedef struct __attribute__((packed)) {
    uint16_t automouse_progress;
    uint8_t  pd_mode_flags;
    uint8_t  pd_mode_locked_flags;
} pd_sync_packet_t;

#if defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

extern pd_sync_packet_t pd_sync_remote;

void split_sync_init(void);
void pd_state_sync_elapsed(uint16_t raw_elapsed);
void pd_state_sync(void);

#else

static const pd_sync_packet_t pd_sync_remote = {0};

static inline void split_sync_init(void) {}
static inline void pd_state_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
static inline void pd_state_sync(void) {}

#endif // defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
