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

// The configurable pointing modes in slot order: PD_MODE_n holds slot n's mode
// and PD_MODE_n_LOCK locks it (noah_keymap_ids.h). Thirty-two fill both
// keycode blocks. The names here only name the modes' C constants
// (POINTING_MODE_*, POINTING_MODE_INDEX_*): the six factory presets, then the
// slot number. A mode's name and settings come from the keymap's compiled
// defaults (noah_pd_defaults in pd_config.c).
#define NOAH_POINTING_MODE_LIST(MODE) \
    MODE(DRAGSCROLL, PD_MODE_0)       \
    MODE(VOLUME, PD_MODE_1)           \
    MODE(BRIGHTNESS, PD_MODE_2)       \
    MODE(ZOOM, PD_MODE_3)             \
    MODE(ARROW, PD_MODE_4)            \
    MODE(PINCH, PD_MODE_5)            \
    MODE(6, PD_MODE_6)                \
    MODE(7, PD_MODE_7)                \
    MODE(8, PD_MODE_8)                \
    MODE(9, PD_MODE_9)                \
    MODE(10, PD_MODE_10)              \
    MODE(11, PD_MODE_11)              \
    MODE(12, PD_MODE_12)              \
    MODE(13, PD_MODE_13)              \
    MODE(14, PD_MODE_14)              \
    MODE(15, PD_MODE_15)              \
    MODE(16, PD_MODE_16)              \
    MODE(17, PD_MODE_17)              \
    MODE(18, PD_MODE_18)              \
    MODE(19, PD_MODE_19)              \
    MODE(20, PD_MODE_20)              \
    MODE(21, PD_MODE_21)              \
    MODE(22, PD_MODE_22)              \
    MODE(23, PD_MODE_23)              \
    MODE(24, PD_MODE_24)              \
    MODE(25, PD_MODE_25)              \
    MODE(26, PD_MODE_26)              \
    MODE(27, PD_MODE_27)              \
    MODE(28, PD_MODE_28)              \
    MODE(29, PD_MODE_29)              \
    MODE(30, PD_MODE_30)              \
    MODE(31, PD_MODE_31)
