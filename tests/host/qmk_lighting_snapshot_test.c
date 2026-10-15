#include "users/noah/lib/compat/qmk_lighting_snapshot_transport.h"
#include "users/noah/lib/rgb/lighting/lighting_snapshot.h"
#include "transactions.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void peer_init(noah_lighting_working_t *,const uint8_t *);
void peer_bind(noah_lighting_facts_t *,noah_lighting_facts_t *);
bool peer_step(void);
void peer_close(void);
static bool master,lose_request,lose_reply;
static uint32_t clocks[2]={5000,100};
static slave_callback_t callback;
static noah_lighting_working_t banks[2];
static noah_lighting_facts_t staged[2],published;
static uint8_t memory[2][NOAH_LIGHTING_WORKING_CAPACITY];
static noah_profile_lighting_v1_program_t program;
bool is_keyboard_master(void){return master;}
uint32_t timer_read32(void){return clocks[master];}
bool noah_lighting_fact_next(uint8_t kind,const noah_lighting_facts_t *f,const uint8_t *p,uint16_t *c,noah_lighting_subject_t *s){(void)kind;(void)f;(void)p;(void)c;(void)s;assert(!"event-only fixture");return false;}
void transaction_register_rpc(int8_t id,slave_callback_t cb) {
    assert(id==PUT_SPLIT_LIGHTING_EVENTS_SYNC);
    uint8_t request[27]={0x81},reply[14];memset(reply,0xff,sizeof reply);
    cb(sizeof request,request,sizeof reply,reply);
    for(unsigned i=0;i<sizeof reply;i++)assert(!reply[i]);
    if(!master)callback=cb;
}
bool transaction_rpc_exec(int8_t id,uint8_t length,const void *request,uint8_t response_size,void *response) {
    assert(master && id==PUT_SPLIT_LIGHTING_EVENTS_SYNC && length<=64 && response_size==14 && callback);
    if(lose_request)return false;
    uint8_t before[sizeof memory[0]];memcpy(before,memory[0],sizeof before);
    clocks[1]+=2;clocks[0]+=2;master=false;
    callback(length,request,response_size,response);
    master=true;clocks[1]+=4;clocks[0]+=4;
    assert(!memcmp(before,memory[0],sizeof before)); // callback stages ONLY
    return !lose_reply;
}
static void scan(void) {
    master=true;noah_lighting_working_advance(&banks[1],clocks[1]);(void)noah_qmk_lighting_snapshot_step();
    master=false;clocks[0]++;clocks[1]++;(void)peer_step();
}
int main(void) {
    const uint8_t bytes[]={5,0,58,0,0,0,0,1,17,0,255,1,28,127,0x40,0,0x60,0,0xa2,1,200,0,0xa2,3,244,1,0};
    noah_profile_lighting_v1_view_t view;noah_profile_lighting_v1_limits_t limits=noah_profile_lighting_v1_default_limits();
    assert(noah_profile_lighting_v1_decode(bytes,sizeof bytes,&limits,&view,NULL)==NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view,&program));
    for(unsigned i=0;i<2;i++)assert(noah_lighting_working_begin(&banks[i],&program,memory[i],sizeof memory[i],1));
    uint8_t identity[22]={1};
    master=false;peer_init(&banks[0],identity);peer_bind(&staged[0],&published);
    master=true;noah_qmk_lighting_snapshot_init(&banks[1],identity);noah_qmk_lighting_snapshot_bind_facts(&staged[1],NULL);
    noah_lighting_working_advance(&banks[1],clocks[1]);
    noah_lighting_event_group_t event={.thing_kind=5,.thing=127};event.qualified[0]=1;
    assert(noah_lighting_working_event(&banks[1],&event));staged[1].terminator=0xa5;
    lose_request=true;scan();lose_request=false;
    lose_reply=true;for(unsigned i=0;i<3;i++)scan();lose_reply=false;
    for(unsigned i=0;i<80 && !published.terminator;i++)scan();
    assert(published.terminator==0xa5);
    noah_lighting_working_rule_t r;assert(noah_lighting_working_rule(&banks[0],0,&r));
    noah_lighting_lifetime_parameters_t timing={.show_after=200,.flash=500};
    // Actual adapter/clock callback evidence under nonzero asymmetric RTT.
    clocks[1]=400;clocks[0]=5300;
    for(unsigned i=0;i<2;i++)noah_lighting_working_advance(&banks[i],clocks[i]);
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&r,127),&timing));
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&banks[1],false,&r,127),&timing));
    clocks[1]=1000;clocks[0]=5900;
    for(unsigned i=0;i<2;i++)noah_lighting_working_advance(&banks[i],clocks[i]);
    assert(!noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&r,127),&timing));
    for(unsigned i=0;i<50;i++)scan(); // same-session late retry cannot resurrect
    assert(!noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&r,127),&timing));
    // A different published profile never consumes a staged old scene. Restore
    // agreement, reboot both roles, then cross the u32 timer wrap with a new
    // live occurrence and arbitrary peer offset under the logical-clock model.
    uint8_t other[22]={2};published.terminator=0;
    master=false;peer_init(&banks[0],other);peer_bind(&staged[0],&published);
    for(unsigned i=0;i<8;i++)scan();
    assert(!published.terminator);
    for(unsigned i=0;i<2;i++)assert(noah_lighting_working_begin(&banks[i],&program,memory[i],sizeof memory[i],2));
    clocks[1]=UINT32_MAX-150u;clocks[0]=clocks[1]+4900u;
    master=false;peer_init(&banks[0],other);peer_bind(&staged[0],&published);
    master=true;noah_qmk_lighting_snapshot_init(&banks[1],other);noah_qmk_lighting_snapshot_bind_facts(&staged[1],NULL);
    noah_lighting_working_advance(&banks[1],clocks[1]);
    assert(noah_lighting_working_event(&banks[1],&event));staged[1].terminator=0x5a;
    for(unsigned i=0;i<80 && !published.terminator;i++)scan();
    assert(published.terminator==0x5a);
    clocks[1]=149u;clocks[0]=clocks[1]+4900u;
    for(unsigned i=0;i<2;i++)noah_lighting_working_advance(&banks[i],clocks[i]);
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&r,127),&timing));
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&banks[1],false,&r,127),&timing));
    peer_close();master=true;noah_qmk_lighting_snapshot_close();
    puts("actual two-half QMK snapshot adapter: strong registration guards/stage-only callbacks/main commit/lost request+reply/causal nonzero asymmetric RTT/both-half positive/show/expiry/no late resurrection PASS (host logical clock model, not hardware)");
}
