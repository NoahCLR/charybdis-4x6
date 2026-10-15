#include "users/noah/lib/compat/qmk_lighting_event_transport.h"
#include "users/noah/lib/rgb/lighting/lighting_lifetime.h"
#include "users/noah/lib/rgb/lighting/lighting_retained_group.h"
#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include "transactions.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void                    peer_init(const uint8_t *profile);
bool                    peer_step(uint32_t now, noah_lighting_event_next_fn next, noah_lighting_event_consume_fn consume, noah_lighting_event_reset_fn reset, noah_lighting_event_ack_fn ack, void *context);
static bool             master, drop_request, drop_reply, accept_consume = true;
static slave_callback_t registered[2];
static unsigned         requests, applies, acknowledgements, seeds, resets;
// One history owner per emulated half. These are host-selected fixture banks,
// not a production allocation/capacity or an extra transport history copy.
typedef struct {
    noah_lighting_batch_arena_t arena;
    noah_lighting_retained_t    cell;
    uint8_t                     bytes[512];
    uint32_t                    last_time;
} history_owner_t;
static history_owner_t histories[2];
#define dirty noah_lighting_retained_dirty(&histories[1].cell)
static void clear_history(bool usb) {
    history_owner_t *owner = &histories[usb];
    noah_lighting_retained_discard(&owner->cell, &owner->arena);
    assert(!owner->arena.used);
    noah_lighting_batch_arena_init(&owner->arena, owner->bytes, sizeof owner->bytes);
    noah_lighting_retained_init(&owner->cell, 254, 127);
}
static uint64_t committed_order;
static uint32_t born;

