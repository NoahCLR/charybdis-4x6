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

enum charybdis_keymap_layers {
    LAYER_BASE = 0,
    LAYER_LOWER,
    LAYER_RAISE,
    LAYER_POINTER,
};

/** \brief Automatically enable sniping-mode on the pointer layer. */
// #define CHARYBDIS_AUTO_SNIPING_ON_LAYER LAYER_POINTER

#define LOWER MO(LAYER_LOWER)
#define RAISE MO(LAYER_RAISE)
#define PT_Z LT(LAYER_POINTER, KC_Z)
#define PT_SLSH LT(LAYER_POINTER, KC_SLSH)

#ifndef POINTING_DEVICE_ENABLE
#    define DRGSCRL KC_NO
#    define DPI_MOD KC_NO
#    define S_D_MOD KC_NO
#    define SNIPING KC_NO
#endif // !POINTING_DEVICE_ENABLE

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [LAYER_BASE] = LAYOUT(
  // ╭──────────────────────────────────────────────────────╮ ╭──────────────────────────────────────────────────────╮
        KC_ESC,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,       KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
        KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,       KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_BSLS,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       KC_LSFT,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,       KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       KC_LCTL,    PT_Z,    KC_X,    KC_C,    KC_V,    KC_B,       KC_N,    KC_M, KC_COMM,  KC_DOT, PT_SLSH, KC_LALT,
  // ╰──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────╯
                                   KC_LGUI, KC_SPC,   LOWER,      RAISE,  KC_ENT,
                                           KC_LALT, KC_BSPC,     KC_DEL
  //                            ╰───────────────────────────╯ ╰──────────────────╯
  ),

  [LAYER_LOWER] = LAYOUT(
  // ╭──────────────────────────────────────────────────────╮ ╭──────────────────────────────────────────────────────╮
       KC_TILD, KC_EXLM,   KC_AT, KC_HASH,  KC_DLR, KC_PERC,    KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_UNDS,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       RGB_MOD, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    KC_LBRC,   KC_P7,   KC_P8,   KC_P9, KC_RBRC, XXXXXXX,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       RGB_TOG, KC_LGUI, KC_LALT, KC_LCTL, KC_LSFT, XXXXXXX,    KC_PPLS,   KC_P4,   KC_P5,   KC_P6, KC_PMNS, KC_PEQL,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
      RGB_RMOD, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    KC_PAST,   KC_P1,   KC_P2,   KC_P3, KC_PSLS, KC_PDOT,
  // ╰──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────╯
                                  XXXXXXX, XXXXXXX, _______,    XXXXXXX, _______,
                                           XXXXXXX, XXXXXXX,      KC_P0
  //                            ╰───────────────────────────╯ ╰──────────────────╯
  ),

  [LAYER_RAISE] = LAYOUT(
  // ╭──────────────────────────────────────────────────────╮ ╭──────────────────────────────────────────────────────╮
        KC_F12,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,      KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       KC_MNXT, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, KC_VOLU,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       KC_MPLY, KC_LEFT,   KC_UP, KC_DOWN, KC_RGHT, XXXXXXX,    XXXXXXX, KC_RSFT, KC_RCTL, KC_RALT, KC_RGUI, KC_MUTE,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       KC_MPRV, KC_HOME, KC_PGUP, KC_PGDN,  KC_END, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, KC_VOLD,
  // ╰──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────╯
                                  _______, _______, XXXXXXX,    _______, XXXXXXX,
                                           _______, _______,    XXXXXXX
  //                            ╰───────────────────────────╯ ╰──────────────────╯
  ),

  [LAYER_POINTER] = LAYOUT(
  // ╭──────────────────────────────────────────────────────╮ ╭──────────────────────────────────────────────────────╮
       QK_BOOT,  EE_CLR, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, QK_BOOT,  EE_CLR,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, DPI_MOD, S_D_MOD,    S_D_MOD, DPI_MOD, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       XXXXXXX, KC_LGUI, KC_LALT, KC_LCTL, KC_LSFT, XXXXXXX,    XXXXXXX, KC_RSFT, KC_RCTL, KC_RALT, KC_RGUI, XXXXXXX,
  // ├──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────┤
       XXXXXXX, _______, DRGSCRL, SNIPING, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, SNIPING, DRGSCRL, _______, XXXXXXX,
  // ╰──────────────────────────────────────────────────────┤ ├──────────────────────────────────────────────────────╯
                                  KC_BTN2, KC_BTN1, KC_BTN3,    KC_BTN3, KC_BTN1,
                                           XXXXXXX, KC_BTN2,    KC_BTN2
  //                            ╰───────────────────────────╯ ╰──────────────────╯
  ),
};
// clang-format on

