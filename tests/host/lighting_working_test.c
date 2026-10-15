#include "users/noah/lib/rgb/lighting/lighting_working.h"
#include "users/noah/lib/rgb/lighting/lighting_history_index.h"
#include "users/noah/lib/rgb/lighting/lighting_freshness.h"
#include "matrix.h"
#include "users/noah/lib/rgb/lighting/lighting_history_work.h"
#include "users/noah/lib/rgb/lighting/lighting_snapshot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

layer_state_t layer_state;
matrix_row_t matrix_get_row(uint8_t row) { (void)row; return 0; }
void key_feedback_lighting_map(uint8_t *bytes) { (void)bytes; assert(!"observation must not recollect"); }
bool noah_lighting_fact_next(uint8_t kind,const noah_lighting_facts_t *facts,const uint8_t *params,uint16_t *cursor,noah_lighting_subject_t *subject) {
    switch(kind) {
        case 1:return noah_lighting_fact_layer_active_next(facts,params,cursor,subject);
        case 8:return noah_lighting_fact_behaviour_next(facts,params,cursor,subject);
        case 14:return noah_lighting_fact_key_pressed_next(facts,params,cursor,subject);
        default:assert(!"unsupported test walker");return false;
    }
}
static uint32_t work_counts[NOAH_LIGHTING_WORK_KINDS];
void noah_lighting_history_work(uint8_t kind,uint32_t amount){assert(kind<NOAH_LIGHTING_WORK_KINDS);work_counts[kind]+=amount;}
static unsigned drawn;
static void draw(void *context,const noah_lighting_subject_t *subject){(void)context;assert(subject->thing==drawn++);}
static noah_profile_lighting_v1_program_t program;
static noah_lighting_working_t work;
static uint8_t storage[NOAH_LIGHTING_WORKING_CAPACITY], saved[NOAH_LIGHTING_WORKING_CAPACITY];
static unsigned load(const uint8_t *bytes,unsigned size) {
    noah_profile_lighting_v1_limits_t limits=noah_profile_lighting_v1_default_limits();
    noah_profile_lighting_v1_view_t view;
    assert(noah_profile_lighting_v1_decode(bytes,size,&limits,&view,NULL)==NOAH_PROFILE_LIGHTING_V1_OK);
    noah_lighting_working_cost_t cost;
    assert(noah_profile_lighting_v1_working_cost(&view,&cost)==NOAH_PROFILE_LIGHTING_V1_OK);

    assert(noah_profile_lighting_v1_program_load(&view,&program));
    return cost.required;
}
static void useful(void) {
    uint8_t bytes[]={5,0,58,0,0,0,0,3,
        13,0,255,1,28,127,0x40,4,0x60,0,0xa2,3,255,255,
        14,0,255,2,25,255,255,0x40,4,0x60,0,0xa2,3,255,255,
        13,0,255,1,1,255,0x40,4,0x60,0,0xa2,2,255,255,0};
    assert(load(bytes,sizeof bytes)==8220u);
    assert(noah_lighting_working_begin(&work,&program,storage,sizeof storage,1));
    noah_lighting_working_advance(&work,100u);
    for(unsigned t=0;t<128;t++) {
        noah_lighting_event_group_t group={.thing_kind=4,.thing=t,.slot=t%8,.sides=3,.has_keys=1};
        for(unsigned key=0;key<MATRIX_ROWS*MATRIX_COLS;key++)group.owners[key/8u]|=1u<<(key%8u);group.qualified[0]=2;
        assert(noah_lighting_working_event(&work,&group));
    }
    noah_lighting_event_group_t macro={.thing_kind=5,.thing=127};macro.qualified[0]=1;
    assert(noah_lighting_working_event(&work,&macro));
    noah_lighting_facts_t facts={0};facts.layer_active[0]=255;facts.layer_active[1]=255;
    assert(noah_lighting_working_observe(&work,&facts));
    for(unsigned i=0;i<3;i++) {
        noah_lighting_working_rule_t r;assert(noah_lighting_working_rule(&work,i,&r));
        for(unsigned t=0;t<r.count;t++) {
            const uint8_t *c=noah_lighting_working_slot(&work,false,&r,r.count==1?127:t);
            noah_lighting_history_rule_t info;assert(noah_lighting_history_rule(&program,r.rule,&info));
            assert(noah_lighting_working_visible(c,&info.timing));
        }
    }
    memset(work_counts,0,sizeof work_counts);drawn=0;
    assert(noah_lighting_working_paint(&work,1u,draw,NULL));assert(drawn==128u);
    assert(work_counts[NOAH_LIGHTING_WORK_SEARCH]==128u*129u);
    noah_lighting_working_snapshot(&work);
    memcpy(saved,storage,sizeof storage);
    macro.slot=7;assert(noah_lighting_working_event(&work,&macro));
    // Pinned snapshot remains byte-identical while current changes.
    assert(!memcmp(saved+work.bank_bytes,storage+work.bank_bytes,work.bank_bytes));
    noah_lighting_working_advance(&work,65635u);
    assert(noah_lighting_working_commit(&work,65535,65535));
    noah_lighting_working_rule_t r;assert(noah_lighting_working_rule(&work,0,&r));
    noah_lighting_lifetime_parameters_t p={.flash=65535};
    assert(!noah_lighting_working_visible(noah_lighting_working_slot(&work,false,&r,127),&p));
    // Every read/admission failure precedes mutation.
    memcpy(saved,storage,sizeof storage);noah_lighting_working_t before=work;
    assert(!noah_lighting_working_begin(&work,&program,storage,8219u,2));
    assert(!memcmp(saved,storage,sizeof storage)&&!memcmp(&before,&work,sizeof work));
}
static void unmet_show(void) {
    const uint8_t bytes[]={5,0,58,0,0,0,0,1,17,0,255,1,14,59,0x40,4,0x60,0,0xa2,1,100,0,0xa2,2,200,0,0};
    load(bytes,sizeof bytes);assert(noah_lighting_working_begin(&work,&program,storage,sizeof storage,2));
    noah_lighting_facts_t facts={0};facts.key_pressed[7]=8;
    noah_lighting_working_advance(&work,100);assert(noah_lighting_working_observe(&work,&facts));
    noah_lighting_working_advance(&work,149);assert(noah_lighting_working_observe(&work,&facts));
    facts.key_pressed[7]=0;noah_lighting_working_advance(&work,250);assert(noah_lighting_working_observe(&work,&facts));
    noah_lighting_working_rule_t r;assert(noah_lighting_working_rule(&work,0,&r));
    noah_lighting_lifetime_parameters_t p={.show_after=100,.linger=200};
    assert(!noah_lighting_working_visible(noah_lighting_working_slot(&work,false,&r,59),&p));
    facts.key_pressed[7]=8;noah_lighting_working_advance(&work,300);assert(noah_lighting_working_observe(&work,&facts));
    noah_lighting_working_advance(&work,400);assert(noah_lighting_working_observe(&work,&facts));
    facts.key_pressed[7]=0;noah_lighting_working_advance(&work,450);assert(noah_lighting_working_observe(&work,&facts));
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&work,false,&r,59),&p));
    noah_lighting_working_advance(&work,650);assert(!noah_lighting_working_visible(noah_lighting_working_slot(&work,false,&r,59),&p));
}
static void independent_full_batches(void) {
    const uint8_t bytes[]={5,0,58,0,0,0,0,2,
        17,0,255,2,8,127,15,1,14,59,0x40,4,0x60,0,0xa2,2,232,3,
        17,0,255,2,8,127,15,1,14,58,0x40,4,0x60,0,0xa2,2,232,3,0};
    assert(load(bytes,sizeof bytes)<=8220);
    assert(noah_lighting_working_begin(&work,&program,storage,sizeof storage,4));
    noah_lighting_facts_t facts={0};facts.key_pressed[7]=12;
    for(unsigned key=0;key<60;key++) {
        facts.behaviour[3*key]=128;
        facts.behaviour[3*key+1]=(uint8_t)(1+key%5);
        facts.behaviour[3*key+2]=(uint8_t)(key%16);
    }
    noah_lighting_working_advance(&work,100);assert(noah_lighting_working_observe(&work,&facts));
    noah_lighting_working_rule_t a,b;
    assert(noah_lighting_working_rule(&work,0,&a)&&noah_lighting_working_rule(&work,1,&b));
    assert(noah_lighting_working_slot(&work,false,&a,127)[5]==60);
    // Partial same-thing departure refreshes the whole batch, without a tail
    // for the departed owner. Different filters retain independent batches.
    facts.behaviour[0]=0;noah_lighting_working_advance(&work,200);
    assert(noah_lighting_working_observe(&work,&facts));
    assert(noah_lighting_working_slot(&work,false,&a,127)[5]==59);
    facts.key_pressed[7]=4;noah_lighting_working_advance(&work,300);
    assert(noah_lighting_working_observe(&work,&facts));
    assert(noah_lighting_working_slot(&work,false,&a,127)[4]==NOAH_LIGHTING_WORKING_TAIL);
    facts.behaviour[3]=0;noah_lighting_working_advance(&work,400);
    assert(noah_lighting_working_observe(&work,&facts));
    assert(noah_lighting_working_slot(&work,false,&a,127)[5]==59);
    assert(noah_lighting_working_slot(&work,false,&b,127)[5]==58);
    noah_lighting_working_snapshot(&work);memcpy(saved,storage,sizeof storage);
    facts.key_pressed[7]=12;noah_lighting_working_advance(&work,500);
    assert(noah_lighting_working_observe(&work,&facts));
    assert(noah_lighting_working_slot(&work,false,&a,127)[4]==NOAH_LIGHTING_WORKING_MATCH);
    assert(!memcmp(saved+work.bank_bytes,storage+work.bank_bytes,work.bank_bytes));
    // Untrusted staged input must fail atomically, even after every slot fits.
    storage[work.bank_bytes+a.offset+5]=61;
    memcpy(saved,storage,work.bank_bytes);
    assert(!noah_lighting_working_commit(&work,0,0));
    assert(!memcmp(saved,storage,work.bank_bytes));
}

