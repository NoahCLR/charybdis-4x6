#include "users/noah/lib/compat/qmk_lighting_live.h"
#include "transactions.h"
#include "users/noah/lib/compat/qmk_split_service.h"
#if NOAH_LIGHTING_LIVE_AVAILABLE
static void service_live_main(void) {noah_split_service_begin();noah_lighting_live_task();}
#define noah_lighting_live_task service_live_main
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool peer_handle(uint8_t *,uint8_t);
void peer_task(void);
void peer_close(void);
bool peer_colour(uint8_t,rgb_t *);
static uint32_t now=100,generation=1,keymap=2,peer_generation=1;
static unsigned instance,callbacks_running,releases[2],rpc_count,wire_bytes;
static bool pump_peer=true,lose_request,lose_reply,old_reply,roles[2]={true,false};
static uint8_t saved_reply[24],last_request[64],last_size;
static slave_callback_t callback[2];
static uint8_t request_id=1;
static const uint8_t profile[22]={1,0,0,0,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0xdd,0xcc,0xbb,0xaa,0x11,0,0xff,0xee,1,0};
uint32_t timer_read32(void) {return now;}
bool is_keyboard_master(void) {return roles[instance];}
bool is_keyboard_left(void) {return instance==1;}
uint32_t noah_effective_lighting_generation(void) {return instance?peer_generation:generation;}
bool noah_effective_lighting_identity(uint8_t p[22]) {memcpy(p,profile,22);return true;}
uint32_t noah_lighting_keymap_generation(void) {return keymap;}
void noah_lighting_facts_collect(noah_lighting_facts_t *p,bool local,bool synced) {
    assert(!callbacks_running && is_keyboard_master() && local && synced);
    for(size_t i=0;i<sizeof *p;i++)((uint8_t *)p)[i]=(uint8_t)i;
}
bool rgb_matrix_get_suspend_state(void) {return false;}
uint8_t rgb_matrix_is_enabled(void) {return 1;}
uint8_t rgb_matrix_get_mode(void) {return 2;}
bool noah_runtime_diag_indicator_active(void) {return false;}
void noah_rgb_runtime_live_release(void) {assert(!callbacks_running);releases[instance]++;}
void transaction_register_rpc(int8_t id,slave_callback_t fn) {
    assert(id==PUT_SPLIT_LIGHTING_LIVE_SYNC);callback[instance]=fn;
    // Registration-time callbacks must observe a closed initialization gate.
    uint8_t p[24]={0},r[24];memset(r,0xaa,sizeof r);fn(sizeof p,p,sizeof r,r);
    for(size_t i=0;i<sizeof r;i++)assert(!r[i]);
}
bool transaction_rpc_exec(int8_t id,uint8_t n,const void *p,uint8_t size,void *r) {
    assert(instance==0 && id==PUT_SPLIT_LIGHTING_LIVE_SYNC && n<=64 && size==24);
    rpc_count++;wire_bytes+=n+41u; // INFO4/REQn/EXEC1/RESP24 +12 handshake/CRC
    memcpy(last_request,p,n);last_size=n;
    if(lose_request)return false;
    if(old_reply) {memcpy(r,saved_reply,24);return true;}
    instance=1;callbacks_running++;callback[1](n,p,size,r);callbacks_running--;
    if(pump_peer)peer_task();
    instance=0;
    if(((uint8_t *)r)[1]==0)memcpy(saved_reply,r,24);
    return !lose_reply;
}
static void tick(void) {now+=5;instance=1;peer_task();instance=0;noah_lighting_live_task();}
static void pump(unsigned n) {while(n--)tick();}
static void request(uint8_t p[32],uint8_t command,uint8_t value,uint8_t op) {
    memset(p,0,32);p[0]=command;p[2]=value;p[3]=request_id++;if(!request_id)request_id=1;p[4]=op;
}
static void put16(uint8_t *p,uint16_t x) {p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8u);}
static void put32(uint8_t *p,uint32_t x) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(x>>(8u*i));}
static uint32_t get32(const uint8_t *p) {return p[0]|((uint32_t)p[1]<<8u)|((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);}
static uint8_t state(uint32_t *token) {
    uint8_t p[32];request(p,8,0xf,0);assert(noah_lighting_live_handle(p,32));assert(p[5]==0&&p[6]==25);
    if(token)*token=get32(p+18);return p[15];
}
static uint8_t command(uint8_t p[32]) {
    uint8_t original[32];memcpy(original,p,32);
    assert(noah_lighting_live_handle(p,32));if(p[5]!=4){uint8_t status=p[5];memcpy(p,original,32);return status;}
    noah_lighting_live_task();memcpy(p,original,32);assert(noah_lighting_live_handle(p,32));uint8_t status=p[5];memcpy(p,original,32);return status;
}
static uint8_t owner[8]={1,2,3,4,5,6,7,8};
static uint32_t token,revision=1,crc;
static uint8_t frame[174];
static void identified(uint8_t p[32],uint8_t op) {
    request(p,7,0x1d,op);memcpy(p+5,owner,8);put32(p+13,token);put32(p+17,revision);
}
static void begin(void) {
    uint8_t p[32];request(p,7,0x1d,2);memcpy(p+5,owner,8);put32(p+13,revision);
    put32(p+17,0x55667788);put16(p+21,174);put32(p+23,crc);
    assert(command(p)==0);state(&token);assert(token);
    for(unsigned i=0;state(NULL)!=2&&i<100;i++)tick();
    assert(state(NULL)==2);
}
static void upload(void) {
    for(unsigned offset=0;offset<174;offset+=8) {
        uint8_t p[32];identified(p,3);put16(p+21,(uint16_t)offset);p[23]=174-offset>8?8:174-offset;
        memcpy(p+24,frame+offset,p[23]);assert(command(p)==0);
    }
}
static void publish(void) {
    uint8_t p[32];identified(p,4);assert(command(p)==0);
    for(unsigned i=0;state(NULL)!=6&&i<100;i++)tick();
    assert(state(NULL)==6);
}
static void visible(bool master,bool slave) {
    static unsigned check;check++;
    rgb_t colour;instance=0;assert(noah_lighting_live_colour(57,&colour)==master);
    if(master)assert(colour.r==frame[171]&&colour.g==frame[172]&&colour.b==frame[173]);
    instance=1;if(peer_colour(0,&colour)!=slave){fprintf(stderr,"visible check%u now%u expected peer%d\n",check,now,slave);abort();}if(slave)assert(colour.r==frame[0]);instance=0;
}
static void hex(const char *s,uint8_t *p,unsigned n) {for(unsigned i=0;i<n;i++){unsigned v;assert(sscanf(s+2*i,"%2x",&v)==1);p[i]=(uint8_t)v;}}
void noah_lighting_live_test_exhaust(uint8_t);
int main(int argc,char **argv) {
    if(argc==3) {
        instance=1;peer_task();instance=0;noah_lighting_live_task();
        uint8_t kind=(uint8_t)atoi(argv[2]),p[32];noah_lighting_live_test_exhaust(kind);
        if(kind==2) {request(p,7,0x1d,1);assert(command(p)==3);}
        else {
            request(p,7,0x1d,2);memcpy(p+5,owner,8);put32(p+13,1);put32(p+17,0x55667788);put16(p+21,174);
            if(kind==1)assert(command(p)==3);else {(void)command(p);pump(3);}
        }
        rgb_t colour;assert(!noah_lighting_live_colour(0,&colour));
        request(p,7,0x1d,1);assert(command(p)==3);
        printf("Lighting Live namespace %u exhaustion closes without ID reuse PASS\n",kind);return 0;
    }
    assert(argc==2);FILE *f=fopen(argv[1],"r");assert(f);char a[1500],b[100];
    uint8_t cap[25],expected[32];assert(fscanf(f,"%1499s",a)==1);hex(a,expected,25);
    assert(noah_lighting_live_capability(cap)==25&&!memcmp(cap,expected,25));
    assert(fscanf(f,"%1499s",a)==1);hex(a,frame,174);assert(fscanf(f,"%u",&crc)==1);
    instance=1;peer_task();instance=0;noah_lighting_live_task();
    uint8_t p[32];request(p,7,0x1d,1);assert(command(p)==0);
    unsigned goldens=0;
    while(goldens<42 && fscanf(f,"%1499s %99s",a,b)==2) {
        hex(a,p,32);hex(b,expected,32);assert(noah_lighting_live_handle(p,32));
        if(memcmp(p,expected,32)){fprintf(stderr,"golden mismatch %u\n",goldens);abort();}goldens++;
    }
    assert(goldens==42);
    unsigned lifecycle;assert(fscanf(f,"%1499s %u",a,&lifecycle)==2 && !strcmp(a,"LIFECYCLE"));
    for(unsigned i=0;i<lifecycle;i++) {
        char c[100];uint8_t original[32];assert(fscanf(f,"%1499s %99s %99s",a,b,c)==3);
        hex(a,original,32);memcpy(p,original,32);hex(b,expected,32);
        assert(noah_lighting_live_handle(p,32) && !memcmp(p,expected,32));
        noah_lighting_live_task();memcpy(p,original,32);hex(c,expected,32);
        if(!noah_lighting_live_handle(p,32) || memcmp(p,expected,32)){fprintf(stderr,"lifecycle completion%u actual%u expected%u state%u now%u\n",i,p[5],expected[5],state(NULL),now);abort();}pump(20);
    }
    fclose(f);
    // Status page revisions reject a mixed multi-page read.
    request(p,8,0xf,0);noah_lighting_live_handle(p,32);uint32_t status_revision=get32(p+7);
    request(p,7,0x1d,1);assert(command(p)==0);
    request(p,8,0xf,1);put32(p+5,status_revision);noah_lighting_live_handle(p,32);assert(p[5]==5);
    // Old snapshots are invalidated by a remap/publication, not relabelled.
    keymap++;request(p,8,0xe,0);put32(p+5,2);noah_lighting_live_handle(p,32);assert(p[5]==5);
    // Exact lengths and every reserved byte are decoded before mailbox use.
    request(p,8,0xe,0);put32(p+5,2);
    for(unsigned n=0;n<32;n++)assert(!noah_lighting_live_handle(p,n));
    for(unsigned i=9;i<32;i++){request(p,8,0xe,0);put32(p+5,2);p[i]=1;assert(noah_lighting_live_handle(p,32)&&p[5]==1);}
    unsigned healthy_rpc=rpc_count,healthy_wire=wire_bytes;uint32_t healthy_start=now;
    begin();visible(false,false);
    identified(p,4);assert(command(p)==7);visible(false,false); // partial never shows
    upload();visible(false,false);
    identified(p,3);p[23]=1;p[24]=frame[0]^1;assert(command(p)==8); // conflicting retry cannot alter stage
    publish();visible(true,true);
    printf("healthy full preview: %u RPC/%u wireB/%u logicalms, max64/24B requests/replies; transfer+handshake main ACKs charged\n",rpc_count-healthy_rpc,wire_bytes-healthy_wire,now-healthy_start);
    for(unsigned n=0;n<24;n++){uint8_t bad[64]={0},out[24];instance=1;callback[1](n,bad,24,out);instance=0;for(unsigned i=0;i<24;i++)assert(!out[i]);}
    visible(true,true);
    // Duplicate publication ACK/request does not extend the peer lease.
    uint8_t old_packet[64];memcpy(old_packet,last_request,last_size);uint8_t old_size=last_size;
    now+=100;instance=1;uint8_t reply[24];callback[1](old_size,old_packet,24,reply);peer_task();instance=0;
    assert(reply[1]==0);visible(true,true);
    // Strictly new correlated renew; exact retry does not extend either lease.
    identified(p,5);put32(p+21,1);assert(command(p)==0);pump(5);visible(true,true);
    uint8_t duplicate[32];identified(duplicate,5);put32(duplicate+21,1);assert(command(duplicate)==0);
    pump(5);now+=1900;assert(command(duplicate)==0);pump(25);visible(false,false);
    identified(p,5);put32(p+21,2);assert(command(p)==5);
    identified(p,4);assert(command(p)==5);visible(false,false);
    // New owner/revision replacement crosses CLEAR before any new colour.
    owner[0]++;revision++;begin();upload();publish();visible(true,true);
    uint8_t old_end[32];identified(old_end,6);
    owner[0]++;revision++;begin();visible(false,false);upload();publish();visible(true,true);
    memcpy(p,old_end,32);assert(command(p)==5);visible(true,true);
    // A lost publish ACK may expose peer-only preview, never unverified master.
    identified(p,6);assert(command(p)==0);pump(5);visible(false,false);
    owner[0]++;revision++;begin();upload();identified(p,4);assert(command(p)==0);
    for(unsigned i=0;state(NULL)!=5&&i<100;i++)tick();
    assert(state(NULL)==5);lose_reply=true;pump(3);visible(false,true);
    now+=2100;pump(2);visible(false,false);lose_reply=false;
    identified(p,5);put32(p+21,3);assert(command(p)==5);
    // Abandoned upload expires, retries cannot revive staging.
    owner[0]++;revision++;begin();now+=2001;pump(1);identified(p,3);p[23]=1;p[24]=frame[0];assert(command(p)==5);
    // Peer main rejection and request loss fail closed, back off, then recover.
    owner[0]++;revision++;lose_request=true;
    request(p,7,0x1d,2);memcpy(p+5,owner,8);put32(p+13,revision);put32(p+17,0x55667788);put16(p+21,174);put32(p+23,crc);
    assert(command(p)==0);unsigned attempts=rpc_count;pump(20);assert(rpc_count-attempts<6);visible(false,false);
    lose_request=false;pump(100);state(&token);assert(state(NULL)==2);upload();publish();visible(true,true);
    // Stale ACK identity cannot authorize a newer revision.
    identified(p,6);assert(command(p)==0);pump(5);owner[0]++;revision++;
    old_reply=true;request(p,7,0x1d,2);memcpy(p+5,owner,8);put32(p+13,revision);put32(p+17,0x55667788);put16(p+21,174);put32(p+23,crc);
    assert(command(p)==0);pump(30);visible(false,false);old_reply=false;pump(100);
    // Profile and explicit role/suspend/close invalidate facts and frame state.
    generation++;peer_generation++;tick();visible(false,false);request(p,8,0xe,0);put32(p+5,2);noah_lighting_live_handle(p,32);assert(p[5]==5);
    owner[0]++;revision++;begin();upload();publish();visible(true,true);
    noah_lighting_live_close();visible(false,true);pump(5);visible(false,false);
    // Explicit close supersedes an already queued PUBLISH; callbacks cannot
    // reopen a closed endpoint or commit a mailbox owned before close.
    owner[0]++;revision++;begin();upload();identified(p,4);assert(command(p)==0);
    for(unsigned i=0;state(NULL)!=5&&i<100;i++)tick();
    pump_peer=false;tick();instance=1;peer_close();peer_task();instance=0;
    pump_peer=true;pump(3);visible(false,false);now+=2100;pump(2);
    // Quiet peer-only reinitialization invalidates its output. A later renewal
    // fails closed at the master instead of extending indefinitely on one side.
    owner[0]++;revision++;begin();upload();publish();peer_generation++;tick();visible(true,false);
    identified(p,5);put32(p+21,1);assert(command(p)==0);pump(30);visible(false,false);
    // Both physical instances can change transport role; neither keeps old
    // frame/sample/mailbox authorization in the new role.
    roles[0]=false;roles[1]=true;tick();visible(false,false);
    request(p,7,0x1d,1);assert(noah_lighting_live_handle(p,32)&&p[5]==3);
    roles[0]=true;roles[1]=false;tick();visible(false,false);
    // Normal timer wrap is elapsed-time arithmetic; backward clock closes.
    now=UINT32_MAX-500;tick();owner[0]++;revision++;begin();upload();publish();visible(true,true);
    now+=800;tick();visible(true,true);now-=1000;tick();visible(false,false);pump(5);visible(false,false);
    assert(releases[0]&&releases[1]);
    printf("Lighting Live production two-instance: %u wire goldens; bounded callback/main capture/upload/publish/clear/expiry/renew/oldACK/loss/backoff/profile/close/wrap PASS; %uRPC/%uwireB (logical host model, not hardware)\n",goldens,rpc_count,wire_bytes);
    return 0;
}
