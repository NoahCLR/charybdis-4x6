#include QMK_KEYBOARD_H
#include "qmk_profile_adjustments.h"
#include "qmk_pointing_contract.h"
#include "../pointing/defs/pointing_modes.h"

#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE) && defined(NOAH_PORTABLE_PROFILE_ENABLE)
#    include "qmk_portable_profile.h"
#    include "../profile/storage/profile_store_runtime.h"
#    include "../profile/schema/profile_settings_v1.h"

// QMK's normal key semantics, with its no-EEPROM setters. Keys have instant
// local feedback; the owner serializes their values as a working-copy edit.
__attribute__((noinline)) bool noah_qmk_profile_adjustment_process(uint16_t keycode, keyrecord_t *record) {
    bool shifted = get_mods() & MOD_MASK_SHIFT;
#    ifdef POINTING_DEVICE_ENABLE
    bool dpi_shifted = get_mods() & MOD_MASK_SHIFT;
#    ifndef NO_ACTION_ONESHOT
    dpi_shifted = (get_mods() | get_oneshot_mods()) & MOD_MASK_SHIFT;
#    endif
#    endif
    uint8_t setting;
    bool shared_rgb = false;
#    if !defined(RGB_MATRIX_DISABLE_SHARED_KEYCODES)
    // QMK's legacy RGB_* keys share the matrix handler but fire on press.
    switch (keycode) {
        case QK_UNDERGLOW_TOGGLE: keycode = QK_RGB_MATRIX_TOGGLE; shared_rgb = true; break;
        case QK_UNDERGLOW_MODE_NEXT: keycode = QK_RGB_MATRIX_MODE_NEXT; shared_rgb = true; break;
        case QK_UNDERGLOW_MODE_PREVIOUS: keycode = QK_RGB_MATRIX_MODE_PREVIOUS; shared_rgb = true; break;
        case QK_UNDERGLOW_HUE_UP: keycode = QK_RGB_MATRIX_HUE_UP; shared_rgb = true; break;
        case QK_UNDERGLOW_HUE_DOWN: keycode = QK_RGB_MATRIX_HUE_DOWN; shared_rgb = true; break;
        case QK_UNDERGLOW_SATURATION_UP: keycode = QK_RGB_MATRIX_SATURATION_UP; shared_rgb = true; break;
        case QK_UNDERGLOW_SATURATION_DOWN: keycode = QK_RGB_MATRIX_SATURATION_DOWN; shared_rgb = true; break;
        case QK_UNDERGLOW_VALUE_UP: keycode = QK_RGB_MATRIX_VALUE_UP; shared_rgb = true; break;
        case QK_UNDERGLOW_VALUE_DOWN: keycode = QK_RGB_MATRIX_VALUE_DOWN; shared_rgb = true; break;
        case QK_UNDERGLOW_SPEED_UP: keycode = QK_RGB_MATRIX_SPEED_UP; shared_rgb = true; break;
        case QK_UNDERGLOW_SPEED_DOWN: keycode = QK_RGB_MATRIX_SPEED_DOWN; shared_rgb = true; break;
    }
#    endif
    switch (keycode) {
#    ifdef POINTING_DEVICE_ENABLE
        case POINTER_DEFAULT_DPI_FORWARD:
        case POINTER_DEFAULT_DPI_REVERSE:
            if (record->event.pressed) {
                charybdis_cycle_pointer_default_dpi_noeeprom((keycode == POINTER_DEFAULT_DPI_FORWARD) != dpi_shifted);
                pointing_mode_forget_applied_dpi(); // QMK's step wrote the CPI
                (void)noah_profile_store_runtime_adjust(NOAH_SETTING_DEFAULT_DPI, charybdis_get_pointer_default_dpi());
            }
            return false;
        case POINTER_SNIPING_DPI_FORWARD:
        case POINTER_SNIPING_DPI_REVERSE:
            if (record->event.pressed) {
                charybdis_cycle_pointer_sniping_dpi_noeeprom((keycode == POINTER_SNIPING_DPI_FORWARD) != dpi_shifted);
                pointing_mode_forget_applied_dpi(); // QMK's step wrote the CPI
                (void)noah_profile_store_runtime_adjust(NOAH_SETTING_SNIPING_DPI, charybdis_get_pointer_sniping_dpi());
            }
            return false;
#    endif
        case QK_RGB_MATRIX_ON:
        case QK_RGB_MATRIX_OFF:
        case QK_RGB_MATRIX_TOGGLE:
        case QK_RGB_MATRIX_MODE_NEXT:
        case QK_RGB_MATRIX_MODE_PREVIOUS:
        case QK_RGB_MATRIX_SPEED_UP:
        case QK_RGB_MATRIX_SPEED_DOWN:
        case QK_RGB_MATRIX_FLAG_NEXT:
        case QK_RGB_MATRIX_FLAG_PREVIOUS:
            setting = NOAH_SETTING_RGB_MODE;
            break;
        case QK_RGB_MATRIX_HUE_UP:
        case QK_RGB_MATRIX_HUE_DOWN:
        case QK_RGB_MATRIX_SATURATION_UP:
        case QK_RGB_MATRIX_SATURATION_DOWN:
        case QK_RGB_MATRIX_VALUE_UP:
        case QK_RGB_MATRIX_VALUE_DOWN:
            setting = NOAH_SETTING_RGB_COLOR;
            break;
        default:
#    ifdef POINTING_DEVICE_ENABLE
            // QMK's own sniping and drag-scroll keys write the sensor's CPI
            // once this record path lets them through.
            if (noah_qmk_contract_pointer_keycode_sets_cpi(keycode)) pointing_mode_forget_applied_dpi();
#    endif
            return true; // participation and pointing toggles stay RAM overrides
    }
#    ifdef RGB_TRIGGER_ON_KEYDOWN
    bool trigger = record->event.pressed;
#    else
    bool trigger = !record->event.pressed;
#    endif
    if (shared_rgb) trigger = record->event.pressed;
    if (!trigger) return false;
    switch (keycode) {
        case QK_RGB_MATRIX_ON: rgb_matrix_enable_noeeprom(); break;
        case QK_RGB_MATRIX_OFF: rgb_matrix_disable_noeeprom(); break;
        case QK_RGB_MATRIX_TOGGLE: rgb_matrix_toggle_noeeprom(); break;
        case QK_RGB_MATRIX_MODE_NEXT:
        case QK_RGB_MATRIX_MODE_PREVIOUS:
            if ((keycode == QK_RGB_MATRIX_MODE_NEXT) != shifted) rgb_matrix_step_noeeprom();
            else rgb_matrix_step_reverse_noeeprom();
            break;
        case QK_RGB_MATRIX_HUE_UP:
        case QK_RGB_MATRIX_HUE_DOWN:
            if ((keycode == QK_RGB_MATRIX_HUE_UP) != shifted) rgb_matrix_increase_hue_noeeprom();
            else rgb_matrix_decrease_hue_noeeprom();
            break;
        case QK_RGB_MATRIX_SATURATION_UP:
        case QK_RGB_MATRIX_SATURATION_DOWN:
            if ((keycode == QK_RGB_MATRIX_SATURATION_UP) != shifted) rgb_matrix_increase_sat_noeeprom();
            else rgb_matrix_decrease_sat_noeeprom();
            break;
        case QK_RGB_MATRIX_VALUE_UP:
        case QK_RGB_MATRIX_VALUE_DOWN:
            if ((keycode == QK_RGB_MATRIX_VALUE_UP) != shifted) rgb_matrix_increase_val_noeeprom();
            else rgb_matrix_decrease_val_noeeprom();
            break;
        case QK_RGB_MATRIX_SPEED_UP:
        case QK_RGB_MATRIX_SPEED_DOWN:
            if ((keycode == QK_RGB_MATRIX_SPEED_UP) != shifted) rgb_matrix_increase_speed_noeeprom();
            else rgb_matrix_decrease_speed_noeeprom();
            break;
        case QK_RGB_MATRIX_FLAG_NEXT:
        case QK_RGB_MATRIX_FLAG_PREVIOUS:
            if ((keycode == QK_RGB_MATRIX_FLAG_NEXT) != shifted) rgb_matrix_flags_step_noeeprom();
            else rgb_matrix_flags_step_reverse_noeeprom();
            break;
    }
    (void)noah_profile_store_runtime_adjust(setting, noah_qmk_portable_live_setting(setting));
    return false;
}
#else
bool noah_qmk_profile_adjustment_process(uint16_t keycode, keyrecord_t *record) {
    (void)record;
#    ifdef POINTING_DEVICE_ENABLE
    // QMK's Charybdis keys write the sensor's CPI once this record path lets
    // them through.
    if (noah_qmk_contract_pointer_keycode_sets_cpi(keycode)) pointing_mode_forget_applied_dpi();
#    else
    (void)keycode;
#    endif
    return true;
}
#endif
