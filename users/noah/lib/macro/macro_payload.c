#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "send_string.h"

#include "../action/owned_keycode.h"
#include "macro_payload.h"

typedef struct {
    const char *name;
    uint8_t     keycode;
} macro_payload_keycode_t;

#define MACRO_PAYLOAD_KEYCODE_NAMES(X)               \
    X(KC_A, X_A)                                     \
    X(KC_B, X_B)                                     \
    X(KC_C, X_C)                                     \
    X(KC_D, X_D)                                     \
    X(KC_E, X_E)                                     \
    X(KC_F, X_F)                                     \
    X(KC_G, X_G)                                     \
    X(KC_H, X_H)                                     \
    X(KC_I, X_I)                                     \
    X(KC_J, X_J)                                     \
    X(KC_K, X_K)                                     \
    X(KC_L, X_L)                                     \
    X(KC_M, X_M)                                     \
    X(KC_N, X_N)                                     \
    X(KC_O, X_O)                                     \
    X(KC_P, X_P)                                     \
    X(KC_Q, X_Q)                                     \
    X(KC_R, X_R)                                     \
    X(KC_S, X_S)                                     \
    X(KC_T, X_T)                                     \
    X(KC_U, X_U)                                     \
    X(KC_V, X_V)                                     \
    X(KC_W, X_W)                                     \
    X(KC_X, X_X)                                     \
    X(KC_Y, X_Y)                                     \
    X(KC_Z, X_Z)                                     \
    X(KC_1, X_1)                                     \
    X(KC_2, X_2)                                     \
    X(KC_3, X_3)                                     \
    X(KC_4, X_4)                                     \
    X(KC_5, X_5)                                     \
    X(KC_6, X_6)                                     \
    X(KC_7, X_7)                                     \
    X(KC_8, X_8)                                     \
    X(KC_9, X_9)                                     \
    X(KC_0, X_0)                                     \
    X(KC_ENTER, X_ENTER)                             \
    X(KC_ESCAPE, X_ESCAPE)                           \
    X(KC_BACKSPACE, X_BACKSPACE)                     \
    X(KC_TAB, X_TAB)                                 \
    X(KC_SPACE, X_SPACE)                             \
    X(KC_MINUS, X_MINUS)                             \
    X(KC_EQUAL, X_EQUAL)                             \
    X(KC_LEFT_BRACKET, X_LEFT_BRACKET)               \
    X(KC_RIGHT_BRACKET, X_RIGHT_BRACKET)             \
    X(KC_BACKSLASH, X_BACKSLASH)                     \
    X(KC_NONUS_HASH, X_NONUS_HASH)                   \
    X(KC_SEMICOLON, X_SEMICOLON)                     \
    X(KC_QUOTE, X_QUOTE)                             \
    X(KC_GRAVE, X_GRAVE)                             \
    X(KC_COMMA, X_COMMA)                             \
    X(KC_DOT, X_DOT)                                 \
    X(KC_SLASH, X_SLASH)                             \
    X(KC_CAPS_LOCK, X_CAPS_LOCK)                     \
    X(KC_F1, X_F1)                                   \
    X(KC_F2, X_F2)                                   \
    X(KC_F3, X_F3)                                   \
    X(KC_F4, X_F4)                                   \
    X(KC_F5, X_F5)                                   \
    X(KC_F6, X_F6)                                   \
    X(KC_F7, X_F7)                                   \
    X(KC_F8, X_F8)                                   \
    X(KC_F9, X_F9)                                   \
    X(KC_F10, X_F10)                                 \
    X(KC_F11, X_F11)                                 \
    X(KC_F12, X_F12)                                 \
    X(KC_PRINT_SCREEN, X_PRINT_SCREEN)               \
    X(KC_SCROLL_LOCK, X_SCROLL_LOCK)                 \
    X(KC_PAUSE, X_PAUSE)                             \
    X(KC_INSERT, X_INSERT)                           \
    X(KC_HOME, X_HOME)                               \
    X(KC_PAGE_UP, X_PAGE_UP)                         \
    X(KC_DELETE, X_DELETE)                           \
    X(KC_END, X_END)                                 \
    X(KC_PAGE_DOWN, X_PAGE_DOWN)                     \
    X(KC_RIGHT, X_RIGHT)                             \
    X(KC_LEFT, X_LEFT)                               \
    X(KC_DOWN, X_DOWN)                               \
    X(KC_UP, X_UP)                                   \
    X(KC_NUM_LOCK, X_NUM_LOCK)                       \
    X(KC_KP_SLASH, X_KP_SLASH)                       \
    X(KC_KP_ASTERISK, X_KP_ASTERISK)                 \
    X(KC_KP_MINUS, X_KP_MINUS)                       \
    X(KC_KP_PLUS, X_KP_PLUS)                         \
    X(KC_KP_ENTER, X_KP_ENTER)                       \
    X(KC_KP_1, X_KP_1)                               \
    X(KC_KP_2, X_KP_2)                               \
    X(KC_KP_3, X_KP_3)                               \
    X(KC_KP_4, X_KP_4)                               \
    X(KC_KP_5, X_KP_5)                               \
    X(KC_KP_6, X_KP_6)                               \
    X(KC_KP_7, X_KP_7)                               \
    X(KC_KP_8, X_KP_8)                               \
    X(KC_KP_9, X_KP_9)                               \
    X(KC_KP_0, X_KP_0)                               \
    X(KC_KP_DOT, X_KP_DOT)                           \
    X(KC_NONUS_BACKSLASH, X_NONUS_BACKSLASH)         \
    X(KC_APPLICATION, X_APPLICATION)                 \
    X(KC_KB_POWER, X_KB_POWER)                       \
    X(KC_KP_EQUAL, X_KP_EQUAL)                       \
    X(KC_F13, X_F13)                                 \
    X(KC_F14, X_F14)                                 \
    X(KC_F15, X_F15)                                 \
    X(KC_F16, X_F16)                                 \
    X(KC_F17, X_F17)                                 \
    X(KC_F18, X_F18)                                 \
    X(KC_F19, X_F19)                                 \
    X(KC_F20, X_F20)                                 \
    X(KC_F21, X_F21)                                 \
    X(KC_F22, X_F22)                                 \
    X(KC_F23, X_F23)                                 \
    X(KC_F24, X_F24)                                 \
    X(KC_EXECUTE, X_EXECUTE)                         \
    X(KC_HELP, X_HELP)                               \
    X(KC_MENU, X_MENU)                               \
    X(KC_SELECT, X_SELECT)                           \
    X(KC_STOP, X_STOP)                               \
    X(KC_AGAIN, X_AGAIN)                             \
    X(KC_UNDO, X_UNDO)                               \
    X(KC_CUT, X_CUT)                                 \
    X(KC_COPY, X_COPY)                               \
    X(KC_PASTE, X_PASTE)                             \
    X(KC_FIND, X_FIND)                               \
    X(KC_KB_MUTE, X_KB_MUTE)                         \
    X(KC_KB_VOLUME_UP, X_KB_VOLUME_UP)               \
    X(KC_KB_VOLUME_DOWN, X_KB_VOLUME_DOWN)           \
    X(KC_LOCKING_CAPS_LOCK, X_LOCKING_CAPS_LOCK)     \
    X(KC_LOCKING_NUM_LOCK, X_LOCKING_NUM_LOCK)       \
    X(KC_LOCKING_SCROLL_LOCK, X_LOCKING_SCROLL_LOCK) \
    X(KC_KP_COMMA, X_KP_COMMA)                       \
    X(KC_KP_EQUAL_AS400, X_KP_EQUAL_AS400)           \
    X(KC_INTERNATIONAL_1, X_INTERNATIONAL_1)         \
    X(KC_INTERNATIONAL_2, X_INTERNATIONAL_2)         \
    X(KC_INTERNATIONAL_3, X_INTERNATIONAL_3)         \
    X(KC_INTERNATIONAL_4, X_INTERNATIONAL_4)         \
    X(KC_INTERNATIONAL_5, X_INTERNATIONAL_5)         \
    X(KC_INTERNATIONAL_6, X_INTERNATIONAL_6)         \
    X(KC_INTERNATIONAL_7, X_INTERNATIONAL_7)         \
    X(KC_INTERNATIONAL_8, X_INTERNATIONAL_8)         \
    X(KC_INTERNATIONAL_9, X_INTERNATIONAL_9)         \
    X(KC_LANGUAGE_1, X_LANGUAGE_1)                   \
    X(KC_LANGUAGE_2, X_LANGUAGE_2)                   \
    X(KC_LANGUAGE_3, X_LANGUAGE_3)                   \
    X(KC_LANGUAGE_4, X_LANGUAGE_4)                   \
    X(KC_LANGUAGE_5, X_LANGUAGE_5)                   \
    X(KC_LANGUAGE_6, X_LANGUAGE_6)                   \
    X(KC_LANGUAGE_7, X_LANGUAGE_7)                   \
    X(KC_LANGUAGE_8, X_LANGUAGE_8)                   \
    X(KC_LANGUAGE_9, X_LANGUAGE_9)                   \
    X(KC_ALTERNATE_ERASE, X_ALTERNATE_ERASE)         \
    X(KC_SYSTEM_REQUEST, X_SYSTEM_REQUEST)           \
    X(KC_CANCEL, X_CANCEL)                           \
    X(KC_CLEAR, X_CLEAR)                             \
    X(KC_PRIOR, X_PRIOR)                             \
    X(KC_RETURN, X_RETURN)                           \
    X(KC_SEPARATOR, X_SEPARATOR)                     \
    X(KC_OUT, X_OUT)                                 \
    X(KC_OPER, X_OPER)                               \
    X(KC_CLEAR_AGAIN, X_CLEAR_AGAIN)                 \
    X(KC_CRSEL, X_CRSEL)                             \
    X(KC_EXSEL, X_EXSEL)                             \
    X(KC_LEFT_CTRL, X_LEFT_CTRL)                     \
    X(KC_LEFT_SHIFT, X_LEFT_SHIFT)                   \
    X(KC_LEFT_ALT, X_LEFT_ALT)                       \
    X(KC_LEFT_GUI, X_LEFT_GUI)                       \
    X(KC_RIGHT_CTRL, X_RIGHT_CTRL)                   \
    X(KC_RIGHT_SHIFT, X_RIGHT_SHIFT)                 \
    X(KC_RIGHT_ALT, X_RIGHT_ALT)                     \
    X(KC_RIGHT_GUI, X_RIGHT_GUI)                     \
    X(KC_SYSTEM_POWER, X_SYSTEM_POWER)               \
    X(KC_SYSTEM_SLEEP, X_SYSTEM_SLEEP)               \
    X(KC_SYSTEM_WAKE, X_SYSTEM_WAKE)                 \
    X(KC_AUDIO_MUTE, X_AUDIO_MUTE)                   \
    X(KC_AUDIO_VOL_UP, X_AUDIO_VOL_UP)               \
    X(KC_AUDIO_VOL_DOWN, X_AUDIO_VOL_DOWN)           \
    X(KC_MEDIA_NEXT_TRACK, X_MEDIA_NEXT_TRACK)       \
    X(KC_MEDIA_PREV_TRACK, X_MEDIA_PREV_TRACK)       \
    X(KC_MEDIA_STOP, X_MEDIA_STOP)                   \
    X(KC_MEDIA_PLAY_PAUSE, X_MEDIA_PLAY_PAUSE)       \
    X(KC_MEDIA_SELECT, X_MEDIA_SELECT)               \
    X(KC_MEDIA_EJECT, X_MEDIA_EJECT)                 \
    X(KC_MAIL, X_MAIL)                               \
    X(KC_CALCULATOR, X_CALCULATOR)                   \
    X(KC_MY_COMPUTER, X_MY_COMPUTER)                 \
    X(KC_WWW_SEARCH, X_WWW_SEARCH)                   \
    X(KC_WWW_HOME, X_WWW_HOME)                       \
    X(KC_WWW_BACK, X_WWW_BACK)                       \
    X(KC_WWW_FORWARD, X_WWW_FORWARD)                 \
    X(KC_WWW_STOP, X_WWW_STOP)                       \
    X(KC_WWW_REFRESH, X_WWW_REFRESH)                 \
    X(KC_WWW_FAVORITES, X_WWW_FAVORITES)             \
    X(KC_MEDIA_FAST_FORWARD, X_MEDIA_FAST_FORWARD)   \
    X(KC_MEDIA_REWIND, X_MEDIA_REWIND)               \
    X(KC_BRIGHTNESS_UP, X_BRIGHTNESS_UP)             \
    X(KC_BRIGHTNESS_DOWN, X_BRIGHTNESS_DOWN)         \
    X(KC_CONTROL_PANEL, X_CONTROL_PANEL)             \
    X(KC_ASSISTANT, X_ASSISTANT)                     \
    X(KC_MS_UP, X_MS_UP)                             \
    X(KC_MS_DOWN, X_MS_DOWN)                         \
    X(KC_MS_LEFT, X_MS_LEFT)                         \
    X(KC_MS_RIGHT, X_MS_RIGHT)                       \
    X(KC_MS_BTN1, X_MS_BTN1)                         \
    X(KC_MS_BTN2, X_MS_BTN2)                         \
    X(KC_MS_BTN3, X_MS_BTN3)                         \
    X(KC_MS_BTN4, X_MS_BTN4)                         \
    X(KC_MS_BTN5, X_MS_BTN5)                         \
    X(KC_MS_BTN6, X_MS_BTN6)                         \
    X(KC_MS_BTN7, X_MS_BTN7)                         \
    X(KC_MS_BTN8, X_MS_BTN8)                         \
    X(KC_MS_WH_UP, X_MS_WH_UP)                       \
    X(KC_MS_WH_DOWN, X_MS_WH_DOWN)                   \
    X(KC_MS_WH_LEFT, X_MS_WH_LEFT)                   \
    X(KC_MS_WH_RIGHT, X_MS_WH_RIGHT)                 \
    X(KC_MS_ACCEL0, X_MS_ACCEL0)                     \
    X(KC_MS_ACCEL1, X_MS_ACCEL1)                     \
    X(KC_MS_ACCEL2, X_MS_ACCEL2)                     \
    X(KC_ENT, X_ENTER)                               \
    X(KC_ESC, X_ESCAPE)                              \
    X(KC_BSPC, X_BACKSPACE)                          \
    X(KC_SPC, X_SPACE)                               \
    X(KC_MINS, X_MINUS)                              \
    X(KC_EQL, X_EQUAL)                               \
    X(KC_LBRC, X_LEFT_BRACKET)                       \
    X(KC_RBRC, X_RIGHT_BRACKET)                      \
    X(KC_BSLS, X_BACKSLASH)                          \
    X(KC_NUHS, X_NONUS_HASH)                         \
    X(KC_SCLN, X_SEMICOLON)                          \
    X(KC_QUOT, X_QUOTE)                              \
    X(KC_GRV, X_GRAVE)                               \
    X(KC_COMM, X_COMMA)                              \
    X(KC_SLSH, X_SLASH)                              \
    X(KC_NUBS, X_NONUS_BACKSLASH)                    \
    X(KC_CAPS, X_CAPS_LOCK)                          \
    X(KC_SCRL, X_SCROLL_LOCK)                        \
    X(KC_NUM, X_NUM_LOCK)                            \
    X(KC_LCAP, X_LOCKING_CAPS_LOCK)                  \
    X(KC_LNUM, X_LOCKING_NUM_LOCK)                   \
    X(KC_LSCR, X_LOCKING_SCROLL_LOCK)                \
    X(KC_PSCR, X_PRINT_SCREEN)                       \
    X(KC_PAUS, X_PAUSE)                              \
    X(KC_BRK, X_PAUSE)                               \
    X(KC_INS, X_INSERT)                              \
    X(KC_PGUP, X_PAGE_UP)                            \
    X(KC_DEL, X_DELETE)                              \
    X(KC_PGDN, X_PAGE_DOWN)                          \
    X(KC_RGHT, X_RIGHT)                              \
    X(KC_APP, X_APPLICATION)                         \
    X(KC_EXEC, X_EXECUTE)                            \
    X(KC_SLCT, X_SELECT)                             \
    X(KC_AGIN, X_AGAIN)                              \
    X(KC_PSTE, X_PASTE)                              \
    X(KC_ERAS, X_ALTERNATE_ERASE)                    \
    X(KC_SYRQ, X_SYSTEM_REQUEST)                     \
    X(KC_CNCL, X_CANCEL)                             \
    X(KC_CLR, X_CLEAR)                               \
    X(KC_PRIR, X_PRIOR)                              \
    X(KC_RETN, X_RETURN)                             \
    X(KC_SEPR, X_SEPARATOR)                          \
    X(KC_CLAG, X_CLEAR_AGAIN)                        \
    X(KC_CRSL, X_CRSEL)                              \
    X(KC_EXSL, X_EXSEL)                              \
    X(KC_PSLS, X_KP_SLASH)                           \
    X(KC_PAST, X_KP_ASTERISK)                        \
    X(KC_PMNS, X_KP_MINUS)                           \
    X(KC_PPLS, X_KP_PLUS)                            \
    X(KC_PENT, X_KP_ENTER)                           \
    X(KC_P1, X_KP_1)                                 \
    X(KC_P2, X_KP_2)                                 \
    X(KC_P3, X_KP_3)                                 \
    X(KC_P4, X_KP_4)                                 \
    X(KC_P5, X_KP_5)                                 \
    X(KC_P6, X_KP_6)                                 \
    X(KC_P7, X_KP_7)                                 \
    X(KC_P8, X_KP_8)                                 \
    X(KC_P9, X_KP_9)                                 \
    X(KC_P0, X_KP_0)                                 \
    X(KC_PDOT, X_KP_DOT)                             \
    X(KC_PEQL, X_KP_EQUAL)                           \
    X(KC_PCMM, X_KP_COMMA)                           \
    X(KC_INT1, X_INTERNATIONAL_1)                    \
    X(KC_INT2, X_INTERNATIONAL_2)                    \
    X(KC_INT3, X_INTERNATIONAL_3)                    \
    X(KC_INT4, X_INTERNATIONAL_4)                    \
    X(KC_INT5, X_INTERNATIONAL_5)                    \
    X(KC_INT6, X_INTERNATIONAL_6)                    \
    X(KC_INT7, X_INTERNATIONAL_7)                    \
    X(KC_INT8, X_INTERNATIONAL_8)                    \
    X(KC_INT9, X_INTERNATIONAL_9)                    \
    X(KC_LNG1, X_LANGUAGE_1)                         \
    X(KC_LNG2, X_LANGUAGE_2)                         \
    X(KC_LNG3, X_LANGUAGE_3)                         \
    X(KC_LNG4, X_LANGUAGE_4)                         \
    X(KC_LNG5, X_LANGUAGE_5)                         \
    X(KC_LNG6, X_LANGUAGE_6)                         \
    X(KC_LNG7, X_LANGUAGE_7)                         \
    X(KC_LNG8, X_LANGUAGE_8)                         \
    X(KC_LNG9, X_LANGUAGE_9)                         \
    X(KC_LCTL, X_LEFT_CTRL)                          \
    X(KC_LSFT, X_LEFT_SHIFT)                         \
    X(KC_LALT, X_LEFT_ALT)                           \
    X(KC_LOPT, X_LEFT_ALT)                           \
    X(KC_LGUI, X_LEFT_GUI)                           \
    X(KC_LCMD, X_LEFT_GUI)                           \
    X(KC_LWIN, X_LEFT_GUI)                           \
    X(KC_RCTL, X_RIGHT_CTRL)                         \
    X(KC_RSFT, X_RIGHT_SHIFT)                        \
    X(KC_RALT, X_RIGHT_ALT)                          \
    X(KC_ALGR, X_RIGHT_ALT)                          \
    X(KC_ROPT, X_RIGHT_ALT)                          \
    X(KC_RGUI, X_RIGHT_GUI)                          \
    X(KC_RCMD, X_RIGHT_GUI)                          \
    X(KC_RWIN, X_RIGHT_GUI)                          \
    X(KC_PWR, X_SYSTEM_POWER)                        \
    X(KC_SLEP, X_SYSTEM_SLEEP)                       \
    X(KC_WAKE, X_SYSTEM_WAKE)                        \
    X(KC_MUTE, X_AUDIO_MUTE)                         \
    X(KC_VOLU, X_AUDIO_VOL_UP)                       \
    X(KC_VOLD, X_AUDIO_VOL_DOWN)                     \
    X(KC_MNXT, X_MEDIA_NEXT_TRACK)                   \
    X(KC_MPRV, X_MEDIA_PREV_TRACK)                   \
    X(KC_MSTP, X_MEDIA_STOP)                         \
    X(KC_MPLY, X_MEDIA_PLAY_PAUSE)                   \
    X(KC_MSEL, X_MEDIA_SELECT)                       \
    X(KC_EJCT, X_MEDIA_EJECT)                        \
    X(KC_CALC, X_CALCULATOR)                         \
    X(KC_MYCM, X_MY_COMPUTER)                        \
    X(KC_WSCH, X_WWW_SEARCH)                         \
    X(KC_WHOM, X_WWW_HOME)                           \
    X(KC_WBAK, X_WWW_BACK)                           \
    X(KC_WFWD, X_WWW_FORWARD)                        \
    X(KC_WSTP, X_WWW_STOP)                           \
    X(KC_WREF, X_WWW_REFRESH)                        \
    X(KC_WFAV, X_WWW_FAVORITES)                      \
    X(KC_MFFD, X_MEDIA_FAST_FORWARD)                 \
    X(KC_MRWD, X_MEDIA_REWIND)                       \
    X(KC_BRIU, X_BRIGHTNESS_UP)                      \
    X(KC_BRID, X_BRIGHTNESS_DOWN)                    \
    X(KC_CPNL, X_CONTROL_PANEL)                      \
    X(KC_ASST, X_ASSISTANT)                          \
    X(KC_BRMU, X_PAUSE)                              \
    X(KC_BRMD, X_SCROLL_LOCK)                        \
    X(KC_MS_U, X_MS_UP)                              \
    X(KC_MS_D, X_MS_DOWN)                            \
    X(KC_MS_L, X_MS_LEFT)                            \
    X(KC_MS_R, X_MS_RIGHT)                           \
    X(KC_BTN1, X_MS_BTN1)                            \
    X(KC_BTN2, X_MS_BTN2)                            \
    X(KC_BTN3, X_MS_BTN3)                            \
    X(KC_BTN4, X_MS_BTN4)                            \
    X(KC_BTN5, X_MS_BTN5)                            \
    X(KC_BTN6, X_MS_BTN6)                            \
    X(KC_BTN7, X_MS_BTN7)                            \
    X(KC_BTN8, X_MS_BTN8)                            \
    X(KC_WH_U, X_MS_WH_UP)                           \
    X(KC_WH_D, X_MS_WH_DOWN)                         \
    X(KC_WH_L, X_MS_WH_LEFT)                         \
    X(KC_WH_R, X_MS_WH_RIGHT)                        \
    X(KC_ACL0, X_MS_ACCEL0)                          \
    X(KC_ACL1, X_MS_ACCEL1)                          \
    X(KC_ACL2, X_MS_ACCEL2)

