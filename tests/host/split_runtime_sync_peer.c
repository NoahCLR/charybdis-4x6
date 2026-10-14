// A second real runtime-sync instance: sender and receiver have independent
// role-owned buffers and publication state, exactly as on the two RP2040s.
#include QMK_KEYBOARD_H
#include "transactions.h"
static slave_callback_t peer_lighting_callback;
static bool peer_is_master(void) { return false; }
static void peer_register_rpc(int8_t id, slave_callback_t callback) {
    if (id == PUT_SPLIT_LIGHTING_FACTS_SYNC) peer_lighting_callback = callback;
}
#define is_keyboard_master peer_is_master
#define transaction_register_rpc peer_register_rpc
#define split_runtime_sync_remote peer_remote
#define split_runtime_sync_init peer_init
#define split_runtime_sync_tick peer_tick
#define split_runtime_sync_elapsed peer_elapsed
#define split_runtime_sync peer_sync
#define split_runtime_sync_test_set_publish_seam peer_set_publish_seam
#define split_runtime_sync_debug_clock_snapshot peer_clock_snapshot
#define split_runtime_sync_debug_transport_snapshot peer_transport_snapshot
#include "users/noah/lib/split/runtime_sync.c"

void test_split_peer_reset(void) { peer_init(); }
void test_split_peer_deliver(const split_runtime_lighting_facts_packet_t *packet) {
    peer_lighting_callback(sizeof(*packet), packet, 0u, NULL);
}
bool test_split_peer_read(uint8_t *frame) { return split_runtime_sync_remote_read_lighting_facts(frame); }
