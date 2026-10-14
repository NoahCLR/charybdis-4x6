#include "users/noah/lib/rgb/lighting/lighting_event_receiver.h"
#include <assert.h>
#include <string.h>

static noah_lighting_event_link_t link;
static uint8_t hello[27] = {NOAH_LIGHTING_EVENT_HELLO}, reply[10];
static unsigned commits, resets;
static uint32_t committed_at;
static bool defer, reset_during_consume;
static void reset(void *ctx) { (void)ctx; resets++; commits = 0; }
static bool consume(void *ctx, const uint8_t *body, uint32_t now) {
    (void)ctx;
    if (defer) return false;
    assert(body[8] == 5 && body[9] == 127);
    commits++;
    committed_at = now;
    if (reset_during_consume) noah_lighting_event_receive(&link,hello,sizeof hello,reply);
    return true;
}
static void main_at(uint32_t now) { (void)noah_lighting_event_receive_main(&link,now,consume,reset,NULL); }
static void session(void) {
    noah_lighting_event_receive(&link,hello,sizeof hello,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_PENDING);
    main_at(0);
    noah_lighting_event_receive(&link,hello,sizeof hello,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_HELLO_READY);
    uint8_t confirm[9] = {NOAH_LIGHTING_EVENT_CONFIRM};
    noah_lighting_event_put_u32(confirm + 1,link.epoch);
    noah_lighting_event_receive(&link,confirm,sizeof confirm,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_PENDING);
    main_at(0);
    noah_lighting_event_receive(&link,confirm,sizeof confirm,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_ACK);
}
static void data(uint8_t *request, uint32_t seq) {
    memset(request,0,63);
    request[0] = NOAH_LIGHTING_EVENT_DATA;
    noah_lighting_event_put_u32(request + 1,link.epoch);
    noah_lighting_event_put_u32(request + 5,seq);
    noah_lighting_event_group_t group = {.order=7,.thing_kind=5,.thing=127,.sides=3,.qualified={1}};
    group.qualified[31] = 0x40;
    assert(noah_lighting_event_group_encode(request + 9,54,&group));
}
int main(void) {
    uint8_t request[63];
    noah_lighting_event_link_begin(&link,false,hello+1);
    noah_lighting_event_receive(&link,hello,sizeof hello,reply);
    assert(reply[0] == 0); // readiness precedes registration/publication
    noah_lighting_event_link_publish(&link);
    data(request,0);
    noah_lighting_event_receive(&link,request,sizeof request,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_NEED_HELLO);
    session();
    assert(resets == 1 && link.epoch == 1);
    uint8_t wrong[27]; memcpy(wrong,hello,sizeof wrong); wrong[1] = 1;
    noah_lighting_event_receive(&link,wrong,sizeof wrong,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_PROFILE_MISMATCH && link.active && !link.reset_requested);
    for (size_t size = 0; size < sizeof hello; size++) {
        noah_lighting_event_receive(&link,hello,size,reply);
        assert(!link.reset_requested && link.active);
    }
    data(request,0);
    noah_lighting_event_receive(&link,request,sizeof request,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_PENDING && !commits);
    noah_lighting_event_receive(&link,request,sizeof request,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_PENDING && !commits);
    defer=true; main_at(500); assert(!commits && link.mailbox_ready);
    defer=false; main_at(1000); assert(commits==1 && committed_at==1000);
    noah_lighting_event_receive(&link,request,sizeof request,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_ACK);
    main_at(1200); assert(commits==1 && committed_at==1000);
    uint8_t old[63]; memcpy(old,request,sizeof old);
    data(request,1);
    noah_lighting_event_receive(&link,request,sizeof request,reply);
    noah_lighting_event_receive(&link,old,sizeof old,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_ACK && commits == 1);
    uint8_t changed[63]; memcpy(changed,request,sizeof changed); changed[17] ^= 1u;
    noah_lighting_event_receive(&link,changed,sizeof changed,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_REJECT && !memcmp(link.packet.mailbox,request,sizeof request));
    main_at(1250); assert(commits == 2);
    data(request,3);
    noah_lighting_event_receive(&link,request,sizeof request,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_REJECT);
    link.next_sequence=UINT32_MAX;
    data(request,UINT32_MAX);
    noah_lighting_event_receive(&link,request,sizeof request,reply); main_at(1300);
    assert(link.next_sequence==0 && commits==3);
    noah_lighting_event_receive(&link,request,sizeof request,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_ACK && commits==3);
    data(request,0);
    noah_lighting_event_receive(&link,request,sizeof request,reply); main_at(1400);
    assert(link.next_sequence==1 && commits==4);
    reset_during_consume=true;
    data(request,1);
    noah_lighting_event_receive(&link,request,sizeof request,reply); main_at(1500);
    assert(!commits && resets==2 && !link.active && !link.mailbox_ready);
    reset_during_consume=false;
    // Reboot can reuse an epoch value: pre-HELLO DATA still rejects.
    link.epoch=0;
    noah_lighting_event_link_begin(&link,false,hello+1);
    noah_lighting_event_link_publish(&link);
    noah_lighting_event_receive(&link,request,sizeof request,reply);
    assert(reply[1] == NOAH_LIGHTING_EVENT_NEED_HELLO);
    session(); assert(link.epoch==1);
    noah_lighting_event_link_begin(&link,true,hello+1);
    noah_lighting_event_link_publish(&link);
    noah_lighting_event_receive(&link,request,sizeof request,reply);
    assert(reply[0]==0);
}