static void matching_restart_commit(void) {
    const uint8_t bytes[]={5,0,58,0,0,0,0,1,17,0,255,1,14,59,0x40,4,0x60,0,0xa2,1,44,1,0xa2,2,200,0,0};
    load(bytes,sizeof bytes);
    noah_lighting_working_t source,target;
    uint8_t src[108],dst[108];
    assert(noah_lighting_working_begin(&source,&program,src,sizeof src,1));
    assert(noah_lighting_working_begin(&target,&program,dst,sizeof dst,1));
    noah_lighting_facts_t facts={0};facts.key_pressed[7]=8;
    noah_lighting_working_advance(&source,100);assert(noah_lighting_working_observe(&source,&facts));
    noah_lighting_working_advance(&target,105);
    noah_lighting_working_snapshot(&source);
    memcpy(dst+target.bank_bytes,src+source.bank_bytes,source.bank_bytes);target.snapshot_ready=true;
    assert(noah_lighting_working_commit(&target,4,6));
    noah_lighting_working_advance(&target,410);noah_lighting_working_seen(&target);
    noah_lighting_working_rule_t r;assert(noah_lighting_working_rule(&target,0,&r));
    noah_lighting_lifetime_parameters_t timing={.show_after=300,.linger=200};
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&target,false,&r,59),&timing));
    // A continuous source match must preserve the half's own later observation.
    noah_lighting_working_release_snapshot(&source);noah_lighting_working_advance(&source,420);
    assert(noah_lighting_working_observe(&source,&facts));noah_lighting_working_snapshot(&source);
    noah_lighting_working_advance(&target,425);
    memcpy(dst+(target.current^1u)*target.bank_bytes,src+(source.current^1u)*source.bank_bytes,source.bank_bytes);target.snapshot_ready=true;
    assert(noah_lighting_working_commit(&target,4,6));
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&target,false,&r,59),&timing));
    // Source departure/reentry between coherent cuts is proved by its age's
    // upper bound being below the receiver's uninterrupted observed lower age.
    noah_lighting_working_release_snapshot(&source);facts.key_pressed[7]=0;
    noah_lighting_working_advance(&source,450);assert(noah_lighting_working_observe(&source,&facts));
    facts.key_pressed[7]=8;noah_lighting_working_advance(&source,500);assert(noah_lighting_working_observe(&source,&facts));
    noah_lighting_working_advance(&source,510);noah_lighting_working_snapshot(&source);
    noah_lighting_working_advance(&target,515);
    memcpy(dst+(target.current^1u)*target.bank_bytes,src+(source.current^1u)*source.bank_bytes,source.bank_bytes);target.snapshot_ready=true;
    assert(noah_lighting_working_commit(&target,4,6));
    assert(!noah_lighting_working_visible(noah_lighting_working_slot(&target,false,&r,59),&timing));
    noah_lighting_working_advance(&target,814);noah_lighting_working_seen(&target);
    assert(!noah_lighting_working_visible(noah_lighting_working_slot(&target,false,&r,59),&timing));
    noah_lighting_working_advance(&target,815);noah_lighting_working_seen(&target);
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&target,false,&r,59),&timing));
}
static void snapshots(void) {
    const uint8_t bytes[]={5,0,58,0,0,0,0,1,13,0,255,1,28,127,0x40,4,0x60,0,0xa2,3,244,1,0};
    load(bytes,sizeof bytes);
    noah_lighting_working_t source={0},target={0};uint8_t src[108],dst[108];
    assert(noah_lighting_working_begin(&source,&program,src,sizeof src,7));
    assert(noah_lighting_working_begin(&target,&program,dst,sizeof dst,7));
    noah_lighting_event_group_t event={.thing_kind=5,.thing=127};event.qualified[0]=1;
    noah_lighting_working_advance(&source,100u);assert(noah_lighting_working_event(&source,&event));
    noah_lighting_facts_t frozen_facts={0},staged_facts={0},published_facts={0};
    frozen_facts.layer_active[0]=5u;frozen_facts.terminator=0x5au;
    noah_lighting_snapshot_t tx={0},rx={0};uint8_t identity[22]={0};identity[0]=7;
    noah_lighting_snapshot_begin(&tx,&source,true,identity);noah_lighting_snapshot_begin(&rx,&target,false,identity);
    noah_lighting_snapshot_bind_facts(&tx,&frozen_facts,NULL);noah_lighting_snapshot_bind_facts(&rx,&staged_facts,&published_facts);
    unsigned complete=0;uint32_t now=110u;
    for(unsigned attempt=0;attempt<100 && !complete;attempt++,now+=6u) {
        uint8_t request[64],reply[14],duplicate[14];
        uint8_t length=noah_lighting_snapshot_request(&tx,now);assert(length);
        memcpy(request,tx.link.packet.outgoing,length);
        // Arbitrary offset4900, asymmetric outward2ms/return4ms; main delay1.
        noah_lighting_snapshot_receive(&rx,request,length,now+4902u,reply);
        if(attempt%3u==0) { // lost request/reply before main: byte-identical retry
            assert(length==noah_lighting_snapshot_request(&tx,now+1u));
            assert(!memcmp(request,tx.link.packet.outgoing,length));
        }
        noah_lighting_snapshot_main(&rx,now+4903u);
        noah_lighting_snapshot_receive(&rx,request,length,now+4904u,reply);
        noah_lighting_snapshot_receive(&rx,request,length,now+4904u,duplicate);
        assert(!memcmp(reply,duplicate,sizeof reply));
        bool final=tx.link.mailbox_ready && tx.link.packet.outgoing[9]==2u;
        (void)noah_lighting_snapshot_reply(&tx,reply,now,now+6u);
        if(final && !source.snapshot_ready)complete++;
        if(!complete)assert(!published_facts.terminator);
    }
    assert(complete);assert(!memcmp(&frozen_facts,&published_facts,sizeof frozen_facts));
    noah_lighting_working_rule_t r;assert(noah_lighting_working_rule(&target,0,&r));
    noah_lighting_lifetime_parameters_t p={.flash=500};
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&source,false,&r,127),&p));
    assert(noah_lighting_working_visible(noah_lighting_working_slot(&target,false,&r,127),&p));
    // Source occurrence100 expires600, not commit+500. Uncertainty only shortens.
    noah_lighting_working_advance(&target,5500u+100u);
    assert(!noah_lighting_working_visible(noah_lighting_working_slot(&target,false,&r,127),&p));
    // Main observes a backward local step after calibration. Evidence from the
    // old clock interval must not qualify a later snapshot in this session.
    assert(rx.clock.valid);
    (void)noah_lighting_snapshot_main(&rx,5700u);
    (void)noah_lighting_snapshot_main(&rx,5699u);
    assert(!rx.clock.valid);
}
int main(int argc,char **argv) {
    matching_restart_commit();useful();unmet_show();independent_full_batches();snapshots();assert(argc==4);FILE *f=fopen(argv[1],"r");assert(f);
    unsigned c,b,s,a,n,known,lo,hi,show,flash,visible,cases=0;
    while(fscanf(f,"%u %u %u %u %u %u %u %u %u %u %u",&c,&b,&s,&a,&n,&known,&lo,&hi,&show,&flash,&visible)==11) {
        noah_lighting_freshness_t clock;uint16_t lower=0,upper=0;
        bool ok=noah_lighting_freshness_calibrate(&clock,c,b,s,a)&&noah_lighting_freshness_age(&clock,n,&lower,&upper);
        assert(ok==!!known);if(ok) {assert(lower==lo&&upper==hi);assert((lower>=show&&upper<flash)==!!visible);}
        noah_lighting_freshness_reset(&clock);assert(!noah_lighting_freshness_age(&clock,n,&lower,&upper));cases++;
    }
    fclose(f);
    f=fopen(argv[2],"r");assert(f);char name[128],hex[17000];unsigned bank,max,required,admit,cost_cases=0;
    while(fscanf(f,"%127s %16999s %u %u %u %u",name,hex,&bank,&max,&required,&admit)==6) {
        uint8_t bytes[8192];unsigned size=(unsigned)strlen(hex)/2u;
        for(unsigned i=0;i<size;i++){unsigned byte;assert(sscanf(hex+2*i,"%2x",&byte)==1);bytes[i]=byte;}
        assert(load(bytes,size)==required);
        noah_profile_lighting_v1_view_t view;noah_profile_lighting_v1_limits_t limits=noah_profile_lighting_v1_default_limits();noah_lighting_working_cost_t cost;
        assert(noah_profile_lighting_v1_decode(bytes,size,&limits,&view,NULL)==NOAH_PROFILE_LIGHTING_V1_OK);
        assert(noah_profile_lighting_v1_working_cost(&view,&cost)==NOAH_PROFILE_LIGHTING_V1_OK);
        assert(cost.bank_bytes==bank && cost.max_slot==max);
        assert((noah_profile_lighting_v1_working_admit(&view,8220,&cost)==NOAH_PROFILE_LIGHTING_V1_OK)==!!admit);
        if(admit && !noah_lighting_working_begin(&work,&program,storage,sizeof storage,3)){fprintf(stderr,"direct begin layout mismatch %s bank%u max%u required%u\n",name,bank,max,required);return 1;}
        cost_cases++;
    }
    fclose(f);assert(cost_cases>=40);
    f=fopen(argv[3],"r");assert(f);unsigned epoch,sequence,wire_cases=0;char payload_hex[111],request_hex[129];
    while(fscanf(f,"%u %u %110s %128s",&epoch,&sequence,payload_hex,request_hex)==4) {
        noah_lighting_event_link_t link={0};uint8_t identity[22]={0},payload[55],expected[64];
        noah_lighting_snapshot_sender_begin(&link,identity);link.active=true;link.epoch=epoch;link.next_sequence=sequence;
        for(unsigned i=0;i<strlen(request_hex)/2u;i++){unsigned v;assert(sscanf(request_hex+2*i,"%2x",&v)==1);expected[i]=v;}
        for(unsigned i=0;i<55;i++){unsigned v;assert(sscanf(payload_hex+2*i,"%2x",&v)==1);payload[i]=v;}
        assert(noah_lighting_snapshot_sender_offer(&link,payload));
        size_t size=strlen(request_hex)/2u;assert(size<=64 && noah_lighting_event_sender_size(&link)==size);
        assert(!memcmp(link.packet.outgoing,expected,size));wire_cases++;
    }
    fclose(f);assert(wire_cases==7);
    printf("shared direct admission: %u profiles/all registry bounds/disabled/high IDs/255/boundary PASS\n",cost_cases);
    printf("direct admitted histories: exact8220 useful mixture/all128 identities+16 layers/current+snapshot overlap/atomic reject/show-tail; freshness%u shared cases PASS\n",cases);
}
