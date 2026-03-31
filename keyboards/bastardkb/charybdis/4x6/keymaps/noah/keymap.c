// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 keymap data
// ────────────────────────────────────────────────────────────────────────────
//
// This translation unit owns the authored keymap data:
//   - key_behaviors[]
//   - macro_dispatch()
//   - keymaps[][]
//   - key_combos[]
//
// Shared layer and custom-keycode declarations live in keymap_defs.h.
// Runtime processing lives in the runtime modules under lib/.
// ────────────────────────────────────────────────────────────────────────────

#include "keymap_defs.h"
#include "lib/key/key_behavior.h"

// Split-role override hook.
// Needed when both halves have their own USB connection so they do not both
// detect USB and fight over who is master.
#if defined(FORCE_SLAVE)
#    include "usb_util.h" // QMK
#endif

#if defined(FORCE_MASTER)
bool is_keyboard_master_impl(void) {
    return true;
}
#elif defined(FORCE_SLAVE)
bool is_keyboard_master_impl(void) {
    usb_disconnect();
    return false;
}
#endif

bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    (void)record;

    switch (keycode) {
        case MT(MOD_LSFT, KC_CAPS):
            return true;
        default:
            return false;
    }
}

// ─── Key Behavior Tables ────────────────────────────────────────────────────
//
// key_behaviors[] is the single authored behavior table for keys handled by
// the custom state machine. One row describes one physical key.
//
// tap_counts[0] = single press
// tap_counts[1] = double tap
// tap_counts[2] = triple tap
// tap_counts[3] = quadruple tap
//
// omit .tap to keep the key's normal tap behavior for that step
// .hold / .long_hold use the helper DSL from lib/key/key_behavior.h
// timing thresholds come from config.h unless overridden per key

const key_behavior_t
    key_behaviors[] =
        {
            // Number row — shifted symbols on hold, media on double-tap
            {.keycode = KC_1, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_EXLM)}}},
            {.keycode = KC_2, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_AT)}}},
            {.keycode = KC_3, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_HASH)}}},
            {.keycode = KC_4, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_DLR)}}},
            {.keycode = KC_5, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_PERC)}}},
            {.keycode = KC_6, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_CIRC)}, [1] = {.tap = TAP_SENDS(KC_MPLY)}}},
            {.keycode = KC_7, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_AMPR)}, [1] = {.tap = TAP_SENDS(KC_MNXT), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)}}},
            {.keycode = KC_8, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_ASTR)}, [1] = {.tap = TAP_SENDS(KC_MPRV), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)}}},
            {.keycode = KC_9, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LPRN)}}},
            {.keycode = KC_0, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RPRN)}}},

            // Punctuation → shifted variants
            {.keycode = KC_MINS, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_UNDS)}}},
            {.keycode = KC_EQL, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_PLUS)}}},
            {.keycode = KC_LBRC, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LCBR)}}},
            {.keycode = KC_RBRC, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RCBR)}}},
            {.keycode = KC_BSLS, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_PIPE)}}},
            {.keycode = KC_GRV, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_TILD)}}},
            {.keycode = KC_SCLN, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_COLN)}}},
            {.keycode = KC_QUOT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_DQUO)}}},
            {.keycode = KC_COMM, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LABK)}}},
            {.keycode = KC_DOT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RABK)}}},

            // Escape → Force Quit on hold, tilde on double-tap
            {.keycode = KC_ESC, .tap_counts = {[0] = {.long_hold = TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))}, [1] = {.tap = TAP_SENDS(S(KC_GRV))}}},

            // Enter → Shift+Enter (new line without send in chat apps)
            {.keycode = KC_ENT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_ENT))}}},

            // Arrows — release-based hold plus immediate long hold
            {.keycode = KC_LEFT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_LEFT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_LEFT))}}},
            {.keycode = KC_RIGHT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_RIGHT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_RIGHT))}}},

            // Layer keys — tap override on single tap, media on multi-tap, layer lock or repeat on hold
            {
                .keycode = MO(LAYER_SYM),
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_SYM))},
                        [1] = {.tap = TAP_SENDS(KC_MPLY), .hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(KC_MNXT), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)},
                        [3] = {.tap = TAP_SENDS(KC_MPRV), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)},
                    },
            },

            {
                .keycode = MO(LAYER_NAV),
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_NAV))},
                        [1] = {.tap = TAP_SENDS(KC_MPLY), .hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(KC_MNXT), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)},
                        [3] = {.tap = TAP_SENDS(KC_MPRV), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)},
                    },
            },

            // Home-row layer-tap key — double-tap locks LAYER_NAV.
            // tap_hold_term inherits the LT() default (TAPPING_TERM) so typing feel is unchanged.
            {
                .keycode = LT(LAYER_NAV, KC_SLSH),
                .tap_counts =
                    {
                        [1] = {.hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NAV))},
                    },
            },

            // Pointing device mode keys.
            // Single tap defaults to the base-layer key at that position unless [0]
            // overrides it here.
            {.keycode = VOLUME_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(KC_MUTE)}}},
            {.keycode = ARROW_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))}}},
            {
                .keycode = PINCH_MODE,
                .tap_counts =
                    {
                        [1] = {.tap = TAP_SENDS(MACRO_6), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(ZOOM_MODE)},
                    },
            },

            // Dragscroll: tap = base-layer key, double-tap = lock, hold = momentary (via pd_mode).
            {.keycode = DRAGSCROLL, .tap_counts = {[1] = {.tap = TAP_SENDS(LOCK_PD_MODE(DRAGSCROLL))}}},
};