static const macro_payload_keycode_t macro_payload_keycodes[] = {
#define MACRO_PAYLOAD_KEYCODE_U8(value_) MACRO_PAYLOAD_KEYCODE_U8_IMPL(value_)
#define MACRO_PAYLOAD_KEYCODE_U8_IMPL(value_) ((uint8_t)(0x##value_))
#define MACRO_PAYLOAD_KEYCODE_ENTRY(name_, keycode_) {.name = #name_, .keycode = MACRO_PAYLOAD_KEYCODE_U8(keycode_)},
    MACRO_PAYLOAD_KEYCODE_NAMES(MACRO_PAYLOAD_KEYCODE_ENTRY)
#undef MACRO_PAYLOAD_KEYCODE_U8_IMPL
#undef MACRO_PAYLOAD_KEYCODE_U8
#undef MACRO_PAYLOAD_KEYCODE_ENTRY
};

#define MACRO_PAYLOAD_MAX_TAP_KEYS 16

typedef enum {
    MACRO_PAYLOAD_COMMAND_DELAY,
    MACRO_PAYLOAD_COMMAND_KEY_DOWN,
    MACRO_PAYLOAD_COMMAND_KEY_UP,
    MACRO_PAYLOAD_COMMAND_TAP_LIST,
} macro_payload_command_kind_t;

