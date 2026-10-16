// Actual production adapters share one charged serial bus and main schedule.
// This proves deterministic service pressure, not a physical freeze or loss.
#define main retained_fixture_main
#define transaction_rpc_exec charged_fixture_exec
#define transaction_rpc_send charged_fixture_send
#include "qmk_lighting_snapshot_test.c"
#undef transaction_rpc_exec
#undef transaction_rpc_send
#undef main
#include <stdlib.h>
#include "users/noah/lib/compat/qmk_via_split_mirror.h"
#include "users/noah/lib/compat/qmk_split_service.h"
void service_profile_init(void);
bool service_profile_step(bool,uint32_t);
void noah_qmk_durable_io_init(void);
void noah_qmk_durable_io_matrix_scan(void);
bool service_profile_receive(uint8_t,const void *,uint8_t,void *);
void service_via_init(void);
void service_via_step(void);
void service_via_dirty(void);
void service_via_mirror_write(void);
void service_via_assert_converged(void);
void service_via_late_mirror_begin(void);
bool service_via_freeze(void);
void service_via_unfreeze(void);
bool service_via_can_mirror(void);
void service_via_boot_fence(void);
void service_mirror_peer_init(void);
bool service_mirror_peer_step(void);
static slave_callback_t service_mirror_callback;
static slave_callback_t *service_mirror_callback_slot(void){return &service_mirror_callback;}
bool service_via_receive(int8_t,uint8_t,const void *,uint8_t,void *);
static unsigned client_calls[8];
static bool durable_load,durable_quiet;
static uint32_t durable_dirty_at;
static unsigned pass_calls, worst_burst, all_calls, request_losses, pending_passes;
static uint64_t longest_gap, measured_blocking, previous_service;
static bool pressure_loss, quiet_current, pressure_history, outage;
static unsigned injected_outages;
static uint32_t history_at;
static uint8_t producer_current;
static unsigned producer_updates;
static unsigned preview_state,preview_renewals;
static uint32_t preview_started,preview_renew_at;
static void preview_input(void) {
    if(!preview_state) {
        uint8_t p[32]={0};p[4]=2;memset(p+5,0x55,8);noah_lighting_event_put_u32(p+13,1);p[21]=174;
        uint32_t crc=UINT32_MAX;
        for(unsigned i=0;i<174;i++){crc^=40;for(unsigned b=0;b<8;b++)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}noah_lighting_event_put_u32(p+23,~crc);
        live_host(p);preview_state=1;
    } else if(preview_state==1 && live_status()==2) {
        for(unsigned offset=0;offset<174;offset+=8) {
            uint8_t p[32]={0};p[4]=3;memset(p+5,0x55,8);noah_lighting_event_put_u32(p+13,live_token);noah_lighting_event_put_u32(p+17,1);p[21]=offset;p[23]=174-offset>8?8:174-offset;memset(p+24,40,p[23]);live_host(p);
        }
        uint8_t p[32]={0};p[4]=4;memset(p+5,0x55,8);noah_lighting_event_put_u32(p+13,live_token);noah_lighting_event_put_u32(p+17,1);live_host(p);preview_state=2;
    } else if(preview_state==2 && live_status()==6) {
        preview_state=3;preview_started=preview_renew_at=clocks[1];
    } else if(preview_state==3 && clocks[1]-preview_started>=1300u) {
        uint8_t p[32]={0};p[4]=6;memset(p+5,0x55,8);noah_lighting_event_put_u32(p+13,live_token);noah_lighting_event_put_u32(p+17,1);live_host(p);
        rgb_t colour;assert(!noah_lighting_live_colour(0,&colour));preview_state=4;
    } else if(preview_state==3 && live_status()==6 && clocks[1]-preview_renew_at>=500u) {
        uint8_t p[32]={0};p[4]=5;memset(p+5,0x55,8);noah_lighting_event_put_u32(p+13,live_token);noah_lighting_event_put_u32(p+17,1);noah_lighting_event_put_u32(p+21,++preview_renewals);live_host(p);preview_renew_at=clocks[1];
    }
}
bool transaction_rpc_send(int8_t id,uint8_t n,const void *p) {
    if(id!=PUT_VIA_KEYMAP_MIRROR) {
        pass_calls++;all_calls++;client_calls[id]++;
        uint32_t cost=serial_us(n+17u)+send_wait_us(12u);
        blocking_us+=cost;measured_blocking+=cost;advance_us(cost);
        return charged_fixture_send(id,n,p);
    }
    extern void service_mirror_receive_frame(uint8_t,const void *);
    service_mirror_receive_frame(n,p);return true;
}
static uint64_t stamp(void) { return (uint64_t)clocks[1]*1000u+clock_fraction_us; }
void service_mirror_receive_frame(uint8_t n,const void *p) {
    pass_calls++;all_calls++;client_calls[4]++;
    uint32_t cost=serial_us(n+17u)+send_wait_us(12);
    blocking_us+=cost;measured_blocking+=cost;advance_us(cost);
    master=false;service_mirror_callback(n,p,0,NULL);master=true;
}
bool transaction_rpc_exec(int8_t id,uint8_t n,const void *p,uint8_t r,void *out) {
    pass_calls++;all_calls++;client_calls[id]++;
    if(outage) {
        uint32_t cost=serial_us(n+r+17u)+send_wait_us(12u)+5000u;
        blocking_us+=cost;measured_blocking+=cost;advance_us(cost);injected_outages++;return false;
    }
    if(id==PUT_PROFILE_SPLIT_SYNC || id==PUT_VIA_KEYMAP_SYNC) {
        uint32_t outward=serial_us(n+14u)+send_wait_us(9u),returning=serial_us(r+3u)+send_wait_us(3u);
        blocking_us+=outward+returning;measured_blocking+=outward+returning;advance_us(outward);
        bool ok=id==PUT_PROFILE_SPLIT_SYNC?service_profile_receive(n,p,r,out):service_via_receive(id,n,p,r,out);
        advance_us(returning);return ok;
    }
    if(pressure_loss && id==PUT_SPLIT_LIGHTING_EVENTS_SYNC) { lose_request=true;request_losses++; }
    uint64_t before=blocking_us;
    bool ok=charged_fixture_exec(id,n,p,r,out);
    measured_blocking+=blocking_us-before;lose_request=false;return ok;
}
static void service_pass(bool preview,bool pending) {
    master=true;
    pass_calls=0;noah_split_service_begin();
    if(durable_load) {
        if(!durable_quiet && clocks[1]-durable_dirty_at>=100u) {service_via_dirty();service_via_mirror_write();durable_dirty_at=clocks[1];}
        noah_qmk_durable_io_matrix_scan();
    }
    noah_lighting_working_advance(&banks[1],clocks[1]);
    // A continuously changing CURRENT fact; the actual endpoint owns its cut.
    if(!quiet_current) {producer_current++;producer_updates++;}
    // Real producers continue during the immutable cut; only the collector
    // copies latest CURRENT facts after its ACK, exactly as production does.
    if(!banks[1].snapshot_ready && staged[1].layer_previewing[0]!=producer_current) {
    staged[1].layer_previewing[0]=producer_current;
    noah_lighting_facts_dirty_range(noah_qmk_lighting_snapshot_fact_changes(),offsetof(noah_lighting_facts_t,layer_previewing),1);
    noah_qmk_lighting_snapshot_changed();
    }
    if(pressure_history && clocks[1]-history_at>=50u) {
        noah_lighting_event_group_t event={.thing_kind=5,.thing=127,.qualified={1}};
        assert(noah_lighting_working_event(&banks[1],&event));history_at=clocks[1];
    }
    split_runtime_sync_tick();
    (void)noah_qmk_lighting_snapshot_step();
    if(preview) {preview_input();noah_lighting_live_task();}
    if(pass_calls>worst_burst)worst_burst=pass_calls;
    advance_us(1000u);
    uint64_t now=stamp();if(now-previous_service>longest_gap)longest_gap=now-previous_service;previous_service=now;
    master=false;if(durable_load){service_profile_step(false,clocks[0]);(void)service_mirror_peer_step();}noah_lighting_working_advance(&banks[0],clocks[0]);
    // Delayed peer main yields real PENDING/cached replies, not fabricated ACKs.
    if(!pending || clocks[1]%40u<5u) { (void)peer_step();if(preview)peer_live_task(); }
    else pending_passes++;
}
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    unsigned mode=argc>1?(unsigned)atoi(argv[1]):0;
    if(mode==4) {normal_feedback(200,true);return 0;}
    if(mode==6) {standard_pulse();return 0;}
    if(mode==5) {useful_mix(500);return 0;}

    const uint8_t bytes[]={5,0,58,0,0,0,0,1,17,0,255,1,28,127,0x40,0,0x60,0,0xa2,1,200,0,0xa2,3,244,1,0};
    noah_profile_lighting_v1_view_t view;noah_profile_lighting_v1_limits_t limits=noah_profile_lighting_v1_default_limits();
    assert(noah_profile_lighting_v1_decode(bytes,sizeof bytes,&limits,&view,NULL)==NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view,&program));
    for(unsigned i=0;i<2;i++)assert(noah_lighting_working_begin(&banks[i],&program,memory[i],sizeof memory[i],1));
    uint8_t identity[22]={1};
    master=false;peer_init(&banks[0],identity);peer_bind(&staged[0],&published);peer_base_init();peer_live_task();
    master=true;noah_qmk_lighting_snapshot_init(&banks[1],identity);noah_qmk_lighting_snapshot_bind_facts(&staged[1],NULL);split_runtime_sync_init();noah_lighting_live_task();
    // Run cold coherent baseline alongside base; existing lifecycle fixture
    // remains the full semantic oracle. Modes add peer-main delay or outage.
    if(mode==11)shared_scene=false;
    durable_load=mode==9 || mode==10;outage=mode==10;
    if(durable_load) {master=true;service_profile_init();service_via_init();master=false;service_mirror_peer_init();master=true;noah_qmk_via_split_mirror_init();noah_qmk_durable_io_init();durable_dirty_at=clocks[1];}
    pressure_loss=mode==2;quiet_current=mode==3;pressure_history=mode==7;history_at=clocks[1];
    measured_blocking=0;all_calls=0;pass_calls=0;
    uint64_t start=stamp();previous_service=start;
    while(stamp()-start<((mode==1 || mode==8 || mode==9 || mode==10)?3000000u:1000000u))service_pass(mode==1 || mode==8 || mode==9,mode==1);
    if(mode==1 || mode==8 || mode==9)printf("active preview state%u strictly-new renewals%u liveRPC%u\n",preview_state,preview_renewals,live_calls);
    if(durable_load)printf("actual consumers base%u retained%u live%u profile%u VIA%u mirror%u\n",client_calls[1],client_calls[6],client_calls[7],client_calls[5],client_calls[3],client_calls[4]);
    if(mode==1 || mode==8 || mode==9)assert(preview_state==4 && preview_renewals>=2u);
    uint64_t elapsed=stamp()-start;
    unsigned fraction=(unsigned)(measured_blocking*1000u/elapsed);
    printf("continuous producer updates%u (coalesced only at immutable collector seam)\n",producer_updates);
    printf("split service mode%u: max_gap=%lluus blocked=%u/1000 calls=%u burst=%u injected_request_losses=%u delayed_peer_passes=%u elapsed=%lluus\n",mode,(unsigned long long)longest_gap,fraction,all_calls,worst_burst,request_losses,pending_passes,(unsigned long long)elapsed);
    // A service interval may contain one worst bounded128/32 RPC plus1ms main
    // work. Sustained optional work must leave at least75%% of this model for
    // service. These are host acceptance budgets, not physical latency proof.
    bool ordinary_budget=fraction<=250u && all_calls*1000000ull<=100u*elapsed;
    bool pass=longest_gap<=12000u && worst_burst<=1u && (mode==7 || ordinary_budget);
    if(mode==7)puts("QUALIFIED 20Hz STRESS: service-gap/burst only; sustained25%/100RPC budget is NOT claimed");
    puts(pass?"SERVICE PASS":"SERVICE FAIL: optional transport starves service budget");
    if(outage) {
        assert(injected_outages<=9u);
        printf("shared all-client outage: %u charged attempts over3s, exponential quiet; local/main service continues\n",injected_outages);
        return pass?0:1;
    }
    if(durable_load) {
        assert(client_calls[1] && client_calls[3] && client_calls[4] && client_calls[5] && client_calls[6] && client_calls[7]);
        quiet_current=durable_quiet=true;
        uint64_t stopped=stamp();
        while(stamp()-stopped<2000000u)service_pass(false,false);
        service_via_assert_converged();
        assert(published.layer_previewing[0]==producer_current);
        puts("all six actual consumers progressed; exact latest storage/digest/CURRENT convergence within2s after producer quiescence PASS");
        master=true;service_via_late_mirror_begin();
        stopped=stamp();
        while(stamp()-stopped<2000000u) {
            master=true;pass_calls=0;noah_split_service_begin();service_via_step();
            assert(pass_calls<=1u);advance_us(1000u);
        }
        service_via_assert_converged();
        unsigned sent_mirrors=client_calls[4];
        // Full reconciliation completed FIRST. The old queued frame must be
        // invalidated at the generation seam without ever reaching the peer.
        for(unsigned i=0;i<100;i++){master=true;noah_split_service_begin();(void)noah_qmk_via_split_mirror_matrix_scan_step();advance_us(1000u);}
        assert(client_calls[4]==sent_mirrors);service_via_assert_converged();
        master=true;service_via_mirror_write();(void)service_via_freeze();
        noah_split_service_begin();assert(!noah_qmk_via_split_mirror_matrix_scan_step());
        assert(client_calls[4]==sent_mirrors);service_via_unfreeze();
        stopped=stamp();
        while(!service_via_can_mirror() && stamp()-stopped<2000000u) {
            master=true;noah_split_service_begin();service_via_step();advance_us(1000u);
        }
        assert(service_via_can_mirror());
        master=true;service_via_mirror_write();
        master=false;(void)noah_qmk_via_split_mirror_matrix_scan_step();
        master=true;noah_split_service_begin();(void)noah_qmk_via_split_mirror_matrix_scan_step();
        assert(client_calls[4]==sent_mirrors);
        service_via_mirror_write();service_via_boot_fence();
        noah_split_service_begin();assert(!noah_qmk_via_split_mirror_matrix_scan_step());
        assert(client_calls[4]==sent_mirrors);
        puts("queued old write then newer same-key/range/reset/macro changes: authoritative convergence precedes old queue grant; no late rollback; logical/role/boot fences PASS");
    }
    return pass?0:1;
}
