#include "lighting_event_sender.h"
#include <string.h>

static void hello(noah_lighting_event_link_t *link) {
    link->active = link->waiting_confirm = link->mailbox_ready = false;
    link->packet.outgoing[0] = NOAH_LIGHTING_EVENT_HELLO;
    memcpy(link->packet.outgoing + 1,link->profile,NOAH_LIGHTING_EVENT_PROFILE_BYTES);
    noah_lighting_event_put_u32(link->packet.outgoing + 23,link->next_sequence);
}
void noah_lighting_event_sender_begin(noah_lighting_event_link_t *link, const uint8_t *profile) {
    noah_lighting_event_link_begin(link,true,profile);
    hello(link);
    noah_lighting_event_link_publish(link);
}
size_t noah_lighting_event_sender_size(const noah_lighting_event_link_t *link) {
    if (!link->ready || !link->master) return 0;
    if (link->active) return link->mailbox_ready ? NOAH_LIGHTING_EVENT_DATA_BYTES : 0;
    return link->waiting_confirm ? NOAH_LIGHTING_EVENT_CONTROL_BYTES : NOAH_LIGHTING_EVENT_HELLO_BYTES;
}
bool noah_lighting_event_sender_offer(noah_lighting_event_link_t *link, const noah_lighting_event_group_t *group) {
    if (!link->ready || !link->master || !link->active || link->mailbox_ready) return false;
    if (!noah_lighting_event_group_encode(link->packet.outgoing + 9,54,group)) return false;
    link->packet.outgoing[0] = NOAH_LIGHTING_EVENT_DATA;
    noah_lighting_event_put_u32(link->packet.outgoing + 1,link->epoch);
    noah_lighting_event_put_u32(link->packet.outgoing + 5,link->next_sequence);
    link->mailbox_ready = true;
    return true;
}
bool noah_lighting_event_sender_reply(noah_lighting_event_link_t *link, const uint8_t *reply, size_t size, noah_lighting_event_reset_fn fresh_session, noah_lighting_event_ack_fn ack, void *context) {
    if (!link->ready || !link->master || !fresh_session || !ack || !reply || size != NOAH_LIGHTING_EVENT_REPLY_BYTES || reply[0] != NOAH_LIGHTING_EVENT_REPLY) return false;
    uint8_t status = reply[1];
    uint32_t epoch = noah_lighting_event_get_u32(reply + 2);
    uint32_t sequence = noah_lighting_event_get_u32(reply + 6);
    if (status == NOAH_LIGHTING_EVENT_NEED_HELLO) {
        link->next_sequence++; // new session cannot accept the old pending sequence
        hello(link);
        return true;
    }
    if (!link->active && !link->waiting_confirm && status == NOAH_LIGHTING_EVENT_HELLO_READY && sequence == link->next_sequence) {
        link->epoch = epoch;
        link->packet.outgoing[0] = NOAH_LIGHTING_EVENT_CONFIRM;
        noah_lighting_event_put_u32(link->packet.outgoing + 1,epoch);
        noah_lighting_event_put_u32(link->packet.outgoing + 5,sequence);
        link->waiting_confirm = true;
        return true;
    }
    if (status != NOAH_LIGHTING_EVENT_ACK || epoch != link->epoch) return false;
    if (!link->active && link->waiting_confirm && sequence == link->next_sequence) {
        fresh_session(context);
        link->waiting_confirm = false;
        link->active = true;
        return true;
    }
    if (!link->active || !link->mailbox_ready || sequence != link->next_sequence) return false;
    ack(context,link->packet.outgoing + 9);
    link->next_sequence++;
    link->mailbox_ready = false;
    return true;
}
