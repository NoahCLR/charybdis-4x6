#pragma once
// QMK_KEYBOARD_H for the portable-profile readback test: the QMK and
// Charybdis surface qmk_portable_profile.c reads, as host declarations.
#include "qmk_stub.h"
#define CHARYBDIS_AUTO_SNIPING_LAYER 3
#define CHARYBDIS_DRAGSCROLL_DPI 100
#define PD_MODE_VOLUME_DPI 100
#define PD_MODE_BRIGHTNESS_DPI 100
#define PD_MODE_ZOOM_DPI 400
#define PD_MODE_ARROW_DPI 400
#define RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS 200
#define AUTOMOUSE_RGB_DEAD_TIME 400
bool     get_auto_mouse_enable(void);
uint8_t  get_auto_mouse_layer(void);
uint16_t get_auto_mouse_timeout(void);
uint8_t  get_auto_mouse_debounce(void);
void     set_auto_mouse_enable(bool value);
void     set_auto_mouse_layer(uint8_t value);
void     set_auto_mouse_timeout(uint16_t value);
void     set_auto_mouse_debounce(uint8_t value);
bool     is_combo_enabled(void);
void     combo_enable(void);
void     combo_disable(void);
bool     rgb_matrix_is_enabled(void);
uint8_t  rgb_matrix_get_mode(void);
uint8_t  rgb_matrix_get_speed(void);
uint8_t  rgb_matrix_get_flags(void);
uint16_t rgb_matrix_get_hue(void);
uint8_t  rgb_matrix_get_sat(void);
uint8_t  rgb_matrix_get_val(void);
uint16_t charybdis_get_pointer_default_dpi(void);
uint16_t charybdis_get_pointer_sniping_dpi(void);
void     charybdis_cycle_pointer_default_dpi(bool forward);
void     charybdis_cycle_pointer_sniping_dpi(bool forward);
void     default_layer_set(uint32_t state);
extern uint32_t default_layer_state;
typedef union {
    uint16_t raw;
} keymap_config_t;
extern keymap_config_t keymap_config;
void eeconfig_update_keymap(const keymap_config_t *config);