typedef struct {
    uint8_t keycodes[MACRO_PAYLOAD_MAX_TAP_KEYS];
    uint8_t count;
} macro_payload_tap_list_t;

typedef struct {
    macro_payload_command_kind_t kind;
    uint16_t                     delay_ms;
    uint8_t                      keycode;
    macro_payload_tap_list_t     tap_list;
} macro_payload_command_t;

typedef bool (*macro_payload_text_visitor_t)(char c, void *context);
typedef bool (*macro_payload_command_visitor_t)(const macro_payload_command_t *command, void *context);

static bool macro_payload_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void macro_payload_trim(const char **start, const char **end) {
    while (*start < *end && macro_payload_is_space(**start)) {
        (*start)++;
    }
    while (*start < *end && macro_payload_is_space((*end)[-1])) {
        (*end)--;
    }
}

static bool macro_payload_lookup_keycode(const char *start, size_t length, uint8_t *keycode) {
    for (size_t i = 0; i < sizeof(macro_payload_keycodes) / sizeof(macro_payload_keycodes[0]); i++) {
        const macro_payload_keycode_t *entry = &macro_payload_keycodes[i];

        if (strlen(entry->name) != length) {
            continue;
        }
        if (memcmp(entry->name, start, length) != 0) {
            continue;
        }

        *keycode = entry->keycode;
        return true;
    }

    return false;
}

