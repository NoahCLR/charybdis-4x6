// ────────────────────────────────────────────────────────────────────────────
// Custom Key Shortcuts
// ────────────────────────────────────────────────────────────────────────────
//
// A custom key may carry a shortcut (D-F26): one key, any of the eight
// modifiers, left and right mixed, and any set of layers with the layer key
// they act as, each part optional. The shortcut is the custom key's own
// action, the way an ordinary key's own keycode is: held while the key is
// held, tapped when it is tapped, and what a behaviour row's unauthored cells
// and bypasses fall back to. Its layers act as one layer key on all of them:
// held as MO() holds (on while it is held, through layer ownership, and only
// for a tap's length when it is tapped), locked as TG() locks, moved to as TO()
// moves, held and locked on the TAPPING_TOGGLE-th tap as TT() does, or held
// and one-shot as OSL() is. A step on another key that names a custom key
// sends only that key's shortcut, layers included, never its behaviour.
//
// Stored in settings v10 as five bytes per slot, in slot order: an 8-bit HID
// report modifier mask (bit 0 Left Ctrl, 1 Left Shift, 2 Left Alt, 3 Left GUI,
// 4 Right Ctrl, 5 Right Shift, 6 Right Alt, 7 Right GUI), the key, the 16-bit
// layer mask, low byte first, then the layer kind. All zero means no shortcut;
// any one part alone (a held Hyper, a key, layers) is a shortcut. The base
// layer and the default layers are always on, so a shortcut never names them,
// and without layers the kind is Hold.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    NOAH_CUSTOM_KEY_SHORTCUT_MODS      = 0, // record byte 0: report modifier mask
    NOAH_CUSTOM_KEY_SHORTCUT_KEY       = 1, // record byte 1: the key, zero for none
    NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_LO = 2, // record bytes 2..3: the layer mask, little-endian
    NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_HI = 3,
    NOAH_CUSTOM_KEY_SHORTCUT_LAYER_KIND = 4, // record byte 4: the layer key its layers act as
    NOAH_CUSTOM_KEY_SHORTCUT_SIZE      = 5,
};

// The layer key a shortcut's layers act as, all of them together.
typedef enum {
    NOAH_SHORTCUT_LAYERS_HOLD       = 0, // MO(): on while held, for a tap's length when tapped
    NOAH_SHORTCUT_LAYERS_LOCK       = 1, // TG(): all locked on, or all off when all are locked
    NOAH_SHORTCUT_LAYERS_MOVE       = 2, // TO(): these locked alone, every other lock released
    NOAH_SHORTCUT_LAYERS_TAP_TOGGLE = 3, // TT(): held as Hold; the TAPPING_TOGGLE-th tap locks as Lock
    NOAH_SHORTCUT_LAYERS_ONESHOT    = 4, // OSL(): held as Hold; a tap turns them on for the next key press
    NOAH_SHORTCUT_LAYER_KIND_COUNT,
} noah_shortcut_layer_kind_t;

typedef struct {
    uint8_t  mods;
    uint8_t  key;
    uint16_t layers;
    uint8_t  layer_kind;
} noah_custom_key_shortcut_t;

// The keys a shortcut may hold: none, the keyboard usages QMK's basic
// keycodes name (KC_A 0x04 through KC_EXSEL 0xa4), or mouse buttons 1-8
// (0xd1-0xd8, after the modifiers so Cmd+click works). Modifiers belong in the
// mask; media and system keys, mouse movement and every non-basic keycode
// (layers, macros, pointing modes, custom keys) stay out. Every accepted key is
// a basic keycode, so one byte holds it.
#define NOAH_CUSTOM_KEY_SHORTCUT_KEY_VALID(key_) ((key_) == 0 || ((key_) >= 0x04 && (key_) <= 0xa4) || ((key_) >= 0xd1 && (key_) <= 0xd8))
// A shortcut's layer mask is 16 bits: layers 0-15, the bank settings v10
// stores, of which a shortcut may name 1-15.
#define NOAH_CUSTOM_KEY_SHORTCUT_LAYERS 16

static inline bool noah_custom_key_shortcut_key_valid(uint8_t key) {
    return NOAH_CUSTOM_KEY_SHORTCUT_KEY_VALID(key);
}

// The parts that reach the report: its modifiers and its key.
static inline bool noah_custom_key_shortcut_has_report_parts(noah_custom_key_shortcut_t shortcut) {
    return shortcut.mods != 0u || shortcut.key != 0u;
}

static inline bool noah_custom_key_shortcut_present(noah_custom_key_shortcut_t shortcut) {
    return noah_custom_key_shortcut_has_report_parts(shortcut) || shortcut.layers != 0u;
}

// A shortcut from its stored record of NOAH_CUSTOM_KEY_SHORTCUT_SIZE bytes.
static inline noah_custom_key_shortcut_t noah_custom_key_shortcut_from_record(const uint8_t *record) {
    return (noah_custom_key_shortcut_t){
        .mods   = record[NOAH_CUSTOM_KEY_SHORTCUT_MODS],
        .key    = record[NOAH_CUSTOM_KEY_SHORTCUT_KEY],
        .layers = (uint16_t)(record[NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_LO] | (record[NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_HI] << 8)),
        .layer_kind = record[NOAH_CUSTOM_KEY_SHORTCUT_LAYER_KIND],
    };
}

// Tap-toggle and One-shot shortcuts make their key a dual-role key, as TT()
// and OSL() are: its layers are on from the press, as theirs is, while the
// runtime decides whether the rest is a tap or a hold.
static inline bool noah_custom_key_shortcut_is_dual_role(noah_custom_key_shortcut_t shortcut) {
    return shortcut.layers != 0u && (shortcut.layer_kind == NOAH_SHORTCUT_LAYERS_TAP_TOGGLE || shortcut.layer_kind == NOAH_SHORTCUT_LAYERS_ONESHOT);
}

// The key runtime names a momentary key's own hold by its layer. A dual-role
// custom key's is its shortcut's layers, named 0x80 | slot, past every layer.
#define NOAH_CUSTOM_KEY_MOMENTARY_LAYERS 0x80u
static inline bool noah_custom_key_momentary_layers_named(uint8_t layer) {
    return layer != UINT8_MAX && (layer & NOAH_CUSTOM_KEY_MOMENTARY_LAYERS) != 0u;
}

// The live shortcut of custom key `slot` (0-127): the active profile's settings
// domain's, or before any is live the keymap's own. No shortcut, every field
// zero, for a slot out of range or a build without one.
noah_custom_key_shortcut_t noah_custom_key_shortcut(uint8_t slot);
// The shortcut of `keycode` if it is a custom key; none otherwise.
noah_custom_key_shortcut_t noah_custom_key_shortcut_for_keycode(uint16_t keycode);
bool                       noah_custom_key_has_shortcut(uint16_t keycode);
// Whether `keycode` is a custom key whose shortcut turns layers on.
bool noah_custom_key_has_layers(uint16_t keycode);
// Whether holding `keycode`, a custom key, changes the keys pressed after it
// (D-F27): its shortcut is modifiers alone, as a mods-only keycode is, or turns
// layers on, as MO() does. A profile waits to activate while one is held, so
// the shortcut asked about is the current one.
bool noah_custom_key_hold_changes_other_keys(uint16_t keycode);
// Whether `keycode` is a custom key whose shortcut makes it a dual-role key,
// and whether that shortcut's layers are One-shot.
bool noah_custom_key_is_dual_role(uint16_t keycode);
bool noah_custom_key_is_oneshot(uint16_t keycode);
