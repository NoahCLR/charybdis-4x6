#pragma once
#include "lighting_event_protocol.h"

// One role-disjoint packet: outgoing on master, immutable main-owned mailbox
// on receiver. Control/profile bytes are accounted separately, not history.
typedef struct {
    volatile uint32_t epoch;
    volatile uint32_t next_sequence;
    volatile uint32_t handshake_sequence;
    uint8_t profile[NOAH_LIGHTING_EVENT_PROFILE_BYTES];
    union { uint8_t outgoing[NOAH_LIGHTING_EVENT_DATA_BYTES]; uint8_t mailbox[NOAH_LIGHTING_EVENT_DATA_BYTES]; } packet;
    volatile bool ready, master, active, waiting_confirm;
    volatile bool reset_requested, confirm_requested, mailbox_ready;
    volatile bool committed;
} noah_lighting_event_link_t;

enum {
    NOAH_LIGHTING_EVENT_PENDING = 1,
    NOAH_LIGHTING_EVENT_ACK = 2,
    NOAH_LIGHTING_EVENT_NEED_HELLO = 3,
    NOAH_LIGHTING_EVENT_REJECT = 4,
    NOAH_LIGHTING_EVENT_PROFILE_MISMATCH = 5,
    NOAH_LIGHTING_EVENT_HELLO_READY = 6,
};

// Main closes readiness before role/profile mutation and RPC registration,
// then publishes it. Preserve epoch across quiescent profile/role reinit;
// physical boot initializes it to zero. It is not a unique boot nonce.
void noah_lighting_event_link_begin(noah_lighting_event_link_t *, bool master, const uint8_t *profile);
void noah_lighting_event_link_publish(noah_lighting_event_link_t *);
void noah_lighting_event_receive(noah_lighting_event_link_t *, const uint8_t *request, size_t request_size, uint8_t reply[NOAH_LIGHTING_EVENT_REPLY_BYTES]);
// Consumer copies/retains all needed bytes synchronously. false means no
// change and retry later; true commits the whole group, once, before ACK.
typedef bool (*noah_lighting_event_consume_fn)(void *, const uint8_t *group, uint32_t now);
typedef void (*noah_lighting_event_reset_fn)(void *);
bool noah_lighting_event_receive_main(noah_lighting_event_link_t *, uint32_t now, noah_lighting_event_consume_fn, noah_lighting_event_reset_fn, void *);

_Static_assert(sizeof(noah_lighting_event_link_t) <= 112u, "event transport control and role union stay explicitly bounded");
