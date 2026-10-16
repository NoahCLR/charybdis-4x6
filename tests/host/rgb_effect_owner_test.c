// Real pinned native effects/state machine; hardware transfer is an explicit
// copy, matching RP2040 ws2812_flush before DMA. Raw and physical RGB differ.
#ifdef NOAH_LIGHTING_LIVE_ENABLE
#    define transaction_register_rpc fixture_register_rpc
#    define transaction_rpc_exec fixture_rpc_exec
#endif
#define rgb_matrix_set_color fixture_old_set_color
#define rgb_matrix_led_index fixture_old_led_index
#define rgb_matrix_get_limits fixture_old_get_limits
#include "rgb_chunk_composition_test.c"
#undef rgb_matrix_set_color
#undef rgb_matrix_led_index
#undef rgb_matrix_get_limits
#undef transaction_register_rpc
#undef transaction_rpc_exec

#include "users/noah/lib/compat/qmk_lighting_live.h"
#include "users/noah/lib/compat/qmk_split_service.h"
#if NOAH_LIGHTING_LIVE_AVAILABLE
static void service_live_main(void) {noah_split_service_begin();noah_lighting_live_task();}
#define noah_lighting_live_task service_live_main
#endif
#if NOAH_LIGHTING_LIVE_AVAILABLE
static slave_callback_t live_callback;
uint8_t dynamic_keymap_macro_get_count(void) {return 128;}
uint16_t dynamic_keymap_macro_get_buffer_size(void) {return 0;}
void dynamic_keymap_macro_get_buffer(uint16_t offset,uint16_t size,uint8_t *data) {(void)offset;memset(data,0,size);}
bool via_macro_provider_start(uint16_t action) {(void)action;return false;}
bool via_macro_provider_try_play(uint16_t action) {(void)action;return false;}
void transaction_register_rpc(int8_t id,slave_callback_t callback) {
    if(id==PUT_SPLIT_LIGHTING_LIVE_SYNC)live_callback=callback;
}
// This fixture isolates real native-effect/output ownership. The independent
// live two-instance runner proves receiver main-before-ACK; this sink supplies
// a correlated successful remote result without duplicating its state machine.
bool transaction_rpc_exec(int8_t id,uint8_t n,const void *p,uint8_t m,void *r) {
    if(id!=PUT_SPLIT_LIGHTING_LIVE_SYNC)return false;
    CHECK(n>=24 && m==24);memcpy(r,p,24);((uint8_t *)r)[0]=0xb2;((uint8_t *)r)[1]=0;
    if(((const uint8_t *)p)[1]==1) {((uint8_t *)r)[4]=1;((uint8_t *)r)[5]=((uint8_t *)r)[6]=((uint8_t *)r)[7]=0;}
    return true;
}
uint8_t rgb_matrix_is_enabled(void);
uint8_t rgb_matrix_get_mode(void);
#endif

