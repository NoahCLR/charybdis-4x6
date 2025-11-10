/**
 * Copyright 2021 Charly Delay <charly@codesink.dev> (@0xcharly)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include QMK_KEYBOARD_H

// ------------------------------------------------------------
// Keymap Layers & Custom Keycodes
// ------------------------------------------------------------
enum charybdis_keymap_layers {
    LAYER_BASE = 0,
    LAYER_LOWER,
    LAYER_RAISE,
    LAYER_POINTER,
};

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
};

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [LAYER_BASE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                   KC_ESC,              KC_1,              KC_2,              KC_3,              KC_4,              KC_5,                 KC_6,              KC_7,              KC_8,              KC_9,              KC_0,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                   KC_TAB,              KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,                 KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,           KC_BSLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),             KC_A,             KC_S,             KC_D,       LT(2,KC_F),              KC_G,                 KC_H,        LT(1,KC_J),              KC_K,              KC_L,           KC_SCLN,           KC_QUOT,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  KC_LCTL,        LT(1,KC_Z),              KC_X,              KC_C,              KC_V,              KC_B,                 KC_N,              KC_M,           KC_COMM,            KC_DOT,     LT(2,KC_SLSH),           KC_LALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           KC_LGUI,            KC_SPC,        KC_MS_BTN8,             MO(2),            KC_ENT,
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
                                                                           KC_LGUI,            KC_SPC,        KC_MS_BTN8,           KC_VOLD,           KC_VOLU,
                                                                                                MO(1),           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
  ),

  [LAYER_RAISE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  RGB_TOG,           XXXXXXX,           DPI_MOD,          DPI_RMOD,           S_D_MOD,          S_D_RMOD,              KC_MPLY,           KC_MNXT,           KC_MPRV,           KC_MUTE,           KC_VOLD,           KC_VOLU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           G(KC_Q),           XXXXXXX,           G(KC_A),           XXXXXXX,           XXXXXXX,              MACRO_2,           G(KC_C),             KC_UP,           G(KC_V),           KC_BRID,           KC_BRIU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),          KC_LGUI,          KC_LALT,          G(KC_C),            MO(2),           XXXXXXX,              MACRO_1,           KC_LEFT,           KC_DOWN,           KC_RGHT,        KC_MS_BTN3,           XXXXXXX,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  KC_LCTL,           G(KC_Z),           G(KC_X),           G(KC_V),           XXXXXXX,           XXXXXXX,              MACRO_0,        KC_MS_BTN1,        KC_MS_BTN2,        KC_MS_BTN8,           XXXXXXX,           KC_RALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           KC_LGUI,            KC_SPC,        KC_MS_BTN8,           KC_LALT,            KC_ENT,
                                                                                                MO(1),           KC_BSPC,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
  ),

  [LAYER_POINTER] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  XXXXXXX,           XXXXXXX,           DPI_MOD,          DPI_RMOD,           S_D_MOD,          S_D_RMOD,              XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),          XXXXXXX,          XXXXXXX,          XXXXXXX,          _______,           XXXXXXX,              XXXXXXX,           XXXXXXX,        KC_MS_BTN3,           XXXXXXX,        KC_MS_BTN3,           XXXXXXX,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              SNP_TOG,        KC_MS_BTN1,        KC_MS_BTN2,        KC_MS_BTN8,           _______,           KC_RALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           KC_LGUI,           XXXXXXX,        KC_MS_BTN8,           KC_LALT,            KC_ENT,
                                                                                              XXXXXXX,           XXXXXXX,           KC_BSPC
  //                                                                    ╰────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────╯
  ),
};
// clang-format on

// ------------------------------------------------------------
// Macros
// ------------------------------------------------------------
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        // Only act on key press
        return true;
    }

    switch (keycode) {
        case MACRO_0:
            // Spotlight Shortcut: Gui+Space
            SEND_STRING(SS_DOWN(X_LGUI) SS_TAP(X_SPACE) SS_UP(X_LGUI));
            return false;

        case MACRO_1:
            // ChatGPT Shortcut: Alt+Space
            SEND_STRING(SS_DOWN(X_LALT) SS_TAP(X_SPACE) SS_UP(X_LALT));
            return false;

        case MACRO_2:
            // Terminal Shortcut: Alt+Gui+Space
            SEND_STRING(SS_DOWN(X_LALT) SS_DOWN(X_LGUI) SS_TAP(X_SPACE) SS_UP(X_LGUI) SS_UP(X_LALT));
            return false;

        case MACRO_3:
            // OCR copy on macOS
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("c"))));
            return false;

        case MACRO_4:
            // Screenshot on macOS
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("x"))));
            return false;

        case MACRO_5:
            // macOS Emoji picker
            SEND_STRING(SS_DOWN(X_LCTL) SS_DOWN(X_LGUI) SS_TAP(X_SPACE) SS_UP(X_LGUI) SS_UP(X_LCTL));
            return false;
    }

    return true;
}

// ------------------------------------------------------------
// Pointing Device Stuff
// ------------------------------------------------------------
#ifndef POINTING_DEVICE_ENABLE
#    define DRGSCRL KC_NO
#    define DPI_MOD KC_NO
#    define S_D_MOD KC_NO
#    define SNIPING KC_NO
#endif // !POINTING_DEVICE_ENABLE

#ifdef POINTING_DEVICE_ENABLE

// Automatically enable sniping-mode on the pointer layer.
#    define CHARYBDIS_AUTO_SNIPING_ON_LAYER LAYER_RAISE

#    ifdef CHARYBDIS_AUTO_SNIPING_ON_LAYER
layer_state_t layer_state_set_user(layer_state_t state) {
    charybdis_set_pointer_sniping_enabled(layer_state_cmp(state, CHARYBDIS_AUTO_SNIPING_ON_LAYER));

    // Manage Auto Mouse enabling/disabling based on layer
    uint8_t layer = get_highest_layer(state);

    switch (layer) {
        case LAYER_RAISE:
            set_auto_mouse_enable(false); // disable Auto Mouse
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
    set_auto_mouse_layer(LAYER_POINTER); // use your pointer layer
    set_auto_mouse_enable(true);         // MUST be enabled to work
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
    }
    return false;
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
 *                       26  27  28     53  54  XX Rightside Thumbkeys have a mapping issue,
 *                           25  24     55  XX
 *                     ╰────────────╯ ╰────────────╯
 *
 * Note: the LED config simulates 58 LEDs instead of the actual 56 to prevent
 * confusion when testing LEDs during assembly when handedness is not set
 * correctly.  Those fake LEDs are bound to the physical bottom-left corner.
 */

