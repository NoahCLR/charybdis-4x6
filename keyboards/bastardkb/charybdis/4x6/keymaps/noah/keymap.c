// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 keymap
// ────────────────────────────────────────────────────────────────────────────
//
// This is a QMK firmware keymap for the Bastard Keyboards Charybdis 4x6,
// a split ergonomic keyboard with a built-in trackball on the right half.
//
// Architecture overview:
//
//   keymap.c (this file)
//     Main keymap file.  Defines layers, key behavior, macros, pointing
//     device integration, and RGB layer indicators.  Ties together the
//     four helper headers below.
//
//   pointing_device_modes.h
//     Bitfield-based mode system that changes what the trackball does.
//     Modes: Volume (Y-axis → volume), Brightness (Y-axis → brightness),
//     Zoom (Y-axis → GUI+Plus/Minus), Arrow (motion → arrow keys),
//     Dragscroll (firmware-native scroll mode).
//
//   split_sync.h
//     Syncs pointing-device state (mode flags + auto-mouse elapsed time)
//     from the master half to the slave half over QMK's split RPC
//     transport.  Both values travel in a single 3-byte packet.
//
//   rgb_automouse.h
//     Renders the auto-mouse countdown gradient (white → red) based on
//     how much of the timeout has elapsed.
//
//   rgb_helpers.h
//     Thin wrappers around rgb_matrix_set_color() that are safe to call
//     on a split keyboard (they clamp to the current half's LED range).
//
// Key concepts for newcomers:
//
//   - "Split keyboard":  Each physical half has its own MCU.  The right
//     half (master) runs the keymap logic and sends state to the left
//     half (slave) so it can update its own LEDs.
//
//   - "Auto-mouse layer":  QMK can automatically activate a layer when
//     it detects trackball movement, and deactivate it after a timeout.
//     We use this for LAYER_POINTER.
//
//   - "Pointing device modes":  We define custom modes that change the behavior of the
//     pointing device (e.g. volume control, scroll, mouse movement) and tie
//     them to keys in the keymap.  These modes are implemented by intercepting
//     the relevant keycodes in process_record_user() and setting flags that
//     the pointing device code checks to decide what to do with pointing device movement.
//
//   - "Tap dance":  Some keys (6, 7, 8, Lower, Raise) use QMK's tap
//     dance for double-tap actions (media controls).  Config is data-
//     driven via td_config[].
//
//   - "Tap vs Hold":  Remaining number/punctuation keys use a custom
//     three-tier system: tap (<150ms), hold (150–400ms), longer hold
//     (>400ms).  Hold fires immediately via matrix_scan_user() for
//     most keys; arrow keys keep release-based timing for three tiers.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H

#include "pointing_device_modes.h"
#include "split_sync.h"
#include "rgb_automouse.h"
#include "rgb_helpers.h"

// Force master/slave role at compile time.  Needed when both halves have
// their own USB connection (e.g. for full LED brightness on each side
// when using a long cable) so they don't both detect USB and fight over
// who is master.  This overrides role detection only — handedness (left/right)
// is still determined by MASTER_RIGHT in the keyboard-level config in the qmk repo.
// Build with: -e FORCE_MASTER=yes or -e FORCE_SLAVE=yes
#if defined(FORCE_MASTER)
bool is_keyboard_master_impl(void) {
    return true;
}
#elif defined(FORCE_SLAVE)
#    include "usb_util.h"
bool is_keyboard_master_impl(void) {
    usb_disconnect();
    return false;
}
#endif

// ─── Custom Keycodes & Keymap Layers ────────────────────────────────────────

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

// Layers are stacked: higher layers override lower ones.
// Keys marked _______ let the layer below show through.
enum charybdis_keymap_layers {
    LAYER_BASE = 0, // Default QWERTY typing layer
    LAYER_NUM,      // Numpad on the right half (activated by holding Z or B)
    LAYER_LOWER,    // Symbols and DPI controls (blue RGB)
    LAYER_RAISE,    // Navigation, media, and mouse buttons (purple RGB, sniping enabled)
    LAYER_POINTER,  // Auto-mouse layer: activates on trackball movement, deactivates after timeout
};

