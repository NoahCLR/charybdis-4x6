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

// ─── Layers ─────────────────────────────────────────────────────────────────

enum charybdis_keymap_layers {
    LAYER_BASE = 0, // Default QWERTY typing layer
    LAYER_NUM,      // Numpad on the right half (activated by holding Z or B)
    LAYER_LOWER,    // Symbols and DPI controls (blue RGB)
    LAYER_RAISE,    // Navigation, media, and mouse buttons (purple RGB, sniping enabled)
    LAYER_POINTER,  // Auto-mouse layer: activates on trackball movement, deactivates after timeout
    LAYER_COUNT,
};

// DYNAMIC_KEYMAP_LAYER_COUNT in config.h must match LAYER_COUNT.
// If you add or remove a layer, update both.
#ifdef VIA_ENABLE
_Static_assert(LAYER_COUNT == DYNAMIC_KEYMAP_LAYER_COUNT, "LAYER_COUNT and DYNAMIC_KEYMAP_LAYER_COUNT are out of sync — update config.h");
#endif

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

// ─── Key Behavior Tables ────────────────────────────────────────────────────
//
// Each table covers one feature.  A key can appear in multiple tables to
// combine behaviors (e.g. KC_6 is in hold_keys AND double_tap_keys).
//
// Timing thresholds are defined in config.h:
//   CUSTOM_TAP_HOLD_TERM    = 150ms  (tap vs hold boundary)
//   CUSTOM_LONGER_HOLD_TERM = 400ms  (hold vs longer-hold boundary)
//   CUSTOM_DOUBLE_TAP_TERM  = 200ms  (max gap between taps for double-tap)

// ─── Hold Keys ──────────────────────────────────────────────────────────────
//
// On tap (< CUSTOM_TAP_HOLD_TERM), sends the keycode itself.
// On hold (>= CUSTOM_TAP_HOLD_TERM), sends the hold keycode.
//
// immediate = true:  hold fires at threshold without waiting for release.
// immediate = false: action is determined on release based on elapsed time.

typedef struct {
    uint16_t keycode;
    uint16_t hold;
    bool     immediate;
} hold_key_t;

// keycode     hold           immediate
static const hold_key_t hold_keys[] = {
    // Number row → shifted symbols
    {KC_1, KC_EXLM, true}, // 1 → !
    {KC_2, KC_AT, true},   // 2 → @
    {KC_3, KC_HASH, true}, // 3 → #
    {KC_4, KC_DLR, true},  // 4 → $
    {KC_5, KC_PERC, true}, // 5 → %
    {KC_6, KC_CIRC, true}, // 6 → ^
    {KC_7, KC_AMPR, true}, // 7 → &
    {KC_8, KC_ASTR, true}, // 8 → *
    {KC_9, KC_LPRN, true}, // 9 → (
    {KC_0, KC_RPRN, true}, // 0 → )

    // Punctuation → shifted variants
    {KC_MINS, KC_UNDS, true}, // - → _
    {KC_EQL, KC_PLUS, true},  // = → +
    {KC_LBRC, KC_LCBR, true}, // [ → {
    {KC_RBRC, KC_RCBR, true}, // ] → }
    {KC_BSLS, KC_PIPE, true}, // \ → |
    {KC_GRV, KC_TILD, true},  // ` → ~
    {KC_SCLN, KC_COLN, true}, // ; → :
    {KC_QUOT, KC_DQUO, true}, // ' → "
    {KC_COMM, KC_LABK, true}, // , → <
    {KC_DOT, KC_RABK, true},  // . → >

    // Enter → Shift+Enter (new line without send in chat apps)
    {KC_ENT, S(KC_ENT), true},

    // Arrows — release-based for three-tier support (see longer_hold_keys)
    {KC_LEFT, A(KC_LEFT), false},   // ← → word jump
    {KC_RIGHT, A(KC_RIGHT), false}, // → → word jump
};

#define HOLD_KEY_COUNT (sizeof(hold_keys) / sizeof(hold_keys[0]))

