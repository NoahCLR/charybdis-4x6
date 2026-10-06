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

// The configurable pointing slots in ID order. Slot n holds with PD_SLOT_n and
// toggles with PD_SLOT_n_LOCK (noah_keymap_ids.h). Thirty-two fill both
// keycode blocks; slots 6..31 carry no factory identity.
#define NOAH_PD_MODE_LIST(PDM) \
    PDM(DRAGSCROLL, PD_SLOT_0) \
    PDM(VOLUME, PD_SLOT_1)     \
    PDM(BRIGHTNESS, PD_SLOT_2) \
    PDM(ZOOM, PD_SLOT_3)       \
    PDM(ARROW, PD_SLOT_4)      \
    PDM(PINCH, PD_SLOT_5)      \
    PDM(SLOT_6, PD_SLOT_6)     \
    PDM(SLOT_7, PD_SLOT_7)     \
    PDM(SLOT_8, PD_SLOT_8)     \
    PDM(SLOT_9, PD_SLOT_9)     \
    PDM(SLOT_10, PD_SLOT_10)   \
    PDM(SLOT_11, PD_SLOT_11)   \
    PDM(SLOT_12, PD_SLOT_12)   \
    PDM(SLOT_13, PD_SLOT_13)   \
    PDM(SLOT_14, PD_SLOT_14)   \
    PDM(SLOT_15, PD_SLOT_15)   \
    PDM(SLOT_16, PD_SLOT_16)   \
    PDM(SLOT_17, PD_SLOT_17)   \
    PDM(SLOT_18, PD_SLOT_18)   \
    PDM(SLOT_19, PD_SLOT_19)   \
    PDM(SLOT_20, PD_SLOT_20)   \
    PDM(SLOT_21, PD_SLOT_21)   \
    PDM(SLOT_22, PD_SLOT_22)   \
    PDM(SLOT_23, PD_SLOT_23)   \
    PDM(SLOT_24, PD_SLOT_24)   \
    PDM(SLOT_25, PD_SLOT_25)   \
    PDM(SLOT_26, PD_SLOT_26)   \
    PDM(SLOT_27, PD_SLOT_27)   \
    PDM(SLOT_28, PD_SLOT_28)   \
    PDM(SLOT_29, PD_SLOT_29)   \
    PDM(SLOT_30, PD_SLOT_30)   \
    PDM(SLOT_31, PD_SLOT_31)