// ─── Tap Dance ──────────────────────────────────────────────────────────────
//
// QMK tap dance lets a single key do different things based on how many
// times it's tapped in quick succession.
//
// Named by LED index (see LED Index Map near RGB section) so the
// identifier stays stable regardless of what keycode is mapped there.
//
//   TD_49 (6 key):   single tap → 6, hold → ^,    double tap → play/pause
//   TD_45 (7 key):   single tap → 7, hold → &,    double tap → next track
//   TD_44 (8 key):   single tap → 8, hold → *,    double tap → prev track
//   TD_28 (L thumb): hold → Lower layer,           double tap → play/pause
//   TD_53 (R thumb): hold → Raise layer,           double tap → play/pause

enum tap_dances {
    TD_49,
    TD_45,
    TD_44,
    TD_28,
    TD_53,
    TD_COUNT,
};

// Per-tap-dance config: what to send on tap, hold, and double tap.
// If hold_layer is non-zero, hold activates that layer instead of sending
// the hold keycode.  (Layer 0 is always active so 0 means "use keycode".)
typedef struct {
    uint16_t tap;
    uint16_t hold;
    uint16_t double_tap;
    uint8_t  hold_layer;
} td_config_t;

static const td_config_t td_config[TD_COUNT] = {
    [TD_49] = {KC_6, KC_CIRC, KC_MPLY, 0}, [TD_45] = {KC_7, KC_AMPR, KC_MNXT, 0}, [TD_44] = {KC_8, KC_ASTR, KC_MPRV, 0}, [TD_28] = {KC_NO, KC_NO, KC_MPLY, LAYER_LOWER}, [TD_53] = {KC_NO, KC_NO, KC_MPLY, LAYER_RAISE},
};

// Tracks which layer a tap-dance hold activated, so reset can deactivate it.
static uint8_t td_hold_layer_active = 0;

// Shared callback — the config is passed via user_data.
static void td_finished(tap_dance_state_t *state, void *user_data) {
    const td_config_t *cfg = (const td_config_t *)user_data;

    if (state->count == 1) {
        if (state->pressed) {
            if (cfg->hold_layer) {
                layer_on(cfg->hold_layer);
                td_hold_layer_active = cfg->hold_layer;
            } else {
                tap_code16(cfg->hold);
            }
        } else {
            if (cfg->tap != KC_NO) tap_code16(cfg->tap);
        }
    } else if (state->count == 2 && !state->pressed) {
        tap_code16(cfg->double_tap);
    }
}

static void td_reset(tap_dance_state_t *state, void *user_data) {
    if (td_hold_layer_active) {
        layer_off(td_hold_layer_active);
        td_hold_layer_active = 0;
    }
}

#define TD_ENTRY(idx)                                     \
    {                                                     \
        .fn        = {NULL, td_finished, td_reset, NULL}, \
        .user_data = (void *)&td_config[idx],             \
    }

tap_dance_action_t tap_dance_actions[] = {
    [TD_49] = TD_ENTRY(TD_49), [TD_45] = TD_ENTRY(TD_45), [TD_44] = TD_ENTRY(TD_44), [TD_28] = TD_ENTRY(TD_28), [TD_53] = TD_ENTRY(TD_53),
};

// ─── Keymap Layouts ─────────────────────────────────────────────────────────
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

// ─── Custom Tap / Hold / Longer-Hold System ─────────────────────────────────
//
// This keymap does NOT use QMK's built-in mod-tap for the number row and
// punctuation.  Instead, it implements its own three-tier timing system:
//
//   Tap   (< 150ms):  Send the plain key.          e.g. "1"
//   Hold  (150–400ms): Send the shifted variant.    e.g. "!" (Shift+1)
//   Longer hold (> 400ms): Send a third action.     e.g. GUI+Arrow
//
// This means key-down does NOT immediately register — the character is only
// sent on key-up, after measuring how long the key was held.
//
// The thresholds are defined in config.h:
//   CUSTOM_TAP_HOLD_TERM      = 150ms
//   CUSTOM_LONGER_HOLD_TERM   = 400ms
static uint16_t tap_hold_timer;
static uint16_t tap_hold_keycode = KC_NO; // which key is currently held
static bool     tap_hold_fired   = false; // whether the hold action already sent

