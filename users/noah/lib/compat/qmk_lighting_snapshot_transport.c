#include "qmk_lighting_snapshot_transport.h"
#if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE && defined(SPLIT_TRANSACTION_IDS_USER)
#    include "../rgb/lighting/lighting_snapshot.h"
#    include "transactions.h"
#    include <string.h>
static noah_lighting_snapshot_t snapshot;
_Static_assert(NOAH_LIGHTING_SNAPSHOT_REQUEST <= RPC_M2S_BUFFER_SIZE, "snapshot request allocation unchanged");
_Static_assert(NOAH_LIGHTING_SNAPSHOT_REPLY <= RPC_S2M_BUFFER_SIZE, "snapshot reply allocation unchanged");
void noah_qmk_lighting_snapshot_close(void) {
    snapshot.link.ready = false;
    __asm__ __volatile__("" ::: "memory");
}
static void snapshot_rpc(uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    if (!response) return;
    memset(response, 0, response_size);
    if (response_size != NOAH_LIGHTING_SNAPSHOT_REPLY || !snapshot.link.ready || snapshot.link.master || is_keyboard_master()) return;
    noah_lighting_snapshot_receive(&snapshot, request, request_size, timer_read32(), response);
}
void noah_qmk_lighting_snapshot_init(noah_lighting_working_t *working, const uint8_t profile[22]) {
    noah_qmk_lighting_snapshot_close();
    noah_lighting_snapshot_begin(&snapshot, working, is_keyboard_master(), profile);
    transaction_register_rpc(PUT_SPLIT_LIGHTING_EVENTS_SYNC, snapshot_rpc);
}
void noah_qmk_lighting_snapshot_bind_facts(noah_lighting_facts_t *a, noah_lighting_facts_t *b) {
    noah_lighting_snapshot_bind_facts(&snapshot, a, b);
}
bool noah_qmk_lighting_snapshot_step(void) {
    if (!snapshot.link.ready || snapshot.link.master != is_keyboard_master()) return false;
    uint32_t now = timer_read32();
    if (!snapshot.link.master) return noah_lighting_snapshot_main(&snapshot, now);
    uint8_t length = noah_lighting_snapshot_request(&snapshot, now);
    if (!length) return false;
    uint8_t  reply[NOAH_LIGHTING_SNAPSHOT_REPLY];
    uint32_t before = timer_read32();
    if (!transaction_rpc_exec(PUT_SPLIT_LIGHTING_EVENTS_SYNC, length, snapshot.link.packet.outgoing, sizeof reply, reply)) return false;
    uint32_t after = timer_read32();
    return noah_lighting_snapshot_reply(&snapshot, reply, before, after);
}
#endif
