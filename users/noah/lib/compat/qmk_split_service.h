#pragma once
#include "qmk_lighting_live.h"

// Shared main-context admission for userspace's optional split RPCs (see
// docs/architecture/split-activity-sync.md). Main opens one pass per matrix
// scan; a client asks ready() before any state side effect, sends at most one
// RPC when granted, and reports complete() with the clock read before it. A
// completion reserves quiet time after the measured transaction; urgent is
// for qualified history and cold sync only. Without Lighting Live every client
// is always ready, preserving the earlier send paths.
enum { NOAH_SPLIT_SERVICE_BASE, NOAH_SPLIT_SERVICE_LIGHTING, NOAH_SPLIT_SERVICE_LIVE, NOAH_SPLIT_SERVICE_PROFILE, NOAH_SPLIT_SERVICE_VIA, NOAH_SPLIT_SERVICE_MIRROR, NOAH_SPLIT_SERVICE_LEGACY, NOAH_SPLIT_SERVICE_COUNT };
#if NOAH_LIGHTING_LIVE_AVAILABLE
static inline uint32_t noah_split_service_clock(void) {
    return timer_read32();
}
void noah_split_service_begin(void);
bool noah_split_service_ready(uint8_t client);
void noah_split_service_cancel(uint8_t client);
void noah_split_service_complete(uint8_t client, uint32_t before, bool success, bool urgent);
#else
static inline uint32_t noah_split_service_clock(void) {
    return 0;
}
static inline void noah_split_service_begin(void) {}
static inline bool noah_split_service_ready(uint8_t client) {
    (void)client;
    return true;
}
static inline void noah_split_service_cancel(uint8_t client) {
    (void)client;
}
static inline void noah_split_service_complete(uint8_t client, uint32_t before, bool success, bool urgent) {
    (void)client;
    (void)before;
    (void)success;
    (void)urgent;
}
#endif
