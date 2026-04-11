#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "quantum_keycodes.h"

#define PROGMEM

#define ARRAY_SIZE(arr_) (sizeof(arr_) / sizeof((arr_)[0]))

#define MATRIX_ROWS 8
#define MATRIX_COLS 8

#ifndef QMK_STUB_SUPPRESS_LAYER_COUNT
#    define LAYER_COUNT 8
#endif

#define KC_NO 0x0000u
#define KC_TRNS 0x0001u
#define KC_LEFT_CTRL 0x00E0u
#define KC_LEFT_SHIFT 0x00E1u
#define KC_LEFT_ALT 0x00E2u
#define KC_LEFT_GUI 0x00E3u
#define KC_RIGHT_CTRL 0x00E4u
#define KC_RIGHT_SHIFT 0x00E5u
#define KC_RIGHT_ALT 0x00E6u
#define KC_RIGHT_GUI 0x00E7u
#define KC_CAPS_LOCK 0x0039u
#define KC_LEFT 0x0050u
#define KC_RIGHT 0x004Fu
#define KC_DOWN 0x0051u
#define KC_UP 0x0052u
#define KC_C 0x0006u
#define KC_V 0x0019u
#define KC_MINS 0x002Du
#define KC_EQL 0x002Eu
#define KC_AUDIO_VOL_DOWN 0x00A9u
#define KC_AUDIO_VOL_UP 0x00AAu
#define KC_BRID 0x006Fu
#define KC_BRIU 0x0070u
#define QK_USER 0x7E40u
#define SAFE_RANGE QK_USER

#define MOD_BIT(keycode_) (1u << ((keycode_) & 0x07u))

#ifndef CUSTOM_TAP_HOLD_TERM
#    define CUSTOM_TAP_HOLD_TERM 200u
#endif
#ifndef CUSTOM_LONGER_HOLD_TERM
#    define CUSTOM_LONGER_HOLD_TERM 350u
#endif
#ifndef CUSTOM_MULTI_TAP_TERM
#    define CUSTOM_MULTI_TAP_TERM 175u
#endif
#ifndef TAPPING_TERM
#    define TAPPING_TERM 200u
#endif
#ifndef KEY_BEHAVIOR_MAX_TAP_COUNT
#    define KEY_BEHAVIOR_MAX_TAP_COUNT 5u
#endif
#define TAP_CODE_DELAY 10u
#define TAP_HOLD_CAPS_DELAY 80u
#ifndef DYNAMIC_KEYMAP_LAYER_COUNT
#    define DYNAMIC_KEYMAP_LAYER_COUNT LAYER_COUNT
#endif
#ifndef DYNAMIC_KEYMAP_MACRO_COUNT
#    define DYNAMIC_KEYMAP_MACRO_COUNT 16u
#endif

#define QK_MODS 0x0100u
#define QK_MODS_MAX 0x1FFFu
#define QK_LAYER_TAP 0x4000u
#define QK_LAYER_TAP_MAX 0x4FFFu
#define QK_MOMENTARY 0x5220u
#define QK_MOMENTARY_MAX 0x523Fu
#define QK_MOUSE_BUTTON_1 0x00D1u
#define QK_MOUSE_BUTTON_8 0x00D8u
#define QK_LCTL 0x0100u
#define QK_LSFT 0x0200u
#define QK_LALT 0x0400u
#define QK_LGUI 0x0800u
#define QK_RMODS_MIN 0x1000u

#define G(keycode_) ((uint16_t)(QK_LGUI | ((keycode_) & 0x00FFu)))

#define MS_BTN1 ((uint16_t)(QK_MOUSE_BUTTON_1 + 0))
#define MS_BTN2 ((uint16_t)(QK_MOUSE_BUTTON_1 + 1))
#define MS_BTN3 ((uint16_t)(QK_MOUSE_BUTTON_1 + 2))

#define MO(layer_) ((uint16_t)(QK_MOMENTARY | ((layer_) & 0x001Fu)))
#define LT(layer_, keycode_) ((uint16_t)(QK_LAYER_TAP | (((layer_) & 0x000Fu) << 8) | ((keycode_) & 0x00FFu)))

