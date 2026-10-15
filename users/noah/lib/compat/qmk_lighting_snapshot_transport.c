#include "qmk_lighting_snapshot_transport.h"
#if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE && defined(SPLIT_TRANSACTION_IDS_USER)
#    include "../rgb/lighting/lighting_snapshot.h"
#    include "transactions.h"
#    include "atomic_util.h"
#    include <string.h>
static noah_lighting_snapshot_t snapshot;
// Match the existing split-runtime failure policy. This throttles failed
// transport attempts only; it is not a scene/input/flash duration limit.
#    ifndef SPLIT_RUNTIME_SYNC_RETRY_INITIAL_MS
#        define SPLIT_RUNTIME_SYNC_RETRY_INITIAL_MS 50u
#    endif
#    ifndef SPLIT_RUNTIME_SYNC_RETRY_MAX_MS
#        define SPLIT_RUNTIME_SYNC_RETRY_MAX_MS 1000u
#    endif
_Static_assert(SPLIT_RUNTIME_SYNC_RETRY_INITIAL_MS > 0u && SPLIT_RUNTIME_SYNC_RETRY_MAX_MS >= SPLIT_RUNTIME_SYNC_RETRY_INITIAL_MS && SPLIT_RUNTIME_SYNC_RETRY_MAX_MS <= UINT32_MAX / 2u, "bounded existing failure backoff");
static uint32_t last_failure, retry_delay;
void noah_qmk_lighting_snapshot_changed(void) { snapshot.dirty = true; }
uint8_t *noah_qmk_lighting_snapshot_fact_changes(void) { return snapshot.fact_dirty; }
_Static_assert(NOAH_LIGHTING_SNAPSHOT_REQUEST <= RPC_M2S_BUFFER_SIZE, "snapshot request allocation unchanged");
_Static_assert(NOAH_LIGHTING_SNAPSHOT_REPLY <= RPC_S2M_BUFFER_SIZE, "snapshot reply allocation unchanged");
uint8_t noah_qmk_lighting_snapshot_readiness(void) { return snapshot.link.scene_readiness; }
void noah_qmk_lighting_snapshot_peer_readiness(uint8_t status) { noah_lighting_snapshot_peer_readiness(&snapshot, status); }
void noah_qmk_lighting_snapshot_close(void) {
    snapshot.link.scene_readiness = NOAH_LIGHTING_SCENE_NOT_READY;
    __asm__ __volatile__("" ::: "memory");
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
    retry_delay = 0u;
    noah_lighting_snapshot_begin(&snapshot, working, is_keyboard_master(), profile);
    transaction_register_rpc(PUT_SPLIT_LIGHTING_EVENTS_SYNC, snapshot_rpc);
}
void noah_qmk_lighting_snapshot_bind_facts(noah_lighting_facts_t *a, noah_lighting_facts_t *b) {
    noah_lighting_snapshot_bind_facts(&snapshot, a, b);
}
bool noah_qmk_lighting_snapshot_step(void) {
    if (!snapshot.link.ready || snapshot.link.master != is_keyboard_master()) return false;
    uint32_t now = timer_read32();
    if (!snapshot.link.master) {
        bool changed = noah_lighting_snapshot_main(&snapshot, now);
        // Only this scalar publication is locked, never validation/copies/RPC.
        // SlaveThread cannot clear readiness between the checks and store.
        ATOMIC_BLOCK_RESTORESTATE {
            if (snapshot.have_baseline && snapshot.link.active && !snapshot.link.reset_requested && !snapshot.link.rejected)
                snapshot.link.scene_readiness = NOAH_LIGHTING_SCENE_READY;
        }
        return changed;
    }
    if (retry_delay && now - last_failure < retry_delay) return false;
    uint8_t length = noah_lighting_snapshot_request(&snapshot, now);
    if (!length) return false;
    uint8_t  reply[NOAH_LIGHTING_SNAPSHOT_REPLY] = {0};
    uint32_t before = timer_read32();
    if (!transaction_rpc_exec(PUT_SPLIT_LIGHTING_EVENTS_SYNC, length, snapshot.link.packet.outgoing, sizeof reply, reply) ||
        reply[0] != (NOAH_LIGHTING_EVENT_REPLY | 0xa0u) || reply[1] < NOAH_LIGHTING_EVENT_PENDING || reply[1] > NOAH_LIGHTING_EVENT_HELLO_READY || reply[1] == NOAH_LIGHTING_EVENT_PROFILE_MISMATCH) {
        last_failure = timer_read32();
        retry_delay = !retry_delay ? SPLIT_RUNTIME_SYNC_RETRY_INITIAL_MS : retry_delay >= SPLIT_RUNTIME_SYNC_RETRY_MAX_MS / 2u ? SPLIT_RUNTIME_SYNC_RETRY_MAX_MS : retry_delay * 2u;
        return false;
    }
    retry_delay = 0u;
    uint32_t after = timer_read32();
    return noah_lighting_snapshot_reply(&snapshot, reply, before, after);
}
#endif
