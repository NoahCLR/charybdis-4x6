// Real pinned native effects/state machine; hardware transfer is an explicit
// copy, matching RP2040 ws2812_flush before DMA. Raw and physical RGB differ.
#define rgb_matrix_set_color fixture_old_set_color
#define rgb_matrix_led_index fixture_old_led_index
#define rgb_matrix_get_limits fixture_old_get_limits
#include "rgb_chunk_composition_test.c"
#undef rgb_matrix_set_color
#undef rgb_matrix_led_index
#undef rgb_matrix_get_limits

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
    chunk_profile.profile.domain_mask=0;noah_effective_rgb_runtime_invalidate(&chunk_runtime,1000,chunk_profile.identity,chunk_profile.identity,&chunk_profile);noah_effective_rgb_runtime_uninstall(&chunk_runtime);
}
int main(void) {
    for(unsigned mapping=0;mapping<2;mapping++)for(unsigned half=0;half<2;half++)for(unsigned role=0;role<2;role++)for(unsigned effect=1;effect<=3;effect++)for(unsigned scenario=0;scenario<3;scenario++) {
        compact_driver=!!mapping;native_case(!half,!!role,effect,scenario);
    }
    puts("native Raindrops/Jellybean/Digital Rain: actual QMK completion/task/control+flush-copy, both roles/halves/mappings, dim/tint/fade, unchanged/selected pixels, no duplicate advanced, mode/enable/toggle/suspend/flags/raw ownership PASS");
}