static bool compact_driver = true;
static uint32_t input_idle;
uint32_t last_input_activity_elapsed(void) {return input_idle;}
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
uint32_t noah_setting(uint8_t id,uint32_t fallback) {(void)id;return fallback;}
#endif
int rgb_matrix_led_index(int index) {
    if(index<0 || index>=RGB_MATRIX_LED_COUNT)return -1;
    unsigned first=fake_is_left?0:RGB_LEFT_LED_COUNT,end=fake_is_left?RGB_LEFT_LED_COUNT:RGB_MATRIX_LED_COUNT;
    if((unsigned)index<first || (unsigned)index>=end)return -1;
    return compact_driver?index-(int)first:index;
}
void rgb_matrix_set_color(int index,uint8_t r,uint8_t g,uint8_t b) {
    int at=rgb_matrix_led_index(index);if(at<0)return;
    ws2812_leds[at]=(ws2812_led_t){.r=r,.g=g,.b=b};led_output[index]=(rgb_t){.r=r,.g=g,.b=b};
}
static void rgb_matrix_set_color_all(uint8_t r,uint8_t g,uint8_t b) {
    for(unsigned i=0;i<RGB_MATRIX_LED_COUNT;i++)rgb_matrix_set_color(i,r,g,b);
}
typedef struct {uint8_t iter,flags;bool init;} effect_params_t;
static struct {uint8_t enable,mode,flags,speed;hsv_t hsv;} rgb_matrix_config;
static enum {SYNCING,STARTING,RENDERING,FLUSHING} rgb_task_state;
static effect_params_t rgb_effect_params;
static uint8_t rgb_last_effect,rgb_last_enable,rgb_current_effect;
static uint32_t rgb_timer_buffer,g_rgb_timer;
static bool suspend_state;
static const uint8_t k_rgb_matrix_split[2]={RGB_LEFT_LED_COUNT,RGB_MATRIX_LED_COUNT-RGB_LEFT_LED_COUNT};
static uint8_t g_rgb_frame_buffer[MATRIX_ROWS][MATRIX_COLS];
static ws2812_led_t physical[WS2812_LED_COUNT],raw_before[WS2812_LED_COUNT],composed[WS2812_LED_COUNT];
static unsigned compositions,flushes,seed=1,selected,oracle_scenario;
static ws2812_led_t fresh_oracle[WS2812_LED_COUNT];
struct rgb_matrix_limits_t rgb_matrix_get_limits(uint8_t iter);
static uint32_t sync_timer_elapsed32(uint32_t n){return sync_timer_read32()-n;}
#define RGB_MATRIX_NONE 0
#define RGB_MATRIX_RAINDROPS 1
#define RGB_MATRIX_JELLYBEAN_RAINDROPS 2
#define RGB_MATRIX_DIGITAL_RAIN 3
#define RGB_MATRIX_EFFECT_MAX 4
#define RGB_MATRIX_LED_FLUSH_LIMIT 1
#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define LED_HITS_TO_REMEMBER 8
#define HAS_ANY_FLAGS(a,b) (((a)&(b))!=0)
#define dprintf(...) ((void)0)
#define RGB_MATRIX_EFFECT(name)
#define RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#define ENABLE_RGB_MATRIX_RAINDROPS
#define ENABLE_RGB_MATRIX_JELLYBEAN_RAINDROPS
#define ENABLE_RGB_MATRIX_DIGITAL_RAIN
#define RGB_MATRIX_USE_LIMITS_ITER(a,b,n) struct rgb_matrix_limits_t limits=rgb_matrix_get_limits(n);uint8_t a=limits.led_min_index,b=limits.led_max_index
#define RGB_MATRIX_USE_LIMITS(a,b) RGB_MATRIX_USE_LIMITS_ITER(a,b,params->iter)
static uint16_t scale16by8(uint16_t n,uint8_t s){return ((uint32_t)n*s)>>8;}
static uint8_t qadd8(uint8_t a,uint8_t b){return (unsigned)a+b>255?255:a+b;}
static uint8_t random8(void){seed=seed*1664525u+1013904223u;return seed>>24;}
static uint8_t random8_max(uint8_t n){return n==RGB_MATRIX_LED_COUNT?selected%n:random8()%n;}
static uint8_t random8_min_max(uint8_t a,uint8_t b){return a+random8()%(b-a);}
static rgb_t rgb_matrix_hsv_to_rgb(hsv_t hsv){return hsv_to_rgb(hsv);}
#include "native_rgb_limits.inc"
#include "raindrops_anim.h"
#include "jellybean_raindrops_anim.h"
#include "digital_rain_anim.h"
#undef RGB_MATRIX_EFFECT

static void eeconfig_flush_rgb_matrix(bool force){(void)force;}
static void eeconfig_flag_rgb_matrix(bool write){(void)write;}
static void rgb_matrix_test(void){rgb_matrix_set_color_all(20,20,20);}
static void rgb_matrix_update_pwm_buffers(void){memcpy(physical,ws2812_leds,sizeof physical);flushes++;}
static void rgb_matrix_indicators(void);
static void rgb_matrix_indicators_advanced(effect_params_t *);
static void rgb_matrix_indicators_modules(void) {} // no configured module override
static void rgb_matrix_indicators_kb(void) {
    memcpy(raw_before,ws2812_leds,sizeof raw_before);
    // Independent public-entry control composes from a freshly restored raw
    // effect buffer; paint/lifetime correctness is covered by shared vectors.
    // It is outside the production completion interval/count below.
    (void)noah_rgb_matrix_indicators_advanced_user(0,RGB_MATRIX_LED_COUNT);
    memcpy(fresh_oracle,ws2812_leds,sizeof fresh_oracle);
    memcpy(ws2812_leds,raw_before,sizeof raw_before);
    unsigned before=rgb_runtime_test_stage_pipeline_count();
    (void)noah_rgb_matrix_indicators_user();
    CHECK(rgb_runtime_test_stage_pipeline_count()==before+1u);
    memcpy(composed,ws2812_leds,sizeof composed);compositions++;
}
static void rgb_matrix_indicators_advanced_modules(uint8_t a,uint8_t b){(void)a;(void)b;}
static void rgb_matrix_indicators_advanced_kb(uint8_t a,uint8_t b) {
    ws2812_led_t before[WS2812_LED_COUNT];memcpy(before,ws2812_leds,sizeof before);
    unsigned count=rgb_runtime_test_stage_pipeline_count();
    (void)noah_rgb_matrix_indicators_advanced_user(a,b);
    if(rgb_task_state==FLUSHING) {
        CHECK(rgb_runtime_test_stage_pipeline_count()==count);
        CHECK(!memcmp(before,ws2812_leds,sizeof before)); // no double dim, even right iter0
    }
}
#include "native_rgb_tasks.inc"

