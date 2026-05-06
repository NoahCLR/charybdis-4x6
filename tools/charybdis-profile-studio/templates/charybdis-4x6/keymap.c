// {{PROFILE_TITLE}} Charybdis 4x6 keymap data.

#define NOAH_KEYMAP_EMPTY_COMBOS
#define NOAH_KEYMAP_EMPTY_KEY_BEHAVIORS

#include "keycodes.h"
#include "lib/key/behavior/key_behavior.h"
#include "noah_keymap.h"

enum keymap_custom_keycodes {
    KEYMAP_CUSTOM_KEYCODE_SENTINEL = NOAH_KEYMAP_SAFE_RANGE - 1,
    // MY_CUSTOM_KEY,
};

#define VIA_MACROS(MACRO)            \
    MACRO(VIA_MACRO_0, "")           \
    MACRO(VIA_MACRO_1, "")           \
    MACRO(VIA_MACRO_2, "")           \
    MACRO(VIA_MACRO_3, "")           \
    MACRO(VIA_MACRO_4, "")           \
    MACRO(VIA_MACRO_5, "")           \
    MACRO(VIA_MACRO_6, "")           \
    MACRO(VIA_MACRO_7, "")           \
    MACRO(VIA_MACRO_8, "")           \
    MACRO(VIA_MACRO_9, "")           \
    MACRO(VIA_MACRO_10, "")          \
    MACRO(VIA_MACRO_11, "")          \
    MACRO(VIA_MACRO_12, "")          \
    MACRO(VIA_MACRO_13, "")          \
    MACRO(VIA_MACRO_14, "")          \
    MACRO(VIA_MACRO_15, "")          \
    MACRO(VIA_MACRO_16, "")          \
    MACRO(VIA_MACRO_17, "")          \
    MACRO(VIA_MACRO_18, "")          \
    MACRO(VIA_MACRO_19, "")          \
    MACRO(VIA_MACRO_20, "")          \
    MACRO(VIA_MACRO_21, "")          \
    MACRO(VIA_MACRO_22, "")          \
    MACRO(VIA_MACRO_23, "")          \
    MACRO(VIA_MACRO_24, "")          \
    MACRO(VIA_MACRO_25, "")          \
    MACRO(VIA_MACRO_26, "")          \
    MACRO(VIA_MACRO_27, "")          \
    MACRO(VIA_MACRO_28, "")          \
    MACRO(VIA_MACRO_29, "")          \
    MACRO(VIA_MACRO_30, "")          \
    MACRO(VIA_MACRO_31, "")          \
    MACRO(VIA_MACRO_32, "")          \
    MACRO(VIA_MACRO_33, "")          \
    MACRO(VIA_MACRO_34, "")          \
    MACRO(VIA_MACRO_35, "")          \
    MACRO(VIA_MACRO_36, "")          \
    MACRO(VIA_MACRO_37, "")          \
    MACRO(VIA_MACRO_38, "")          \
    MACRO(VIA_MACRO_39, "")          \
    MACRO(VIA_MACRO_40, "")          \
    MACRO(VIA_MACRO_41, "")          \
    MACRO(VIA_MACRO_42, "")          \
    MACRO(VIA_MACRO_43, "")          \
    MACRO(VIA_MACRO_44, "")          \
    MACRO(VIA_MACRO_45, "")          \
    MACRO(VIA_MACRO_46, "")          \
    MACRO(VIA_MACRO_47, "")          \
    MACRO(VIA_MACRO_48, "")          \
    MACRO(VIA_MACRO_49, "")          \
    MACRO(VIA_MACRO_50, "")          \
    MACRO(VIA_MACRO_51, "")          \
    MACRO(VIA_MACRO_52, "")          \
    MACRO(VIA_MACRO_53, "")          \
    MACRO(VIA_MACRO_54, "")          \
    MACRO(VIA_MACRO_55, "")          \
    MACRO(VIA_MACRO_56, "")          \
    MACRO(VIA_MACRO_57, "")          \
    MACRO(VIA_MACRO_58, "")          \
    MACRO(VIA_MACRO_59, "")          \
    MACRO(VIA_MACRO_60, "")          \
    MACRO(VIA_MACRO_61, "")          \
    MACRO(VIA_MACRO_62, "")          \
    MACRO(VIA_MACRO_63, "")

#define HARDCODED_MACROS(MACRO) \
    MACRO(MACRO_0, "")          \
    MACRO(MACRO_1, "")          \
    MACRO(MACRO_2, "")          \
    MACRO(MACRO_3, "")          \
    MACRO(MACRO_4, "")          \
    MACRO(MACRO_5, "")          \
    MACRO(MACRO_6, "")          \
    MACRO(MACRO_7, "")          \
    MACRO(MACRO_8, "")          \
    MACRO(MACRO_9, "")          \
    MACRO(MACRO_10, "")         \
    MACRO(MACRO_11, "")         \
    MACRO(MACRO_12, "")         \
    MACRO(MACRO_13, "")         \
    MACRO(MACRO_14, "")         \
    MACRO(MACRO_15, "")

#define COMBOS(COMBO)                     \
    /* COMBO(KC_TAB, (KC_D, KC_F)) */     \
    /* COMBO(MACRO_0, (KC_Q, KC_W)) */    \
    /* COMBO(VIA_MACRO_0, (KC_U, KC_I)) */\
    /* COMBO(..., (...)) */

const key_behavior_t key_behaviors[] = {
    {0},
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [LAYER_BASE] = LAYOUT(
        KC_ESC,        KC_1,          KC_2,          KC_3,          KC_4,          KC_5,          KC_6,          KC_7,          KC_8,          KC_9,          KC_0,          KC_MINS,
        KC_TAB,        KC_Q,          KC_W,          KC_E,          KC_R,          KC_T,          KC_Y,          KC_U,          KC_I,          KC_O,          KC_P,          KC_BSLS,
        KC_LEFT_SHIFT, KC_A,          KC_S,          KC_D,          KC_F,          KC_G,          KC_H,          KC_J,          KC_K,          KC_L,          KC_SCLN,       KC_QUOT,
        KC_LEFT_CTRL,  KC_Z,          KC_X,          KC_C,          KC_V,          KC_B,          KC_N,          KC_M,          KC_COMM,       KC_DOT,        KC_SLSH,       KC_RIGHT_ALT,
        KC_LEFT_GUI,   KC_SPC,        KC_BSPC,       KC_ENT,        KC_NO,
                       KC_DEL,        KC_BSPC,       KC_NO
    ),

    [LAYER_NUM] = LAYOUT(
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______,
                 _______, _______, _______
    ),

    [LAYER_SYM] = LAYOUT(
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______,
                 _______, _______, _______
    ),

    [LAYER_NAV] = LAYOUT(
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______,
                 _______, _______, _______
    ),

    [LAYER_POINTER] = LAYOUT(
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______,
                 _______, _______, _______
    ),
};

MATERIALIZE_KEYMAP_DATA();