static bool macro_payload_parse_delay(const char *start, const char *end, uint16_t *delay_ms) {
    uint32_t delay = 0;

    if (start == end) {
        return false;
    }

    for (const char *cursor = start; cursor < end; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        delay = (delay * 10) + (uint32_t)(*cursor - '0');
        if (delay > UINT16_MAX) {
            delay = UINT16_MAX;
        }
    }

    *delay_ms = (uint16_t)delay;
    return true;
}

static void macro_payload_wait_interval(void) {
    wait_ms(TAP_CODE_DELAY);
}

static bool macro_payload_parse_tap_list(const char *start, const char *end, uint8_t *keycodes, size_t capacity, size_t *keycode_count) {
    size_t      count      = 0;
    const char *item_start = start;

    for (const char *cursor = start; cursor <= end; cursor++) {
        if (cursor != end && *cursor != ',') {
            continue;
        }

        const char *trimmed_start = item_start;
        const char *trimmed_end   = cursor;
        uint8_t     keycode       = 0;

        macro_payload_trim(&trimmed_start, &trimmed_end);
        if (trimmed_start == trimmed_end) {
            return false;
        }
        if (count == capacity) {
            return false;
        }
        if (!macro_payload_lookup_keycode(trimmed_start, (size_t)(trimmed_end - trimmed_start), &keycode)) {
            return false;
        }

        keycodes[count++] = keycode;
        item_start        = cursor + 1;
    }

    if (count == 0) {
        return false;
    }

    *keycode_count = count;
    return true;
}