// Send the hold variant (triggered at 150–400ms hold).
// Maps each key to its shifted symbol.
static void send_hold_variant(uint16_t keycode) {
    switch (keycode) {
        // Number row → shifted symbols
        case KC_1:
            tap_code16(KC_EXLM);
            break; // !
        case KC_2:
            tap_code16(KC_AT);
            break; // @
        case KC_3:
            tap_code16(KC_HASH);
            break; // #
        case KC_4:
            tap_code16(KC_DLR);
            break; // $
        case KC_5:
            tap_code16(KC_PERC);
            break; // %
        case KC_6:
            tap_code16(KC_CIRC);
            break; // ^
        case KC_7:
            tap_code16(KC_AMPR);
            break; // &
        case KC_8:
            tap_code16(KC_ASTR);
            break; // *
        case KC_9:
            tap_code16(KC_LPRN);
            break; // (
        case KC_0:
            tap_code16(KC_RPRN);
            break; // )

        // Punctuation → shifted variants
        case KC_MINS:
            tap_code16(KC_UNDS);
            break; // _
        case KC_EQL:
            tap_code16(KC_PLUS);
            break; // +
        case KC_LBRC:
            tap_code16(KC_LCBR);
            break; // {
        case KC_RBRC:
            tap_code16(KC_RCBR);
            break; // }
        case KC_BSLS:
            tap_code16(KC_PIPE);
            break; // |
        case KC_GRV:
            tap_code16(KC_TILD);
            break; // ~
        case KC_SCLN:
            tap_code16(KC_COLN);
            break; // :
        case KC_QUOT:
            tap_code16(KC_DQUO);
            break; // "
        case KC_COMM:
            tap_code16(KC_LABK);
            break; // <
        case KC_DOT:
            tap_code16(KC_RABK);
            break; // >

        // Arrows → Alt+Arrow (word-jump on macOS)
        case KC_LEFT:
            tap_code16(A(KC_LEFT));
            break;
        case KC_RIGHT:
            tap_code16(A(KC_RIGHT));
            break;

        // Enter → Shift+Enter (e.g. new line without send in chat apps)
        case KC_ENT:
            tap_code16(S(KC_ENT));
            break;

        default:
            tap_code16(keycode);
            break;
    }
}

// Send the longer-hold variant (triggered at >400ms hold).
// Only a few keys have a third tier; everything else falls back to hold variant.
static void send_longer_hold_variant(uint16_t keycode) {
    switch (keycode) {
        // Arrows → GUI+Arrow (line-jump on macOS: Home/End equivalent)
        case KC_LEFT:
            tap_code16(G(KC_LEFT));
            break;
        case KC_RIGHT:
            tap_code16(G(KC_RIGHT));
            break;

        // All other keys: fall back to the normal hold variant.
        default:
            send_hold_variant(keycode);
            break;
    }
}

// Simulate a full press+release of a Charybdis firmware keycode (e.g.
// DRAGSCROLL_MODE_TOGGLE).  Uses process_record_kb (not _user) to avoid
// infinite recursion back into our own handler.
static void tap_custom_bk_keycode(uint16_t kc) {
    keyrecord_t rec = {0};

    rec.event.pressed = true;
    process_record_kb(kc, &rec);

    rec.event.pressed = false;
    process_record_kb(kc, &rec);
}