// Actual QMK task once, followed by actual userspace housekeeping boundary.
static void loop(void) {
    noah_lighting_live_task();
    runtime_fixture.time32++; // every actual main loop advances logical time
    unsigned completed=compositions,flushed=flushes;
    // NONE owns a new black raw buffer and never calls the basic indicator.
    // Only a completed non-NONE frame has a saved native base to restore.
    bool owned_flush=rgb_task_state==FLUSHING && rgb_current_effect!=0 && !suspend_state;
    rgb_matrix_task();
    if(compositions!=completed) {
        noah_rgb_runtime_housekeeping();
        CHECK(!memcmp(composed,ws2812_leds,sizeof composed)); // same-loop never restores
    } else {
        noah_rgb_runtime_housekeeping();
        if(flushes!=flushed && owned_flush)CHECK(!memcmp(raw_before,ws2812_leds,sizeof raw_before));
    }
}
static uint8_t scaled(uint8_t v){return ((uint16_t)v*128u+127u)/255u;}
static void expected_frame(unsigned scenario) {
    unsigned first=fake_is_left?0:RGB_LEFT_LED_COUNT,end=fake_is_left?RGB_LEFT_LED_COUNT:RGB_MATRIX_LED_COUNT;
    for(unsigned led=first;led<end;led++) {
        int at=rgb_matrix_led_index(led);rgb_t raw={.r=raw_before[at].r,.g=raw_before[at].g,.b=raw_before[at].b};
        rgb_t want={.r=scaled(raw.r),.g=scaled(raw.g),.b=scaled(raw.b)};
        if(scenario==1)want=(rgb_t){.r=(raw.r*127u+127u)/255u,.g=(raw.g*127u+255u*128u+127u)/255u,.b=(raw.b*127u+200u*128u+127u)/255u};
        if(scenario==2 || oracle_scenario)want=(rgb_t){.r=fresh_oracle[at].r,.g=fresh_oracle[at].g,.b=fresh_oracle[at].b};
        CHECK(physical[at].r==want.r && physical[at].g==want.g && physical[at].b==want.b);
    }
}
static void frame(void) {
    unsigned count=flushes;runtime_fixture.time32+=20;
    for(unsigned i=0;i<64 && flushes==count;i++)loop();
    CHECK(flushes==count+1);
}
#if NOAH_LIGHTING_LIVE_AVAILABLE
static void live16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8u);}
static void live32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t live_word(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint8_t live_id;
static void live_host(uint8_t p[32]) {
    p[0]=7;p[2]=0x1d;p[3]=++live_id;if(!p[3])p[3]=++live_id;
    CHECK(noah_lighting_live_handle(p,32));CHECK(p[5]==4);noah_lighting_live_task();
}
static unsigned live_sequence;
static uint32_t live_peer_epoch, live_token;
static void live_peer(uint8_t op,const uint8_t *data,unsigned n) {
    uint8_t p[64]={0xb1,op},r[24];live16(p+2,++live_sequence);live32(p+4,op==1?0:live_peer_epoch);
    memset(p+8,0x5a,8);live32(p+16,live_token);live32(p+20,1);memcpy(p+24,data,n);
    CHECK(live_callback);live_callback(24+n,p,24,r);CHECK(r[1]==4);noah_lighting_live_task();
    live_callback(24+n,p,24,r);CHECK(r[1]==0);if(op==1)live_peer_epoch=live_word(r+4);
}
static void live_start(void) {
    noah_lighting_live_task();
    uint8_t bytes[3*RGB_MATRIX_LED_COUNT],p[32]={0},profile[22];
    for(unsigned i=0;i<sizeof bytes;i+=3){bytes[i]=128;bytes[i+1]=0;bytes[i+2]=64;}
    uint32_t crc=UINT32_MAX;for(unsigned i=0;i<sizeof bytes;i++){crc^=bytes[i];for(unsigned b=0;b<8;b++)crc=(crc>>1)^((crc&1)?0xedb88320u:0); }crc=~crc;
    CHECK(noah_effective_lighting_identity(profile));
    if(fake_is_master) {
        p[4]=2;memset(p+5,0x5a,8);live32(p+13,1);live32(p+17,live_word(profile+8));live16(p+21,sizeof bytes);live32(p+23,crc);live_host(p);
        for(unsigned i=0;i<4;i++){runtime_fixture.time32+=5;noah_lighting_live_task();}
        memset(p,0,32);p[0]=8;p[2]=0xf;p[3]=1;CHECK(noah_lighting_live_handle(p,32));CHECK(p[15]==2);live_token=live_word(p+18);
        for(unsigned offset=0;offset<sizeof bytes;offset+=8){
            memset(p,0,32);p[4]=3;memset(p+5,0x5a,8);live32(p+13,live_token);live32(p+17,1);live16(p+21,offset);p[23]=sizeof bytes-offset>8?8:sizeof bytes-offset;memcpy(p+24,bytes+offset,p[23]);live_host(p);
        }
        memset(p,0,32);p[4]=4;memset(p+5,0x5a,8);live32(p+13,live_token);live32(p+17,1);live_host(p);
        for(unsigned i=0;i<16;i++){runtime_fixture.time32+=5;noah_lighting_live_task();}
    } else {
        live_sequence=0;live_token++;live_peer(1,profile,22);uint8_t data[40]={0};live16(data,sizeof bytes);live32(data+2,crc);live_peer(2,data,6);
        for(unsigned offset=0;offset<sizeof bytes;offset+=37){live16(data,offset);data[2]=sizeof bytes-offset>37?37:sizeof bytes-offset;memcpy(data+3,bytes+offset,data[2]);live_peer(3,data,3+data[2]);}
        live_peer(4,data,0);
    }
    rgb_t colour;CHECK(noah_lighting_live_colour(0,&colour));CHECK(colour.r==128&&colour.b==64);
}
static void live_native_release(void) {
    live_start();frame();
    for(unsigned i=0;i<RGB_MATRIX_LED_COUNT;i++) {int at=rgb_matrix_led_index(i);if(at>=0)CHECK(physical[at].r==128&&!physical[at].g&&physical[at].b==64);}
    // Current histories change under preview, then end exactly while composed
    // bytes wait in FLUSHING. Compare against an independent ordinary resolve
    // from the saved raw base, and the actual subsequent transfer copy.
    unsigned old=compositions;for(unsigned i=0;i<64&&old==compositions;i++)loop();CHECK(rgb_task_state==FLUSHING);
    chunk_event();
    if(fake_is_master){uint8_t p[32]={0};p[4]=6;memset(p+5,0x5a,8);live32(p+13,live_token);live32(p+17,1);live_host(p);}
    else {uint8_t data[1]={0};live_peer(6,data,0);}
    memcpy(composed,ws2812_leds,sizeof composed);memcpy(ws2812_leds,raw_before,sizeof raw_before);
    (void)noah_rgb_matrix_indicators_advanced_user(0,RGB_MATRIX_LED_COUNT);memcpy(fresh_oracle,ws2812_leds,sizeof fresh_oracle);
    CHECK(!memcmp(composed,fresh_oracle,sizeof composed));memcpy(ws2812_leds,composed,sizeof composed);
    loop();CHECK(!memcmp(physical,composed,sizeof physical));CHECK(!memcmp(ws2812_leds,raw_before,sizeof raw_before));
    live_start();frame();noah_rgb_runtime_suspend();rgb_matrix_set_suspend_state(true);
    for(unsigned i=0;i<RGB_MATRIX_LED_COUNT;i++){int at=rgb_matrix_led_index(i);if(at>=0)CHECK(!physical[at].r&&!physical[at].g&&!physical[at].b);}
    rgb_matrix_set_suspend_state(false);noah_lighting_live_task();
}
#endif

static void native_case(bool left,bool master_role,unsigned effect,unsigned scenario) {
    oracle_scenario=0;input_idle=0;
    test_reset();fake_is_left=left;fake_is_master=master_role;runtime_fixture.time32=100;
    memset(&chunk_profile,0,sizeof chunk_profile);noah_effective_rgb_runtime_init(&chunk_runtime);CHECK(noah_effective_rgb_runtime_install(&chunk_runtime));
    uint8_t profile[sizeof fade_event];const uint8_t *p=scenario==1?tint_event:scenario==2?fade_event:dim_event;
    unsigned n=scenario==2?sizeof fade_event:scenario==1?sizeof tint_event:sizeof dim_event;
    memcpy(profile,p,n);profile[2]=RGB_MATRIX_LED_COUNT;chunk_publish(profile,n);
    // Use pure current-state dim/tint on both roles here. Main-owned scene
    // transport tests independently cover actual event qualification/commits.
    if(scenario==2){layer_state=1u<<LAYER_POINTER;fake_auto_mouse_active=false;fake_auto_mouse_elapsed=AUTOMOUSE_RGB_DEAD_TIME+AUTOMOUSE_RGB_ACTIVE_SPAN/2;}
    for(unsigned row=0;row<MATRIX_ROWS;row++)for(unsigned col=0;col<MATRIX_COLS;col++)g_led_config.matrix_co[row][col]=row*MATRIX_COLS+col<RGB_MATRIX_LED_COUNT?row*MATRIX_COLS+col:NO_LED;
    memset(g_led_config.flags,255,sizeof g_led_config.flags);
    memset(ws2812_leds,0,sizeof ws2812_leds);memset(physical,0,sizeof physical);
    rgb_matrix_config=(__typeof__(rgb_matrix_config)){.enable=1,.mode=effect,.flags=255,.speed=100,.hsv={.h=160,.s=255,.v=200}};
    rgb_task_state=STARTING;rgb_last_effect=255;rgb_last_enable=0;rgb_effect_params=(effect_params_t){0};suspend_state=false;
    for(unsigned f=0;f<8;f++) {
        if(effect==RGB_MATRIX_DIGITAL_RAIN && f==1)memset(g_rgb_frame_buffer,100,sizeof g_rgb_frame_buffer);
        frame();expected_frame(scenario);
        CHECK(!memcmp(raw_before,ws2812_leds,sizeof raw_before));
    }
    // Selected single-pixel update in native Raindrops/Jellybean then several
    // unchanged frames. The raw base must not contain any owned dim/tint.
    if(effect!=RGB_MATRIX_DIGITAL_RAIN) {
        rgb_matrix_config.hsv.v=120;selected=(left?0:RGB_LEFT_LED_COUNT)+1u;
        runtime_fixture.time32=2560;frame();expected_frame(scenario);
    }
    // Real synchronous event/publication/remap/expiry changes can occur while
    // old composed bytes await flush. Raw ownership is independent of scene
    // generation; compare future output with the fresh-base public control.
    oracle_scenario=1;
    chunk_event();frame();expected_frame(scenario);
    runtime_fixture.time32+=501;frame();expected_frame(scenario);
    if(scenario==0) {
        uint8_t replacement[sizeof dim_event];memcpy(replacement,dim_event,sizeof replacement);replacement[2]=RGB_MATRIX_LED_COUNT;replacement[18]=64;
        chunk_publish(replacement,sizeof replacement);frame();expected_frame(scenario);
        uint8_t query[sizeof query_event];memcpy(query,query_event,sizeof query);query[2]=RGB_MATRIX_LED_COUNT;
        test_keymap[0][0][0]=KC_A;chunk_publish(query,sizeof query);frame();expected_frame(scenario);
        test_keymap[0][0][0]=KC_NO;noah_rgb_runtime_invalidate_layer_maps();frame();expected_frame(scenario);
    }
    // Diagnostics override all local pixels but must preserve the native raw
    // state for subsequent partial effects, including currently unpainted LEDs.
    noah_runtime_diag_post_init();frame();expected_frame(scenario);
    noah_runtime_diag_reset_for_test();frame();expected_frame(scenario);
    // Mode/enable/toggle can replace pending FLUSHING with STARTING; STARTING
    // writes no effect pixels, so returning ownership there cannot erase a
    // new effect. These are the actual pinned setters, not guessed updates.
    for(unsigned wait=0;wait<64 && rgb_task_state!=RENDERING;wait++)loop();CHECK(rgb_task_state==RENDERING);
    unsigned old=compositions;for(unsigned wait=0;wait<64 && compositions==old;wait++)loop();CHECK(compositions>old);
    unsigned sent=flushes;rgb_matrix_mode_eeprom_helper(effect==1?2:1,false);
    loop();CHECK(flushes==sent);CHECK(!memcmp(raw_before,ws2812_leds,sizeof raw_before));
    frame();expected_frame(scenario);
    rgb_matrix_disable_noeeprom();loop();rgb_matrix_enable_noeeprom();frame();expected_frame(scenario);
    rgb_matrix_toggle_eeprom_helper(false);loop();rgb_matrix_toggle_eeprom_helper(false);frame();expected_frame(scenario);
    // Master suspend hook precedes synchronous NONE flush. Resume may happen
    // before another ordinary housekeeping; no stale cache may replace black.
    for(unsigned wait=0;wait<64 && rgb_task_state!=RENDERING;wait++)loop();CHECK(rgb_task_state==RENDERING);old=compositions;for(unsigned wait=0;wait<64 && compositions==old;wait++)loop();CHECK(compositions>old);
    noah_rgb_runtime_suspend();rgb_matrix_set_suspend_state(true);
    for(unsigned i=0;i<WS2812_LED_COUNT;i++)if(rgb_matrix_led_index(i)>=0){int at=rgb_matrix_led_index(i);CHECK(!physical[at].r&&!physical[at].g&&!physical[at].b);}
    rgb_matrix_set_suspend_state(false);loop();frame();expected_frame(scenario);
    // Slave applies synchronous suspend once per matrix pass before RGB step;
    // housekeeping discards superseded cache on observing suspended state.
    for(unsigned wait=0;wait<64 && rgb_task_state!=RENDERING;wait++)loop();CHECK(rgb_task_state==RENDERING);old=compositions;for(unsigned wait=0;wait<64 && compositions==old;wait++)loop();CHECK(compositions>old);
    rgb_matrix_set_suspend_state(true);loop();rgb_matrix_set_suspend_state(false);frame();expected_frame(scenario);
    rgb_matrix_config.flags=0;frame();expected_frame(scenario);rgb_matrix_config.flags=255;frame();expected_frame(scenario);
    // NONE is deliberately not an indicator completion. Disable renders/flushes
    // black once, subsequent NONE passes cannot invoke owned composition.
    rgb_matrix_disable_noeeprom();unsigned painted=compositions;frame();
    CHECK(compositions==painted);
    for(unsigned i=0;i<10;i++)loop();CHECK(compositions==painted);
    for(unsigned led=0;led<RGB_MATRIX_LED_COUNT;led++)if(rgb_matrix_led_index(led)>=0){int at=rgb_matrix_led_index(led);CHECK(!physical[at].r&&!physical[at].g&&!physical[at].b);}
    rgb_matrix_enable_noeeprom();frame();expected_frame(scenario);
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    // Preserved effective900000ms sleep override owns composed black, never the
    // underlying native effect. Raw storage remains exact across both edges.
    input_idle=900001u;frame();expected_frame(scenario);
    for(unsigned led=0;led<RGB_MATRIX_LED_COUNT;led++)if(rgb_matrix_led_index(led)>=0){int at=rgb_matrix_led_index(led);CHECK(!physical[at].r&&!physical[at].g&&!physical[at].b);}
    input_idle=0;frame();expected_frame(scenario);
#endif
    // Public advanced legacy entry still composes when called independently.
    noah_rgb_runtime_post_init();for(unsigned i=0;i<WS2812_LED_COUNT;i++)ws2812_leds[i]=(ws2812_led_t){.b=200};
    CHECK(noah_rgb_matrix_indicators_advanced_user(0,RGB_MATRIX_LED_COUNT));
#if NOAH_LIGHTING_LIVE_AVAILABLE
    live_native_release();
#endif
    chunk_profile.profile.domain_mask=0;noah_effective_rgb_runtime_invalidate(&chunk_runtime,1000,chunk_profile.identity,chunk_profile.identity,&chunk_profile);noah_effective_rgb_runtime_uninstall(&chunk_runtime);
}
int main(void) {
    for(unsigned mapping=0;mapping<2;mapping++)for(unsigned half=0;half<2;half++)for(unsigned role=0;role<2;role++)for(unsigned effect=1;effect<=3;effect++)for(unsigned scenario=0;scenario<3;scenario++) {
        compact_driver=!!mapping;native_case(!half,!!role,effect,scenario);
    }
    puts("native Raindrops/Jellybean/Digital Rain: actual QMK completion/task/control+flush-copy, both roles/halves/mappings, dim/tint/fade, unchanged/selected pixels, no duplicate advanced, mode/enable/toggle/suspend/flags/raw ownership PASS");
}
