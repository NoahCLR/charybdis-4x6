#include "qmk_lighting_event_transport.h"

#if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE && defined(SPLIT_TRANSACTION_IDS_USER)
#include "transactions.h"
#include <string.h>

static noah_lighting_event_link_t link;
_Static_assert(NOAH_LIGHTING_EVENT_DATA_BYTES <= RPC_M2S_BUFFER_SIZE, "event RPC request allocation");
_Static_assert(NOAH_LIGHTING_EVENT_REPLY_BYTES <= RPC_S2M_BUFFER_SIZE, "event RPC response allocation");

static void event_rpc(uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    if (!response || !response_size) return;
    memset(response, 0, response_size);
    if (response_size != NOAH_LIGHTING_EVENT_REPLY_BYTES || !link.ready || link.master || is_keyboard_master()) return;
    noah_lighting_event_receive(&link, request, request_size, response);
}
void noah_qmk_lighting_event_transport_init(const uint8_t profile[NOAH_LIGHTING_EVENT_PROFILE_BYTES]) {
    // Slave worker can preempt main; main cannot preempt an active callback.
    // Closing ready therefore quiesces that role before replacing its arm.
    link.ready = false;
    __asm__ __volatile__("" ::: "memory");
    bool master = is_keyboard_master();
    if (master) noah_lighting_event_sender_begin(&link, profile);
    else noah_lighting_event_link_begin(&link, false, profile);
    // sender_begin publishes its initialized arm; close registration gate until
    // transaction_register_rpc returns, including registration-time callbacks.
    link.ready = false;
    transaction_register_rpc(PUT_SPLIT_LIGHTING_EVENTS_SYNC, event_rpc);
    noah_lighting_event_link_publish(&link);
}
static __attribute__((noinline)) void offer_next(noah_lighting_event_next_fn next, void *context) {
    if (!link.active || link.mailbox_ready) return;
    noah_lighting_event_group_t group;
    if (next(context, &group)) (void)noah_lighting_event_sender_offer(&link, &group);
}
bool noah_qmk_lighting_event_transport_step(uint32_t now, noah_lighting_event_next_fn next, noah_lighting_event_consume_fn consume, noah_lighting_event_reset_fn reset, noah_lighting_event_ack_fn ack, void *context) {
    if (!link.ready || link.master != is_keyboard_master() || !reset) return false;
    if (!link.master) return noah_lighting_event_receive_main(&link, now, consume, reset, context);
    if (!next || !ack) return false;
    offer_next(next, context); // group workspace ends before synchronous RPC
    size_t size = noah_lighting_event_sender_size(&link);
    if (!size) return false;
    uint8_t response[NOAH_LIGHTING_EVENT_REPLY_BYTES];
    if (!transaction_rpc_exec(PUT_SPLIT_LIGHTING_EVENTS_SYNC, (uint8_t)size, link.packet.outgoing, sizeof response, response)) return false;
    return noah_lighting_event_sender_reply(&link, response, sizeof response, reset, ack, context);
}
#endif