static bool macro_payload_parse_command(const char *start, const char *end, macro_payload_command_t *command) {
    const char *trimmed_start = start;
    const char *trimmed_end   = end;
    uint8_t     keycode       = 0;
    uint16_t    delay_ms      = 0;
    size_t      keycode_count = 0;

    macro_payload_trim(&trimmed_start, &trimmed_end);
    if (trimmed_start == trimmed_end) {
        return false;
    }

    if (macro_payload_parse_delay(trimmed_start, trimmed_end, &delay_ms)) {
        command->kind     = MACRO_PAYLOAD_COMMAND_DELAY;
        command->delay_ms = delay_ms;
        return true;
    }

    if (*trimmed_start == '+' || *trimmed_start == '-') {
        bool        is_keydown = *trimmed_start == '+';
        const char *key_start  = trimmed_start + 1;
        const char *key_end    = trimmed_end;

        macro_payload_trim(&key_start, &key_end);
        if (key_start == key_end) {
            return false;
        }
        if (memchr(key_start, ',', (size_t)(key_end - key_start)) != NULL) {
            return false;
        }
        if (!macro_payload_lookup_keycode(key_start, (size_t)(key_end - key_start), &keycode)) {
            return false;
        }

        command->kind    = is_keydown ? MACRO_PAYLOAD_COMMAND_KEY_DOWN : MACRO_PAYLOAD_COMMAND_KEY_UP;
        command->keycode = keycode;
        return true;
    }

    if (!macro_payload_parse_tap_list(trimmed_start, trimmed_end, command->tap_list.keycodes, sizeof(command->tap_list.keycodes) / sizeof(command->tap_list.keycodes[0]), &keycode_count)) {
        return false;
    }

    command->kind           = MACRO_PAYLOAD_COMMAND_TAP_LIST;
    command->tap_list.count = (uint8_t)keycode_count;
    return true;
}

