#include QMK_KEYBOARD_H
#include "lib/profile/schema/profile_pd_v1.h"
#include "lib/pointing/defs/pd_mode_defaults.h"

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

const noah_pd_config_t noah_pd_defaults[8] = {
    {.id=0, .name="Dragscroll", .kind=2, .dpi=CHARYBDIS_DRAGSCROLL_DPI, SCROLL_TUNING},
    {.id=1, .name="Volume", .kind=1, .dpi=PD_MODE_VOLUME_DPI, .threshold_y=VOLUME_THRESHOLD,
     .directions={[2]={KC_AUDIO_VOL_UP,0,0}, [3]={KC_AUDIO_VOL_DOWN,0,0}}},
    {.id=2, .name="Brightness", .kind=1, .dpi=PD_MODE_BRIGHTNESS_DPI, .threshold_y=BRIGHTNESS_THRESHOLD,
     .directions={[2]={KC_BRIGHTNESS_UP,0,0}, [3]={KC_BRIGHTNESS_DOWN,0,0}}},
    {.id=3, .name="Zoom", .kind=1, .dpi=PD_MODE_ZOOM_DPI, .threshold_y=ZOOM_THRESHOLD,
     .directions={[2]={G(KC_EQL),0,0}, [3]={G(KC_MINS),0,0}}},
    {.id=4, .name="Arrow", .kind=1, .pointer_layer=1, .axis=2, .dpi=PD_MODE_ARROW_DPI,
     .threshold_x=ARROW_THRESHOLD_X, .threshold_y=ARROW_THRESHOLD_Y,
     .directions={{KC_LEFT,0,0}, {KC_RIGHT,0,0}, {KC_UP,1,0x44}, {KC_DOWN,1,0x44}},
     .buttons={{3,0x20,{0}}, {2,0,{G(KC_C),2,0}}, {2,0,{G(KC_V),2,0}}}},
    {.id=5, .name="Pinch", .kind=2, .dpi=CHARYBDIS_DRAGSCROLL_DPI, .held_modifiers=0x08, SCROLL_TUNING},
    {.id=6, .name="Undo / Redo", .kind=1, .axis=1, .dpi=100, .threshold_x=40,
     .directions={{G(KC_Z),0,0}, {S(G(KC_Z)),0,0}}},
    {.id=7},
};
