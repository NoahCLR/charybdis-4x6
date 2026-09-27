// Stable pointing-slot identities. Behavior comes from the effective profile.
#pragma once

#include <stdint.h>

typedef uint16_t pd_mode_traits_t;

enum {
    PD_MODE_TRAIT_NONE                        = 0,
    PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED    = (pd_mode_traits_t)1u << 0,
    PD_MODE_TRAIT_PREFER_TYPING_LAYER         = (pd_mode_traits_t)1u << 1,
    PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND   = (pd_mode_traits_t)1u << 2,
    PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE = (pd_mode_traits_t)1u << 3,
};

// The first six keycodes and their lock partners retain deployed numeric values.
#define NOAH_PD_MODE_BASE_LIST(PDM) \
    PDM(DRAGSCROLL, DRAGSCROLL) \
    PDM(VOLUME, VOLUME_MODE) \
    PDM(BRIGHTNESS, BRIGHTNESS_MODE) \
    PDM(ZOOM, ZOOM_MODE) \
    PDM(ARROW, ARROW_MODE) \
    PDM(PINCH, PINCH_MODE)

#define NOAH_PD_MODE_LIST(PDM) \
    NOAH_PD_MODE_BASE_LIST(PDM) \
    PDM(SLOT_6, PD_SLOT_6) \
    PDM(SLOT_7, PD_SLOT_7)
