#include "qmk_lighting_live.h"
#if NOAH_LIGHTING_LIVE_AVAILABLE
#    include "../profile/runtime/effective_rgb_runtime.h"
#    include "../profile/runtime/effective_settings_runtime.h"
#    include "../rgb/core/rgb_runtime.h"
#    include "../state/diagnostics/runtime_diag.h"
#    include "transactions.h"
#    include <string.h>

#define BARRIER() __asm__ __volatile__("" ::: "memory")
#define LEAF __attribute__((noinline))
enum { FRAME_BYTES = 3u * NOAH_LIGHTING_LED_COUNT, FACT_BYTES = sizeof(noah_lighting_facts_t) - 1u,
       FACT_TLV_BYTES = FACT_BYTES + 3u * NOAH_LIGHTING_FACT_SLICE_COUNT, FACT_STREAM_BYTES = 48u + FACT_TLV_BYTES };
enum { IDLE, CLEAR, UPLOAD, SEND_BEGIN, SEND_CHUNK, SEND_PUBLISH, LIVE, RENEW, CLOSE };
enum { RPC_CLEAR = 1u, RPC_BEGIN, RPC_CHUNK, RPC_PUBLISH, RPC_RENEW, RPC_END };
enum { RELEASE_PENDING = 1u, NAMESPACE_CLOSED = 2u, PUBLISH_PENDING = 4u };
static struct {
    noah_lighting_facts_t facts;
    uint8_t profile[22];
    uint32_t id, captured, generation, keymap;
    uint8_t role, source, validity, ready;
} live_sample;
static uint8_t live_frames[2][FRAME_BYTES];
static struct {
    uint8_t profile[22], owner[8];
    uint32_t token, revision, last_time, token_counter, peer_epoch;
    uint32_t deadline, renewal, last_attempt, crc;
    uint16_t offset;
    uint8_t operation, state, role, ready, flags, status;
} live_control;
static struct {
    uint8_t packet[64], reply[24];
    uint32_t epoch, sequence, last_failure, retry_delay;
    uint16_t offset;
    uint8_t phase, ready, pending, committed, reset, rejected;
} live_split;
static struct {
    uint8_t mailbox[32], reply[32];
    uint32_t revision;
    uint8_t ready, pending, operation, status;
} live_hid;
_Static_assert(sizeof live_sample + sizeof live_frames + sizeof live_control + sizeof live_split + sizeof live_hid ==
               (sizeof(noah_lighting_facts_t)==621u && NOAH_LIGHTING_LED_COUNT==58u ? 1272u : sizeof live_sample + sizeof live_frames + 260u), "complete fixed live state ledger");
_Static_assert(offsetof(noah_lighting_facts_t, terminator)==sizeof(noah_lighting_facts_t)-1u, "wire excludes only the explicit sentinel");
_Static_assert(RPC_M2S_BUFFER_SIZE >= 64 && RPC_S2M_BUFFER_SIZE >= 24, "live RPC fits existing allocation");

