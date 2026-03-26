// ────────────────────────────────────────────────────────────────────────────
// Key Configuration — what each key does
// ────────────────────────────────────────────────────────────────────────────
//
// This file holds all the behavioral data for the keymap: per-feature config
// tables, macro definitions, and LAYOUT arrays.  The processing logic that
// uses these tables lives in keymap.c.
//
// Want to change what a key does?  Edit this file.
// Want to change how keys are processed?  Edit keymap.c.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

#include "lib/key_types.h"

// ─── Layers ─────────────────────────────────────────────────────────────────
//
// If you add or remove a layer, also update DYNAMIC_KEYMAP_LAYER_COUNT in config.h.
enum charybdis_keymap_layers {
    LAYER_BASE = 0, // Default QWERTY typing layer
    LAYER_NUM,      // Numpad on the right half
    LAYER_LOWER,    // Symbols and DPI controls
    LAYER_RAISE,    // Navigation, media, and mouse buttons (sniping enabled)
    LAYER_POINTER,  // Auto-mouse layer: activates on trackball movement, deactivates after timeout
    LAYER_COUNT,
};

// ─── Custom Keycodes ────────────────────────────────────────────────────────
//
// Custom keycodes are assigned values starting from SAFE_RANGE so they don't
// collide with any built-in QMK or Charybdis keycodes.
//
// MACRO_0–15 are generic macro slots (some used, rest reserved for VIA).
// VOLUME_MODE / BRIGHTNESS_MODE / ARROW_MODE / ZOOM_MODE activate pointing
// device modes while held.
// DRG_TOG_ON_HOLD is a dual-purpose key: tap sends the base-layer key at
// that position, hold toggles drag-scroll lock.
// LAYER_LOCK_BASE reserves LAYER_COUNT keycodes for layer locking via
// actions authored in key_behaviors[].  Use the LAYER_LOCK(n) macro there.

enum custom_keycodes {
    MACRO_0 = SAFE_RANGE,
    MACRO_1,
    MACRO_2,
    MACRO_3,
    MACRO_4,
    MACRO_5,
    MACRO_6,  // reserved for VIA
    MACRO_7,  // reserved for VIA
    MACRO_8,  // reserved for VIA
    MACRO_9,  // reserved for VIA
    MACRO_10, // reserved for VIA
    MACRO_11, // reserved for VIA
    MACRO_12, // reserved for VIA
    MACRO_13, // reserved for VIA
    MACRO_14, // reserved for VIA
    MACRO_15, // reserved for VIA
    VOLUME_MODE,
    BRIGHTNESS_MODE,
    ARROW_MODE,
    ZOOM_MODE,
    DRG_TOG_ON_HOLD,
    LAYER_LOCK_BASE, // reserves LAYER_COUNT keycodes — use LAYER_LOCK(n) macro
    CUSTOM_KEYCODES_END = LAYER_LOCK_BASE + LAYER_COUNT,
};

// Lock a layer on from any tap or hold tier in key_behaviors[].
// Example: {MO(LAYER_LOWER), {[1] = STEP_TAP_HOLD(KC_MPLY, LAYER_LOCK(LAYER_NUM), true)}}
#define LAYER_LOCK(layer) (LAYER_LOCK_BASE + (layer))

// ─── Key Behavior Tables ────────────────────────────────────────────────────
//
// key_behaviors[] is the single authored behavior table for keys handled by
// the custom state machine.  One row describes one physical key.
//
// Each row contains up to KEY_BEHAVIOR_MAX_TAP_COUNT steps:
//   steps[0] = single press
//   steps[1] = double tap
//   steps[2] = triple tap
//
// Within each step:
//   tap_overrides_default = false → use the key's normal tap behavior
//   tap_overrides_default = true  → use tap_action
//   hold        = second tier (≥ CUSTOM_TAP_HOLD_TERM)
//   longer_hold = third tier  (≥ CUSTOM_LONGER_HOLD_TERM)
//
// This keeps all behavior for one key in one place instead of splitting it
// across separate hold and multi-tap tables.
//
// Timing thresholds are defined in config.h:
//   CUSTOM_TAP_HOLD_TERM    — tap vs hold boundary (150ms)
//   CUSTOM_LONGER_HOLD_TERM — hold vs longer-hold boundary (400ms)
//   CUSTOM_MULTI_TAP_TERM   — max gap between consecutive taps (150ms)
//   COMBO_TERM              — max gap between keys for combos (50ms)

