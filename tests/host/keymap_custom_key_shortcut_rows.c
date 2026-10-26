// CUSTOM_KEYS rows as keymap.c writes them (D-F26), expanded by
// keymap_materialize.h's row checks and shortcut table. The runner compiles
// this file as is and with each SHORTCUT_ROW_BAD case, which must not compile.
#include <stdio.h>
#include <string.h>

#include "noah_keymap.h"

#ifndef SHORTCUT_ROW_BAD
#    define SHORTCUT_ROW_BAD(SHORTCUT)
#endif

#define CUSTOM_KEYS(KEY, SHORTCUT)                                                                                                         \
    KEY(CUSTOM_KEY_0, "Right Thumb")                                                                                                       \
    SHORTCUT(CUSTOM_KEY_4, "Hyper K", SHORTCUT_MODS(KC_LCTL, KC_RALT), KC_K, SHORTCUT_NO_LAYERS)                                           \
    SHORTCUT(CUSTOM_KEY_5, "", SHORTCUT_MODS(KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI), KC_NO, SHORTCUT_NO_LAYERS)                                \
    SHORTCUT(CUSTOM_KEY_7, "All", SHORTCUT_MODS(KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI, KC_RCTL, KC_RSFT, KC_RALT, KC_RGUI), KC_A, SHORTCUT_NO_LAYERS) \
    SHORTCUT(CUSTOM_KEY_8, "Cmd + Nav", SHORTCUT_MODS(KC_LGUI), KC_NO, SHORTCUT_LAYERS(3, 9))                                            \
    SHORTCUT(CUSTOM_KEY_9, "Layers", SHORTCUT_NO_MODS, KC_NO, SHORTCUT_LAYERS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15))          \
    SHORTCUT(CUSTOM_KEY_10, "Lock", SHORTCUT_NO_MODS, KC_NO, SHORTCUT_LOCK_LAYERS(2, 4))                                                  \
    SHORTCUT(CUSTOM_KEY_11, "Move", SHORTCUT_MODS(KC_LSFT), KC_NO, SHORTCUT_MOVE_TO_LAYERS(15))                                            \
    SHORTCUT(CUSTOM_KEY_12, "Tap-toggle", SHORTCUT_NO_MODS, KC_ESC, SHORTCUT_TAP_TOGGLE_LAYERS(1))                                          \
    SHORTCUT(CUSTOM_KEY_13, "One-shot", SHORTCUT_NO_MODS, KC_NO, SHORTCUT_ONESHOT_LAYERS(5, 6))                                            \
    SHORTCUT(CUSTOM_KEY_127, "Click", SHORTCUT_NO_MODS, MS_BTN1, SHORTCUT_NO_LAYERS)                                                       \
    SHORTCUT_ROW_BAD(SHORTCUT)

CUSTOM_KEYS(_KEYMAP_CUSTOM_KEY_NAME_CHECK, _KEYMAP_CUSTOM_KEY_SHORTCUT_CHECK)
static const char    names[CUSTOM_KEY_SLOT_COUNT][NOAH_MACRO_NAME_SIZE]                = {CUSTOM_KEYS(_KEYMAP_CUSTOM_KEY_NAME_ENTRY, _KEYMAP_CUSTOM_KEY_SHORTCUT_NAME_ENTRY)};
static const uint8_t shortcuts[CUSTOM_KEY_SLOT_COUNT][NOAH_CUSTOM_KEY_SHORTCUT_SIZE] = {CUSTOM_KEYS(_KEYMAP_CUSTOM_KEY_NO_SHORTCUT, _KEYMAP_CUSTOM_KEY_SHORTCUT_ENTRY)};

#define CHECK(expr_)                                                       \
    do {                                                                   \
        if (!(expr_)) {                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr_);    \
            return 1;                                                      \
        }                                                                  \
    } while (0)

int main(void) {
    CHECK(!strcmp(names[0], "Right Thumb") && !strcmp(names[4], "Hyper K") && !strcmp(names[127], "Click"));
    CHECK(shortcuts[0][0] == 0 && shortcuts[0][1] == 0);
    CHECK(shortcuts[4][0] == 0x41 && shortcuts[4][1] == 0x0e);
    CHECK(shortcuts[5][0] == 0x0f && shortcuts[5][1] == 0);
    CHECK(shortcuts[127][0] == 0 && shortcuts[127][1] == 0xd1);
    CHECK(shortcuts[6][0] == 0 && shortcuts[6][1] == 0);
    CHECK(shortcuts[7][0] == 0xff && shortcuts[7][1] == 0x04 && shortcuts[7][2] == 0 && shortcuts[7][3] == 0);
    CHECK(shortcuts[8][0] == 0x08 && shortcuts[8][1] == 0 && shortcuts[8][2] == 0x08 && shortcuts[8][3] == 0x02);
    CHECK(shortcuts[9][0] == 0 && shortcuts[9][1] == 0 && shortcuts[9][2] == 0xfe && shortcuts[9][3] == 0xff && shortcuts[9][4] == NOAH_SHORTCUT_LAYERS_HOLD);
    CHECK(shortcuts[4][4] == NOAH_SHORTCUT_LAYERS_HOLD && shortcuts[0][4] == 0);
    CHECK(shortcuts[10][2] == 0x14 && shortcuts[10][3] == 0 && shortcuts[10][4] == NOAH_SHORTCUT_LAYERS_LOCK);
    CHECK(shortcuts[11][0] == 0x02 && shortcuts[11][3] == 0x80 && shortcuts[11][4] == NOAH_SHORTCUT_LAYERS_MOVE);
    CHECK(shortcuts[12][1] == 0x29 && shortcuts[12][2] == 0x02 && shortcuts[12][4] == NOAH_SHORTCUT_LAYERS_TAP_TOGGLE);
    CHECK(shortcuts[13][2] == 0x60 && shortcuts[13][4] == NOAH_SHORTCUT_LAYERS_ONESHOT);
    puts("keymap custom key shortcut rows passed");
    return 0;
}