static bool macro_payload_run_command(const macro_payload_command_t *command) {
    switch (command->kind) {
        case MACRO_PAYLOAD_COMMAND_DELAY:
            wait_ms(command->delay_ms);
            macro_payload_wait_interval();
            return true;
        case MACRO_PAYLOAD_COMMAND_KEY_DOWN:
            (void)owned_keycode_register(command->keycode);
            macro_payload_wait_interval();
            return true;
        case MACRO_PAYLOAD_COMMAND_KEY_UP:
            (void)owned_keycode_unregister(command->keycode);
            macro_payload_wait_interval();
            return true;
        case MACRO_PAYLOAD_COMMAND_TAP_LIST:
            for (uint8_t i = 0; i + 1 < command->tap_list.count; i++) {
                (void)owned_keycode_register(command->tap_list.keycodes[i]);
            }

            (void)owned_keycode_tap(command->tap_list.keycodes[command->tap_list.count - 1]);

            for (uint8_t i = command->tap_list.count - 1; i > 0; i--) {
                (void)owned_keycode_unregister(command->tap_list.keycodes[i - 1]);
            }

            macro_payload_wait_interval();
            return true;
    }

    return false;
}

typedef struct {
    uint8_t *buffer;
    uint16_t capacity;
    uint16_t length;
} macro_payload_buffer_t;