static uint16_t live_u16(const uint8_t *p) {return (uint16_t)(p[0] | ((uint16_t)p[1] << 8u));}
static uint32_t live_u32(const uint8_t *p) {return (uint32_t)p[0] | ((uint32_t)p[1]<<8u) | ((uint32_t)p[2]<<16u) | ((uint32_t)p[3]<<24u);}
static void live_put16(uint8_t *p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8u);}
static void live_put32(uint8_t *p,uint32_t v) {for(uint8_t i=0;i<4;i++)p[i]=(uint8_t)(v>>(8u*i));}
static bool live_zero(const uint8_t *p,uint8_t n) {for(uint8_t i=0;i<n;i++)if(p[i])return false;return true;}
static bool live_elapsed(uint32_t now,uint32_t start) {return now-start >= NOAH_LIGHTING_LIVE_LEASE;}
static void live_changed(void) {
    if (live_hid.revision == UINT32_MAX) {live_control.flags |= NAMESPACE_CLOSED | RELEASE_PENDING;live_sample.ready=0;live_control.ready=0;}
    else live_hid.revision++;
}
static void live_drop(void) {
    if(live_control.ready)live_control.flags |= RELEASE_PENDING;
    live_control.ready=0;
}
static void live_end(uint32_t now,uint8_t status) {
    live_drop();live_control.state=live_control.role?CLOSE:IDLE;live_control.deadline=now;live_control.status=status;
    live_split.phase=0;live_split.committed=0;live_changed();
}
LEAF void noah_lighting_live_close(void) {
    if (!live_split.ready) return;
    live_split.reset=1;BARRIER();live_split.pending=0;
    live_sample.ready=0;live_hid.pending=0;memset(live_hid.mailbox,0,32);memset(live_hid.reply,0,32);
    live_end(timer_read32(),NOAH_LIGHTING_LIVE_STALE);
}
static bool live_identity(const uint8_t *p) {
    return !memcmp(live_control.owner,p,8) && live_control.token==live_u32(p+8) && live_control.revision==live_u32(p+12);
}
static LEAF uint32_t live_crc32(const uint8_t *p,uint16_t n) {
    uint32_t crc=UINT32_MAX;
    for(uint16_t i=0;i<n;i++) {
        crc^=p[i];
        for(uint8_t b=0;b<8;b++)crc=(crc>>1u)^((crc&1u)?0xedb88320u:0u);
    }
    return ~crc;
}
static void live_response(uint8_t *p,uint8_t status,uint8_t n) {
    memset(p+5,0,27);p[5]=status;p[6]=n;
}