const uint8_t key_behavior_count = sizeof(key_behaviors) / sizeof(key_behaviors[0]);

// ─── Macros ─────────────────────────────────────────────────────────────────

bool macro_dispatch(uint16_t keycode) {
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
        case MACRO_6: // MacOS Zoom: Alt + GUI + 8
            SEND_STRING(SS_LALT(SS_LGUI(SS_TAP(X_8))));
            return true;
        default:
            return false;
    }
}

// ─── Combos ─────────────────────────────────────────────────────────────────

const uint16_t PROGMEM combo_keys_1[] = {KC_D, LT(LAYER_NAV, KC_F), COMBO_END};

combo_t key_combos[] = {
    COMBO(combo_keys_1, KC_TAB), // D + F → Tab
};

// ─── Keymap Layouts ─────────────────────────────────────────────────────────

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
// See lib/key/key_runtime.c for details.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // clang-format off
    [LAYER_BASE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                   KC_ESC,              KC_1,              KC_2,              KC_3,              KC_4,              KC_5,                 KC_6,              KC_7,              KC_8,              KC_9,              KC_0,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                   KC_TAB,              KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,                 KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,           KC_BSLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),            KC_A,            KC_S,             KC_D,  LT(LAYER_NAV,KC_F),             KC_G,                KC_H,  LT(LAYER_SYM,KC_J),             KC_K,              KC_L,           KC_SCLN,           KC_QUOT,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_CTRL,  LT(LAYER_SYM,KC_Z),             KC_X,              KC_C,              KC_V,              KC_B,                KC_N,             KC_M,          KC_COMM,           KC_DOT,  LT(LAYER_NAV,KC_SLSH),     KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,     MO(LAYER_SYM),        MO(LAYER_NAV),            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NUM] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,             KC_P7,             KC_P8,             KC_P9,           XXXXXXX,           KC_PPLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,     MO(LAYER_NAV),           XXXXXXX,              XXXXXXX,             KC_P4,             KC_P5,             KC_P6,           XXXXXXX,           KC_PEQL,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           _______,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,             KC_P1,             KC_P2,             KC_P3,           KC_COMM,            KC_DOT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,           _______,              _______,             KC_P0,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_SYM] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                   KC_ESC,           XXXXXXX,           DPI_MOD,          DPI_RMOD,           S_D_MOD,          S_D_RMOD,              XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LPRN,           KC_RPRN,           KC_QUOT,           KC_PPLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,           XXXXXXX,           XXXXXXX,           XXXXXXX,            KC_ESC,           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LBRC,           KC_RBRC,           KC_DQUO,           KC_PEQL,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
             KC_LEFT_CTRL,           _______,        LCAG(KC_X),        LCAG(KC_C),         LSG(KC_V),           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LCBR,           KC_RCBR,           _______,      KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,           _______,              _______,            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NAV] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
              LAG(KC_ESC),           XXXXXXX,           XXXXXXX,        LCAG(KC_V),           XXXXXXX,           XXXXXXX,              KC_MPLY,           KC_MNXT,           KC_MPRV,           KC_MUTE,           KC_VOLD,           KC_VOLU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           G(KC_Q),           G(KC_W),           G(KC_A),           XXXXXXX,           XXXXXXX,              MACRO_2,           G(KC_C),             KC_UP,           G(KC_V),           KC_BRID,           KC_BRIU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),        LSG(KC_Z),          XXXXXXX,          G(KC_C),          XXXXXXX,           XXXXXXX,              MACRO_1,           KC_LEFT,           KC_DOWN,           KC_RGHT,            KC_ESC,           XXXXXXX,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  MACRO_5,           G(KC_Z),           G(KC_X),           G(KC_V),           XXXXXXX,           XXXXXXX,              MACRO_0,           MS_BTN1,           MS_BTN2,        DRAGSCROLL,           _______,        ARROW_MODE,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,           _______,              _______,            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_POINTER] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,           _______,           _______,           _______,           _______,           _______,      BRIGHTNESS_MODE,        PINCH_MODE,           MS_BTN3,        ARROW_MODE,           XXXXXXX,      KC_RIGHT_GUI,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,          VOLUME_MODE,           MS_BTN1,           MS_BTN2,        DRAGSCROLL,           _______,        ARROW_MODE,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                     KC_LEFT_GUI,  LT(LAYER_NUM,KC_SPC),         _______,              _______,            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),
    // clang-format on
};
