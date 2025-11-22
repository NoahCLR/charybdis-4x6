#include QMK_KEYBOARD_H
#include "rgb_helpers.h"
#include "trackerball_helpers.h"

// ------------------------------------------------------------
// Custom Keycodes & Keymap Layers
// ------------------------------------------------------------
enum custom_keycodes {
    MACRO_0 = SAFE_RANGE,
    MACRO_1,
    MACRO_2,
    MACRO_3,
    MACRO_4,
    MACRO_5,
    MACRO_6,
    MACRO_7,
    MACRO_8,
    MACRO_9,
    MACRO_10,
    MACRO_11,
    MACRO_12,
    MACRO_13,
    MACRO_14,
    MACRO_15,
    VOLMODE,
    CARET_MODE,
};

enum charybdis_keymap_layers {
    LAYER_BASE = 0,
    LAYER_LOWER,
    LAYER_RAISE,
    LAYER_POINTER,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // clang-format off
  [LAYER_BASE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  QK_GESC,              KC_1,              KC_2,              KC_3,              KC_4,              KC_5,                 KC_6,              KC_7,              KC_8,              KC_9,              KC_0,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                   KC_TAB,              KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,                 KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,           KC_BSLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),             KC_A,             KC_S,             KC_D,       LT(2,KC_F),              KC_G,                 KC_H,        LT(1,KC_J),              KC_K,              KC_L,           KC_SCLN,           KC_QUOT,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  KC_LCTL,        LT(1,KC_Z),              KC_X,              KC_C,              KC_V,              KC_B,                 KC_N,              KC_M,           KC_COMM,            KC_DOT,     LT(2,KC_SLSH),           KC_LALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           KC_LGUI,            KC_SPC,           DRGSCRL,             MO(2),            KC_ENT,
                                                                                                MO(1),           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
  ),

  [LAYER_LOWER] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  RGB_TOG,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LPRN,           KC_RPRN,           KC_QUOT,           KC_PPLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),          XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LBRC,           KC_RBRC,        S(KC_QUOT),           KC_PEQL,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  MACRO_5,           MACRO_3,           MACRO_4,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,        S(KC_LBRC),        S(KC_RBRC),           XXXXXXX,           XXXXXXX,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           KC_LGUI,            KC_SPC,           DRGSCRL,           XXXXXXX,           XXXXXXX,
                                                                                                MO(1),           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
  ),

  [LAYER_RAISE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              KC_MPLY,           KC_MNXT,           KC_MPRV,           KC_MUTE,           KC_VOLD,           KC_VOLU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           G(KC_Q),           XXXXXXX,           G(KC_A),           XXXXXXX,           XXXXXXX,              MACRO_2,           G(KC_C),             KC_UP,           G(KC_V),           KC_BRID,           KC_BRIU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),          KC_LGUI,          KC_LALT,          G(KC_C),            MO(2),           XXXXXXX,              MACRO_1,           KC_LEFT,           KC_DOWN,           KC_RGHT,           XXXXXXX,           XXXXXXX,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  KC_LCTL,           G(KC_Z),           G(KC_X),           G(KC_V),           XXXXXXX,           XXXXXXX,              MACRO_0,        KC_MS_BTN1,        KC_MS_BTN2,           DRGSCRL,           XXXXXXX,           KC_RALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           KC_LGUI,            KC_SPC,           DRGSCRL,           KC_LALT,            KC_ENT,
                                                                                                MO(1),           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
  ),

  [LAYER_POINTER] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           DPI_MOD,          DPI_RMOD,           S_D_MOD,          S_D_RMOD,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,        KC_MS_BTN3,           DRG_TOG,           SNP_TOG,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              VOLMODE,        KC_MS_BTN1,        KC_MS_BTN2,           DRGSCRL,           _______,        CARET_MODE,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           KC_LGUI,           _______,           DRGSCRL,           _______,            KC_ENT,
                                                                                              _______,           _______,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
  ),
    // clang-format on
};

// ------------------------------------------------------------
// Macros
// ------------------------------------------------------------
static uint16_t tap_hold_timer;

