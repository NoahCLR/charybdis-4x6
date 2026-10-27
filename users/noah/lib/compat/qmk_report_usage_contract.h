// QMK's basic-keycode ranges for the usages a custom key's shortcut may hold,
// and the 6-key report that holds its keys (D-F26). custom_key_shortcut.h
// states them as numbers, so the settings validator and the keymap's
// compile-time checks need no QMK header; this holds those numbers to the
// selected QMK's.
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../action/custom_key_shortcut.h"

_Static_assert(KC_A == 0x04 && KC_EXSEL == 0xa4, "shortcut keyboard keys are QMK's basic keycodes");
_Static_assert(KC_SYSTEM_POWER == 0xa5 && KC_SYSTEM_WAKE == 0xa7, "shortcut system keys are QMK's system keycodes");
_Static_assert(KC_AUDIO_MUTE == 0xa8 && KC_LAUNCHPAD == 0xc2, "shortcut media keys are QMK's consumer keycodes");
_Static_assert(QK_MOUSE_CURSOR_UP == 0xcd && QK_MOUSE_WHEEL_RIGHT == 0xdc && QK_MOUSE_ACCELERATION_0 == 0xdd, "shortcut mouse keys are QMK's, up to the wheel");
// A held shortcut's keyboard keys all sit in the 6-key report at once, so the
// record holds no more than it has slots (no NKRO).
_Static_assert(NOAH_CUSTOM_KEY_SHORTCUT_KEY_COUNT <= KEYBOARD_REPORT_KEYS, "a shortcut holds no more keys than QMK's 6-key report");
_Static_assert(NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_SYSTEM(KC_SYSTEM_SLEEP) && NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_CONSUMER(KC_MEDIA_PLAY_PAUSE) && !NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_KEY(QK_MOUSE_ACCELERATION_2),
               "the shortcut key classes follow QMK's ranges");
