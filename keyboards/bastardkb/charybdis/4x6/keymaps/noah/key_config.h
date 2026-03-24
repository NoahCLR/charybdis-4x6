// ────────────────────────────────────────────────────────────────────────────
// Key Configuration — what each key does
// ────────────────────────────────────────────────────────────────────────────
//
// This file holds all the behavioral data for the keymap: enums, tap dance
// configs, tap/hold mappings, and macro definitions.  The processing logic
// that uses these tables lives in keymap.c.
//
// Want to change what a key does?  Edit this file.
// Want to change how keys are processed?  Edit keymap.c.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H

// ─── Layers ─────────────────────────────────────────────────────────────────

enum charybdis_keymap_layers {
    LAYER_BASE = 0, // Default QWERTY typing layer
    LAYER_NUM,      // Numpad on the right half (activated by holding Z or B)
    LAYER_LOWER,    // Symbols and DPI controls (blue RGB)
    LAYER_RAISE,    // Navigation, media, and mouse buttons (purple RGB, sniping enabled)
    LAYER_POINTER,  // Auto-mouse layer: activates on trackball movement, deactivates after timeout
    LAYER_COUNT,
};

// ─── Custom Keycodes ────────────────────────────────────────────────────────
//
// Custom keycodes are assigned values starting from SAFE_RANGE so they don't
// collide with any built-in QMK or Charybdis keycodes.
//
// MACRO_0–15 are generic macro slots (some used, rest reserved for VIA).
// VOLUME_MODE / BRIGHTNESS_MODE / ARROW_MODE / ZOOM_MODE activate pointing device modes while held.
// DRG_TOG_ON_HOLD is a dual-purpose key: tap sends the base-layer key at
// that position, hold toggles drag-scroll lock.

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
};

// ─── Tap Dance ──────────────────────────────────────────────────────────────
//
// Named by LED index (see LED Index Map in keymap.c) so the identifier
// stays stable regardless of what keycode is mapped at that position.
//
//   TD_49 (6 key):   single tap → 6, hold → ^,    double tap → play/pause
//   TD_45 (7 key):   single tap → 7, hold → &,    double tap → next track
//   TD_44 (8 key):   single tap → 8, hold → *,    double tap → prev track
//   TD_28 (L thumb): hold → Lower layer,           double tap → play/pause
//   TD_53 (R thumb): hold → Raise layer,           double tap → play/pause
//
// ╭────────────────────────╮                 ╭────────────────────────╮
//    0   7   8  15  16  20                     49  45  44  37  36  29
// ├────────────────────────┤                 ├────────────────────────┤
//    1   6   9  14  17  21                     50  46  43  38  35  30
// ├────────────────────────┤                 ├────────────────────────┤
//    2   5  10  13  18  22                     51  47  42  39  34  31
// ├────────────────────────┤                 ├────────────────────────┤
//    3   4  11  12  19  23                     52  48  41  40  33  32
// ╰────────────────────────╯                 ╰────────────────────────╯
//                       26  27  28     53  54  XX
//                           25  24     55  XX
//                     ╰────────────╯ ╰────────────╯
//
// Per-tap-dance config: what to send on tap, hold, and double tap.
// If hold_layer is non-zero, hold activates that layer instead of sending
// the hold keycode.  (Layer 0 is always active so 0 means "use keycode".)
typedef struct {
    uint16_t tap;
    uint16_t hold;
    uint16_t double_tap;
    uint8_t  hold_layer;
} td_config_t;

// Single source of truth for tap dances.  To add a new one, add one X()
// line here — the enum, config array, and TD_COUNT are all generated from it.
//
//              name    tap    hold      double-tap   hold_layer
#define TD_LIST \
    X(TD_49,    KC_6,  KC_CIRC, KC_MPLY, 0)            \
    X(TD_45,    KC_7,  KC_AMPR, KC_MNXT, 0)            \
    X(TD_44,    KC_8,  KC_ASTR, KC_MPRV, 0)            \
    X(TD_28,    KC_NO, KC_NO,   KC_MPLY, LAYER_LOWER)  \
    X(TD_53,    KC_NO, KC_NO,   KC_MPLY, LAYER_RAISE)

#define X(name, ...) name,
enum tap_dances { TD_LIST TD_COUNT };
#undef X

#define X(name, tap, hold, dtap, layer) [name] = {tap, hold, dtap, layer},
static const td_config_t td_config[TD_COUNT] = { TD_LIST };
#undef X

// ─── Tap / Hold / Longer-Hold Mappings ──────────────────────────────────────
//
// Data-driven three-tier key behavior.  Each entry maps a keycode to:
//   tap          — sent on quick press (< CUSTOM_TAP_HOLD_TERM)
//   hold         — sent when held past CUSTOM_TAP_HOLD_TERM
//   longer_hold  — sent when held past CUSTOM_LONGER_HOLD_TERM (KC_NO = fall back to hold)
//   immediate    — if true, hold fires at threshold without waiting for release
//
// The processing logic in keymap.c iterates this table to decide what to do.

typedef struct {
    uint16_t tap;
    uint16_t hold;
    uint16_t longer_hold; // KC_NO = fall back to hold
    bool     immediate;   // true = fire hold at threshold, false = wait for release
} tap_hold_config_t;