static void send_hold_variant(uint16_t keycode) {
    switch (keycode) {
        // Number row
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

        // Punctuation row
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

        // Right-hand punctuation
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

        // Arrow keys with Alt modifier
        case KC_LEFT:
            tap_code16(A(KC_LEFT));
            break; // Alt + Left Arrow
        case KC_RIGHT:
            tap_code16(A(KC_RIGHT));
            break; // Alt + Right Arrow

        // Fallback: just send the original unshifted key if we forgot a mapping
        default:
            tap_code(keycode);
            break;
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // --- 1) Hold-type keys (react on press + release) ---
    switch (keycode) {
        case VOLMODE:
            volmode_active = record->event.pressed;
            if (!volmode_active) {
                vol_acc      = 0;
                vol_last_dir = 0;
            }
            return false;

        case CARET_MODE:
            caret_active = record->event.pressed;
            if (!caret_active) {
                dominant_axis = '\0';
            }
            return false;
    }

    // --- 2) Tap/Hold symbol & number keys (Custom Auto Shift) ---
    switch (keycode) {
        case KC_1:
        case KC_2:
        case KC_3:
        case KC_4:
        case KC_5:
        case KC_6:
        case KC_7:
        case KC_8:
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
            if (record->event.pressed) {
                // key down: start timer, don't send anything yet
                tap_hold_timer = timer_read();
            } else {
                // key up: decide tap vs hold
                if (timer_elapsed(tap_hold_timer) < CUSTOM_TAP_HOLD_TERM) {
                    // TAP: send normal version (1, 2, -, =, etc.)
                    tap_code(keycode);
                } else {
                    // HOLD: send hold variant (Shifted symbol, Alt + Arrow, etc.)
                    send_hold_variant(keycode);
                }
            }
            return false;
    }

    // --- 3) Ignore releases for everything else ---
    if (!record->event.pressed) {
        return true;
    }

    // --- 4) Macros (press-only) ---
    switch (keycode) {
        case MACRO_0: // Spotlight: GUI + Space
            SEND_STRING(SS_LGUI(SS_TAP(X_SPACE)));
            return false;

        case MACRO_1: // ChatGPT: Alt + Space
            SEND_STRING(SS_LALT(SS_TAP(X_SPACE)));
            return false;

        case MACRO_2: // Terminal: Alt + GUI + Space
            SEND_STRING(SS_LALT(SS_LGUI(SS_TAP(X_SPACE))));
            return false;

        case MACRO_3: // OCR copy on macOS: Ctrl + Alt + GUI + C
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("c"))));
            return false;

        case MACRO_4: // Screenshot on macOS: Ctrl + Alt + GUI + X
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("x"))));
            return false;

        case MACRO_5: // Emoji picker: Ctrl + GUI + Space
            SEND_STRING(SS_LCTL(SS_LGUI(SS_TAP(X_SPACE))));
            return false;
    }

    return true;
}

// ------------------------------------------------------------
// Pointing Device Stuff
// ------------------------------------------------------------
#ifdef POINTING_DEVICE_ENABLE

// Automatically enable sniping-mode on the chosen layer.
#    define CHARYBDIS_AUTO_SNIPING_ON_LAYER LAYER_RAISE
#    ifdef CHARYBDIS_AUTO_SNIPING_ON_LAYER
layer_state_t layer_state_set_user(layer_state_t state) {
    charybdis_set_pointer_sniping_enabled(layer_state_cmp(state, CHARYBDIS_AUTO_SNIPING_ON_LAYER));

    // Manage Auto Mouse enabling/disabling based on layer to avoid conflicts with sniping
    uint8_t layer = get_highest_layer(state);

    switch (layer) {
        case LAYER_RAISE:
            set_auto_mouse_enable(false); // disable Auto Mouse when in sniping layer
            break;

        default:
            set_auto_mouse_enable(true); // enable it again
            break;
    }
    return state;
}
#    endif // CHARYBDIS_AUTO_SNIPING_ON_LAYER

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
void pointing_device_init_user(void) {
    set_auto_mouse_layer(LAYER_POINTER); // set default pointer layer
    set_auto_mouse_enable(true);         // enable Auto Mouse by default
}
#    endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case SNIPING_MODE:
            // Treat SNIPING as a mouse key so it WON'T deactivate the auto mouse layer
            return true;

        case SNIPING_MODE_TOGGLE:
            // Treat SNIPING as a mouse key so it WON'T deactivate the auto mouse layer
            return true;

        case DRAGSCROLL_MODE:
            // Treat DRAGSCROLL as a mouse key so it WON'T deactivate the auto mouse layer
            return true;

        case DRAGSCROLL_MODE_TOGGLE:
            // Treat DRAGSCROLL as a mouse key so it WON'T deactivate the auto mouse layer
            return true;
    }
    return false;
}

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    // Volume mode (held custom key)
    if (volmode_active) {
        return handle_volume_mode(mouse_report);
    }
    // Caret mode (held custom key)
    else if (caret_active) {
        return handle_caret_mode(mouse_report);
    }
    // Default: pass through unchanged
    else {
        return mouse_report;
    }
}
#endif // POINTING_DEVICE_ENABLE