// ─── Key Event Processing ───────────────────────────────────────────────────
//
// QMK calls this function for every key press and release.  Returning false
// tells QMK "I handled it, don't process further."  Returning true means
// "pass it along to the next handler."
//
// The logic is organized in stages:
//   1) Pointing device mode keys (press + release)
//   2) Tap/hold/longer-hold keys (press + release)
//   3) Early return for releases (everything below is press-only)
//   4) Macros (press-only)

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // --- 1) Pointing device mode keys (react on both press and release) ---
    switch (keycode) {
        // VOLUME_MODE / BRIGHTNESS_MODE / ZOOM_MODE / ARROW_MODE:
        // Dual-purpose keys: tap sends the base-layer key at this position,
        // hold activates the trackball mode.  The mode activates immediately
        // on press so it's ready if you move the trackball; if released
        // quickly (< CUSTOM_TAP_HOLD_TERM), the mode is cancelled and the
        // base-layer key is sent instead.
        case VOLUME_MODE:
        case BRIGHTNESS_MODE:
        case ZOOM_MODE:
        case ARROW_MODE: {
            uint8_t mode = (keycode == VOLUME_MODE) ? PD_MODE_VOLUME : (keycode == BRIGHTNESS_MODE) ? PD_MODE_BRIGHTNESS : (keycode == ZOOM_MODE) ? PD_MODE_ZOOM : PD_MODE_ARROW;
            if (record->event.pressed) {
                tap_hold_timer = timer_read();
                pd_mode_update(mode, true);
            } else {
                pd_mode_update(mode, false);
                if (timer_elapsed(tap_hold_timer) < CUSTOM_TAP_HOLD_TERM) {
                    uint16_t fallback_key = keymap_key_to_keycode(LAYER_BASE, record->event.key);
                    tap_code16(fallback_key);
                }
            }
            pd_state_sync();
            return false;
        }

        // DRAGSCROLL_MODE (Charybdis firmware keycode, not in our enum):
        // Dual-purpose like the other mode keys: tap sends the base-layer
        // key, hold activates drag-scroll.  Also adds unlock behavior:
        // if drag-scroll was already *toggled on* (locked), pressing and
        // releasing the momentary key will unlock it instead.
        case DRAGSCROLL_MODE: {
            static bool dragscroll_was_locked = false;
            if (record->event.pressed) {
                tap_hold_timer        = timer_read();
                dragscroll_was_locked = charybdis_get_pointer_dragscroll_enabled();
                pd_mode_update(PD_MODE_DRAGSCROLL, true);
                pd_state_sync();
                return true;
            } else {
                if (dragscroll_was_locked) {
                    // It was locked — turn off the toggle and clear mode flag.
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                    return false;
                }
                pd_mode_update(PD_MODE_DRAGSCROLL, false);
                pd_state_sync();
                if (timer_elapsed(tap_hold_timer) < CUSTOM_TAP_HOLD_TERM) {
                    // Tap: disable dragscroll if it was enabled, then send the base-layer key.
                    charybdis_set_pointer_dragscroll_enabled(false);
                    uint16_t fallback_key = keymap_key_to_keycode(LAYER_BASE, record->event.key);
                    tap_code16(fallback_key);
                    return false;
                }
                return true;
            }
        }

        // DRG_TOG_ON_HOLD: Dual-purpose key for the auto-mouse layer.
        //   Tap  → sends whatever key is at this position on LAYER_BASE.
        //   Hold → enables drag-scroll lock.
        //   When already locked, any press (tap or hold) unlocks.
        case DRG_TOG_ON_HOLD:
            if (record->event.pressed) {
                tap_hold_timer = timer_read();
            } else {
                bool dragscroll_was_locked = charybdis_get_pointer_dragscroll_enabled();

                if (dragscroll_was_locked) {
                    // Already locked — any release (tap or hold) unlocks.
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                } else if (timer_elapsed(tap_hold_timer) > CUSTOM_TAP_HOLD_TERM) {
                    // HOLD: toggle drag-scroll lock on
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                } else {
                    // TAP: look up and send the base-layer key at this matrix position
                    uint16_t fallback_key = keymap_key_to_keycode(LAYER_BASE, record->event.key);
                    tap_code16(fallback_key);
                }
            }
            return false;
    }

    // --- 2) Custom tap/hold/longer-hold keys (react on both press and release) ---
    // Most keys fire their hold variant immediately when the threshold is
    // reached (via matrix_scan_user), so you don't have to release the key.
    // Arrow keys keep the old release-based behavior because they have a
    // third tier (longer hold → GUI+Arrow) that requires waiting for release.
    switch (keycode) {
        case KC_1:
        case KC_2:
        case KC_3:
        case KC_4:
        case KC_5:
        case KC_9:
        case KC_0:
        case KC_MINS:
        case KC_EQL:
        case KC_LBRC:
        case KC_RBRC:
        case KC_BSLS:
        case KC_GRV:
        case KC_SCLN:
        case KC_QUOT:
        case KC_COMM:
        case KC_DOT:
        case KC_LEFT:
        case KC_RIGHT:
        case KC_ENT:
            if (record->event.pressed) {
                tap_hold_timer   = timer_read();
                tap_hold_keycode = keycode;
                tap_hold_fired   = false;
            } else {
                tap_hold_keycode = KC_NO;
                if (tap_hold_fired) {
                    // Hold variant was already sent by matrix_scan_user.
                    tap_hold_fired = false;
                } else {
                    uint16_t tap_hold_elapsed_time = timer_elapsed(tap_hold_timer);

                    if (tap_hold_elapsed_time < CUSTOM_TAP_HOLD_TERM) {
                        // TAP: send the plain key (1, 2, -, etc.)
                        tap_code16(keycode);
                    } else if (tap_hold_elapsed_time > CUSTOM_LONGER_HOLD_TERM) {
                        // LONGER HOLD: third-tier action (GUI+Arrow for navigation)
                        send_longer_hold_variant(keycode);
                    } else {
                        // HOLD: shifted variant (!, @, _, etc.)
                        send_hold_variant(keycode);
                    }
                }
            }
            return false;
    }

    // --- 3) Everything below is press-only — let releases pass through. ---
    if (!record->event.pressed) {
        return true;
    }

    // --- 4) Macros (fire once on key-down, using SEND_STRING for combos) ---
    switch (keycode) {
        case MACRO_0: // Spotlight search (macOS): GUI + Space
            SEND_STRING(SS_LGUI(SS_TAP(X_SPACE)));
            return false;

        case MACRO_1: // Claude: Alt + Space
            SEND_STRING(SS_LALT(SS_TAP(X_SPACE)));
            return false;

        case MACRO_2: // Terminal: Alt + GUI + Space
            SEND_STRING(SS_LALT(SS_LGUI(SS_TAP(X_SPACE))));
            return false;

        case MACRO_3: // OCR text copy (macOS): Ctrl + Alt + GUI + C
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("c"))));
            return false;

        case MACRO_4: // Screenshot (macOS): Ctrl + Alt + GUI + X
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("x"))));
            return false;

        case MACRO_5: // Emoji picker (macOS): Ctrl + GUI + Space
            SEND_STRING(SS_LCTL(SS_LGUI(SS_TAP(X_SPACE))));
            return false;
    }

    return true;
}

