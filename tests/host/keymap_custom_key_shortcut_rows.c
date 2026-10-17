// CUSTOM_KEYS rows as keymap.c writes them (D-F26), expanded by
// keymap_materialize.h's row checks and shortcut table. The runner compiles
// this file as is and with each SHORTCUT_ROW_BAD case, which must not compile.
#include <stdio.h>
#include <string.h>

#include "noah_keymap.h"

#ifndef SHORTCUT_ROW_BAD
#    define SHORTCUT_ROW_BAD(SHORTCUT)
#endif

#define CUSTOM_KEYS(KEY, SHORTCUT)                                                    \
    KEY(CUSTOM_KEY_0, "Right Thumb")                                                  \
    SHORTCUT(CUSTOM_KEY_4, "Hyper K", SHORTCUT_MODS(KC_LCTL, KC_RALT), KC_K)                         \
    SHORTCUT(CUSTOM_KEY_5, "", SHORTCUT_MODS(KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI), KC_NO)              \
    SHORTCUT(CUSTOM_KEY_7, "All", SHORTCUT_MODS(KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI, KC_RCTL, KC_RSFT, KC_RALT, KC_RGUI), KC_A) \
    SHORTCUT(CUSTOM_KEY_127, "Click", SHORTCUT_NO_MODS, MS_BTN1)                                       \
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
    CHECK(shortcuts[7][0] == 0xff && shortcuts[7][1] == 0x04);
    puts("keymap custom key shortcut rows passed");
    return 0;
}
