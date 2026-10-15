#include "users/noah/lib/compat/qmk_lighting_snapshot_transport.h"
#include "users/noah/lib/rgb/lighting/lighting_snapshot.h"
#include "transactions.h"
#include "users/noah/lib/split/runtime_sync.h"
#include "users/noah/lib/rgb/lighting/kinds/fact_layer_active.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void peer_init(noah_lighting_working_t *,const uint8_t *);
void peer_bind(noah_lighting_facts_t *,noah_lighting_facts_t *);
bool peer_step(void);
void peer_close(void);
void peer_changed(void);
uint8_t peer_readiness(void);
void peer_base_init(void);
static bool use_base, shared_scene = true;
static unsigned zero_response_base, legacy_fact_calls;
static unsigned base_calls, base_wire;
static int readiness_override = -1;
static bool lose_base, omit_base_reply;
static slave_callback_t base_callback;
bool noah_lighting_runtime_shared_scene(void) {return shared_scene;}
// The legacy fact broadcaster is forbidden in retained-scene tests. These
// builders are required only for role initialization/disabled branch linkage.
void noah_lighting_facts_build_idle(uint8_t *p) {memset(p,0,NOAH_LIGHTING_SYNCED_BYTES);}
void noah_lighting_facts_build_synced(uint8_t *p,bool gated) {(void)gated;assert(!shared_scene);memset(p,0,NOAH_LIGHTING_SYNCED_BYTES);}
bool noah_lighting_facts_active(const uint8_t *p,bool gated) {(void)p;(void)gated;assert(!shared_scene);return false;}

_Static_assert(sizeof(noah_lighting_facts_t)==621u,"two-instance transport must use the real complete compiled facts");
static bool master,lose_request,lose_reply,bad_offset,bad_commit;
static unsigned rpc_calls,wire_bytes,rejections,full_begins,facts_begins,history_chunks;
static uint32_t clocks[2]={5000,100};
static uint32_t clock_fraction_us;
static uint64_t blocking_us;
static bool pending_input;
static uint8_t input_layers,input_keys;
static void advance_us(uint32_t us) {
    uint32_t total=clock_fraction_us+us;
    clocks[0]+=total/1000u;clocks[1]+=total/1000u;clock_fraction_us=total%1000u;
}
static uint32_t serial_us(unsigned bytes) {return (bytes*10u*1000000u+230399u)/230400u;}
// Pinned RP PIO enter_rx_state waits floor(11 bits) after each send. Charge
// its entire47us in addition to8N1 wire time (conservatively double-counting
// the last character), not as a measured physical upper bound on CPU/scheduler.
static uint32_t send_wait_us(unsigned sends) {return sends*(1000000u*11u/230400u);}

