// Stable pointing-slot identities. Behavior comes from the active profile.
#pragma once

#include <stdint.h>

typedef uint16_t pointing_mode_traits_t;

enum {
    POINTING_MODE_TRAIT_NONE                        = 0,
    POINTING_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED    = (pointing_mode_traits_t)1u << 0,
    POINTING_MODE_TRAIT_PREFER_TYPING_LAYER         = (pointing_mode_traits_t)1u << 1,
    POINTING_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE = (pointing_mode_traits_t)1u << 3,
};

// The configurable pointing slots in ID order. Slot n holds with PD_SLOT_n and
// toggles with PD_SLOT_n_LOCK (noah_keymap_ids.h). Thirty-two fill both
// keycode blocks. The names here only name the slot's C constants
// (POINTING_MODE_*, POINTING_MODE_INDEX_*); a slot's name and settings come from the
// keymap's compiled defaults (noah_pd_defaults in pd_config.c).
#define NOAH_POINTING_MODE_LIST(MODE) \
    MODE(DRAGSCROLL, PD_SLOT_0)       \
    MODE(VOLUME, PD_SLOT_1)           \
    MODE(BRIGHTNESS, PD_SLOT_2)       \
    MODE(ZOOM, PD_SLOT_3)             \
    MODE(ARROW, PD_SLOT_4)            \
    MODE(PINCH, PD_SLOT_5)            \
    MODE(SLOT_6, PD_SLOT_6)           \
    MODE(SLOT_7, PD_SLOT_7)           \
    MODE(SLOT_8, PD_SLOT_8)           \
    MODE(SLOT_9, PD_SLOT_9)           \
    MODE(SLOT_10, PD_SLOT_10)         \
    MODE(SLOT_11, PD_SLOT_11)         \
    MODE(SLOT_12, PD_SLOT_12)         \
    MODE(SLOT_13, PD_SLOT_13)         \
    MODE(SLOT_14, PD_SLOT_14)         \
    MODE(SLOT_15, PD_SLOT_15)         \
    MODE(SLOT_16, PD_SLOT_16)         \
    MODE(SLOT_17, PD_SLOT_17)         \
    MODE(SLOT_18, PD_SLOT_18)         \
    MODE(SLOT_19, PD_SLOT_19)         \
    MODE(SLOT_20, PD_SLOT_20)         \
    MODE(SLOT_21, PD_SLOT_21)         \
    MODE(SLOT_22, PD_SLOT_22)         \
    MODE(SLOT_23, PD_SLOT_23)         \
    MODE(SLOT_24, PD_SLOT_24)         \
    MODE(SLOT_25, PD_SLOT_25)         \
    MODE(SLOT_26, PD_SLOT_26)         \
    MODE(SLOT_27, PD_SLOT_27)         \
    MODE(SLOT_28, PD_SLOT_28)         \
    MODE(SLOT_29, PD_SLOT_29)         \
    MODE(SLOT_30, PD_SLOT_30)         \
    MODE(SLOT_31, PD_SLOT_31)