// ─── Immediate Hold Detection ────────────────────────────────────────────────
//
// Called every matrix scan cycle (~1ms).  When a tap/hold key has been held
// past CUSTOM_TAP_HOLD_TERM, fire the hold variant immediately instead of
// waiting for release.  Arrow keys are excluded — they use the release-based
// three-tier system so you can choose between hold (Alt+Arrow) and longer
// hold (GUI+Arrow).
void matrix_scan_user(void) {
    if (tap_hold_keycode != KC_NO && !tap_hold_fired) {
        // Arrow keys keep release-based behavior for their three-tier system.
        if (tap_hold_keycode == KC_LEFT || tap_hold_keycode == KC_RIGHT) {
            return;
        }
        if (timer_elapsed(tap_hold_timer) >= CUSTOM_TAP_HOLD_TERM) {
            send_hold_variant(tap_hold_keycode);
            tap_hold_fired = true;
        }
    }
}

// ─── Pointing Device Integration ────────────────────────────────────────────
//
// The Charybdis has a trackball on the right half.  QMK's pointing
// device subsystem calls these hooks to let us customize behavior.
//
// Key features:
//   - Auto-mouse: LAYER_POINTER activates when the trackball moves and
//     deactivates after AUTO_MOUSE_TIME (1200ms) of no movement.
//   - Sniping: Automatically enabled on LAYER_RAISE for precision work
//     (lower DPI while that layer is active).
//   - Mode interception: Volume, Brightness, Zoom, and Arrow modes intercept
//     trackball reports before they reach the OS (see pointing_device_modes.h).
#ifdef POINTING_DEVICE_ENABLE

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
void pointing_device_init_user(void) {
    set_auto_mouse_layer(LAYER_POINTER);
    set_auto_mouse_enable(true);
    split_sync_init(); // register split RPC handler for state sync
}
// Tell QMK which custom keycodes count as "mouse activity" so that pressing
// them keeps the auto-mouse layer alive (prevents it from timing out while
// you're actively using trackball features).
bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        // Charybdis firmware keycodes
        case SNIPING_MODE:
        case SNIPING_MODE_TOGGLE:
        case DRAGSCROLL_MODE:
        case DRAGSCROLL_MODE_TOGGLE:
        case DPI_MOD:
        case DPI_RMOD:
        case S_D_MOD:
        case S_D_RMOD:
        // Custom keycodes (from our enum)
        case DRG_TOG_ON_HOLD:
        case ARROW_MODE:
        case ZOOM_MODE:
        case BRIGHTNESS_MODE:
        case VOLUME_MODE:
            return true;
    }
    return false;
}
#    endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