// Helper macros for concise key_behaviors[] entries.
#define HOLD_TIER(action_, immediate_) \
    { .present = true, .action = (action_), .immediate = (immediate_) }

#define STEP_HOLD(hold_action, immediate_) \
    { .present = true, .hold = HOLD_TIER((hold_action), (immediate_)) }

#define STEP_HOLD_LONGER(hold_action, hold_immediate, longer_action, longer_immediate) \
    {                                                                                    \
        .present     = true,                                                             \
        .hold        = HOLD_TIER((hold_action), (hold_immediate)),                       \
        .longer_hold = HOLD_TIER((longer_action), (longer_immediate)),                   \
    }

#define STEP_TAP(tap_action_) \
    { .present = true, .tap_overrides_default = true, .tap_action = (tap_action_) }

#define STEP_TAP_HOLD(tap_action_, hold_action, hold_immediate) \
    {                                                           \
        .present               = true,                          \
        .tap_overrides_default = true,                          \
        .tap_action            = (tap_action_),                 \
        .hold                  = HOLD_TIER((hold_action), (hold_immediate)), \
    }

#define STEP_TAP_HOLD_LONGER(tap_action_, hold_action, hold_immediate, longer_action, longer_immediate) \
    {                                                                                                     \
        .present               = true,                                                                    \
        .tap_overrides_default = true,                                                                    \
        .tap_action            = (tap_action_),                                                           \
        .hold                  = HOLD_TIER((hold_action), (hold_immediate)),                             \
        .longer_hold           = HOLD_TIER((longer_action), (longer_immediate)),                         \
    }

#define KEY_BEHAVIOR(keycode_, ...) \
    { .keycode = (keycode_), .steps = { __VA_ARGS__ } }

// keycode                single / double / triple behavior
static const key_behavior_t key_behaviors[] = {
    // Number row — shifted symbols on hold, media on double-tap
    KEY_BEHAVIOR(KC_1, [0] = STEP_HOLD(KC_EXLM, true)),
    KEY_BEHAVIOR(KC_2, [0] = STEP_HOLD(KC_AT, true)),
    KEY_BEHAVIOR(KC_3, [0] = STEP_HOLD(KC_HASH, true)),
    KEY_BEHAVIOR(KC_4, [0] = STEP_HOLD(KC_DLR, true)),
    KEY_BEHAVIOR(KC_5, [0] = STEP_HOLD(KC_PERC, true)),
    KEY_BEHAVIOR(KC_6, [0] = STEP_HOLD(KC_CIRC, true), [1] = STEP_TAP(KC_MPLY)),
    KEY_BEHAVIOR(KC_7, [0] = STEP_HOLD(KC_AMPR, true), [1] = STEP_TAP(KC_MNXT)),
    KEY_BEHAVIOR(KC_8, [0] = STEP_HOLD(KC_ASTR, true), [1] = STEP_TAP(KC_MPRV)),
    KEY_BEHAVIOR(KC_9, [0] = STEP_HOLD(KC_LPRN, true)),
    KEY_BEHAVIOR(KC_0, [0] = STEP_HOLD(KC_RPRN, true)),

    // Punctuation → shifted variants
    KEY_BEHAVIOR(KC_MINS, [0] = STEP_HOLD(KC_UNDS, true)),
    KEY_BEHAVIOR(KC_EQL, [0] = STEP_HOLD(KC_PLUS, true)),
    KEY_BEHAVIOR(KC_LBRC, [0] = STEP_HOLD(KC_LCBR, true)),
    KEY_BEHAVIOR(KC_RBRC, [0] = STEP_HOLD(KC_RCBR, true)),
    KEY_BEHAVIOR(KC_BSLS, [0] = STEP_HOLD(KC_PIPE, true)),
    KEY_BEHAVIOR(KC_GRV, [0] = STEP_HOLD(KC_TILD, true)),
    KEY_BEHAVIOR(KC_SCLN, [0] = STEP_HOLD(KC_COLN, true)),
    KEY_BEHAVIOR(KC_QUOT, [0] = STEP_HOLD(KC_DQUO, true)),
    KEY_BEHAVIOR(KC_COMM, [0] = STEP_HOLD(KC_LABK, true)),
    KEY_BEHAVIOR(KC_DOT, [0] = STEP_HOLD(KC_RABK, true)),

    // Enter → Shift+Enter (new line without send in chat apps)
    KEY_BEHAVIOR(KC_ENT, [0] = STEP_HOLD(S(KC_ENT), true)),

    // Arrows — release-based hold plus immediate longer-hold
    KEY_BEHAVIOR(KC_LEFT, [0] = STEP_HOLD_LONGER(A(KC_LEFT), false, G(KC_LEFT), true)),
    KEY_BEHAVIOR(KC_RIGHT, [0] = STEP_HOLD_LONGER(A(KC_RIGHT), false, G(KC_RIGHT), true)),

    // Layer keys — tap override on single tap, media on multi-tap, layer lock or repeat on hold
    KEY_BEHAVIOR(MO(LAYER_LOWER),
                 [0] = STEP_TAP(LAYER_LOCK(LAYER_LOWER)),
                 [1] = STEP_TAP_HOLD(KC_MPLY, LAYER_LOCK(LAYER_NUM), true),
                 [2] = STEP_TAP_HOLD(KC_MPRV, KC_MPRV, true)),

    KEY_BEHAVIOR(MO(LAYER_RAISE),
                 [0] = STEP_TAP(LAYER_LOCK(LAYER_RAISE)),
                 [1] = STEP_TAP_HOLD(KC_MPLY, LAYER_LOCK(LAYER_NUM), true),
                 [2] = STEP_TAP_HOLD(KC_MNXT, KC_MNXT, true)),

    // Pointing device mode keys
    KEY_BEHAVIOR(VOLUME_MODE, [1] = STEP_TAP(KC_MUTE)),
};

