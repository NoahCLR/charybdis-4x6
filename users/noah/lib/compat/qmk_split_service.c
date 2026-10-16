#include "qmk_split_service.h"
#if NOAH_LIGHTING_LIVE_AVAILABLE
_Static_assert(NOAH_SPLIT_SERVICE_COUNT <= 8u, "bounded client mask");
// One shared main-owned admission record. No requests, history or RGB buffers.
static struct {
    uint32_t next, last;
    uint16_t retry;
    uint8_t  wanted, seen, turn;
    bool     used, known, master;
} service;
void noah_split_service_begin(void) {
    uint32_t now    = timer_read32();
    bool     master = is_keyboard_master();
    if (!service.known || master != service.master || now - service.last > INT32_MAX) {
        service.next   = now;
        service.wanted = 0;
        service.turn   = 0;
        service.retry  = 0;
        service.known  = true;
        service.master = master;
    }
    service.last = now;
    service.used = false;
    service.wanted &= service.seen;
    service.seen = 0;
}
bool noah_split_service_ready(uint8_t client) {
    if (client >= NOAH_SPLIT_SERVICE_COUNT) return false;
    // Post-init can attempt a base RPC before the first matrix pass. Preserve
    // its completion quiet interval rather than resetting it on that pass.
    if (!service.known) {
        service.known  = true;
        service.master = is_keyboard_master();
        service.next = service.last = timer_read32();
    }
    service.wanted |= (uint8_t)(1u << client);
    service.seen |= (uint8_t)(1u << client);
    if (service.used || (int32_t)(timer_read32() - service.next) < 0) return false;
    for (uint8_t n = 0; n < NOAH_SPLIT_SERVICE_COUNT; n++) {
        uint8_t selected = service.turn + n;
        if (selected >= NOAH_SPLIT_SERVICE_COUNT) selected -= NOAH_SPLIT_SERVICE_COUNT;
        if (service.wanted & (1u << selected)) return selected == client;
    }
    return false;
}
void noah_split_service_cancel(uint8_t client) {
    service.wanted &= (uint8_t)~(1u << client);
}
void noah_split_service_complete(uint8_t client, uint32_t before, bool success, bool urgent) {
    uint32_t now = timer_read32(), elapsed = now - before;
    service.used = true;
    service.last = now;
    service.turn = client + 1u;
    if (service.turn == NOAH_SPLIT_SERVICE_COUNT) service.turn = 0;
    noah_split_service_cancel(client);
    // Reserve service time AFTER completion, also for healthy/PENDING traffic.
    // Millisecond quantization rounds the observed transaction cost upward.
    uint32_t quiet = urgent ? 1u : elapsed < 250u ? 4u * (elapsed + 1u) : 1000u;
    if (success)
        service.retry = 0;
    else {
        service.retry = !service.retry ? 50u : service.retry >= 500u ? 1000u : 2u * service.retry;
        if (quiet < service.retry) quiet = service.retry;
    }
    service.next = now + quiet;
}
#endif
