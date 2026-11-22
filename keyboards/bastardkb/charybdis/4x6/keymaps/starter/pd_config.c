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
//
// To add a mode, fill an empty slot's record and name it, then put PD_MODE_n
// (runs while held) or PD_MODE_n_LOCK (locks on) on a layer in keymap.c. Its
// colour is the slot's row in rgb_config.c's pointing_mode_colors[]. For
// example, in place of {.id=1} and {.id=2}:
//   {.id=1, .name="Volume", .kind=1, .threshold_y=60,
//    .directions={[2]={KC_VOLU,0,0}, [3]={KC_VOLD,0,0}}},
//   {.id=2, .name="Zoom", .kind=2, .axis=2, .dpi=CHARYBDIS_DRAGSCROLL_DPI, .held_modifiers=0x01, SCROLL_TUNING},
// The first sends volume up and down as the ball rolls up and down; the second
// scrolls vertically with Ctrl held, which zooms in most apps.
//
// Fields:
//   .name            what Charybdis Ark shows: at most 32 bytes of UTF-8
//   .kind            1 directional (sends actions), 2 scrolling; 0 is empty
//   .axis            directional: 0 vertical, 1 horizontal, 2 dominant axis,
//                    3 eight directions
//                    scrolling: 0 both, 1 horizontal only, 2 vertical only
//   .dpi             pointer speed while it runs; 0 keeps the normal speed
//   .pointer_layer   0 keeps the pointer layer on, 1 gives way to the typing
//                    layer
// Directional:
//   .threshold_x, .threshold_y   sensor counts per step
//   .directions      left, right, up, down, each {action, policy, mask}
//   .diagonals       up-left, up-right, down-left, down-right, for .axis 3
//                    action: a key, a shortcut such as C(KC_Z), MS_BTN1 to
//                    MS_BTN8, or MACRO_n
//                    policy: 0 adds the modifiers you hold, 1 leaves out
//                    those in mask, 2 sends the action alone
//   .direction_output = NOAH_PD_DIRECTION_OUTPUT_ONCE   once per movement
//                    instead of once per step
// Scrolling:
//   .held_modifiers  held while it scrolls: 0x01 Ctrl, 0x02 Shift, 0x04 Alt,
//                    0x08 GUI (left; the right ones are 0x10 to 0x80)
//   SCROLL_TUNING    the scroll tuning above, as Dragscroll uses it
// Link, left out for none:
//   .linked_layers   a mask of the layers that start the mode, such as
//                    (1u << LAYER_RAISE)
//   .link_turns_on   a mask of the layers its key turns on
//   .link_ends       NOAH_PD_LINK_ENDS_HIGHER_LAYER or
//                    NOAH_PD_LINK_ENDS_OTHER_LAYER
// The constant names and every field are in
// users/noah/lib/profile/schema/profile_pd_v1.h.
const noah_pd_config_t noah_pd_defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT] = {
    {.id=0, .name="Dragscroll", .kind=2, .dpi=CHARYBDIS_DRAGSCROLL_DPI, SCROLL_TUNING},
    {.id=1},  {.id=2},  {.id=3},  {.id=4},  {.id=5},  {.id=6},  {.id=7},
    {.id=8},  {.id=9},  {.id=10}, {.id=11}, {.id=12}, {.id=13}, {.id=14},
    {.id=15}, {.id=16}, {.id=17}, {.id=18}, {.id=19}, {.id=20}, {.id=21}, {.id=22},
    {.id=23}, {.id=24}, {.id=25}, {.id=26}, {.id=27}, {.id=28}, {.id=29}, {.id=30},
    {.id=31},
};
