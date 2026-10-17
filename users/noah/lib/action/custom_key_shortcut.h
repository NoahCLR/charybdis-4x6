// ────────────────────────────────────────────────────────────────────────────
// Custom Key Shortcuts
// ────────────────────────────────────────────────────────────────────────────
//
// A custom key may carry a shortcut (D-F26): one key and any of the eight
// modifiers, left and right mixed. The shortcut is the custom key's own
// action, the way an ordinary key's own keycode is: held while the key is
// held, tapped when it is tapped, and what a behaviour row's unauthored cells
// and bypasses fall back to. A step on another key that names a custom key
// sends only that key's shortcut, never its behaviour.
//
// Stored in settings v8 as two bytes per slot, in slot order: an 8-bit HID
// report modifier mask (bit 0 Left Ctrl, 1 Left Shift, 2 Left Alt, 3 Left GUI,
// 4 Right Ctrl, 5 Right Shift, 6 Right Alt, 7 Right GUI), then the key. Both
// zero means no shortcut. The mask alone (a held Hyper) and the key alone are
// both shortcuts.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    NOAH_CUSTOM_KEY_SHORTCUT_MODS = 0, // record byte 0: report modifier mask
    NOAH_CUSTOM_KEY_SHORTCUT_KEY  = 1, // record byte 1: the key, zero for none
    NOAH_CUSTOM_KEY_SHORTCUT_SIZE = 2,
};

typedef struct {
    uint8_t mods;
    uint8_t key;
} noah_custom_key_shortcut_t;

// The keys a shortcut may hold: none, the keyboard usages QMK's basic
// keycodes name (KC_A 0x04 through KC_EXSEL 0xa4), or mouse buttons 1-8
// (0xd1-0xd8, after the modifiers so Cmd+click works). Modifiers belong in the
// mask; media and system keys, mouse movement and every non-basic keycode
// (layers, macros, pointing modes, custom keys) stay out. Every accepted key is
// a basic keycode, so one byte holds it.
#define NOAH_CUSTOM_KEY_SHORTCUT_KEY_VALID(key_) ((key_) == 0 || ((key_) >= 0x04 && (key_) <= 0xa4) || ((key_) >= 0xd1 && (key_) <= 0xd8))

static inline bool noah_custom_key_shortcut_key_valid(uint8_t key) {
    return NOAH_CUSTOM_KEY_SHORTCUT_KEY_VALID(key);
}

static inline bool noah_custom_key_shortcut_present(noah_custom_key_shortcut_t shortcut) {
    return shortcut.mods != 0u || shortcut.key != 0u;
}

// The live shortcut of custom key `slot` (0-127): the effective settings
// domain's, or before any is live the keymap's own. No shortcut, both fields
// zero, for a slot out of range or a build without one.
noah_custom_key_shortcut_t noah_custom_key_shortcut(uint8_t slot);
// The shortcut of `keycode` if it is a custom key; none otherwise.
noah_custom_key_shortcut_t noah_custom_key_shortcut_for_keycode(uint16_t keycode);
bool                       noah_custom_key_has_shortcut(uint16_t keycode);