#define IS_QK_MOMENTARY(keycode_) ((keycode_) >= QK_MOMENTARY && (keycode_) <= QK_MOMENTARY_MAX)
#define IS_QK_LAYER_TAP(keycode_) ((keycode_) >= QK_LAYER_TAP && (keycode_) <= QK_LAYER_TAP_MAX)
#define IS_QK_MODS(keycode_) ((keycode_) >= QK_MODS && (keycode_) <= QK_MODS_MAX)
#define IS_QK_MACRO(keycode_) ((keycode_) >= QK_MACRO_0 && (keycode_) < (QK_MACRO_0 + DYNAMIC_KEYMAP_MACRO_COUNT))
#define IS_QK_TO(keycode_) (false)
#define IS_QK_DEF_LAYER(keycode_) (false)
#define IS_QK_TOGGLE_LAYER(keycode_) (false)
#define IS_QK_ONE_SHOT_LAYER(keycode_) (false)
#define IS_QK_LAYER_TAP_TOGGLE(keycode_) (false)
#define IS_QK_LAYER_MOD(keycode_) (false)
#define IS_QK_ONE_SHOT_MOD(keycode_) (false)
#define IS_QK_MOD_TAP(keycode_) (false)
#define IS_MOUSEKEY_BUTTON(keycode_) ((keycode_) >= QK_MOUSE_BUTTON_1 && (keycode_) <= QK_MOUSE_BUTTON_8)
#define IS_MODIFIER_KEYCODE(keycode_) (((keycode_) & 0xFFF8u) == KC_LEFT_CTRL)
#define QK_MODS_GET_BASIC_KEYCODE(keycode_) ((uint8_t)((keycode_) & 0x00FFu))
#define QK_MOMENTARY_GET_LAYER(keycode_) ((uint8_t)((keycode_) & 0x001Fu))
#define QK_LAYER_TAP_GET_LAYER(keycode_) ((uint8_t)(((keycode_) >> 8) & 0x000Fu))
#define QK_LAYER_TAP_GET_TAP_KEYCODE(keycode_) ((uint8_t)((keycode_) & 0x00FFu))

#define COMBO_END 0
#define COMBO(keys_, result_) {0}

typedef struct {
    uint8_t row;
    uint8_t col;
} keypos_t;

typedef struct {
    keypos_t key;
    bool     pressed;
} keyevent_t;

typedef struct {
    uint8_t count;
} tap_t;

typedef struct {
    keyevent_t event;
    tap_t      tap;
    uint16_t   keycode;
} keyrecord_t;

#define MAKE_KEYEVENT(row_, col_, pressed_) \
    ((keyevent_t){                          \
        .key     = {.row = (row_), .col = (col_)}, \
        .pressed = (pressed_),             \
    })

#define IS_NOEVENT(event_) (false)

typedef struct {
    int8_t  x;
    int8_t  y;
    int8_t  h;
    int8_t  v;
    uint8_t buttons;
} report_mouse_t;

typedef uint32_t layer_state_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

typedef struct {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} hsv_t;

typedef struct {
    uint16_t dummy;
} combo_t;

uint16_t timer_read(void);
uint16_t timer_elapsed(uint16_t last);
uint32_t timer_read32(void);
uint32_t timer_elapsed32(uint32_t last);
bool     is_keyboard_master(void);
void     eeconfig_update_user(uint32_t value);
bool     process_record_user(uint16_t keycode, keyrecord_t *record);
bool     process_record(keyrecord_t *record);
typedef uint16_t action_t;
action_t action_for_keycode(uint16_t keycode);
void     process_action(keyrecord_t *record, action_t action);

extern layer_state_t layer_state;
bool                 layer_state_cmp(layer_state_t state, uint8_t layer);
void                 layer_on(uint8_t layer);
void                 layer_off(uint8_t layer);

uint8_t get_mods(void);
uint8_t get_weak_mods(void);
uint8_t get_oneshot_mods(void);
uint8_t get_oneshot_locked_mods(void);
void    set_mods(uint8_t mods);
void    set_weak_mods(uint8_t mods);
void    set_oneshot_mods(uint8_t mods);
void    set_oneshot_locked_mods(uint8_t mods);
void    clear_mods(void);
void    clear_weak_mods(void);
void    clear_oneshot_mods(void);
void    clear_oneshot_locked_mods(void);
void    add_mods(uint8_t mods);
void    del_mods(uint8_t mods);
void    send_keyboard_report(void);
void    tap_code16(uint16_t keycode);
void    register_code(uint8_t keycode);
void    unregister_code(uint8_t keycode);
void    register_code16(uint16_t keycode);
void    unregister_code16(uint16_t keycode);
void    wait_ms(uint16_t ms);

bool     charybdis_get_pointer_dragscroll_enabled(void);
bool     charybdis_get_pointer_sniping_enabled(void);
uint16_t charybdis_get_pointer_default_dpi(void);
void     charybdis_set_pointer_dragscroll_enabled(bool enabled);
void     charybdis_set_pointer_sniping_enabled(bool enabled);
void     pointing_device_set_cpi(uint16_t cpi);