// Called every scan cycle with the trackball's motion report.
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (!pd_mode_active(pd_mode_priority[i])) continue;
        switch (pd_mode_priority[i]) {
            case PD_MODE_VOLUME:
                return handle_volume_mode(mouse_report);
            case PD_MODE_BRIGHTNESS:
                return handle_brightness_mode(mouse_report);
            case PD_MODE_ZOOM:
                return handle_zoom_mode(mouse_report);
            case PD_MODE_ARROW:
                return handle_arrow_mode(mouse_report);
            default:
                break; // PD_MODE_DRAGSCROLL: handled by charybdis firmware
        }
    }
    return mouse_report;
}

// Called whenever the active layer set changes.
// We use this to:
//   1. Prevent the auto-mouse pointer layer from stacking on top of other
//      active layers (which would cause flickering / stuck states).
//   2. Enable sniping (lower DPI) automatically on LAYER_RAISE.
//   3. Disable auto-mouse while LAYER_RAISE is active, because sniping
//      and auto-mouse fight over pointer behavior.
layer_state_t layer_state_set_user(layer_state_t state) {
    // Enable sniping (lower DPI) automatically on LAYER_RAISE.
    charybdis_set_pointer_sniping_enabled(layer_state_cmp(state, LAYER_RAISE));

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    // Strip LAYER_POINTER when LAYER_RAISE is active to avoid key
    // conflicts (both layers define different keys at the same positions).
    // Auto-mouse stays enabled — it may re-activate POINTER on trackball
    // movement, but the strip will remove it again on the next layer
    // state update.  This avoids calling set_auto_mouse_enable() which
    // destructively resets all auto-mouse state (key tracker, toggle,
    // timers), breaking drag-scroll lock and momentary drag-scroll.
    if (layer_state_cmp(state, LAYER_POINTER) && layer_state_cmp(state, LAYER_RAISE)) {
        state &= ~((layer_state_t)1 << LAYER_POINTER);
    }
    // For other non-base layers, strip POINTER only if nothing is holding
    // it active (no toggle, no held mouse key, no drag scroll).
    else if (layer_state_cmp(state, LAYER_POINTER) && (state & ~((layer_state_t)1 << LAYER_POINTER)) != 0 && !get_auto_mouse_toggle() && get_auto_mouse_key_tracker() == 0 && !charybdis_get_pointer_dragscroll_enabled()) {
        state &= ~((layer_state_t)1 << LAYER_POINTER);
    }
#    endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

    return state;
}

#endif // POINTING_DEVICE_ENABLE

#ifdef RGB_MATRIX_ENABLE

// ─── RGB Matrix Layer Indicators ────────────────────────────────────────────
//
// Each layer gets a distinct LED color so you always know which layer is
// active at a glance.  Pointing device modes overlay a color on the right
// half (the trackball side) so you can tell which mode the trackball is in.
//
// Color scheme:
//   LAYER_POINTER → animated white-to-red gradient (auto-mouse countdown)
//   LAYER_NUM     → green
//   LAYER_LOWER   → blue
//   LAYER_RAISE   → purple
//
// Mode overlays (right half only, highest priority first):
//   Drag scroll   → orange
//   Volume        → yellow
//   Brightness    → magenta
//   Zoom          → chartreuse/lime
//   Arrow         → cyan
//
// LAYER_BASE has no custom indicator — it uses the default RGB matrix effect.
//
// See rgb_helpers.h for the full split-safe helper API (rgb_set_both_halves,
// rgb_set_right_half, rgb_set_led_group, etc.).

// ─── LED Index Map ──────────────────────────────────────────────────────────
//
// Physical LED positions for reference when targeting specific LEDs.
// Numbers are the LED index passed to rgb_matrix_set_color().
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
// 0–28  → left half (29 LEDs)
// 29–57 → right half (29 LEDs, 2 are dummy/unused on pointer side)

// LED groups for future per-key highlights (e.g. lighting up modifier keys on specific layers).
static const uint8_t layer_raise_mods[] = {33, 18};
static const uint8_t layer_lower_mods[] = {4, 47};

