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
    SHORTCUT(CUSTOM_KEY_4, "Hyper K", SHORTCUT_MODS(KC_LCTL, KC_RALT), SHORTCUT_KEYS(KC_K), SHORTCUT_NO_LAYERS)                            \
    SHORTCUT(CUSTOM_KEY_5, "", SHORTCUT_MODS(KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI), SHORTCUT_NO_KEYS, SHORTCUT_NO_LAYERS)                    \
    SHORTCUT(CUSTOM_KEY_7, "All", SHORTCUT_MODS(KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI, KC_RCTL, KC_RSFT, KC_RALT, KC_RGUI), SHORTCUT_KEYS(KC_A), SHORTCUT_NO_LAYERS) \
    SHORTCUT(CUSTOM_KEY_8, "Cmd + Nav", SHORTCUT_MODS(KC_LGUI), SHORTCUT_NO_KEYS, SHORTCUT_LAYERS(3, 9))                                  \
    SHORTCUT(CUSTOM_KEY_9, "Layers", SHORTCUT_NO_MODS, SHORTCUT_NO_KEYS, SHORTCUT_LAYERS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)) \
    SHORTCUT(CUSTOM_KEY_10, "Lock", SHORTCUT_NO_MODS, SHORTCUT_NO_KEYS, SHORTCUT_LOCK_LAYERS(2, 4))                                       \
    SHORTCUT(CUSTOM_KEY_11, "Move", SHORTCUT_MODS(KC_LSFT), SHORTCUT_NO_KEYS, SHORTCUT_MOVE_TO_LAYERS(15))                                 \
    SHORTCUT(CUSTOM_KEY_12, "Tap-toggle", SHORTCUT_NO_MODS, SHORTCUT_KEYS(KC_ESC), SHORTCUT_TAP_TOGGLE_LAYERS(1))                          \
    SHORTCUT(CUSTOM_KEY_13, "One-shot", SHORTCUT_NO_MODS, SHORTCUT_NO_KEYS, SHORTCUT_ONESHOT_LAYERS(5, 6))                                 \
    SHORTCUT(CUSTOM_KEY_14, "Six", SHORTCUT_MODS(KC_LSFT), SHORTCUT_KEYS(KC_A, KC_S, KC_D, KC_LEFT, KC_UP, KC_EXSEL), SHORTCUT_NO_LAYERS) \
    SHORTCUT(CUSTOM_KEY_15, "Media", SHORTCUT_NO_MODS, SHORTCUT_KEYS(KC_LPAD, KC_PWR, MS_WHLR, MS_UP, KC_Z), SHORTCUT_LOCK_LAYERS(2))  \
    SHORTCUT(CUSTOM_KEY_127, "Click", SHORTCUT_NO_MODS, SHORTCUT_KEYS(MS_BTN1), SHORTCUT_NO_LAYERS)                                       \
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
    static const uint8_t six[NOAH_CUSTOM_KEY_SHORTCUT_SIZE]   = {0x02, 0x04, 0x16, 0x07, 0x50, 0x52, 0xa4, 0, 0, NOAH_SHORTCUT_LAYERS_HOLD};
    static const uint8_t media[NOAH_CUSTOM_KEY_SHORTCUT_SIZE] = {0, 0xc2, 0xa5, 0xdc, 0xcd, 0x1d, 0, 0x04, 0, NOAH_SHORTCUT_LAYERS_LOCK};
    static const uint8_t none[NOAH_CUSTOM_KEY_SHORTCUT_SIZE]  = {0};

    _Static_assert(NOAH_CUSTOM_KEY_SHORTCUT_SIZE == 10 && NOAH_CUSTOM_KEY_SHORTCUT_KEYS_OFFSET == 1 && NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_LO == 7 && NOAH_CUSTOM_KEY_SHORTCUT_LAYER_KIND == 9, "a shortcut record is mods, six keys, the layer mask and the kind");
    CHECK(!strcmp(names[0], "Right Thumb") && !strcmp(names[4], "Hyper K") && !strcmp(names[127], "Click"));
    CHECK(!memcmp(shortcuts[0], none, sizeof(none)) && !memcmp(shortcuts[6], none, sizeof(none)));
    CHECK(shortcuts[4][0] == 0x41 && shortcuts[4][1] == 0x0e && shortcuts[4][2] == 0);
    CHECK(shortcuts[5][0] == 0x0f && shortcuts[5][1] == 0);
    CHECK(shortcuts[127][0] == 0 && shortcuts[127][1] == 0xd1 && shortcuts[127][2] == 0);
    CHECK(shortcuts[7][0] == 0xff && shortcuts[7][1] == 0x04 && shortcuts[7][7] == 0 && shortcuts[7][8] == 0);
    CHECK(shortcuts[8][0] == 0x08 && shortcuts[8][1] == 0 && shortcuts[8][7] == 0x08 && shortcuts[8][8] == 0x02);
    CHECK(shortcuts[9][0] == 0 && shortcuts[9][1] == 0 && shortcuts[9][7] == 0xfe && shortcuts[9][8] == 0xff && shortcuts[9][9] == NOAH_SHORTCUT_LAYERS_HOLD);
    CHECK(shortcuts[4][9] == NOAH_SHORTCUT_LAYERS_HOLD && shortcuts[0][9] == 0);
    CHECK(shortcuts[10][7] == 0x14 && shortcuts[10][8] == 0 && shortcuts[10][9] == NOAH_SHORTCUT_LAYERS_LOCK);
    CHECK(shortcuts[11][0] == 0x02 && shortcuts[11][8] == 0x80 && shortcuts[11][9] == NOAH_SHORTCUT_LAYERS_MOVE);
    CHECK(shortcuts[12][1] == 0x29 && shortcuts[12][7] == 0x02 && shortcuts[12][9] == NOAH_SHORTCUT_LAYERS_TAP_TOGGLE);
    CHECK(shortcuts[13][7] == 0x60 && shortcuts[13][9] == NOAH_SHORTCUT_LAYERS_ONESHOT);
    // Several keys in the order written, zero after the last.
    CHECK(!memcmp(shortcuts[14], six, sizeof(six)));
    CHECK(!memcmp(shortcuts[15], media, sizeof(media)));
    puts("keymap custom key shortcut rows passed");
    return 0;
}