static const noah_lighting_lifetime_parameters_t duration = {.flash = 500};
bool                                             is_keyboard_master(void) {
    return master;
}
void transaction_register_rpc(int8_t id, slave_callback_t callback) {
    assert(id == PUT_SPLIT_LIGHTING_EVENTS_SYNC);
    registered[master] = callback;
    // Registration-time callback cannot observe an open/uninitialized arm.
    uint8_t request[NOAH_LIGHTING_EVENT_HELLO_BYTES] = {NOAH_LIGHTING_EVENT_HELLO};
    uint8_t response[NOAH_LIGHTING_EVENT_REPLY_BYTES];
    memset(response, 0xaa, sizeof response);
    callback(sizeof request, request, sizeof response, response);
    for (unsigned i = 0; i < sizeof response; i++)
        assert(response[i] == 0);
}
bool transaction_rpc_exec(int8_t id, uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    assert(master && id == PUT_SPLIT_LIGHTING_EVENTS_SYNC && request_size <= 64 && response_size == 10);
    requests++;
    if (drop_request) return false;
    assert(registered[0]);
    // The actual RPC callback can preempt main/render, but stages ONLY. No
    // retained record/arena mutation is permitted before the main commit.
    noah_lighting_retained_t before = histories[0].cell;
    uint32_t                 used   = histories[0].arena.used;
    master                          = false;
    registered[0](request_size, request, response_size, response);
    master = true;
    assert(!memcmp(&before, &histories[0].cell, sizeof before) && histories[0].arena.used == used);
    return !drop_reply;
}
static bool prepare(void *context, const noah_lighting_event_group_t *group) {
    (void)context;
    assert(group->thing == 127);
    return true;
}
static noah_lighting_retained_t *record(void *context, uint8_t rule, uint8_t thing, noah_lighting_lifetime_parameters_t *parameters) {
    history_owner_t *owner = context;
    assert(rule == 254 && thing == 127);
    *parameters = duration;
    return &owner->cell;
}
static bool retain_group(history_owner_t *owner, const noah_lighting_event_group_t *group, bool source) {
    uint8_t encoded[NOAH_LIGHTING_GROUP_BATCH_BYTES];
    return noah_lighting_retained_group_apply(&owner->arena, group, source, encoded, prepare, record, owner);
}
static void advance_history(bool usb, uint32_t now) {
    history_owner_t *owner = &histories[usb];
    uint32_t         delta = now - owner->last_time;
    owner->last_time       = now;
    assert(noah_lighting_retained_observe(&owner->cell, &owner->arena, &duration, delta, NOAH_LIGHTING_LIFETIME_IDLE, 5, NULL, 0, 0, false));
}
static bool next(void *context, noah_lighting_event_group_t *group) {
    (void)context;
    assert(master);
    if (!dirty) return false;
    noah_lighting_owned_batch_reader_t reader;
    noah_lighting_subject_t            subject;
    assert(noah_lighting_owned_batch_begin(&histories[1].arena, noah_lighting_retained_batch(&histories[1].cell), &reader));
    assert(noah_lighting_owned_batch_next(&histories[1].arena, &reader, &subject) && !reader.remaining);
    *group = (noah_lighting_event_group_t){.order = noah_lighting_retained_order(&histories[1].cell), .thing_kind = subject.thing_kind, .thing = subject.thing, .slot = subject.slot, .outcome = subject.outcome, .sides = subject.sides, .has_keys = subject.has_keys};
    memcpy(group->owners, subject.keys, sizeof subject.keys);
    group->qualified[31] = 0x40;
    return true;
}
static bool consume(void *context, const uint8_t *body, uint32_t now) {
    (void)context;
    assert(!master);
    if (!accept_consume) return false;
    noah_lighting_event_group_t group;
    assert(noah_lighting_event_group_decode(&group, body, 54));
    applies++;
    committed_order = group.order;
    born            = now;
    assert(retain_group(&histories[0], &group, false));
    return true;
}
static void reset(void *context) {
    (void)context;
    if (master) {
        seeds++;
        noah_lighting_retained_seed(&histories[1].cell, &histories[1].arena);
    } else {
        resets++;
        clear_history(false);
        committed_order = 0;
    }
}
static void ack(void *context, const uint8_t *body) {
    (void)context;
    assert(master);
    acknowledgements++;
    noah_lighting_event_group_t sent;
    assert(noah_lighting_event_group_decode(&sent, body, 54));
    (void)noah_lighting_retained_ack(&histories[1].cell, &histories[1].arena, sent.order);
}
static bool step(bool usb, uint32_t now) {
    advance_history(usb, now);
    master = usb;
    return usb ? noah_qmk_lighting_event_transport_step(now, next, consume, reset, ack, NULL) : peer_step(now, next, consume, reset, ack, NULL);
}
static void handshake(uint32_t now) {
    for (unsigned i = 0; i < 6; i++) {
        step(true, now);
        step(false, now);
    }
}
static void occurrence(uint64_t order, uint32_t now) {
    advance_history(true, now);
    noah_lighting_event_group_t group = {.order = order, .thing_kind = 5, .thing = 127};
    group.qualified[31]               = 0x40;
    assert(retain_group(&histories[1], &group, true));
}
int main(void) {
    uint8_t profile[22] = {1, 2, 3};
    clear_history(false);
    clear_history(true);
    master = false;
    peer_init(profile);
    master = true;
    noah_qmk_lighting_event_transport_init(profile);
    handshake(0);
    assert(seeds == 1 && resets == 1 && applies == 0);
    occurrence(100, 100);
    drop_request = true;
    step(true, 100);
    assert(applies == 0 && dirty);
    drop_request = false;
    // Same session: source marker may expire locally while link cannot commit.
    step(true, 1000);
    assert(applies == 0);
    for (unsigned i = 0; i < 3; i++)
        step(true, 1000);
    assert(applies == 0); // no main => no ACK
    accept_consume = false;
    step(false, 1000);
    step(true, 1000);
    assert(applies == 0 && dirty);
    accept_consume = true;
    step(false, 1000);
    assert(applies == 1 && born == 1000 && committed_order == 100);
    assert(noah_lighting_retained_visible(&histories[0].cell, &duration));
    drop_reply = true;
    step(true, 1000);
    assert(dirty && applies == 1);
    drop_reply = false;
    occurrence(101, 1001); // immutable100 retry remains;101 must stay dirty after100 ACK
    step(true, 1001);
    assert(applies == 1 && dirty && acknowledgements == 1);
    step(true, 1002);
    step(false, 1002);
    assert(applies == 2 && committed_order == 101 && born == 1002);
    step(true, 1002);
    assert(!dirty && acknowledgements == 2);
    unsigned before = requests;
    step(true, 1003);
    assert(requests == before);
    // No role mismatch can consume the other arm or call another RPC.
    master = false;
    assert(!noah_qmk_lighting_event_transport_step(1004, next, consume, reset, ack, NULL));
    // Callback from the master registration also refuses a master-side receive.
    master = true;
    uint8_t response[10];
    memset(response, 0xaa, 10);
    registered[1](1, "x", 10, response);
    for (unsigned i = 0; i < 10; i++)
        assert(!response[i]);
    // Explicit peer profile reset refuses old profile masks. New matching main
    // init restores the session without replaying an already ACKed marker.
    uint8_t changed[22] = {9};
    clear_history(false);
    master = false;
    peer_init(changed);
    occurrence(102, 2000);
    for (unsigned i = 0; i < 6; i++) {
        step(true, 2000);
        step(false, 2000);
    }
    assert(applies == 2);
    clear_history(true);
    master = true;
    noah_qmk_lighting_event_transport_init(changed);
    handshake(2000);
    assert(applies == 2);
    assert(!noah_lighting_retained_visible(&histories[0].cell, &duration));
    clear_history(false);
    clear_history(true);
    puts("actual QMK event adapter+owned histories: main commit ACK, failed delivery, delayed local birth, immutable retry/newer marker, role/registration/profile reset passed");
}