// Stable slice IDs are the original state fact's ID; disabled slices disappear
// without renumbering. All bytes in each source slice have explicit semantics.
#define SLICE_ID_layer_active 1
#define SLICE_ID_layer_previewing 2
#define SLICE_ID_pointing_mode_active 3
#define SLICE_ID_combos_held 4
#define SLICE_ID_behaviour_state 5
#define SLICE_ID_combo_held 7
#define SLICE_ID_behaviour 8
#define SLICE_ID_layer_status 9
#define SLICE_ID_mode_status 10
#define SLICE_ID_combo_layer 11
#define SLICE_ID_behaviour_phase 12
#define SLICE_ID_macro_playing 13
#define SLICE_ID_key_pressed 14
#define SLICE_ID_modifier 15
#define SLICE_ID_host_lock 16
#define SLICE_ID_pointer 17
#define SLICE_ID_idle 18
static uint8_t live_fact_byte(uint16_t offset) {
    if(offset<48u) {
        if(offset==0)return 1;
        if(offset==1)return live_sample.source;
        if(offset==2)return live_sample.role;
        if(offset==3)return live_sample.validity;
        if(offset<20u) {
            uint32_t word=offset<8?live_sample.id:offset<12?live_sample.captured:offset<16?live_sample.generation:live_sample.keymap;
            return (uint8_t)(word>>(8u*((offset-4u)%4u)));
        }
        if(offset<42u)return live_sample.profile[offset-20u];
        if(offset<44u)return (uint8_t)(FACT_STREAM_BYTES>>(8u*(offset-42u)));
        if(offset==44u)return NOAH_LIGHTING_FACT_SLICE_COUNT;
        if(offset==45u)return NOAH_LIGHTING_LED_COUNT;
        if(offset==46u)return RGB_LEFT_LED_COUNT;
        return NOAH_LIGHTING_MATRIX_KEYS;
    }
    offset-=48u;
#define SLICE_BYTE(name,bytes,synced,gated) \
    if(offset < 3u+(bytes)) {if(!offset)return SLICE_ID_##name;if(offset<3u)return (uint8_t)((bytes)>>(8u*(offset-1u)));return live_sample.facts.name[offset-3u];} \
    offset-=3u+(bytes);
    NOAH_LIGHTING_SLICE_ROWS(SLICE_BYTE)
#undef SLICE_BYTE
    return 0;
}
static LEAF uint8_t live_status_page(uint8_t page,uint8_t *p) {
    memset(p,0,25);live_put32(p,live_hid.revision);p+=4;
    if(page==0) {
        p[0]=1;p[1]=live_hid.operation;p[2]=live_hid.mailbox[3];p[3]=live_hid.status;p[4]=live_control.state;p[5]=live_split.phase;
        p[6]=live_control.operation;
        p[6]|=(live_control.ready?8u:0u);live_put32(p+7,live_control.token);live_put32(p+11,live_control.revision);
        live_put16(p+15,live_control.offset);live_put16(p+17,FRAME_BYTES);p[19]=live_control.status;p[20]=live_control.flags & NAMESPACE_CLOSED;
    } else if(page==1) {
        memcpy(p,live_control.owner,8);live_put32(p+8,live_control.deadline);live_put32(p+12,live_control.renewal);live_put32(p+16,live_control.crc);p[20]=live_control.role;
    } else if(page==2) {
        live_put32(p,live_sample.id);p[4]=live_sample.ready;live_put32(p+5,live_u32(live_control.profile+8));
        live_put32(p+9,live_sample.generation);live_put32(p+13,live_sample.captured);live_put32(p+17,live_control.peer_epoch);
    } else return 0;
    return 25;
}
static bool live_valid_command(const uint8_t *p) {
    if(p[0]!=7 || p[1] || p[2]!=NOAH_LIGHTING_LIVE_CONTROL || !p[3])return false;
    switch(p[4]) {
        case NOAH_LIGHTING_LIVE_CAPTURE:return live_zero(p+5,27);
        case NOAH_LIGHTING_LIVE_BEGIN:return !live_zero(p+5,8) && live_u32(p+13) && live_u16(p+21)==FRAME_BYTES && live_zero(p+27,5);
        case NOAH_LIGHTING_LIVE_CHUNK:return p[23] && p[23]<=8 && (uint32_t)live_u16(p+21)+p[23]<=FRAME_BYTES && live_zero(p+24+p[23],8-p[23]);
        case NOAH_LIGHTING_LIVE_PUBLISH:case NOAH_LIGHTING_LIVE_END:return live_zero(p+21,11);
        case NOAH_LIGHTING_LIVE_RENEW:return live_u32(p+21) && live_zero(p+25,7);
        default:return false;
    }
}
LEAF bool noah_lighting_live_handle(uint8_t *p,uint8_t n) {
    if(!p || n!=32 || p[1] || !((p[0]==7 && p[2]==NOAH_LIGHTING_LIVE_CONTROL) ||
       (p[0]==8 && (p[2]==NOAH_LIGHTING_LIVE_FACTS || p[2]==NOAH_LIGHTING_LIVE_STATUS))))return false;
    if(!p[3]) {live_response(p,NOAH_LIGHTING_LIVE_MALFORMED,0);return true;}
    if(p[0]==7) {
        if(!live_valid_command(p))live_response(p,NOAH_LIGHTING_LIVE_MALFORMED,0);
        else if(!live_hid.ready || !live_control.role || (live_control.flags & NAMESPACE_CLOSED))live_response(p,NOAH_LIGHTING_LIVE_UNAVAILABLE,0);
        else if(!memcmp(live_hid.mailbox,p,32))memcpy(p,live_hid.reply,32);
        else if(live_hid.pending)live_response(p,NOAH_LIGHTING_LIVE_BUSY,0);
        else {
            memcpy(live_hid.mailbox,p,32);live_response(p,NOAH_LIGHTING_LIVE_PENDING,0);memcpy(live_hid.reply,p,32);
            BARRIER();live_hid.pending=1;
        }
        return true;
    }
    uint32_t expected=live_u32(p+5);uint8_t page=p[4];
    if(!live_zero(p+9,23)) {live_response(p,NOAH_LIGHTING_LIVE_MALFORMED,0);return true;}
    if(!live_hid.ready || !live_control.role) {live_response(p,NOAH_LIGHTING_LIVE_UNAVAILABLE,0);return true;}
    if(p[2]==NOAH_LIGHTING_LIVE_STATUS) {
        if(expected && expected!=live_hid.revision)live_response(p,NOAH_LIGHTING_LIVE_STALE,0);
        else if(page && !expected)live_response(p,NOAH_LIGHTING_LIVE_MALFORMED,0);
        else {live_response(p,NOAH_LIGHTING_LIVE_OK,25);p[6]=live_status_page(page,p+7);if(!p[6])p[5]=NOAH_LIGHTING_LIVE_UNKNOWN_PAGE;}
    } else if(!live_sample.ready || !expected || expected!=live_sample.id || live_sample.keymap!=noah_lighting_keymap_generation())live_response(p,NOAH_LIGHTING_LIVE_STALE,0);
    else {
        uint16_t offset=(uint16_t)page*21u;
        if(offset>=FACT_STREAM_BYTES)live_response(p,NOAH_LIGHTING_LIVE_UNKNOWN_PAGE,0);
        else {
            uint8_t count=(FACT_STREAM_BYTES-offset)>21u?21u:(uint8_t)(FACT_STREAM_BYTES-offset);
            live_response(p,NOAH_LIGHTING_LIVE_OK,4u+count);live_put32(p+7,live_sample.id);
            for(uint8_t i=0;i<count;i++)p[11+i]=live_fact_byte(offset+i);
        }
    }
    return true;
}
static LEAF uint8_t live_capture(void) {
    if(live_sample.id==UINT32_MAX) {live_sample.ready=0;live_control.flags|=NAMESPACE_CLOSED;live_end(timer_read32(),NOAH_LIGHTING_LIVE_UNAVAILABLE);return NOAH_LIGHTING_LIVE_UNAVAILABLE;}
    live_sample.ready=0;
    if(!noah_effective_lighting_identity(live_sample.profile))return NOAH_LIGHTING_LIVE_UNAVAILABLE;
    live_sample.generation=noah_effective_lighting_generation();live_sample.keymap=noah_lighting_keymap_generation();
    noah_lighting_facts_collect(&live_sample.facts,true,true);live_sample.facts.terminator=0;
    live_sample.captured=timer_read32();live_sample.role=1;live_sample.source=1;live_sample.validity=1;live_sample.id++;
    BARRIER();live_sample.ready=1;return NOAH_LIGHTING_LIVE_OK;
}
static LEAF uint8_t live_upload(uint16_t offset,const uint8_t *p,uint8_t n,uint32_t now) {
    if(live_control.state!=UPLOAD || live_elapsed(now,live_control.deadline))return NOAH_LIGHTING_LIVE_STALE;
    if(offset<live_control.offset) return offset+n<=live_control.offset && !memcmp(live_frames[1]+offset,p,n)?NOAH_LIGHTING_LIVE_OK:NOAH_LIGHTING_LIVE_CONFLICT;
    if(offset!=live_control.offset || (uint32_t)offset+n>FRAME_BYTES)return NOAH_LIGHTING_LIVE_RANGE;
    memcpy(live_frames[1]+offset,p,n);live_control.offset+=n;live_control.deadline=now;return NOAH_LIGHTING_LIVE_OK;
}
static LEAF uint8_t live_host_command(uint32_t now) {
    const uint8_t *p=live_hid.mailbox;uint8_t op=p[4];
    if(op==NOAH_LIGHTING_LIVE_CAPTURE)return live_capture();
    if(op==NOAH_LIGHTING_LIVE_BEGIN) {
        if(live_u32(p+17)!=live_u32(live_control.profile+8))return NOAH_LIGHTING_LIVE_PROFILE_CHANGED;
        if(live_control.token_counter==UINT32_MAX) {live_control.flags|=NAMESPACE_CLOSED;live_end(now,NOAH_LIGHTING_LIVE_STALE);return NOAH_LIGHTING_LIVE_UNAVAILABLE;}
        live_drop();live_split.reset=0;live_control.flags&=(uint8_t)~PUBLISH_PENDING;memcpy(live_control.owner,p+5,8);live_control.token=++live_control.token_counter;live_control.revision=live_u32(p+13);
        live_control.crc=live_u32(p+23);live_control.offset=0;live_control.renewal=0;live_control.deadline=now;
        live_control.state=CLEAR;live_split.phase=0;live_split.pending=0;live_control.status=NOAH_LIGHTING_LIVE_PENDING;return NOAH_LIGHTING_LIVE_OK;
    }
    if(!live_identity(p+5))return NOAH_LIGHTING_LIVE_STALE;
    if(op==NOAH_LIGHTING_LIVE_END) {live_end(now,NOAH_LIGHTING_LIVE_OK);return NOAH_LIGHTING_LIVE_OK;}
    if(op==NOAH_LIGHTING_LIVE_CHUNK)return live_upload(live_u16(p+21),p+24,p[23],now);
    if(op==NOAH_LIGHTING_LIVE_PUBLISH) {
        if(live_control.state!=UPLOAD || live_elapsed(now,live_control.deadline))return NOAH_LIGHTING_LIVE_STALE;
        if(live_control.offset!=FRAME_BYTES)return NOAH_LIGHTING_LIVE_RANGE;
        if(live_crc32(live_frames[1],FRAME_BYTES)!=live_control.crc)return NOAH_LIGHTING_LIVE_CONFLICT;
        live_control.flags|=PUBLISH_PENDING;live_control.state=CLEAR;live_control.deadline=now;live_split.phase=0;return NOAH_LIGHTING_LIVE_OK;
    }
    if(op==NOAH_LIGHTING_LIVE_RENEW) {
        if(live_control.state!=LIVE || !live_control.ready || live_elapsed(now,live_control.deadline))return NOAH_LIGHTING_LIVE_STALE;
        uint32_t sequence=live_u32(p+21);
        if(sequence==live_control.renewal)return NOAH_LIGHTING_LIVE_OK;
        if(sequence<live_control.renewal)return NOAH_LIGHTING_LIVE_STALE;
        live_control.renewal=sequence;live_control.state=RENEW;live_split.phase=0;return NOAH_LIGHTING_LIVE_OK;
    }
    return NOAH_LIGHTING_LIVE_MALFORMED;
}

static uint8_t live_packet_length(const uint8_t *p) {
    switch(p[1]) {
        case RPC_CLEAR:return 46;
        case RPC_BEGIN:return 30;
        case RPC_CHUNK:return 27u+p[26];
        case RPC_PUBLISH:case RPC_END:return 24;
        case RPC_RENEW:return 28;
        default:return 0;
    }
}
static bool live_valid_rpc(const uint8_t *p,uint8_t n) {
    if(!p || n<24 || n>64 || p[0]!=0xb1 || !live_u16(p+2) || live_zero(p+8,8) || !live_u32(p+16) || !live_u32(p+20))return false;
    if(p[1]==RPC_CHUNK && (n<28 || !p[26] || p[26]>37 || (uint32_t)live_u16(p+24)+p[26]>FRAME_BYTES))return false;
    return n==live_packet_length(p) && ((p[1]==RPC_CLEAR && !live_u32(p+4)) || (p[1]!=RPC_CLEAR && live_u32(p+4)));
}
static void live_rpc_reply(uint8_t *p,uint8_t status) {
    p[0]=0xb2;p[1]=status;memcpy(p+2,live_split.packet+2,22);live_put32(p+4,live_split.epoch);
}
// The callback never changes frame/session/fact state. Main cannot preempt this
// callback; initialization/close gates quiesce it before replacing owned data.
static LEAF void live_rpc(uint8_t n,const void *request,uint8_t capacity,void *response_bytes) {
    if(!response_bytes)return;
    memset(response_bytes,0,capacity);
    if(capacity!=24 || !live_split.ready || live_split.reset || live_control.role || is_keyboard_master() || !live_valid_rpc(request,n))return;
    const uint8_t *p=request;
    if((live_split.pending || live_split.committed) && n==live_packet_length(live_split.packet) && !memcmp(p,live_split.packet,n)) {
        memcpy(response_bytes,live_split.reply,24);return;
    }
    if(live_split.pending)return;
    if(p[1]==RPC_CLEAR && live_control.state!=IDLE && live_u16(p+2)<=live_split.sequence)return;
    // Old/out-of-order packets cannot overwrite a main-owned or committed
    // request. CLEAR is the explicit identity reset and gets its own main ACK.
    if(p[1]!=RPC_CLEAR && (live_u32(p+4)!=live_split.epoch || !live_identity(p+8) || live_u16(p+2)<=live_split.sequence))return;
    memset(live_split.packet,0,sizeof live_split.packet);memcpy(live_split.packet,p,n);live_rpc_reply(live_split.reply,NOAH_LIGHTING_LIVE_PENDING);
    live_split.committed=0;BARRIER();live_split.pending=1;memcpy(response_bytes,live_split.reply,24);
}
static LEAF uint8_t live_peer_command(uint32_t now) {
    const uint8_t *p=live_split.packet;
    if(p[1]==RPC_CLEAR) {
        if(memcmp(live_control.profile,p+24,22))return NOAH_LIGHTING_LIVE_PROFILE_CHANGED;
        if(live_split.epoch==UINT32_MAX)return NOAH_LIGHTING_LIVE_UNAVAILABLE;
        live_drop();live_split.epoch++;memcpy(live_control.owner,p+8,8);live_control.token=live_u32(p+16);live_control.revision=live_u32(p+20);
        live_control.state=UPLOAD;live_control.offset=0;live_control.renewal=0;live_control.deadline=now;return NOAH_LIGHTING_LIVE_OK;
    }
    if(!live_identity(p+8) || live_u32(p+4)!=live_split.epoch)return NOAH_LIGHTING_LIVE_STALE;
    if(p[1]==RPC_END) {live_end(now,NOAH_LIGHTING_LIVE_OK);return NOAH_LIGHTING_LIVE_OK;}
    if(live_elapsed(now,live_control.deadline))return NOAH_LIGHTING_LIVE_STALE;
    if(p[1]==RPC_BEGIN) {
        if(live_control.state!=UPLOAD || live_u16(p+24)!=FRAME_BYTES)return NOAH_LIGHTING_LIVE_STALE;
        live_control.crc=live_u32(p+26);live_control.offset=0;live_control.deadline=now;return NOAH_LIGHTING_LIVE_OK;
    }
    if(p[1]==RPC_CHUNK)return live_upload(live_u16(p+24),p+27,p[26],now);
    if(p[1]==RPC_PUBLISH) {
        if(live_control.state!=UPLOAD || live_control.offset!=FRAME_BYTES || live_crc32(live_frames[1],FRAME_BYTES)!=live_control.crc)return NOAH_LIGHTING_LIVE_CONFLICT;
        memcpy(live_frames[0],live_frames[1],FRAME_BYTES);live_control.ready=1;live_control.state=LIVE;live_control.deadline=now;return NOAH_LIGHTING_LIVE_OK;
    }
    if(p[1]==RPC_RENEW) {
        uint32_t sequence=live_u32(p+24);
        if(live_control.state!=LIVE || !live_control.ready || !sequence || sequence<live_control.renewal)return NOAH_LIGHTING_LIVE_STALE;
        if(sequence>live_control.renewal) {live_control.renewal=sequence;live_control.deadline=now;}
        return NOAH_LIGHTING_LIVE_OK;
    }
    return NOAH_LIGHTING_LIVE_MALFORMED;
}
static LEAF void live_receive_main(uint32_t now) {
    if(!live_split.pending)return;
    BARRIER();
    uint8_t result=live_peer_command(now);
    // No session reinitialization can be accepted by another callback while
    // pending. Publish result only after complete main consumption/release.
    live_split.sequence=live_u16(live_split.packet+2);live_rpc_reply(live_split.reply,result);live_split.committed=1;
    BARRIER();live_split.pending=0;live_changed();
}
static LEAF bool live_prepare_rpc(void) {
    if(live_split.sequence>=UINT16_MAX) {live_control.flags|=NAMESPACE_CLOSED;live_end(timer_read32(),NOAH_LIGHTING_LIVE_STALE);return false;}
    uint8_t *p=live_split.packet;memset(p,0,64);p[0]=0xb1;
    switch(live_control.state) {
        case CLEAR:p[1]=RPC_CLEAR;memcpy(p+24,live_control.profile,22);break;
        case SEND_BEGIN:p[1]=RPC_BEGIN;live_put16(p+24,FRAME_BYTES);live_put32(p+26,live_control.crc);live_split.offset=0;break;
        case SEND_CHUNK: {
            p[1]=RPC_CHUNK;live_put16(p+24,live_split.offset);
            uint16_t remaining=FRAME_BYTES-live_split.offset;p[26]=remaining>37u?37u:(uint8_t)remaining;
            memcpy(p+27,live_frames[1]+live_split.offset,p[26]);break;
        }
        case SEND_PUBLISH:p[1]=RPC_PUBLISH;break;
        case RENEW:p[1]=RPC_RENEW;live_put32(p+24,live_control.renewal);break;
        case CLOSE:p[1]=RPC_END;break;
        default:return false;
    }
    live_put16(p+2,++live_split.sequence);live_put32(p+4,p[1]==RPC_CLEAR?0:live_control.peer_epoch);
    memcpy(p+8,live_control.owner,8);live_put32(p+16,live_control.token);live_put32(p+20,live_control.revision);
    live_split.phase=p[1];return true;
}
static LEAF void live_send_main(uint32_t now) {
    if(!live_split.phase && !live_prepare_rpc())return;
    if(now-live_split.last_failure < (live_split.retry_delay?live_split.retry_delay:5u))return;
    live_split.last_failure=now;
    bool sent=transaction_rpc_exec(PUT_SPLIT_LIGHTING_LIVE_SYNC,live_packet_length(live_split.packet),live_split.packet,24,live_split.reply);
    now=timer_read32();
    const uint8_t *r=live_split.reply;const uint8_t *p=live_split.packet;
    bool correlated=sent && r[0]==0xb2 && !memcmp(r+2,p+2,2) && !memcmp(r+8,p+8,16) &&
        ((p[1]==RPC_CLEAR && live_u32(r+4)) || (p[1]!=RPC_CLEAR && live_u32(r+4)==live_control.peer_epoch));
    if(!correlated) {
        live_split.retry_delay=!live_split.retry_delay?50u:live_split.retry_delay>=500u?1000u:live_split.retry_delay*2u;
        // A lost renewal/publish acknowledgement does not authorize local
        // display/extension. An existing local display is released on link loss.
        live_drop();live_control.status=NOAH_LIGHTING_LIVE_PEER_UNAVAILABLE;live_changed();return;
    }
    live_split.retry_delay=0;
    if(r[1]==NOAH_LIGHTING_LIVE_PENDING)return;
    if(r[1]!=NOAH_LIGHTING_LIVE_OK) {live_end(now,r[1]);return;}
    uint8_t op=live_split.phase;live_split.phase=0;
    switch(op) {
        case RPC_CLEAR:
            live_control.peer_epoch=live_u32(r+4);
            live_control.state=(live_control.flags & PUBLISH_PENDING)?SEND_BEGIN:UPLOAD;
            live_control.flags&=(uint8_t)~PUBLISH_PENDING;break;
        case RPC_BEGIN:live_control.state=SEND_CHUNK;break;
        case RPC_CHUNK:live_split.offset+=p[26];if(live_split.offset==FRAME_BYTES)live_control.state=SEND_PUBLISH;break;
        case RPC_PUBLISH:
            if(live_elapsed(now,live_control.deadline)) {live_end(now,NOAH_LIGHTING_LIVE_STALE);return;}
            memcpy(live_frames[0],live_frames[1],FRAME_BYTES);live_control.ready=1;live_control.state=LIVE;live_control.deadline=now;break;
        case RPC_RENEW:
            if(!live_control.ready || live_elapsed(now,live_control.deadline)) {live_end(now,NOAH_LIGHTING_LIVE_STALE);return;}
            live_control.state=LIVE;live_control.deadline=now;break;
        case RPC_END:live_control.state=IDLE;break;
        default:break;
    }
    live_control.status=NOAH_LIGHTING_LIVE_OK;live_changed();
}
static LEAF void live_initialize(uint32_t now) {
    live_split.ready=0;live_hid.ready=0;BARRIER();live_drop();live_sample.ready=0;live_hid.pending=0;
    live_split.pending=live_split.committed=live_split.phase=live_split.reset=0;live_split.retry_delay=0;
    if(live_split.epoch==UINT32_MAX)live_control.flags|=NAMESPACE_CLOSED;
    else live_split.epoch++;
    memset(live_hid.mailbox,0,32);memset(live_hid.reply,0,32);
    live_control.role=is_keyboard_master();live_control.state=IDLE;live_control.last_time=now;
    live_sample.generation=noah_effective_lighting_generation();
    if(!noah_effective_lighting_identity(live_control.profile))return;
    transaction_register_rpc(PUT_SPLIT_LIGHTING_LIVE_SYNC,live_rpc);
    BARRIER();live_split.ready=1;live_hid.ready=1;live_changed();
}
LEAF void noah_lighting_live_task(void) {
    uint32_t now=timer_read32();
    if(rgb_matrix_get_suspend_state()) {
        live_drop();live_sample.ready=0;live_hid.ready=0;live_hid.pending=0;live_split.ready=0;live_split.pending=0;
        return;
    }
    // A backward/discontinuous logical clock is not a lease extension. Normal
    // u32 wrap has a small unsigned delta and remains valid.
    if(!live_split.ready || (live_split.reset && !live_control.role) || live_control.role!=is_keyboard_master() || live_sample.generation!=noah_effective_lighting_generation() ||
       now-live_control.last_time>INT32_MAX)live_initialize(now);
    live_control.last_time=now;
    if(!live_split.ready)return;
    uint8_t suppression=(noah_runtime_diag_indicator_active()?2u:0u) |
        ((!rgb_matrix_is_enabled() || rgb_matrix_get_mode()==0u)?4u:0u);
#    ifdef NOAH_PORTABLE_PROFILE_ENABLE
    uint32_t timeout=noah_setting(NOAH_SETTING_RGB_TIMEOUT,900000);
    if(timeout && last_input_activity_elapsed()>timeout)suppression|=16u;
#    endif
    if(live_sample.ready && live_sample.keymap!=noah_lighting_keymap_generation()) {live_sample.ready=0;live_changed();}
    if(live_control.operation!=suppression) {live_control.operation=suppression;live_changed();}
    if(live_control.state!=IDLE && live_elapsed(now,live_control.deadline)) {
        if(live_control.state==CLOSE) {live_control.state=IDLE;live_split.phase=0;live_changed();}
        else live_end(now,NOAH_LIGHTING_LIVE_STALE);
    }
    if(live_control.role) {
        if(live_hid.pending) {
            BARRIER();
            live_hid.operation=live_hid.mailbox[4];live_hid.status=live_host_command(now);
            memcpy(live_hid.reply,live_hid.mailbox,5);live_response(live_hid.reply,live_hid.status,0);
            BARRIER();live_hid.pending=0;live_changed();
        }
        if(!(live_control.flags & NAMESPACE_CLOSED))live_send_main(now);
    } else live_receive_main(now);
    // A cancellation may replace a pending composed physical flush. This runs
    // only after all session/producer/publication work, outside callbacks.
    if(live_control.flags & RELEASE_PENDING) {live_control.flags&=(uint8_t)~RELEASE_PENDING;noah_rgb_runtime_live_release();}
}
LEAF bool noah_lighting_live_colour(uint8_t led,rgb_t *colour) {
    if(!live_control.ready || led>=NOAH_LIGHTING_LED_COUNT)return false;
    const uint8_t *p=live_frames[0]+3u*led;*colour=(rgb_t){p[0],p[1],p[2]};return true;
}
#endif

#if defined(NOAH_LIGHTING_LIVE_TEST) && NOAH_LIGHTING_LIVE_AVAILABLE
// Test-only exhaustion injection; no production declaration, symbol or state.
void noah_lighting_live_test_exhaust(uint8_t kind) {
    if(kind==1)live_control.token_counter=UINT32_MAX;
    if(kind==2)live_sample.id=UINT32_MAX;
    if(kind==3)live_split.sequence=UINT16_MAX;
    if(kind==4)live_hid.revision=UINT32_MAX;
}
#endif