#undef KEY_BEHAVIOR
#undef HOLD_TIER
#undef STEP_HOLD
#undef STEP_HOLD_LONGER
#undef STEP_TAP
#undef STEP_TAP_HOLD
#undef STEP_TAP_HOLD_LONGER

// ─── Pointing Device Mode Tap Overrides ─────────────────────────────────────
//
// Pointing device mode keys (VOLUME_MODE, etc.) default to sending whatever
// key is at that position on LAYER_BASE when tapped.  Add an entry here to
// override that fallback with a specific keycode.

// keycode            tap
static const mode_tap_override_t mode_tap_overrides[] = {
    {VOLUME_MODE, KC_N}, // tap volume mode key → N

};

// ─── Combos ─────────────────────────────────────────────────────────────────
//
// Press multiple keys simultaneously to trigger a different action.
// Each combo can use 2 or more trigger keys — there is no hard limit.
// COMBO_TERM (in config.h) controls the max time window for keys to
// register as a combo.
//
// To add a combo:
//   1. Define a PROGMEM key array ending with COMBO_END.
//   2. Add a COMBO() entry to key_combos[] in the same order.
//
// Important: trigger keys must match the exact keycode in the layout,
// including any LT() or MT() wrappers (e.g. LT(LAYER_RAISE, KC_F), not KC_F).

const uint16_t PROGMEM combo_keys_1[] = {KC_D, LT(LAYER_RAISE, KC_F), COMBO_END};

combo_t key_combos[] = {
    COMBO(combo_keys_1, KC_TAB), // D + F → Tab
};

// ─── Macros ─────────────────────────────────────────────────────────────────
//
// Each macro maps a custom keycode to a SEND_STRING expression.
// Returns true if the keycode was handled, false otherwise.
//
// To add a macro: add a case below, and make sure the keycode exists in the
// custom_keycodes enum above.

static inline bool macro_dispatch(uint16_t keycode) {
    switch (keycode) {
        case MACRO_0: // Spotlight search (macOS): GUI + Space
            SEND_STRING(SS_LGUI(SS_TAP(X_SPACE)));
            return true;
        case MACRO_1: // Claude: Alt + Space
            SEND_STRING(SS_LALT(SS_TAP(X_SPACE)));
            return true;
        case MACRO_2: // Terminal: Alt + GUI + Space
            SEND_STRING(SS_LALT(SS_LGUI(SS_TAP(X_SPACE))));
            return true;
        case MACRO_3: // OCR text copy (macOS): Ctrl + Alt + GUI + C
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("c"))));
            return true;
        case MACRO_4: // Screenshot (macOS): Ctrl + Alt + GUI + X
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("x"))));
            return true;
        case MACRO_5: // Emoji picker (macOS): Ctrl + GUI + Space
            SEND_STRING(SS_LCTL(SS_LGUI(SS_TAP(X_SPACE))));
            return true;
        default:
            return false;
    }
}