// ------------------------------------------------------------
// RGB Stuff
// ------------------------------------------------------------
#ifdef RGB_MATRIX_ENABLE
/**
 * LEDs index.
 *
 * ╭────────────────────────╮                 ╭────────────────────────╮
 *    0   7   8  15  16  20                     49  45  44  37  36  29
 * ├────────────────────────┤                 ├────────────────────────┤
 *    1   6   9  14  17  21                     50  46  43  38  35  30
 * ├────────────────────────┤                 ├────────────────────────┤
 *    2   5  10  13  18  22                     51  47  42  39  34  31
 * ├────────────────────────┤                 ├────────────────────────┤
 *    3   4  11  12  19  23                     52  48  41  40  33  32
 * ╰────────────────────────╯                 ╰────────────────────────╯
 *                       26  27  28     53  54  XX
 *                           25  24     55  XX
 *                     ╰────────────╯ ╰────────────╯
 *
 * 0–28  → left half
 * 29–55 → right half
 */

// LED Groups
static const uint8_t layer_raise_mods[] = {33, 18};
static const uint8_t layer_lower_mods[] = {4, 47};

// ─── RGB HELPER SUMMARY ─────────────────────────────────────────────
// set_led_color(i, color)        → set single LED by index (side-aware)
// set_led_group(list, n, color)  → set non-contiguous LEDs
// fill_led_range(from, to, col)  → fill continuous LED range
// set_left_side(color)           → color all left-side LEDs
// set_right_side(color)          → color all right-side LEDs
// set_both_sides(color)          → color all LEDs on both halves
//
// Use this for color: hsv_to_rgb((hsv_t){.h = 180, .s = 255, .v = current_brightness})
// ───────────────────────────────────────────────────────────────────

// ------------------------------------------------------------
// RGB Matrix per-layer indicators
// ------------------------------------------------------------
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint8_t top = get_highest_layer(layer_state | default_layer_state);

    if (top != LAYER_POINTER && top != LAYER_LOWER && top != LAYER_RAISE) {
        return false;
    }

    uint8_t current_brightness = rgb_matrix_get_val();

    switch (top) {
        case LAYER_POINTER: {
            hsv_t hsv = (hsv_t){.h = 0, .s = 0, .v = 75};
            set_both_sides(hsv_to_rgb(hsv));
            //            set_led_group(layer_raise_mods, sizeof(layer_raise_mods), hsv_to_rgb((hsv_t){.h = 180, .s = 255, .v = current_brightness}));
            //            set_led_group(layer_lower_mods, sizeof(layer_lower_mods), hsv_to_rgb((hsv_t){.h = 169, .s = 255, .v = current_brightness}));
        } break;

        case LAYER_LOWER: {
            hsv_t hsv = (hsv_t){.h = 169, .s = 255, .v = current_brightness};
            set_both_sides(hsv_to_rgb(hsv));
        } break;

        case LAYER_RAISE: {
            hsv_t hsv = (hsv_t){.h = 180, .s = 255, .v = current_brightness};
            set_both_sides(hsv_to_rgb(hsv));
        } break;
    }

    return true;
}
#endif // RGB_MATRIX_ENABLE