#ifdef POINTING_DEVICE_ENABLE

#    ifdef CHARYBDIS_AUTO_SNIPING_ON_LAYER
layer_state_t layer_state_set_user(layer_state_t state) {
    charybdis_set_pointer_sniping_enabled(layer_state_cmp(state, CHARYBDIS_AUTO_SNIPING_ON_LAYER));
    return state;
}
#    endif // CHARYBDIS_AUTO_SNIPING_ON_LAYER

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
void pointing_device_init_user(void) {
    set_auto_mouse_layer(LAYER_POINTER); // use your pointer layer
    set_auto_mouse_enable(true);         // MUST be enabled to work
}
#    endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

#endif // POINTING_DEVICE_ENABLE

#ifdef RGB_MATRIX_ENABLE
// Treat both OS Caps Lock and QMK Caps Word as "caps active"
static inline bool caps_is_active(void) {
#    ifdef CAPS_WORD_ENABLE
    extern bool is_caps_word_on(void);
    if (is_caps_word_on()) return true;
#    endif
    return host_keyboard_led_state().caps_lock;
}

// --- Caps Lock indicator: support multiple LED indices ---
#    define CAPS_LED_COUNT 3
static const uint8_t CAPS_LED_INDICES[CAPS_LED_COUNT] = {1, 5, 7}; // edit this list to your desired LEDs

static inline void paint_caps_yellow_if_on(uint8_t led_min, uint8_t led_max) {
    if (!caps_is_active()) return;
    hsv_t caps_hsv = {.h = 43, .s = 255, .v = rgb_matrix_get_val()}; // yellow at current brightness
    rgb_t caps_rgb = hsv_to_rgb(caps_hsv);
    for (uint8_t i = 0; i < CAPS_LED_COUNT; i++) {
        uint8_t idx = CAPS_LED_INDICES[i];
        if (idx >= led_min && idx < led_max) {
            RGB_MATRIX_INDICATOR_SET_COLOR(idx, caps_rgb.r, caps_rgb.g, caps_rgb.b);
        }
    }
}
// --- end caps list helpers ---

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint8_t top = get_highest_layer(layer_state | default_layer_state);

    // Non-overriding path: when effects are running, paint Caps LEDs yellow if Caps is ON
    paint_caps_yellow_if_on(led_min, led_max);

    if (top == LAYER_POINTER) {
        // White = saturation 0; keep current brightness
        hsv_t hsv = {.h = 0, .s = 0, .v = rgb_matrix_get_val()};
        rgb_t rgb = hsv_to_rgb(hsv);

        for (uint8_t i = led_min; i < led_max; i++) {
            // Paint every LED; if you only want key LEDs, wrap this in:
            // if (HAS_FLAGS(g_led_config.flags[i], LED_FLAG_KEYLIGHT)) { ... }
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        }

        // Overwrite the solid layer color for Caps LEDs when Caps is ON
        paint_caps_yellow_if_on(led_min, led_max);

        // Return true to fully override the active RGB effect with solid white.
        return true;
    } else if (top == LAYER_LOWER) {
        // Green = hue 85°, full saturation, keep current brightness
        hsv_t hsv = {.h = 85, .s = 255, .v = rgb_matrix_get_val()};
        rgb_t rgb = hsv_to_rgb(hsv);

        for (uint8_t i = led_min; i < led_max; i++) {
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        }

        // Overwrite the solid layer color for Caps LEDs when Caps is ON
        paint_caps_yellow_if_on(led_min, led_max);

        // Override active RGB effect with solid green
        return true;
    } else if (top == LAYER_RAISE) {
        // Purple = hue 180°, full saturation, keep current brightness
        hsv_t hsv = {.h = 180, .s = 255, .v = rgb_matrix_get_val()};
        rgb_t rgb = hsv_to_rgb(hsv);

        for (uint8_t i = led_min; i < led_max; i++) {
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        }

        // Overwrite the solid layer color for Caps LEDs when Caps is ON
        paint_caps_yellow_if_on(led_min, led_max);

        // Override active RGB effect with solid purple
        return true;
    }

    // Not on the pointer layer → don’t interfere with effects/other indicators
    return false;
}
#endif // RGB_MATRIX_ENABLE