static slave_callback_t callback;
static noah_lighting_working_t banks[2];
static noah_lighting_facts_t staged[2],published;
static uint8_t memory[2][NOAH_LIGHTING_WORKING_CAPACITY];
static noah_profile_lighting_v1_program_t program;
bool is_keyboard_master(void){return master;}
uint32_t timer_read32(void){return clocks[master];}
bool noah_lighting_fact_next(uint8_t kind,const noah_lighting_facts_t *f,const uint8_t *p,uint16_t *c,noah_lighting_subject_t *s){if(kind==1)return noah_lighting_fact_layer_active_next(f,p,c,s);assert(!"unexpected fixture walker");return false;}
layer_state_t layer_state;
void transaction_register_rpc(int8_t id,slave_callback_t cb) {
    if(id==PUT_SPLIT_RUNTIME_BASE_SYNC) {
        uint8_t response=255;
        split_runtime_base_sync_packet_t packet={0};
        cb(sizeof packet,&packet,1,&response);
        assert(response==NOAH_LIGHTING_SCENE_NOT_READY); // init registration is closed
        if(!master)base_callback=cb;
        return;
    }
    if(id==PUT_SPLIT_LIGHTING_FACTS_SYNC)return;
    assert(id==PUT_SPLIT_LIGHTING_EVENTS_SYNC);
    uint8_t request[27]={0x81},reply[14];memset(reply,0xff,sizeof reply);
    cb(sizeof request,request,sizeof reply,reply);
    for(unsigned i=0;i<sizeof reply;i++)assert(!reply[i]);
    if(!master)callback=cb;
}
bool transaction_rpc_exec(int8_t id,uint8_t length,const void *request,uint8_t response_size,void *response) {
    if(id==PUT_SPLIT_RUNTIME_BASE_SYNC) {
        assert(master && base_callback && response_size==1 && length==sizeof(split_runtime_base_sync_packet_t));
        base_calls++;base_wire+=length+18u;
        uint32_t outward=serial_us(length+14u)+send_wait_us(9u);
        uint32_t returning=serial_us(4u)+send_wait_us(3u);
        blocking_us+=outward+returning;
        if(lose_base) {advance_us(outward+returning+5000u);blocking_us+=5000u;return false;}
        advance_us(outward);master=false;
        if(!omit_base_reply)base_callback(length,request,response_size,response);
        master=true;advance_us(returning);
        if(readiness_override>=0)*(uint8_t *)response=(uint8_t)readiness_override;
        return true;
    }
    assert(master && id==PUT_SPLIT_LIGHTING_EVENTS_SYNC && length<=64 && response_size==14 && callback);
    rpc_calls++;wire_bytes+=length+31u; // four pinned QMK id/handshakes + metadata4 + execute1 + CRC4 + reply14
    uint32_t outward=serial_us(length+14u)+send_wait_us(9u);
    uint32_t returning=serial_us(17u)+send_wait_us(3u);
    blocking_us+=outward+returning;
    if(lose_request){advance_us(outward+returning+5000u);blocking_us+=5000u;return false;}
    uint8_t request_copy[64];memcpy(request_copy,request,length);
    const uint8_t *bytes=request;
    if(bytes[0]==0xa3 && bytes[9]==0){if(bytes[28] & 2u)facts_begins++;else full_begins++;}
    if(bytes[0]==0xa3 && bytes[9]==1 && bytes[12])history_chunks++;
    if(bad_offset && bytes[0]==0xa3 && bytes[9]==1){request_copy[10]=0xff;request_copy[11]=0xff;request=request_copy;bad_offset=false;}

    uint8_t before[sizeof memory[0]];memcpy(before,memory[0],sizeof before);
    advance_us(outward);master=false;
    callback(length,request,response_size,response);
    master=true;advance_us(returning);
    assert(!memcmp(before,memory[0],sizeof before)); // callback stages ONLY
    if(bad_commit && bytes[0]==0xa3 && bytes[9]==2) {
        memory[0][(banks[0].current^1u)*banks[0].bank_bytes+12u*banks[0].rules+4u]=0xff;
        bad_commit=false;
    }
    if(((uint8_t *)response)[1]==NOAH_LIGHTING_EVENT_REJECT)rejections++;
    if(lose_reply){advance_us(5000u);blocking_us+=5000u;}
    return !lose_reply;
}
bool transaction_rpc_send(int8_t id,uint8_t n,const void *p) {
    assert(!shared_scene);
    if(id==PUT_SPLIT_RUNTIME_BASE_SYNC) {
        assert(n==sizeof(split_runtime_base_sync_packet_t));zero_response_base++;
        master=false;base_callback(n,p,0,NULL);master=true;
    } else {assert(id==PUT_SPLIT_LIGHTING_FACTS_SYNC);legacy_fact_calls++;}
    return true;
}
static void scan(void) {
    master=true;noah_lighting_working_advance(&banks[1],clocks[1]);
    if(pending_input && !banks[1].snapshot_ready) {
        pending_input=false;staged[1].layer_active[0]=input_layers;staged[1].key_pressed[0]=input_keys;
        noah_lighting_facts_dirty_range(noah_qmk_lighting_snapshot_fact_changes(),offsetof(noah_lighting_facts_t,layer_active),2);
        noah_lighting_facts_dirty_range(noah_qmk_lighting_snapshot_fact_changes(),offsetof(noah_lighting_facts_t,key_pressed),KEY_ORIGIN_BITMAP_SIZE);
        noah_qmk_lighting_snapshot_changed();assert(noah_lighting_working_observe(&banks[1],&staged[1]));
    }
    if(use_base)split_runtime_sync_tick();
    (void)noah_qmk_lighting_snapshot_step();
    advance_us(1000u); // explicit host main scheduling, not measured target time
    master=false;noah_lighting_working_advance(&banks[0],clocks[0]);(void)peer_step();
}
static void normal_feedback(unsigned duration, bool mixed) {
    uint8_t bytes[]={5,0,58,0,0,0,0,3,
        13,0,255,1,28,127,0x40,4,0x60,0,0xa2,3,255,255,
        14,0,255,2,25,255,255,0x40,4,0x60,0,0xa2,3,255,255,
        13,0,255,1,1,255,0x40,4,0x60,0,0xa2,2,255,255,0};
    for(unsigned i=0;i+3<sizeof bytes;i++)if(bytes[i]==0xa2){bytes[i+2]=(uint8_t)duration;bytes[i+3]=(uint8_t)(duration>>8);}
    unsigned used=sizeof bytes;
    if(!mixed){bytes[7]=1;used=8u+1u+bytes[8];bytes[used++]=0;}
    noah_profile_lighting_v1_view_t view;noah_profile_lighting_v1_limits_t limits=noah_profile_lighting_v1_default_limits();
    assert(noah_profile_lighting_v1_decode(bytes,used,&limits,&view,NULL)==NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view,&program));
    for(unsigned i=0;i<2;i++)assert(noah_lighting_working_begin(&banks[i],&program,memory[i],sizeof memory[i],duration));
    memset(staged,0,sizeof staged);memset(&published,0,sizeof published);staged[1].layer_active[0]=1;staged[1].layer_previewing[0]=255;
    noah_lighting_working_advance(&banks[1],clocks[1]);assert(noah_lighting_working_observe(&banks[1],&staged[1]));
    uint8_t identity[22]={4,(uint8_t)duration,(uint8_t)(duration>>8)};
    master=false;peer_init(&banks[0],identity);peer_bind(&staged[0],&published);
    master=true;noah_qmk_lighting_snapshot_init(&banks[1],identity);noah_qmk_lighting_snapshot_bind_facts(&staged[1],NULL);
    for(unsigned i=0;i<120;i++)scan(); // settle the complete baseline before input
    unsigned start=clocks[1],calls=rpc_calls,wire=wire_bytes,first=UINT32_MAX,last=0,off=0;
    uint64_t blocking=blocking_us;
    noah_lighting_event_group_t macro={.thing_kind=5,.thing=127,.qualified={1}};
    noah_lighting_event_group_t behaviour={.thing_kind=4,.thing=7,.slot=2,.sides=1,.has_keys=1,.owners={1},.qualified={2}};
    assert(noah_lighting_working_event(&banks[1],&macro));if(mixed)assert(noah_lighting_working_event(&banks[1],&behaviour));
    bool changed_pending=false,repeated=false;unsigned repeat_at=0;
    noah_lighting_lifetime_parameters_t timing={.flash=duration};
    noah_lighting_working_rule_t rule;assert(noah_lighting_working_rule(&banks[0],0,&rule));
    for(unsigned i=0;i<1000 && clocks[1]-start<duration*2u+100u;i++) {
        if(banks[1].snapshot_ready && !changed_pending) {pending_input=true;input_layers=3;input_keys=1;changed_pending=true;}
        if(!repeated && clocks[1]-start>=duration/2u) {
            repeated=true;repeat_at=clocks[1]-start;macro.slot=3;
            assert(noah_lighting_working_event(&banks[1],&macro));
            noah_lighting_event_group_t nonqualifying=macro;memset(nonqualifying.qualified,0,sizeof nonqualifying.qualified);
            unsigned revision=banks[1].revision;assert(noah_lighting_working_event(&banks[1],&nonqualifying));assert(banks[1].revision==revision);
        }
        scan();
        bool visible=noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&rule,127),&timing);
        if(visible){if(first==UINT32_MAX)first=clocks[1]-start;last=clocks[1]-start;}
        else if(first!=UINT32_MAX && !off && clocks[1]-start>repeat_at+duration)off=clocks[1]-start;
        if(clocks[1]-start>=repeat_at+duration)assert(!visible);
    }
    assert(changed_pending && repeated && first<duration && last>=duration && off);
    assert(published.key_pressed[0]==1 && published.layer_active[0]==3);
    printf("SHORT %s / %ums flash+linger: first remote visible%u, last%u, off%u, qualifying repeat%u; %u RPC/%u wireB/%llu us scheduled blocking (230400/8N1+PIO waits; logical-model only)\n",mixed?"useful8220 profile":"named macro",duration,first,last,off,repeat_at,rpc_calls-calls,wire_bytes-wire,(unsigned long long)(blocking_us-blocking));
    unsigned full=full_begins,facts=facts_begins;calls=rpc_calls;wire=wire_bytes;start=clocks[1];
    // Changed-to-zero key data on an ACKed baseline sends that8-byte slice,
    // preserving all unrelated621-frame slices and every live retained tail.
    staged[1].key_pressed[0]=0;master=true;noah_lighting_facts_dirty_range(noah_qmk_lighting_snapshot_fact_changes(),offsetof(noah_lighting_facts_t,key_pressed),KEY_ORIGIN_BITMAP_SIZE);noah_qmk_lighting_snapshot_changed();
    for(unsigned i=0;i<100 && published.key_pressed[0];i++)scan();
    assert(!published.key_pressed[0] && full_begins==full && facts_begins>facts);
    printf("ordinary key update after settle: %u RPC/%u wireB/%ums source-to-commit,8 fact bytes, unchanged history not transferred\n",rpc_calls-calls,wire_bytes-wire,clocks[1]-start);
    scan();scan();
}
static void standard_pulse(void) {
    // A retained profile is active, but this is Standard's unchanged serialized
    // feedback pulse (configured default200ms), not a generic flash surrogate.
    const unsigned duration=KEY_FEEDBACK_FLASH_HALF_PERIOD_MS;
    unsigned start=clocks[1],first=UINT32_MAX,last=0,ended=0,calls=rpc_calls,wire=wire_bytes;
    keypos_t key={0,0};
    key_origin_bitmap_fill_single(staged[1].behaviour_state,key);
    key_feedback_semantic_map_set(staged[1].behaviour_state+KEY_ORIGIN_BITMAP_SIZE,key,KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED);
    master=true;noah_lighting_facts_dirty_range(noah_qmk_lighting_snapshot_fact_changes(),offsetof(noah_lighting_facts_t,behaviour_state),sizeof staged[1].behaviour_state);noah_qmk_lighting_snapshot_changed();
    bool stopped=false;
    while(clocks[1]-start<duration+200u) {
        if(!stopped && clocks[1]-start>=duration && !banks[1].snapshot_ready) {
            stopped=true;memset(staged[1].behaviour_state,0,sizeof staged[1].behaviour_state);
            master=true;noah_lighting_facts_dirty_range(noah_qmk_lighting_snapshot_fact_changes(),offsetof(noah_lighting_facts_t,behaviour_state),sizeof staged[1].behaviour_state);noah_qmk_lighting_snapshot_changed();
        }
        scan();
        bool visible=key_origin_bitmap_has_keypos(published.behaviour_state,key) && key_feedback_semantic_map_get(published.behaviour_state+KEY_ORIGIN_BITMAP_SIZE,key)==KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED;
        if(visible){if(first==UINT32_MAX)first=clocks[1]-start;last=clocks[1]-start;}
        else if(first!=UINT32_MAX && !ended)ended=clocks[1]-start;
    }
    assert(first<duration && stopped && ended && last>=duration);
    printf("Standard configured%ums serialized feedback pulse: peer on%u..%u, off%u (ordinary state-link latency), %u RPC/%u wireB; no new expiry/physical guarantee\n",duration,first,last,ended,rpc_calls-calls,wire_bytes-wire);
}
static void useful_mix(unsigned macro_duration) {
    uint8_t bytes[]={5,0,58,0,0,0,0,3,
        13,0,255,1,28,127,0x40,4,0x60,0,0xa2,3,255,255,
        14,0,255,2,25,255,255,0x40,4,0x60,0,0xa2,3,255,255,
        13,0,255,1,1,255,0x40,4,0x60,0,0xa2,2,255,255,0};
    bytes[20]=(uint8_t)macro_duration;bytes[21]=(uint8_t)(macro_duration>>8);
    noah_profile_lighting_v1_view_t view;noah_profile_lighting_v1_limits_t limits=noah_profile_lighting_v1_default_limits();
    assert(noah_profile_lighting_v1_decode(bytes,sizeof bytes,&limits,&view,NULL)==NOAH_PROFILE_LIGHTING_V1_OK);
    noah_lighting_working_cost_t cost;assert(noah_profile_lighting_v1_working_admit(&view,8220,&cost)==NOAH_PROFILE_LIGHTING_V1_OK && cost.required==8220);
    assert(noah_profile_lighting_v1_program_load(&view,&program));
    for(unsigned i=0;i<2;i++)assert(noah_lighting_working_begin(&banks[i],&program,memory[i],sizeof memory[i],3));
    clocks[1]=1000;clocks[0]=5900;memset(staged,0,sizeof staged);memset(&published,0,sizeof published);
    noah_lighting_working_advance(&banks[1],clocks[1]);
    for(unsigned t=0;t<128;t++) {
        noah_lighting_event_group_t event={.thing_kind=4,.thing=t,.slot=t%8,.outcome=t%9,.sides=3,.has_keys=1,.qualified={2}};
        event.owners[t%8]=(uint8_t)(1u<<(t%8));
        if(t%8==7)event.owners[7]&=15u;
        if(!event.owners[t%8])event.owners[0]=1;
        assert(noah_lighting_working_event(&banks[1],&event));
    }
    staged[1].layer_active[0]=staged[1].layer_active[1]=255;
    assert(noah_lighting_working_observe(&banks[1],&staged[1]));
    noah_lighting_event_group_t macro={.thing_kind=5,.thing=127,.qualified={1}};
    if(macro_duration==65535)assert(noah_lighting_working_event(&banks[1],&macro));staged[1].layer_previewing[0]=0x4c;
    uint8_t identity[22]={3};
    master=false;peer_init(&banks[0],identity);peer_bind(&staged[0],&published);
    master=true;noah_qmk_lighting_snapshot_init(&banks[1],identity);noah_qmk_lighting_snapshot_bind_facts(&staged[1],NULL);
    unsigned before=rpc_calls,wire_before=wire_bytes,start=clocks[1];
    for(unsigned i=0;i<1000 && !published.layer_previewing[0];i++)scan();
    assert(published.layer_previewing[0]==0x4c);
    noah_lighting_working_advance(&banks[0],clocks[0]);
    for(unsigned i=0;i<3;i++) {
        if(!i && macro_duration!=65535)continue;
        noah_lighting_working_rule_t rule;assert(noah_lighting_working_rule(&banks[0],i,&rule));
        noah_lighting_lifetime_parameters_t timing={.flash=65535,.linger=65535};
        for(unsigned t=0;t<rule.count;t++)assert(noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&rule,rule.count==1?127:t),&timing));
    }
    printf("8220 useful mix %u occupied subjects / real621 facts: %u RPC, %u wire bytes, %u logical ms to coherent positive peer; model schedule, not hardware\n",macro_duration==65535?145u:144u,rpc_calls-before,wire_bytes-wire_before,clocks[1]-start);
    scan();scan();
    if(macro_duration==500) {
        unsigned start=clocks[1],calls=rpc_calls,wire=wire_bytes;
        noah_lighting_working_rule_t r;assert(noah_lighting_working_rule(&banks[0],0,&r));
        noah_lighting_lifetime_parameters_t timing={.flash=500};
        assert(noah_lighting_working_event(&banks[1],&macro));
        for(unsigned i=0;i<100 && !noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&r,127),&timing);i++)scan();
        assert(noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&r,127),&timing));
        assert(clocks[1]-start<200);
        printf("warm144 unrelated histories + NEW named500: first positive peer%ums/%uRPC/%uwireB, fixed8220/real621; no unchanged history resend\n",clocks[1]-start,rpc_calls-calls,wire_bytes-wire);
        while(clocks[1]-start<501u)scan();
        assert(!noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&r,127),&timing));
        return;
    }
    // One new occurrence against the acknowledged fully occupied baseline
    // sends one exact calendar/payload and compact order-only changes. Other
    // independent clocks and whole owners must not restart or get recopied.
    unsigned delta_calls=rpc_calls,delta_wire=wire_bytes,delta_start=clocks[1];
    noah_lighting_working_rule_t behaviour_rule;assert(noah_lighting_working_rule(&banks[0],1,&behaviour_rule));
    const uint8_t *old_other=noah_lighting_working_slot(&banks[0],false,&behaviour_rule,126);
    uint16_t old_age=(uint16_t)old_other[0]|(uint16_t)old_other[1]<<8;
    noah_lighting_event_group_t repeat={.thing_kind=4,.thing=0,.slot=7,.sides=1,.has_keys=1,.owners={2},.qualified={2}};
    unsigned committed=banks[0].revision;
    assert(noah_lighting_working_event(&banks[1],&repeat));
    for(unsigned i=0;i<120 && banks[0].revision==committed;i++)scan();
    assert(banks[0].revision>committed);
    const uint8_t *new_other=noah_lighting_working_slot(&banks[0],false,&behaviour_rule,126);
    assert(((uint16_t)new_other[0]|(uint16_t)new_other[1]<<8)>=old_age);
    noah_lighting_subject_t subject;const uint8_t *updated=noah_lighting_working_slot(&banks[0],false,&behaviour_rule,0);
    noah_lighting_working_member(updated+12,4,127,&subject,NULL);assert(subject.slot==7 && subject.keys[0]==2);
    assert(rpc_calls-delta_calls<30u && clocks[1]-delta_start<200u);
    printf("warm145-subject baseline one repeat +127 changed entry ranks: %u RPC/%u wireB/%ums commit; unrelated lifetime preserved\n",rpc_calls-delta_calls,wire_bytes-delta_wire,clocks[1]-delta_start);
    scan();scan();
    // Continuous qualifying input coalesces current while the immutable old
    // cut progresses; old ACK cannot clear the newer current occurrence.
    unsigned peer_revision=banks[0].revision;
    for(unsigned i=0;i<1200;i++) {
        if(i%4==0){macro.slot=(i/4)%8;assert(noah_lighting_working_event(&banks[1],&macro));}
        scan();
    }
    assert(banks[0].revision>=peer_revision+3);
    for(unsigned i=0;i<1000;i++)scan();
    unsigned idle=rpc_calls;for(unsigned i=0;i<100;i++)scan();assert(rpc_calls==idle);
    // Unchanged bulk history is an acknowledged baseline, not sent again for
    // an ordinary fact-only change. Current histories continue local aging.
    unsigned full=full_begins,facts=facts_begins;
    staged[1].layer_previewing[0]=0x4d;master=true;noah_lighting_facts_dirty_range(noah_qmk_lighting_snapshot_fact_changes(),offsetof(noah_lighting_facts_t,layer_previewing),1);noah_qmk_lighting_snapshot_changed();
    for(unsigned i=0;i<100 && published.layer_previewing[0]!=0x4d;i++)scan();
    assert(published.layer_previewing[0]==0x4d && full_begins==full && facts_begins>facts);
}
static void quiet_reboot_recovery(void) {
    normal_feedback(500,true);
    uint8_t identity[22]={4,244,1};
    master=false;peer_base_init();master=true;use_base=true;split_runtime_sync_init();
    for(unsigned i=0;i<120;i++)scan();
    unsigned idle=rpc_calls;for(unsigned i=0;i<2100;i++)scan();
    assert(rpc_calls==idle && base_calls>0 && peer_readiness()==NOAH_LIGHTING_SCENE_READY);
    assert(published.layer_active[0]==3);
    unsigned before_base=base_calls,before_rpc=rpc_calls;
    // Same-profile, peer-only reboot, independent offset/clock reset. Source
    // stays entirely quiet. No event/journal/new full flash window is created.
    master=false;peer_close();assert(peer_readiness()==NOAH_LIGHTING_SCENE_NOT_READY);
    assert(noah_lighting_working_begin(&banks[0],&program,memory[0],sizeof memory[0],500));
    clocks[0]=33;memset(&published,0,sizeof published);peer_init(&banks[0],identity);peer_bind(&staged[0],&published);peer_base_init();
    for(unsigned i=0;i<2400 && published.layer_active[0]!=3;i++)scan();
    assert(published.layer_active[0]==3 && base_calls>before_base && rpc_calls>before_rpc);
    for(unsigned i=0;i<20;i++)scan();assert(peer_readiness()==NOAH_LIGHTING_SCENE_READY);
    // Heartbeat NOT_READY while a resync is pending cannot reset progress.
    readiness_override=NOAH_LIGHTING_SCENE_NOT_READY;
    master=true;split_runtime_sync();
    unsigned full=full_begins;
    for(unsigned i=0;i<120;i++) {
        master=true;split_runtime_sync();scan();
        if(full_begins>full && peer_readiness()==NOAH_LIGHTING_SCENE_READY)break;
    }
    assert(full_begins==full+2u && peer_readiness()==NOAH_LIGHTING_SCENE_READY); // PENDING then committed ACK of one BEGIN
    readiness_override=-1;
    for(unsigned i=0;i<120;i++)scan();
    // Unknown/missing mixed-peer reply invalidates once, remains diagnosable
    // via existing transport backoff, and cannot spin one RPC per scan.
    for(unsigned fault=0;fault<3;fault++) {
        if(fault==0)readiness_override=0x81;
        if(fault==1)omit_base_reply=true;
        if(fault==2)lose_base=true;
        before_base=base_calls;master=true;split_runtime_sync();
        for(unsigned i=0;i<400;i++)scan();
        assert(base_calls-before_base<12u);
        readiness_override=-1;omit_base_reply=lose_base=false;
        for(unsigned i=0;i<1400;i++)scan();
        assert(peer_readiness()==NOAH_LIGHTING_SCENE_READY && published.layer_active[0]==3);
    }
    // A state change after recovery uses the coherent acknowledged baseline.
    pending_input=true;input_layers=5;input_keys=2;
    for(unsigned i=0;i<160 && published.layer_active[0]!=5;i++)scan();
    assert(published.layer_active[0]==5 && published.key_pressed[0]==2);
    for(unsigned i=0;i<650;i++)scan();idle=rpc_calls; // settle the actual500ms departed layer tail too
    for(unsigned i=0;i<2400;i++)scan();assert(rpc_calls==idle);
    // Profile mismatch/role reinitialization closes readiness and does not
    // consume a stale generation. A subsequent matching session recovers.
    uint8_t other[22]={9};
    master=false;peer_init(&banks[0],other);peer_bind(&staged[0],&published);
    assert(peer_readiness()==NOAH_LIGHTING_SCENE_NOT_READY);
    before_base=base_calls;master=true;split_runtime_sync();
    for(unsigned i=0;i<1400;i++)scan();
    assert(peer_readiness()==NOAH_LIGHTING_SCENE_NOT_READY && base_calls-before_base<12u);
    master=false;peer_init(&banks[0],identity);peer_bind(&staged[0],&published);
    for(unsigned i=0;i<1600;i++)scan();
    assert(peer_readiness()==NOAH_LIGHTING_SCENE_READY);
    master=false;noah_qmk_lighting_snapshot_init(&banks[1],identity);
    assert(noah_qmk_lighting_snapshot_readiness()==NOAH_LIGHTING_SCENE_NOT_READY);
    master=true;noah_qmk_lighting_snapshot_init(&banks[1],identity);noah_qmk_lighting_snapshot_bind_facts(&staged[1],NULL);
    for(unsigned i=0;i<200;i++)scan();
    assert(peer_readiness()==NOAH_LIGHTING_SCENE_READY);
    printf("ACTUAL BASE heartbeat readiness: quiet same-profile peer reboot/clock reset, once-only resync, pending/unknown/absent/lost/repeated reply, subsequent delta, idle0 lightingRPC PASS; %u base RPC/%u wireB (1B reply within existing allocation)\n",base_calls,base_wire);
    // Capture-enabled capability alone does not add a response: a program
    // without retained constructions takes the original base/fact path.
    unsigned no_response=zero_response_base,old_facts=legacy_fact_calls;
    shared_scene=false;master=true;split_runtime_sync();
    assert(zero_response_base==no_response+1u && legacy_fact_calls>old_facts);
    shared_scene=true;
    use_base=false;
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
    assert(noah_lighting_working_event(&banks[1],&event));staged[1].layer_previewing[0]=0xa5;
    lose_request=true;scan();lose_request=false;
    lose_reply=true;for(unsigned i=0;i<3;i++)scan();lose_reply=false;
    for(unsigned i=0;i<400 && !published.layer_previewing[0];i++)scan();
    assert(published.layer_previewing[0]==0xa5);
    // Old unreleased snapshot peers fail closed even after registration.
    uint8_t mixed_request[10]={0x83},mixed_reply[14];
    master=false;callback(sizeof mixed_request,mixed_request,sizeof mixed_reply,mixed_reply);
    assert(mixed_reply[1]!=NOAH_LIGHTING_EVENT_ACK);master=true;
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
    for(unsigned i=0;i<200;i++)scan();
    unsigned settled=rpc_calls;
    for(unsigned i=0;i<200;i++)scan();
    assert(rpc_calls==settled); // no idle blocking RPC, even with timed rules
    unsigned old_full=full_begins,old_facts=facts_begins;
    staged[1].layer_previewing[0]=0x6b;master=true;noah_lighting_facts_dirty_range(noah_qmk_lighting_snapshot_fact_changes(),offsetof(noah_lighting_facts_t,layer_previewing),1);noah_qmk_lighting_snapshot_changed();
    for(unsigned i=0;i<200 && published.layer_previewing[0]!=0x6b;i++)scan();
    assert(published.layer_previewing[0]==0x6b && facts_begins>old_facts && full_begins==old_full);
    // Failed staging and failed whole-bank commit never ACK/publish their cut;
    // the actual endpoint reports REJECT and resumes through a fresh session.
    for(unsigned fault=0;fault<2;fault++) {
        unsigned rejected=rejections;
        staged[1].layer_previewing[0]=(uint8_t)(0x70+fault);
        noah_lighting_working_advance(&banks[1],clocks[1]);
        assert(noah_lighting_working_event(&banks[1],&event));
        if(fault)bad_commit=true;else bad_offset=true;
        master=true;noah_lighting_facts_dirty_range(noah_qmk_lighting_snapshot_fact_changes(),offsetof(noah_lighting_facts_t,layer_previewing),1);noah_qmk_lighting_snapshot_changed();
        for(unsigned i=0;i<400 && published.layer_previewing[0]!=(uint8_t)(0x70+fault);i++)scan();
        assert(!bad_offset && !bad_commit && rejections>rejected);
        assert(published.layer_previewing[0]==(uint8_t)(0x70+fault));
        scan();scan(); // finish the immutable COMMIT ACK before the next injected transaction
    }
    printf("two-instance real621 facts: idle0 RPC, compact complete/facts-only baseline, stage+commit rejection/resync; %u RPC / %u wire bytes (host schedule)\n",rpc_calls,wire_bytes);
    // A different published profile never consumes a staged old scene. Restore
    // agreement, reboot both roles, then cross the u32 timer wrap with a new
    // live occurrence and arbitrary peer offset under the logical-clock model.
    uint8_t other[22]={2};published.layer_previewing[0]=0;
    master=false;peer_init(&banks[0],other);peer_bind(&staged[0],&published);
    for(unsigned i=0;i<8;i++)scan();
    assert(!published.layer_previewing[0]);
    for(unsigned i=0;i<2;i++)assert(noah_lighting_working_begin(&banks[i],&program,memory[i],sizeof memory[i],2));
    clocks[1]=UINT32_MAX-150u;clocks[0]=clocks[1]+4900u;
    master=false;peer_init(&banks[0],other);peer_bind(&staged[0],&published);
    master=true;noah_qmk_lighting_snapshot_init(&banks[1],other);noah_qmk_lighting_snapshot_bind_facts(&staged[1],NULL);
    noah_lighting_working_advance(&banks[1],clocks[1]);
    assert(noah_lighting_working_event(&banks[1],&event));staged[1].layer_previewing[0]=0x5a;
    for(unsigned i=0;i<400 && !published.layer_previewing[0];i++)scan();
    assert(published.layer_previewing[0]==0x5a);
    clocks[1]=149u;clocks[0]=clocks[1]+4900u;
    for(unsigned i=0;i<2;i++)noah_lighting_working_advance(&banks[i],clocks[i]);
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&banks[0],false,&r,127),&timing));
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&banks[1],false,&r,127),&timing));
    normal_feedback(500,false);normal_feedback(200,true);normal_feedback(500,true);standard_pulse();useful_mix(65535);useful_mix(500);
    quiet_reboot_recovery();
    peer_close();master=true;noah_qmk_lighting_snapshot_close();
    puts("actual two-half QMK snapshot adapter: strong registration guards/stage-only callbacks/main commit/lost request+reply/causal nonzero asymmetric RTT/both-half positive/show/expiry/no late resurrection PASS (host logical clock model, not hardware)");
    return 0;
}
