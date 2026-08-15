#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quantum_keycodes.h"

#define NOAH_HOST_TEST_ENV 1

#define PROGMEM
static inline uint8_t pgm_read_byte(const void *addr_) {
    return *(const uint8_t *)addr_;
}
static inline uint16_t pgm_read_word(const void *addr_) {
    return *(const uint16_t *)addr_;
}

#define ARRAY_SIZE(arr_) (sizeof(arr_) / sizeof((arr_)[0]))

#define MATRIX_ROWS 8
#define MATRIX_COLS 8

#ifndef QMK_STUB_SUPPRESS_LAYER_COUNT
#    define LAYER_COUNT 8
#endif

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
#ifndef TAP_CODE_DELAY
#    define TAP_CODE_DELAY 10u
#endif
#ifndef TAP_HOLD_CAPS_DELAY
#    define TAP_HOLD_CAPS_DELAY 80u
#endif
#ifndef DYNAMIC_KEYMAP_LAYER_COUNT
#    define DYNAMIC_KEYMAP_LAYER_COUNT LAYER_COUNT
#endif
#ifndef DYNAMIC_KEYMAP_MACRO_COUNT
#    define DYNAMIC_KEYMAP_MACRO_COUNT 64u
#endif

#ifndef MOD_BIT
#    define MOD_BIT(keycode_) (1u << ((keycode_) & 0x07u))
#endif

#ifndef MOD_LCTL
#    define MOD_LCTL 0x01u
#endif
#ifndef MOD_LSFT
#    define MOD_LSFT 0x02u
#endif
#ifndef MOD_LALT
#    define MOD_LALT 0x04u
#endif
#ifndef MOD_LGUI
#    define MOD_LGUI 0x08u
#endif

#ifndef IS_MOUSEKEY_BUTTON
#    define IS_MOUSEKEY_BUTTON(keycode_) ((keycode_) >= QK_MOUSE_BUTTON_1 && (keycode_) <= QK_MOUSE_BUTTON_8)
#endif

#ifndef POINTER_DEFAULT_DPI_FORWARD
#    define POINTER_DEFAULT_DPI_FORWARD QK_KB_0
#endif
#ifndef POINTER_DEFAULT_DPI_REVERSE
#    define POINTER_DEFAULT_DPI_REVERSE QK_KB_1
#endif
#ifndef POINTER_SNIPING_DPI_FORWARD
#    define POINTER_SNIPING_DPI_FORWARD QK_KB_2
#endif
#ifndef POINTER_SNIPING_DPI_REVERSE
#    define POINTER_SNIPING_DPI_REVERSE QK_KB_3
#endif

#ifndef DPI_MOD
#    define DPI_MOD POINTER_DEFAULT_DPI_FORWARD
#endif
#ifndef DPI_RMOD
#    define DPI_RMOD POINTER_DEFAULT_DPI_REVERSE
#endif
#ifndef S_D_MOD
#    define S_D_MOD POINTER_SNIPING_DPI_FORWARD
#endif
#ifndef S_D_RMOD
#    define S_D_RMOD POINTER_SNIPING_DPI_REVERSE
#endif

typedef enum keyevent_type_t {
    TICK_EVENT  = 0,
    KEY_EVENT   = 1,
    COMBO_EVENT = 4,
} keyevent_type_t;

#define COMBO_END 0
#define COMBO(keys_, result_)  \
    {                          \
        .keys     = (keys_),   \
        .keycode  = (result_), \
        .disabled = false,     \
        .active   = false,     \
        .state    = 0,         \
    }

typedef struct {
    uint8_t row;
    uint8_t col;
} keypos_t;

typedef struct {
    keypos_t key;
    bool     pressed;
    uint8_t  type;
} keyevent_t;

typedef struct {
    uint8_t count;
} tap_t;

typedef struct {
    keyevent_t event;
    tap_t      tap;
    uint16_t   keycode;
} keyrecord_t;

#define MAKE_KEYEVENT(row_, col_, pressed_)        \
    ((keyevent_t){                                 \
        .key     = {.row = (row_), .col = (col_)}, \
        .pressed = (pressed_),                     \
        .type    = KEY_EVENT,                      \
    })

#define MAKE_COMBOEVENT(press_)          \
    ((keyevent_t){                       \
        .key     = {.row = 0, .col = 0}, \
        .pressed = (press_),             \
        .type    = COMBO_EVENT,          \
    })

#define IS_NOEVENT(event_) ((event_).type == TICK_EVENT)

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
    const uint16_t *keys;
    uint16_t        keycode;
#ifdef EXTRA_SHORT_COMBOS
    uint8_t state;
#else
    bool    disabled;
    bool    active;
    uint8_t state;
#endif
} combo_t;

static inline void noah_host_test_fail_runtime_overflow(const char *surface, unsigned int kind, unsigned int capacity) {
    fprintf(stderr, "host test failed: %s overflow dropping effect kind %u after %u queued effects\n", surface, kind, capacity);
    exit(1);
}

uint16_t         timer_read(void);
uint16_t         timer_elapsed(uint16_t last);
uint32_t         timer_read32(void);
uint32_t         timer_elapsed32(uint32_t last);
uint32_t         last_input_activity_elapsed(void);
uint32_t         last_matrix_activity_elapsed(void);
bool             is_keyboard_master(void);
uint32_t         eeconfig_read_user(void);
void             eeconfig_update_user(uint32_t value);
bool             process_record_user(uint16_t keycode, keyrecord_t *record);
bool             process_record(keyrecord_t *record);
typedef uint16_t action_t;
action_t         action_for_keycode(uint16_t keycode);
void             process_action(keyrecord_t *record, action_t action);

extern layer_state_t layer_state;
extern layer_state_t default_layer_state;
bool                 layer_state_cmp(layer_state_t state, uint8_t layer);
void                 layer_on(uint8_t layer);
void                 layer_off(uint8_t layer);
uint8_t              get_highest_layer(layer_state_t state);
uint8_t              combo_ref_from_layer(uint8_t layer);
uint16_t             keymap_key_to_keycode(uint8_t layer, keypos_t key);
uint16_t             get_record_keycode(keyrecord_t *record, bool update_layer_cache);

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
uint16_t charybdis_get_pointer_sniping_dpi(void);
void     charybdis_set_pointer_dragscroll_enabled(bool enabled);
void     charybdis_set_pointer_sniping_enabled(bool enabled);
void     pointing_device_set_cpi(uint16_t cpi);