static bool macro_payload_buffer_write_byte(macro_payload_buffer_t *state, uint8_t byte) {
    if (state->length >= state->capacity) {
        return false;
    }

    state->buffer[state->length++] = byte;
    return true;
}

static bool macro_payload_buffer_write_delay(macro_payload_buffer_t *state, uint16_t delay_ms) {
    char   digits[5];
    size_t digit_count = 0;

    if (!macro_payload_buffer_write_byte(state, SS_QMK_PREFIX) || !macro_payload_buffer_write_byte(state, SS_DELAY_CODE)) {
        return false;
    }

    do {
        digits[digit_count++] = (char)('0' + (delay_ms % 10));
        delay_ms /= 10;
    } while (delay_ms > 0 && digit_count < sizeof(digits));

    while (digit_count > 0) {
        digit_count--;
        if (!macro_payload_buffer_write_byte(state, (uint8_t)digits[digit_count])) {
            return false;
        }
    }

    return macro_payload_buffer_write_byte(state, (uint8_t)'|');
}

static bool macro_payload_buffer_write_key_action(macro_payload_buffer_t *state, uint8_t action, uint8_t keycode) {
    return macro_payload_buffer_write_byte(state, SS_QMK_PREFIX) && macro_payload_buffer_write_byte(state, action) && macro_payload_buffer_write_byte(state, keycode);
}

