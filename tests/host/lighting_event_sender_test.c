#include "users/noah/lib/rgb/lighting/lighting_event_sender.h"
#include <assert.h>
#include <string.h>
static noah_lighting_event_link_t source,target;
static uint8_t profile[22],reply[10];
static unsigned seeds,commits,acks;
static uint64_t latest,acked;
static uint32_t born;
static void seed(void *ctx) { (void)ctx; seeds++; }
static void reset(void *ctx) { (void)ctx; commits=0; }
static bool consume(void *ctx, const uint8_t *body,uint32_t now) { (void)ctx;(void)body;commits++;born=now;return true; }
static void ack(void *ctx, const uint8_t *body) {
    (void)ctx;
    noah_lighting_event_group_t group;
    assert(noah_lighting_event_group_decode(&group,body,54));
    acked=group.order;
    if (group.order==latest) acks++;
}
static void rpc(bool deliver_reply) {
    noah_lighting_event_receive(&target,source.packet.outgoing,noah_lighting_event_sender_size(&source),reply);
    if (deliver_reply) (void)noah_lighting_event_sender_reply(&source,reply,sizeof reply,seed,ack,NULL);
}
static void main_at(uint32_t now) { (void)noah_lighting_event_receive_main(&target,now,consume,reset,NULL); }
static void connect(void) {
    for(unsigned i=0;i<8&&!source.active;i++) { rpc(true); main_at(0); }
    assert(source.active&&target.active);
}
int main(void) {
    noah_lighting_event_link_begin(&target,false,profile); noah_lighting_event_link_publish(&target);
    noah_lighting_event_sender_begin(&source,profile);
    connect(); assert(seeds==1);
    noah_lighting_event_group_t group={.order=1,.thing_kind=5,.thing=127,.sides=3,.qualified={1}};
    latest=1;
    assert(noah_lighting_event_sender_offer(&source,&group));
    uint8_t immutable[63];memcpy(immutable,source.packet.outgoing,63);
    rpc(false);rpc(true); // loss/pending: packet unchanged, no main/ACK
    assert(!commits&&!acks&&!memcmp(immutable,source.packet.outgoing,63));
    group.order=latest=2; // new occurrence while old packet waits
    assert(!noah_lighting_event_sender_offer(&source,&group));
    main_at(1000);assert(commits==1&&born==1000);
    rpc(false);main_at(1100);assert(commits==1&&born==1000); // lost ACK never restarts
    rpc(true);assert(acked==1&&!acks&&!source.mailbox_ready);
    assert(noah_lighting_event_sender_offer(&source,&group));rpc(true);main_at(1200);rpc(true);
    assert(acks==1&&commits==2&&born==1200);
    source.next_sequence=target.next_sequence=UINT32_MAX;
    group.order=latest=3;assert(noah_lighting_event_sender_offer(&source,&group));rpc(true);main_at(1300);rpc(true);
    assert(source.next_sequence==0&&target.next_sequence==0&&acks==2);
    // Plain disconnect: no response handler, immutable outgoing persists.
    group.order=latest=4;assert(noah_lighting_event_sender_offer(&source,&group));
    memcpy(immutable,source.packet.outgoing,63);assert(!memcmp(immutable,source.packet.outgoing,63));
    target.epoch=0;noah_lighting_event_link_begin(&target,false,profile);noah_lighting_event_link_publish(&target);
    rpc(true);assert(!source.active);connect();assert(seeds==2&&!commits&&!source.mailbox_ready);
    // Old expired DATA was invalidated, not offered after fresh session.
    noah_lighting_event_receive(&target,immutable,63,reply);assert(reply[1]==NOAH_LIGHTING_EVENT_REJECT||reply[1]==NOAH_LIGHTING_EVENT_NEED_HELLO);
}