// Auto-mouse gradient: starts white, fades to red as the timeout approaches.
// White is capped at v=150 (not MAX_BRIGHTNESS) to limit current draw — all LEDs
// lit white at full brightness exceeds the USB power budget.
static const hsv_t automouse_color_start = {.h = 0, .s = 0, .v = 150};                             // white
static const hsv_t automouse_color_end   = {.h = 0, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS}; // red

// Layer indicator colors (defined as HSV, converted to RGB once on first render).
static const hsv_t layer_lower_hsv = {.h = 169, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS}; // blue
static const hsv_t layer_raise_hsv = {.h = 180, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS}; // purple
static const hsv_t layer_num_hsv   = {.h = 85, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS};  // green

// Pointing device mode overlay colors.
static const hsv_t mode_dragscroll_hsv = {.h = 21, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS};  // orange
static const hsv_t mode_volume_hsv     = {.h = 43, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS};  // yellow
static const hsv_t mode_brightness_hsv = {.h = 213, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS}; // magenta
static const hsv_t mode_arrow_hsv      = {.h = 127, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS}; // cyan
static const hsv_t mode_zoom_hsv       = {.h = 64, .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS};  // chartreuse/lime

// Pre-computed RGB cache.  hsv_to_rgb() is called once at init instead of
// every frame (~30fps × 2 chunks = ~60 calls/sec saved).
static rgb_t layer_lower_rgb, layer_raise_rgb, layer_num_rgb;
// Mode overlay colors, indexed to match pd_mode_priority[].
static rgb_t pd_mode_rgb[PD_MODE_COUNT];
static bool  rgb_colors_init = false;

// QMK calls this in batches (led_min to led_max) — potentially multiple
// times per frame, once per "chunk" of LEDs.  On a split keyboard, each
// half only processes its own LEDs.
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    // One-time initialization: convert all HSV colors to RGB.
    if (!rgb_colors_init) {
        layer_lower_rgb = hsv_to_rgb(layer_lower_hsv);
        layer_raise_rgb = hsv_to_rgb(layer_raise_hsv);
        layer_num_rgb   = hsv_to_rgb(layer_num_hsv);
        // Mode overlay colors, indexed to match pd_mode_priority[]:
        // [0] dragscroll, [1] volume, [2] brightness, [3] zoom, [4] arrow
        pd_mode_rgb[0]  = hsv_to_rgb(mode_dragscroll_hsv);
        pd_mode_rgb[1]  = hsv_to_rgb(mode_volume_hsv);
        pd_mode_rgb[2]  = hsv_to_rgb(mode_brightness_hsv);
        pd_mode_rgb[3]  = hsv_to_rgb(mode_zoom_hsv);
        pd_mode_rgb[4]  = hsv_to_rgb(mode_arrow_hsv);
        rgb_colors_init = true;
    }

    // Paint all LEDs with the active layer's color.
    // Check explicitly-activated layers before auto-activated LAYER_POINTER,
    // so that e.g. sniping (LAYER_RAISE) shows purple even when LAYER_POINTER
    // is also active due to a held mode key.
    // On LAYER_BASE, skip layer painting — the default RGB effect has already
    // rendered — but still fall through to the mode overlay below so that
    // e.g. drag-scroll lock shows orange on the right half.
    bool layer_painted = true;
    if (layer_state_cmp(layer_state, LAYER_RAISE)) {
        rgb_set_both_halves(layer_raise_rgb, led_min, led_max);
    } else if (layer_state_cmp(layer_state, LAYER_LOWER)) {
        rgb_set_both_halves(layer_lower_rgb, led_min, led_max);
    } else if (layer_state_cmp(layer_state, LAYER_NUM)) {
        rgb_set_both_halves(layer_num_rgb, led_min, led_max);
    } else if (layer_state_cmp(layer_state, LAYER_POINTER)) {
        automouse_rgb_render(led_min, led_max, automouse_color_start, automouse_color_end);
    } else {
        layer_painted = false;
    }

    // If a pointing device mode is active, override the right half with the
    // mode's color.  This provides instant visual feedback for which mode
    // the trackball is in.  Priority order comes from pd_mode_priority[].
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_mode_priority[i])) {
            rgb_set_right_half(pd_mode_rgb[i], led_min, led_max);
            break;
        }
    }

    return layer_painted;
}

#endif // RGB_MATRIX_ENABLE