static bool macro_payload_encode_command(const macro_payload_command_t *command, macro_payload_buffer_t *state) {
    switch (command->kind) {
        case MACRO_PAYLOAD_COMMAND_DELAY:
            return macro_payload_buffer_write_delay(state, command->delay_ms);
        case MACRO_PAYLOAD_COMMAND_KEY_DOWN:
            return macro_payload_buffer_write_key_action(state, SS_DOWN_CODE, command->keycode);
        case MACRO_PAYLOAD_COMMAND_KEY_UP:
            return macro_payload_buffer_write_key_action(state, SS_UP_CODE, command->keycode);
        case MACRO_PAYLOAD_COMMAND_TAP_LIST:
            for (uint8_t i = 0; i + 1 < command->tap_list.count; i++) {
                if (!macro_payload_buffer_write_key_action(state, SS_DOWN_CODE, command->tap_list.keycodes[i])) {
                    return false;
                }
            }

            if (!macro_payload_buffer_write_key_action(state, SS_TAP_CODE, command->tap_list.keycodes[command->tap_list.count - 1])) {
                return false;
            }

            for (uint8_t i = command->tap_list.count - 1; i > 0; i--) {
                if (!macro_payload_buffer_write_key_action(state, SS_UP_CODE, command->tap_list.keycodes[i - 1])) {
                    return false;
                }
            }

            return true;
    }

    return false;
}

static bool macro_payload_visit_text_noop(char c, void *context) {
    (void)c;
    (void)context;
    return true;
}

static bool macro_payload_visit_command_noop(const macro_payload_command_t *command, void *context) {
    (void)command;
    (void)context;
    return true;
}

static bool macro_payload_visit_text_send_char(char c, void *context) {
    (void)context;
    send_char(c);
    return true;
}

static bool macro_payload_visit_command_run(const macro_payload_command_t *command, void *context) {
    (void)context;
    return macro_payload_run_command(command);
}

static bool macro_payload_visit_text_write_byte(char c, void *context) {
    macro_payload_buffer_t *state = (macro_payload_buffer_t *)context;

    return macro_payload_buffer_write_byte(state, (uint8_t)c);
}

static bool macro_payload_visit_command_encode(const macro_payload_command_t *command, void *context) {
    macro_payload_buffer_t *state = (macro_payload_buffer_t *)context;

    return macro_payload_encode_command(command, state);
}

static bool macro_payload_visit(const char *payload, macro_payload_text_visitor_t visit_text, macro_payload_command_visitor_t visit_command, void *context) {
    const char *cursor = payload;

    if (!payload || !visit_text || !visit_command) {
        return false;
    }

    while (*cursor) {
        if (*cursor == '{') {
            const char             *command_start = cursor + 1;
            const char             *command_end   = command_start;
            macro_payload_command_t command       = {0};

            while (*command_end && *command_end != '}') {
                command_end++;
            }
            if (*command_end != '}') {
                return false;
            }
            if (!macro_payload_parse_command(command_start, command_end, &command)) {
                return false;
            }
            if (!visit_command(&command, context)) {
                return false;
            }

            cursor = command_end + 1;
            continue;
        }

        if (*cursor == '}') {
            return false;
        }
        if ((uint8_t)*cursor > 0x7F) {
            return false;
        }
        if (!visit_text(*cursor, context)) {
            return false;
        }

        cursor++;
    }

    return true;
}

bool macro_payload_validate(const char *payload) {
    return macro_payload_visit(payload, macro_payload_visit_text_noop, macro_payload_visit_command_noop, NULL);
}

bool macro_payload_play(const char *payload) {
    return macro_payload_visit(payload, macro_payload_visit_text_send_char, macro_payload_visit_command_run, NULL);
}

bool macro_payload_encode(const char *payload, uint8_t *buffer, uint16_t capacity, uint16_t *written) {
    macro_payload_buffer_t state = {
        .buffer   = buffer,
        .capacity = capacity,
        .length   = 0,
    };

    if (written) {
        *written = 0;
    }

    if (!macro_payload_visit(payload, macro_payload_visit_text_write_byte, macro_payload_visit_command_encode, &state)) {
        return false;
    }

    if (written) {
        *written = state.length;
    }

    return true;
}