// ------------------------------------------------------------
// RGB Matrix per-layer indicators
// ------------------------------------------------------------
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint8_t top = get_highest_layer(layer_state | default_layer_state);

    // Only override on specific layers
    if (top != LAYER_POINTER && top != LAYER_LOWER && top != LAYER_RAISE) {
        return false;
    }

    uint8_t val = rgb_matrix_get_val();
    hsv_t   hsv;

    switch (top) {
        case LAYER_POINTER: // white
            hsv = (hsv_t){.h = 0, .s = 0, .v = 75};
            break;
        case LAYER_LOWER: // blue
            hsv = (hsv_t){.h = 169, .s = 255, .v = val};
            break;
        case LAYER_RAISE: // purple
            hsv = (hsv_t){.h = 180, .s = 255, .v = val};
            break;
    }

    rgb_t base_rgb = hsv_to_rgb(hsv);

    // set all LEDs to the layer color
    for (uint8_t i = led_min; i < led_max; i++) {
        rgb_matrix_set_color(i, base_rgb.r, base_rgb.g, base_rgb.b);
    }

    // Example: Different color on LED 3 depending on left/right half
    //    if (is_keyboard_left()) {
    //        rgb_matrix_set_color(3, 255, 0, 0);
    //    } else {
    //        rgb_matrix_set_color(3, 0, 0, 255);
    //    }

    //    rgb_matrix_set_color(32, 0, 0, 255);

    return true;
}

void rgb_matrix_set_color_all_debug(void) {
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        uint8_t color_index = i / 5;
        switch (color_index % 6) {
            case 0:
                rgb_matrix_set_color(i, 255, 0, 0);
                break; // red
            case 1:
                rgb_matrix_set_color(i, 0, 255, 0);
                break; // green
            case 2:
                rgb_matrix_set_color(i, 0, 0, 255);
                break; // blue
            case 3:
                rgb_matrix_set_color(i, 255, 255, 0);
                break; // yellow
            case 4:
                rgb_matrix_set_color(i, 255, 0, 255);
                break; // magenta
            case 5:
                rgb_matrix_set_color(i, 0, 255, 255);
                break; // cyan
        }
    }
}
#endif // RGB_MATRIX_ENABLE