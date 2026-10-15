// Real chunk/effect/main-loop ordering, using the runtime's integration fixture.
#define main existing_scene_test_main
#include "rgb_layer_render_test.c"
#undef main

static noah_effective_rgb_runtime_t chunk_runtime;
static noah_effective_profile_snapshot_t chunk_profile;
static void chunk_publish(const uint8_t *bytes, size_t size) {
    noah_profile_lighting_v1_limits_t limits=noah_profile_rgb_compiled_v1_limits();
    CHECK(noah_profile_lighting_v1_decode(bytes,size,&limits,&chunk_profile.profile.rgb,NULL)==NOAH_PROFILE_LIGHTING_V1_OK);
    chunk_profile.profile.domain_mask=NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    chunk_profile.identity.kind=NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE;
    chunk_profile.identity.generation++;
    noah_effective_rgb_runtime_invalidate(&chunk_runtime,chunk_profile.identity.generation,(noah_effective_profile_identity_t){0},chunk_profile.identity,&chunk_profile);
}
static const uint8_t dim_event[]={5,0,58,1,0,0,0,2,0,255,200,
    7,0,255,0x40,0,0x61,5,128,
    14,0,255,1,28,127,0x40,0,0x61,2,0,0xa2,3,244,1,0};
static const uint8_t tint_event[]={5,0,58,1,0,0,0,2,0,255,200,
    8,0,255,0x40,0,0x62,6,0,128,
    14,0,255,1,28,127,0x40,0,0x61,2,0,0xa2,3,244,1,0};
static const uint8_t fade_event[]={5,0,58,1,0,0,0,3,0,255,200,
    14,0,255,1,28,127,0x40,0,0x61,2,0,0xa2,3,244,1,
    7,0,255,0x40,0,0x61,5,128,
    10,0,255,1,1,LAYER_POINTER,0x40,0,0x61,2,0,
    1,1,4,1,0,0,2};
static const uint8_t query_event[]={5,0,58,1,0,0,0,3,0,255,200,
    7,0,255,0x40,0,0x61,5,128,
    9,0,255,0x42,8,0,255,0x61,2,0,
    14,0,255,1,28,127,0x40,0,0x61,2,0,0xa2,3,244,1,0};
static void chunk_event(void) {
    noah_lighting_event_t event={.kind=NOAH_LIGHTING_EVENT_MACRO_PLAYED,.thing=127,.key={MATRIX_ROWS,MATRIX_COLS}};
    noah_lighting_event_emit(&event);
}
static void chunk_case(bool left, unsigned scenario) {
    test_reset();fake_is_left=left;fake_is_master=true;runtime_fixture.time32=100;
    memset(&chunk_profile,0,sizeof chunk_profile);
    noah_effective_rgb_runtime_init(&chunk_runtime);CHECK(noah_effective_rgb_runtime_install(&chunk_runtime));
    if(scenario==1)chunk_publish(tint_event,sizeof tint_event);
    else if(scenario==2)chunk_publish(fade_event,sizeof fade_event);
    else if(scenario==6)chunk_publish(query_event,sizeof query_event);
    else chunk_publish(dim_event,sizeof dim_event);
    if(scenario==2) {layer_state=1u<<LAYER_POINTER;fake_auto_mouse_active=false;fake_auto_mouse_elapsed=AUTOMOUSE_RGB_DEAD_TIME+AUTOMOUSE_RGB_ACTIVE_SPAN/2u;}
    if(scenario==4)chunk_event();
    if(scenario==6)test_keymap[0][0][0]=KC_A;
    for(unsigned frame=0;frame<4;frame++) {
        // Each effect chunk initially contains last frame's composed output.
        if(frame)for(unsigned led=0;led<RGB_MATRIX_LED_COUNT;led++)ws2812_leds[led]=(ws2812_led_t){.r=led_output[led].r,.g=led_output[led].g,.b=led_output[led].b};
        bool mutated=false;
        for(uint8_t iter=0;iter<5;iter++) {
            struct rgb_matrix_limits_t limits=rgb_matrix_get_limits(iter);
            for(unsigned led=limits.led_min_index;led<limits.led_max_index;led++)ws2812_leds[led]=(ws2812_led_t){.b=200};
            (void)render_output_range(limits.led_min_index,limits.led_max_index);
            if(!mutated && limits.led_min_index<limits.led_max_index) {
                mutated=true;
                if(scenario==3)chunk_event();
                if(scenario==4)runtime_fixture.time32=600;
                if(scenario==5) {uint8_t replacement[sizeof dim_event];memcpy(replacement,dim_event,sizeof replacement);replacement[18]=64;chunk_publish(replacement,sizeof replacement);}
                if(scenario==6) {test_keymap[0][0][0]=KC_NO;noah_rgb_runtime_invalidate_layer_maps();}
            }
            noah_lighting_runtime_task();
        }
        rgb_t expected={.b=100};
        if(scenario==1)expected=(rgb_t){.g=128,.b=200};
        if(scenario==2)expected=rgb_blend((rgb_t){.g=128,.b=100},(rgb_t){.b=100},automouse_blend_amount_from_elapsed(fake_auto_mouse_elapsed));
        if(scenario==3)expected=(rgb_t){.g=128,.b=100};
        if(scenario==5)expected=(rgb_t){.b=50};
        unsigned begin=left?0:RGB_LEFT_LED_COUNT,end=left?RGB_LEFT_LED_COUNT:RGB_MATRIX_LED_COUNT;
        for(unsigned led=begin;led<end;led++)check_led(led,expected);
    }
    chunk_profile.profile.domain_mask=0;
    noah_effective_rgb_runtime_invalidate(&chunk_runtime,1000,chunk_profile.identity,chunk_profile.identity,&chunk_profile);
    noah_effective_rgb_runtime_uninstall(&chunk_runtime);
}
int main(void) {
    for(unsigned half=0;half<2;half++)for(unsigned scenario=0;scenario<7;scenario++)chunk_case(half==0,scenario);
    puts("production chunk composition: both halves, 4 frames, dim/tint/auto-mouse fade/inactive timed rule/main/event/expiry/publication/remap PASS");
}
