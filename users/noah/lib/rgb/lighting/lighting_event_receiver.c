#include "lighting_event_receiver.h"
#include <string.h>
#define BARRIER() __asm__ __volatile__("" ::: "memory")

void noah_lighting_event_link_begin(noah_lighting_event_link_t *link, bool master, const uint8_t *profile) {
    link->ready = false;
    BARRIER();
    link->master = master;
    link->active = link->waiting_confirm = false;
    link->reset_requested = link->confirm_requested = link->mailbox_ready = false;
    link->committed = false;
    link->next_sequence = link->handshake_sequence = 0;
    memcpy(link->profile, profile, NOAH_LIGHTING_EVENT_PROFILE_BYTES);
    memset(&link->packet, 0, sizeof link->packet);
}
void noah_lighting_event_link_publish(noah_lighting_event_link_t *link) {
    BARRIER();
    link->ready = true;
}
static void respond(const noah_lighting_event_link_t *link, uint8_t status, uint32_t sequence, uint8_t *reply) {
    reply[0] = NOAH_LIGHTING_EVENT_REPLY;
    reply[1] = status;
    noah_lighting_event_put_u32(reply + 2, link->epoch);
    noah_lighting_event_put_u32(reply + 6, sequence);
}
static bool data_valid(const uint8_t *request) {
    uint8_t any = 0;
    for (uint8_t i = 0; i < 32; i++) any |= request[31u + i];
    return any && !(request[62] & 0x80u) && request[21] <= 3u && request[22] <= 1u;
}
void noah_lighting_event_receive(noah_lighting_event_link_t *link, const uint8_t *request, size_t size, uint8_t reply[NOAH_LIGHTING_EVENT_REPLY_BYTES]) {
    memset(reply, 0, NOAH_LIGHTING_EVENT_REPLY_BYTES);
    if (!link->ready || link->master || !request || !size) return;
    BARRIER();
    if (request[0] == NOAH_LIGHTING_EVENT_HELLO && size == NOAH_LIGHTING_EVENT_HELLO_BYTES) {
        uint32_t first = noah_lighting_event_get_u32(request + 23);
        if (memcmp(request + 1, link->profile, NOAH_LIGHTING_EVENT_PROFILE_BYTES)) {
            respond(link, NOAH_LIGHTING_EVENT_PROFILE_MISMATCH, 0, reply);
        } else if (link->waiting_confirm && !link->reset_requested && first == link->next_sequence) {
            respond(link, NOAH_LIGHTING_EVENT_HELLO_READY, link->next_sequence, reply);
        } else {
            // Do not overwrite a mailbox owned by main, even for reset.
            // This flag supersedes that packet at the next main boundary.
            link->handshake_sequence = first;
            link->reset_requested = true;
            respond(link, NOAH_LIGHTING_EVENT_PENDING, 0, reply);
        }
        return;
    }
    if (size < NOAH_LIGHTING_EVENT_CONTROL_BYTES) return;
    uint32_t epoch = noah_lighting_event_get_u32(request + 1);
    uint32_t sequence = noah_lighting_event_get_u32(request + 5);
    if (link->reset_requested || epoch != link->epoch || (!link->active && !link->waiting_confirm)) {
        respond(link, NOAH_LIGHTING_EVENT_NEED_HELLO, sequence, reply);
        return;
    }
    if (request[0] == NOAH_LIGHTING_EVENT_CONFIRM && size == NOAH_LIGHTING_EVENT_CONTROL_BYTES && sequence == link->next_sequence) {
        if (link->active && !link->confirm_requested) respond(link, NOAH_LIGHTING_EVENT_ACK, sequence, reply);
        else {
            link->confirm_requested = true;
            respond(link, NOAH_LIGHTING_EVENT_PENDING, sequence, reply);
        }
        return;
    }
    if (request[0] != NOAH_LIGHTING_EVENT_DATA || size != NOAH_LIGHTING_EVENT_DATA_BYTES || !data_valid(request)) {
        respond(link, NOAH_LIGHTING_EVENT_REJECT, sequence, reply);
        return;
    }
    if (!link->active || link->confirm_requested) {
        respond(link, NOAH_LIGHTING_EVENT_NEED_HELLO, sequence, reply);
        return;
    }
    if (link->committed && sequence == link->next_sequence - 1u) {
        respond(link, NOAH_LIGHTING_EVENT_ACK, sequence, reply);
        return;
    }
    if (link->mailbox_ready) {
        bool same = !memcmp(link->packet.mailbox, request, NOAH_LIGHTING_EVENT_DATA_BYTES);
        respond(link, same ? NOAH_LIGHTING_EVENT_PENDING : NOAH_LIGHTING_EVENT_REJECT, sequence, reply);
        return;
    }
    if (sequence != link->next_sequence) {
        respond(link, NOAH_LIGHTING_EVENT_REJECT, sequence, reply);
        return;
    }
    memcpy(link->packet.mailbox, request, NOAH_LIGHTING_EVENT_DATA_BYTES);
    BARRIER();
    link->mailbox_ready = true;
    respond(link, NOAH_LIGHTING_EVENT_PENDING, sequence, reply);
}
static void reset_main(noah_lighting_event_link_t *link, noah_lighting_event_reset_fn reset, void *context) {
    reset(context);
    link->epoch++;
    link->next_sequence = link->handshake_sequence;
    link->committed = false;
    link->active = false;
    link->waiting_confirm = true;
    link->confirm_requested = link->mailbox_ready = false;
    BARRIER();
    link->reset_requested = false;
}
bool noah_lighting_event_receive_main(noah_lighting_event_link_t *link, uint32_t now, noah_lighting_event_consume_fn consume, noah_lighting_event_reset_fn reset, void *context) {
    if (!link->ready || link->master || !consume || !reset) return false;
    BARRIER();
    if (link->reset_requested) {
        reset_main(link, reset, context);
        return true;
    }
    if (link->confirm_requested) {
        link->active = true;
        link->waiting_confirm = false;
        BARRIER();
        link->confirm_requested = false;
        return true;
    }
    if (!link->active || !link->mailbox_ready) return false;
    if (!consume(context, link->packet.mailbox + NOAH_LIGHTING_EVENT_CONTROL_BYTES, now)) return false;
    // A HELLO may preempt consume. Reset wins; no ACK of the old generation.
    if (link->reset_requested) {
        reset_main(link, reset, context);
        return true;
    }
    link->next_sequence++;
    link->committed = true;
    BARRIER();
    link->mailbox_ready = false;
    return true;
}