// ─── Keymap Layouts ─────────────────────────────────────────────────────────
//
// QMK key prefixes quick reference:
//   KC_     = plain keycode              MT() = mod on hold, key on tap
//   LT(l,k) = layer l on hold, k on tap  MO() = momentary layer while held
//   G()     = GUI + key                   A()  = Alt + key
//   S()     = Shift + key                 LCAG() = Ctrl+Alt+GUI + key
//   LSG()   = Left Shift+GUI + key        LAG()  = Left Alt+GUI + key
//
//   - XXXXXXX = key does nothing on this layer.
//     _______ = transparent, falls through to the layer below.
//
// The number row (KC_1–KC_0) and punctuation keys use the key behavior
// tables defined above — they are NOT using QMK's built-in mod-tap.
// See process_record_user() in keymap.c for details.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // clang-format off
    [LAYER_BASE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  QK_GESC,              KC_1,              KC_2,              KC_3,              KC_4,              KC_5,                 KC_6,              KC_7,              KC_8,              KC_9,              KC_0,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                   KC_TAB,              KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,                 KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,           KC_BSLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),             KC_A,             KC_S,             KC_D,       LT(3,KC_F),              KC_G,                 KC_H,        LT(2,KC_J),              KC_K,              KC_L,           KC_SCLN,           KC_QUOT,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
             KC_LEFT_CTRL,        LT(2,KC_Z),              KC_X,              KC_C,              KC_V,        LT(1,KC_B),                 KC_N,              KC_M,           KC_COMM,            KC_DOT,     LT(3,KC_SLSH),      KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,             MO(2),                MO(3),            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NUM] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,             KC_P7,             KC_P8,             KC_P9,           XXXXXXX,           KC_PPLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,             MO(3),           XXXXXXX,              XXXXXXX,             KC_P4,             KC_P5,             KC_P6,           XXXXXXX,           KC_PEQL,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,             KC_P1,             KC_P2,             KC_P3,           KC_COMM,            KC_DOT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,             MO(2),                MO(3),             KC_P0,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_LOWER] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                   KC_ESC,           XXXXXXX,           DPI_MOD,          DPI_RMOD,           S_D_MOD,          S_D_RMOD,              XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LPRN,           KC_RPRN,           KC_QUOT,           KC_PPLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,           XXXXXXX,           XXXXXXX,           XXXXXXX,            KC_ESC,           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LBRC,           KC_RBRC,           KC_DQUO,           KC_PEQL,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
             KC_LEFT_CTRL,           XXXXXXX,        LCAG(KC_X),        LCAG(KC_C),         LSG(KC_V),           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LCBR,           KC_RCBR,           XXXXXXX,      KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,           XXXXXXX,                MO(3),           XXXXXXX,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_RAISE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
              LAG(KC_ESC),           XXXXXXX,           XXXXXXX,        LCAG(KC_V),           XXXXXXX,           XXXXXXX,              KC_MPLY,           KC_MNXT,           KC_MPRV,           KC_MUTE,           KC_VOLD,           KC_VOLU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           G(KC_Q),           G(KC_W),           G(KC_A),           XXXXXXX,           XXXXXXX,              MACRO_2,           G(KC_C),             KC_UP,           G(KC_V),           KC_BRID,           KC_BRIU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),        LSG(KC_Z),          XXXXXXX,          G(KC_C),          XXXXXXX,           XXXXXXX,              MACRO_1,           KC_LEFT,           KC_DOWN,           KC_RGHT,            KC_ESC,           XXXXXXX,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  MACRO_5,           G(KC_Z),           G(KC_X),           G(KC_V),           XXXXXXX,           XXXXXXX,              MACRO_0,           MS_BTN1,           MS_BTN2,           DRGSCRL,           XXXXXXX,        ARROW_MODE,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,           XXXXXXX,          KC_LEFT_ALT,            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_POINTER] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,           _______,           _______,           _______,           _______,           _______,      BRIGHTNESS_MODE,         ZOOM_MODE,           MS_BTN3,   DRG_TOG_ON_HOLD,           SNP_TOG,      KC_RIGHT_GUI,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,          VOLUME_MODE,           MS_BTN1,           MS_BTN2,           DRGSCRL,           _______,        ARROW_MODE,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,      LT(1,KC_SPC),             MO(2),                MO(3),            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),
    // clang-format on
};
