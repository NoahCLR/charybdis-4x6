// ────────────────────────────────────────────────────────────────────────────
// Custom Key Shortcuts
// ────────────────────────────────────────────────────────────────────────────
//
// A custom key may carry a shortcut (D-F26): up to six keys held together,
// any of the eight modifiers, left and right mixed, and any set of layers with the
// layer key they act as, each part optional. The shortcut is the custom key's
// own action, the way an ordinary key's own keycode is: held while the key is
// held, tapped when it is tapped, and what a behaviour row's unauthored cells
// and bypasses fall back to. Its modifiers go down first, then its keys in
// order, all held together, and they come back up in reverse. Its layers act
// as one layer key on all of them: held as MO() holds (on while it is held,
// through layer ownership, and only for a tap's length when it is tapped),
// locked as TG() locks, moved to as TO() moves, held and locked on the
// TAPPING_TOGGLE-th tap as TT() does, or held and one-shot as OSL() is. A step
// on another key that names a custom key sends only that key's shortcut,
// layers included, never its behaviour.
//
// Stored in settings v11 as ten bytes per slot, in slot order: an 8-bit HID
// report modifier mask (bit 0 Left Ctrl, 1 Left Shift, 2 Left Alt, 3 Left GUI,
// 4 Right Ctrl, 5 Right Shift, 6 Right Alt, 7 Right GUI), six key bytes in the
// order they are pressed, zero after the last, the 16-bit layer mask, low byte
// first, then the layer kind. All zero means no shortcut; any one part alone (a
// held Hyper, a key, layers) is a shortcut. The base layer and the default
// layers are always on, so a shortcut never names them, and without layers the
// kind is Hold.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

// How many keys a shortcut holds: six, the 6-key report's limit, whatever the
// build (no NKRO). This header owns the number; the owned output's chord takes
// its size from it, and compat/qmk_report_usage_contract.h holds it to QMK's
// report.
enum { NOAH_CUSTOM_KEY_SHORTCUT_KEY_COUNT = 6 };

// The record's bytes.
enum {
    NOAH_CUSTOM_KEY_SHORTCUT_MODS        = 0, // record byte 0: report modifier mask
    NOAH_CUSTOM_KEY_SHORTCUT_KEYS_OFFSET = 1, // record bytes 1..6: the keys in press order, zero after the last
    NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_LO   = 7, // record bytes 7..8: the layer mask, little-endian
    NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_HI   = 8,
    NOAH_CUSTOM_KEY_SHORTCUT_LAYER_KIND  = 9, // record byte 9: the layer key its layers act as
    NOAH_CUSTOM_KEY_SHORTCUT_SIZE        = 10,
};
_Static_assert(NOAH_CUSTOM_KEY_SHORTCUT_KEYS_OFFSET + NOAH_CUSTOM_KEY_SHORTCUT_KEY_COUNT == NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_LO, "the record holds every key between its modifiers and its layers");

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
    uint8_t  keys[NOAH_CUSTOM_KEY_SHORTCUT_KEY_COUNT]; // in press order, zero after the last
    uint16_t layers;
    uint8_t  layer_kind;
} noah_custom_key_shortcut_t;

// The keys a shortcut may hold: the basic keycodes for usages a host sees.
// Keyboard keys (KC_A 0x04 through KC_EXSEL 0xa4), the system keys (0xa5-0xa7),
// the consumer (media) keys (0xa8-0xc2), and the mouse keys that move, click
// and scroll (0xcd-0xdc), by QMK's ranges in quantum/keycodes.h;
// compat/qmk_report_usage_contract.h holds these numbers to the selected QMK's.
// Modifiers belong in the mask; mouse-key acceleration (0xdd-0xdf) sends
// nothing of its own, and every non-basic
// keycode (layers, macros, pointing modes, custom keys) is a behaviour's job.
// Every key is a basic keycode, so one byte holds it. The system and consumer
// reports carry one usage each, so a shortcut holds at most one of each.
#define NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_SYSTEM(key_) ((key_) >= 0xa5 && (key_) <= 0xa7)
#define NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_CONSUMER(key_) ((key_) >= 0xa8 && (key_) <= 0xc2)
#define NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_KEY(key_) (((key_) >= 0x04 && (key_) <= 0xc2) || ((key_) >= 0xcd && (key_) <= 0xdc))
// A shortcut's layer mask is 16 bits: layers 0-15, the bank settings v11
// stores, of which a shortcut may name 1-15.
#define NOAH_CUSTOM_KEY_SHORTCUT_LAYERS 16

// A key byte: a key, or zero for none.
static inline bool noah_custom_key_shortcut_key_valid(uint8_t key) {
    return key == 0u || NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_KEY(key);
}

// How many keys a shortcut's key list holds: they are packed from the first,
// and zero ends them. Every reader of a key list counts it here.
static inline uint8_t noah_custom_key_shortcut_key_count(const uint8_t keys[NOAH_CUSTOM_KEY_SHORTCUT_KEY_COUNT]) {
    uint8_t count = 0u;

    while (count < NOAH_CUSTOM_KEY_SHORTCUT_KEY_COUNT && keys[count] != 0u) {
        count++;
    }
    return count;
}

static inline bool noah_custom_key_shortcut_has_keys(noah_custom_key_shortcut_t shortcut) {
    return noah_custom_key_shortcut_key_count(shortcut.keys) != 0u;
}

// The parts that reach the report: its modifiers and its keys.
static inline bool noah_custom_key_shortcut_has_report_parts(noah_custom_key_shortcut_t shortcut) {
    return shortcut.mods != 0u || noah_custom_key_shortcut_has_keys(shortcut);
}

static inline bool noah_custom_key_shortcut_present(noah_custom_key_shortcut_t shortcut) {
    return noah_custom_key_shortcut_has_report_parts(shortcut) || shortcut.layers != 0u;
}

// A shortcut from its stored record of NOAH_CUSTOM_KEY_SHORTCUT_SIZE bytes.
static inline noah_custom_key_shortcut_t noah_custom_key_shortcut_from_record(const uint8_t *record) {
    noah_custom_key_shortcut_t shortcut = {
        .mods       = record[NOAH_CUSTOM_KEY_SHORTCUT_MODS],
        .layers     = (uint16_t)(record[NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_LO] | (record[NOAH_CUSTOM_KEY_SHORTCUT_LAYERS_HI] << 8)),
        .layer_kind = record[NOAH_CUSTOM_KEY_SHORTCUT_LAYER_KIND],
    };
    for (uint8_t key = 0u; key < NOAH_CUSTOM_KEY_SHORTCUT_KEY_COUNT; key++)
        shortcut.keys[key] = record[NOAH_CUSTOM_KEY_SHORTCUT_KEYS_OFFSET + key];
    return shortcut;
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
