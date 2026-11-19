#include QMK_KEYBOARD_H
#include "lib/profile/schema/profile_pd_v1.h"
#include "lib/pointing/defs/pointing_mode_defaults.h"

#ifdef NOAH_DRAGSCROLL_REVERSE_X
#    define INVERT_X 1
#else
#    define INVERT_X 0
#endif
#ifdef NOAH_DRAGSCROLL_REVERSE_Y
#    define INVERT_Y 2
#else
#    define INVERT_Y 0
#endif
#define SCROLL_TUNING \
    .scroll = {NOAH_DRAGSCROLL_THRESHOLD_H, NOAH_DRAGSCROLL_THRESHOLD_V, NOAH_DRAGSCROLL_DIVISOR_H, NOAH_DRAGSCROLL_DIVISOR_V, NOAH_DRAGSCROLL_RATE_LIMIT_MS, NOAH_DRAGSCROLL_BUFFER_EXPIRE_MS, NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS}, \
    .scroll_policy = {NOAH_DRAGSCROLL_LOCK_START_RATIO_NUM, NOAH_DRAGSCROLL_LOCK_START_RATIO_DEN, NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_NUM, NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_DEN, NOAH_DRAGSCROLL_CROSS_AXIS_DECAY_DIVISOR, INVERT_X | INVERT_Y}

// Every slot, in ID order. The starter keymap configures only drag-scroll
// (PD_MODE_0, on the Pointer layer); Charybdis Ark sets up the rest. A
// disabled slot without a name is not stored in the profile; it only needs its
// ID here.
const noah_pd_config_t noah_pd_defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT] = {
    {.id=0, .name="Dragscroll", .kind=2, .dpi=CHARYBDIS_DRAGSCROLL_DPI, SCROLL_TUNING},
    {.id=1},  {.id=2},  {.id=3},  {.id=4},  {.id=5},  {.id=6},  {.id=7},
    {.id=8},  {.id=9},  {.id=10}, {.id=11}, {.id=12}, {.id=13}, {.id=14},
    {.id=15}, {.id=16}, {.id=17}, {.id=18}, {.id=19}, {.id=20}, {.id=21}, {.id=22},
    {.id=23}, {.id=24}, {.id=25}, {.id=26}, {.id=27}, {.id=28}, {.id=29}, {.id=30},
    {.id=31},
};
