#include QMK_KEYBOARD_H // QMK

#include "noah_runtime.h"
#include "noah_keymap.h"
#include "lib/macro/macro_payload.h"

#ifdef VIA_ENABLE
#    include "dynamic_keymap.h"
#    include "eeprom.h"
#    include "nvm_eeprom_eeconfig_internal.h"
#    include "via.h"
#    include "nvm_eeprom_via_internal.h"
#    ifdef ENCODER_MAP_ENABLE
#        include "encoder.h"
#    endif
#endif

#ifdef VIA_ENABLE
#    ifndef DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#        define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR (TOTAL_EEPROM_BYTE_COUNT - 1)
#    endif

#    ifndef DYNAMIC_KEYMAP_EEPROM_ADDR
#        define DYNAMIC_KEYMAP_EEPROM_ADDR (VIA_EEPROM_CONFIG_END)
#    endif

#    ifndef DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR
#        define DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR (DYNAMIC_KEYMAP_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2))
#    endif

#    ifdef ENCODER_MAP_ENABLE
#        ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#            define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * NUM_ENCODERS * 2 * 2))
#        endif
#    else
#        ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#            define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR)
#        endif
#    endif

#    ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE
#        define DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR - DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + 1)
#    endif

// Stage VIA macro defaults in BSS; the dynamic macro region is larger than
// the RP2040 process stack on this build.
static uint8_t via_macro_seed_buffer[DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE];
static bool    via_macro_seed_post_init_pending = false;
static bool    via_macro_seed_scan_pending      = false;
#endif

__attribute__((weak)) bool get_hold_on_other_key_press_keymap(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

__attribute__((weak)) bool process_record_keymap(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return true;
}

__attribute__((weak)) void matrix_scan_keymap(void) {
}

__attribute__((weak)) void keyboard_post_init_keymap(void) {
}

__attribute__((weak)) void eeconfig_init_keymap(void) {
}

__attribute__((weak)) layer_state_t layer_state_set_keymap(layer_state_t state) {
    return state;
}

__attribute__((weak)) report_mouse_t pointing_device_task_keymap(report_mouse_t mouse_report) {
    return mouse_report;
}

__attribute__((weak)) void pointing_device_init_keymap(void) {
}

__attribute__((weak)) bool is_mouse_record_keymap(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

__attribute__((weak)) bool rgb_matrix_indicators_advanced_keymap(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return true;
}

#ifdef VIA_ENABLE
static bool build_via_default_macro_seed_buffer(uint16_t capacity, uint16_t *written) {
    uint8_t *buffer = via_macro_seed_buffer;
    uint16_t offset = 0;

    for (uint8_t slot = 0; slot < VIA_MACRO_SLOT_COUNT; slot++) {
        const char *payload = via_macro_payloads[slot];
        uint16_t    encoded = 0;

        if (payload && *payload) {
            if (!macro_payload_encode(payload, &buffer[offset], capacity - offset, &encoded)) {
                return false;
            }
            offset += encoded;
        }

        if (offset >= capacity) {
            return false;
        }
        buffer[offset++] = 0x00;
    }

    *written = offset;
    return true;
}

static void seed_via_default_macros(void) {
    uint16_t capacity = dynamic_keymap_macro_get_buffer_size();
    uint16_t written  = 0;

    if (capacity == 0 || capacity > DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE) {
        return;
    }

    // Every call site runs after QMK has already reset the macro region to
    // zero, so we only need to write the authored macro prefix here.
    if (!build_via_default_macro_seed_buffer(capacity, &written) || written == 0) {
        return;
    }

    dynamic_keymap_macro_set_buffer(0, written, via_macro_seed_buffer);
}
#endif

void eeconfig_init_user(void) {
#if (EECONFIG_USER_DATA_SIZE) == 0
    eeconfig_update_user(0);
#endif

#ifdef VIA_ENABLE
    seed_via_default_macros();
    via_macro_seed_post_init_pending = false;
#endif

    eeconfig_init_keymap();
}

bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    if (noah_get_hold_on_other_key_press(keycode, record)) {
        return true;
    }

    return get_hold_on_other_key_press_keymap(keycode, record);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!noah_process_record_user(keycode, record)) {
        return false;
    }

    return process_record_keymap(keycode, record);
}

void matrix_scan_user(void) {
#ifdef VIA_ENABLE
    if (via_macro_seed_scan_pending) {
        seed_via_default_macros();
        via_macro_seed_scan_pending = false;
    }
#endif

    noah_matrix_scan_user();
    matrix_scan_keymap();
}

void keyboard_post_init_user(void) {
#ifdef VIA_ENABLE
    if (via_macro_seed_post_init_pending) {
        seed_via_default_macros();
        via_macro_seed_post_init_pending = false;
    }
#endif

    noah_keyboard_post_init_user();
    keyboard_post_init_keymap();
}

layer_state_t layer_state_set_user(layer_state_t state) {
    state = noah_layer_state_set_user(state);
    return layer_state_set_keymap(state);
}

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    mouse_report = noah_pointing_device_task_user(mouse_report);
    return pointing_device_task_keymap(mouse_report);
}

void pointing_device_init_user(void) {
    noah_pointing_device_init_user();
    pointing_device_init_keymap();
}

bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    if (noah_is_mouse_record_user(keycode, record)) {
        return true;
    }

    return is_mouse_record_keymap(keycode, record);
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool continue_kb = noah_rgb_matrix_indicators_advanced_user(led_min, led_max);

    if (!rgb_matrix_indicators_advanced_keymap(led_min, led_max)) {
        return false;
    }

    return continue_kb;
}

#ifdef VIA_ENABLE
void via_init_kb(void) {
    via_macro_seed_post_init_pending = !via_eeprom_is_valid();
}

bool via_command_kb(uint8_t *data, uint8_t length) {
    (void)length;

    switch (data[0]) {
#ifdef VIA_EEPROM_ALLOW_RESET
        case id_eeprom_reset:
            via_macro_seed_scan_pending = true;
            return false;
#endif
        case id_dynamic_keymap_macro_reset:
            via_macro_seed_scan_pending = true;
            return false;
        default:
            return false;
    }
}
#endif