static const tap_hold_config_t tap_hold_config[] = {
    // Number row → shifted symbols
    {KC_1, KC_EXLM, KC_NO, true}, // 1 → !
    {KC_2, KC_AT, KC_NO, true},   // 2 → @
    {KC_3, KC_HASH, KC_NO, true}, // 3 → #
    {KC_4, KC_DLR, KC_NO, true},  // 4 → $
    {KC_5, KC_PERC, KC_NO, true}, // 5 → %
    {KC_9, KC_LPRN, KC_NO, true}, // 9 → (
    {KC_0, KC_RPRN, KC_NO, true}, // 0 → )

    // Punctuation → shifted variants
    {KC_MINS, KC_UNDS, KC_NO, true}, // - → _
    {KC_EQL, KC_PLUS, KC_NO, true},  // = → +
    {KC_LBRC, KC_LCBR, KC_NO, true}, // [ → {
    {KC_RBRC, KC_RCBR, KC_NO, true}, // ] → }
    {KC_BSLS, KC_PIPE, KC_NO, true}, // \ → |
    {KC_GRV, KC_TILD, KC_NO, true},  // ` → ~
    {KC_SCLN, KC_COLN, KC_NO, true}, // ; → :
    {KC_QUOT, KC_DQUO, KC_NO, true}, // ' → "
    {KC_COMM, KC_LABK, KC_NO, true}, // , → <
    {KC_DOT, KC_RABK, KC_NO, true},  // . → >

    // Enter → Shift+Enter (new line without send in chat apps)
    {KC_ENT, S(KC_ENT), KC_NO, true},

    // Arrows — release-based (immediate=false) for three-tier support
    {KC_LEFT, A(KC_LEFT), G(KC_LEFT), false},    // tap → ←, hold → word jump, longer → line jump
    {KC_RIGHT, A(KC_RIGHT), G(KC_RIGHT), false}, // tap → →, hold → word jump, longer → line jump
};

#define TAP_HOLD_CONFIG_COUNT (sizeof(tap_hold_config) / sizeof(tap_hold_config[0]))

// ─── Macros ─────────────────────────────────────────────────────────────────
//
// Each macro maps a custom keycode to a SEND_STRING expression.
// SEND_STRING is a macro itself, so we can't store the strings in a plain
// array — instead we use a lookup function.  But the definitions live here
// so all key behavior config is in one place.
//
// To add a macro: add a case below, and make sure the keycode exists in the
// custom_keycodes enum above.

#define MACRO_DISPATCH(keycode)                                         \
    switch (keycode) {                                                  \
        case MACRO_0: /* Spotlight search (macOS): GUI + Space */       \
            SEND_STRING(SS_LGUI(SS_TAP(X_SPACE)));                      \
            break;                                                      \
        case MACRO_1: /* Claude: Alt + Space */                         \
            SEND_STRING(SS_LALT(SS_TAP(X_SPACE)));                      \
            break;                                                      \
        case MACRO_2: /* Terminal: Alt + GUI + Space */                 \
            SEND_STRING(SS_LALT(SS_LGUI(SS_TAP(X_SPACE))));             \
            break;                                                      \
        case MACRO_3: /* OCR text copy (macOS): Ctrl + Alt + GUI + C */ \
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("c"))));                \
            break;                                                      \
        case MACRO_4: /* Screenshot (macOS): Ctrl + Alt + GUI + X */    \
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("x"))));                \
            break;                                                      \
        case MACRO_5: /* Emoji picker (macOS): Ctrl + GUI + Space */    \
            SEND_STRING(SS_LCTL(SS_LGUI(SS_TAP(X_SPACE))));             \
            break;                                                      \
        default:                                                        \
            return true;                                                \
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
// The number row (KC_1–KC_0) and punctuation keys use a custom tap/hold
// system defined below — they are NOT using QMK's built-in mod-tap.
// See process_record_user() for details.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // clang-format off
    [LAYER_BASE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  QK_GESC,              KC_1,              KC_2,              KC_3,              KC_4,              KC_5,        TD(TD_49),     TD(TD_45),     TD(TD_44),              KC_9,              KC_0,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                   KC_TAB,              KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,                 KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,           KC_BSLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),             KC_A,             KC_S,             KC_D,       LT(3,KC_F),              KC_G,                 KC_H,        LT(2,KC_J),              KC_K,              KC_L,           KC_SCLN,           KC_QUOT,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
             KC_LEFT_CTRL,        LT(2,KC_Z),              KC_X,              KC_C,              KC_V,        LT(1,KC_B),                 KC_N,              KC_M,           KC_COMM,            KC_DOT,     LT(3,KC_SLSH),      KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,       TD(TD_28),       TD(TD_53),            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
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
                                                                       KC_LEFT_GUI,            KC_SPC,             MO(2),             MO(3),             KC_P0,
                                                                                               KC_DEL,           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
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
                                                                       KC_LEFT_GUI,            KC_SPC,           XXXXXXX,             MO(3),           XXXXXXX,
                                                                                               KC_DEL,           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
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
                                                                       KC_LEFT_GUI,            KC_SPC,           XXXXXXX,       KC_LEFT_ALT,            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
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
                                                                       KC_LEFT_GUI,      LT(1,KC_SPC),             MO(2),             MO(3),            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
    ),
    // clang-format on
};