// ─── Longer Hold Keys ───────────────────────────────────────────────────────
//
// Third-tier action when held past CUSTOM_LONGER_HOLD_TERM.
// The key must also appear in hold_keys for the base hold behavior.
//
// immediate = true:  longer hold fires at threshold without waiting for release.
// immediate = false: action is determined on release based on elapsed time.

typedef struct {
    uint16_t keycode;
    uint16_t longer_hold;
    bool     immediate;
} longer_hold_key_t;

// keycode     longer_hold    immediate
static const longer_hold_key_t longer_hold_keys[] = {
    {KC_LEFT, G(KC_LEFT), true},   // ← → line jump
    {KC_RIGHT, G(KC_RIGHT), true}, // → → line jump
};

#define LONGER_HOLD_KEY_COUNT (sizeof(longer_hold_keys) / sizeof(longer_hold_keys[0]))

// ─── Multi-Tap Keys ─────────────────────────────────────────────────────────
//
// Sends an action when tapped twice (or three times) within CUSTOM_DOUBLE_TAP_TERM.
// Note: adds a small delay to single taps (waiting for potential second press).
// If triple_action is set, double-tap is also deferred while waiting for a
// potential third press.

typedef struct {
    uint16_t keycode;
    uint16_t action; // double-tap action
} double_tap_key_t;

// keycode                action
static const double_tap_key_t double_tap_keys[] = {
    {KC_6, KC_MPLY},            // play/pause
    {KC_7, KC_MNXT},            // next track
    {KC_8, KC_MPRV},            // prev track
    {MO(LAYER_LOWER), KC_MPLY}, // play/pause
    {MO(LAYER_RAISE), KC_MPLY}, // play/pause
    {VOLUME_MODE, KC_MUTE},     // mute
};

#define DOUBLE_TAP_KEY_COUNT (sizeof(double_tap_keys) / sizeof(double_tap_keys[0]))

// ─── Triple-Tap Keys ────────────────────────────────────────────────────────
//
// Third-tap action for keys that already appear in double_tap_keys[].
// Adds one extra deferral window (CUSTOM_DOUBLE_TAP_TERM) to double-taps
// while waiting for a potential third press.

typedef struct {
    uint16_t keycode;
    uint16_t action; // triple-tap action
} triple_tap_key_t;

// keycode                action
static const triple_tap_key_t triple_tap_keys[] = {
    {MO(LAYER_RAISE), KC_MNXT}, // next track on triple-tap
    {MO(LAYER_LOWER), KC_MPRV}, // prev track on triple-tap
};

#define TRIPLE_TAP_KEY_COUNT (sizeof(triple_tap_keys) / sizeof(triple_tap_keys[0]))

// ─── Mode Tap Overrides ─────────────────────────────────────────────────────
//
// By default, tapping a pointing device mode key sends whatever is at that
// position on LAYER_BASE.  Add an entry here to override that default.

typedef struct {
    uint16_t keycode;
    uint16_t tap;
} mode_tap_override_t;

// keycode            tap
static const mode_tap_override_t mode_tap_overrides[] = {
    // {VOLUME_MODE,     KC_MUTE},   // example: tap volume mode → mute
};

#define MODE_TAP_OVERRIDE_COUNT (sizeof(mode_tap_overrides) / sizeof(mode_tap_overrides[0]))

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
                  QK_GESC,              KC_1,              KC_2,              KC_3,              KC_4,              KC_5,              KC_6,              KC_7,              KC_8,              KC_9,              KC_0,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                   KC_TAB,              KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,                 KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,           KC_BSLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),             KC_A,             KC_S,             KC_D,       LT(3,KC_F),              KC_G,                 KC_H,        LT(2,KC_J),              KC_K,              KC_L,           KC_SCLN,           KC_QUOT,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
             KC_LEFT_CTRL,        LT(2,KC_Z),              KC_X,              KC_C,              KC_V,        LT(1,KC_B),                 KC_N,              KC_M,           KC_COMM,            KC_DOT,     LT(3,KC_SLSH),      KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,    MO(LAYER_LOWER),    MO(LAYER_RAISE),            KC_ENT,
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
                                                                       KC_LEFT_GUI,      LT(1,KC_SPC),             MO(2),    MO(LAYER_RAISE),            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
    ),
    // clang-format on
};
