#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifndef NOAH_LIGHTING_LIVE_ENABLE
#    define NOAH_LIGHTING_LIVE_ENABLE 0
#endif
#if NOAH_LIGHTING_LIVE_ENABLE && defined(RGB_MATRIX_ENABLE) && defined(VIA_ENABLE) && defined(NOAH_LIVE_PROFILE_OWNER_ENABLE) && defined(SPLIT_KEYBOARD)
#    define NOAH_LIGHTING_LIVE_AVAILABLE 1
#else
#    define NOAH_LIGHTING_LIVE_AVAILABLE 0
#endif

enum {
    NOAH_LIGHTING_LIVE_LEASE = 2000u,
    NOAH_LIGHTING_LIVE_FACTS = 0x0eu,
    NOAH_LIGHTING_LIVE_STATUS = 0x0fu,
    NOAH_LIGHTING_LIVE_CONTROL = 0x1du,
    NOAH_LIGHTING_LIVE_CAPTURE = 1u,
    NOAH_LIGHTING_LIVE_BEGIN = 2u,
    NOAH_LIGHTING_LIVE_CHUNK = 3u,
    NOAH_LIGHTING_LIVE_PUBLISH = 4u,
    NOAH_LIGHTING_LIVE_RENEW = 5u,
    NOAH_LIGHTING_LIVE_END = 6u,
    NOAH_LIGHTING_LIVE_OK = 0u,
    NOAH_LIGHTING_LIVE_MALFORMED = 1u,
    NOAH_LIGHTING_LIVE_UNKNOWN_PAGE = 2u,
    NOAH_LIGHTING_LIVE_UNAVAILABLE = 3u,
    NOAH_LIGHTING_LIVE_PENDING = 4u,
    NOAH_LIGHTING_LIVE_STALE = 5u,
    NOAH_LIGHTING_LIVE_BUSY = 6u,
    NOAH_LIGHTING_LIVE_RANGE = 7u,
    NOAH_LIGHTING_LIVE_CONFLICT = 8u,
    NOAH_LIGHTING_LIVE_PROFILE_CHANGED = 9u,
    NOAH_LIGHTING_LIVE_PEER_UNAVAILABLE = 10u,
};

#if NOAH_LIGHTING_LIVE_AVAILABLE
#    include "../rgb/lighting/lighting.h"
_Static_assert(NOAH_LIGHTING_LED_COUNT < 256u, "live geometry fits one byte");
_Static_assert(NOAH_LIGHTING_FACT_SLICE_COUNT <= 255u, "live slice count fits one byte");
// Main owns all semantic state. Receive handlers only decode/copy bounded
// commands or encode an identified immutable/published read. No borrowed view.
bool noah_lighting_live_handle(uint8_t *report, uint8_t length);
void noah_lighting_live_task(void);
void noah_lighting_live_close(void);
bool noah_lighting_live_colour(uint8_t led, rgb_t *colour);
static inline uint8_t noah_lighting_live_capability(uint8_t p[25]) {
    const uint16_t frame = 3u * NOAH_LIGHTING_LED_COUNT;
    const uint16_t facts = sizeof(noah_lighting_facts_t) - 1u + 3u * NOAH_LIGHTING_FACT_SLICE_COUNT;
    for (uint8_t i=0;i<25;i++) p[i]=0;
    p[0]=1;p[1]=3;p[2]=1;p[3]=1;p[4]=0x0e;p[5]=0x0f;p[6]=0x1d;p[7]=8;
    p[8]=NOAH_LIGHTING_LED_COUNT;p[9]=RGB_LEFT_LED_COUNT;
    p[10]=(uint8_t)frame;p[11]=(uint8_t)(frame>>8u);p[12]=0xd0;p[13]=7;
    p[14]=(uint8_t)facts;p[15]=(uint8_t)(facts>>8u);p[16]=48;p[17]=1;
    p[18]=0xd0;p[19]=7;p[20]=0xf4;p[21]=1;return 25;
}
#else
static inline bool noah_lighting_live_handle(uint8_t *p, uint8_t n) {(void)p;(void)n;return false;}
static inline void noah_lighting_live_task(void) {}
static inline void noah_lighting_live_close(void) {}
static inline uint8_t noah_lighting_live_capability(uint8_t p[25]) {(void)p;return 0;}
#endif
