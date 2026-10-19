#include <assert.h>
#include <stdio.h>
#include "users/noah/lib/compat/qmk_profile_adjustments.h"
#include "users/noah/lib/profile/schema/profile_settings_v1.h"

static uint8_t mods, oneshot, last_setting;
static uint32_t calls, queued;
static uint8_t last_action;
uint8_t get_mods(void) { return mods; }
uint8_t get_oneshot_mods(void) { return oneshot; }
uint16_t charybdis_get_pointer_default_dpi(void) { return 800u; }
uint16_t charybdis_get_pointer_sniping_dpi(void) { return 300u; }
void charybdis_cycle_pointer_default_dpi_noeeprom(bool forward) { last_action = forward ? 1u : 2u; calls++; }
void charybdis_cycle_pointer_sniping_dpi_noeeprom(bool forward) { last_action = forward ? 3u : 4u; calls++; }
#define ACTION(name, id) void name(void) { last_action = id; calls++; }
ACTION(rgb_matrix_enable_noeeprom, 5u)
ACTION(rgb_matrix_disable_noeeprom, 6u)
ACTION(rgb_matrix_toggle_noeeprom, 7u)
ACTION(rgb_matrix_step_noeeprom, 8u)
ACTION(rgb_matrix_step_reverse_noeeprom, 9u)
ACTION(rgb_matrix_increase_hue_noeeprom, 10u)
ACTION(rgb_matrix_decrease_hue_noeeprom, 11u)
ACTION(rgb_matrix_increase_sat_noeeprom, 12u)
ACTION(rgb_matrix_decrease_sat_noeeprom, 13u)
ACTION(rgb_matrix_increase_val_noeeprom, 14u)
ACTION(rgb_matrix_decrease_val_noeeprom, 15u)
ACTION(rgb_matrix_increase_speed_noeeprom, 16u)
ACTION(rgb_matrix_decrease_speed_noeeprom, 17u)
ACTION(rgb_matrix_flags_step_noeeprom, 18u)
ACTION(rgb_matrix_flags_step_reverse_noeeprom, 19u)
uint32_t noah_qmk_portable_live_setting(uint8_t id) { return id; }
bool noah_profile_store_runtime_adjust(uint8_t id, uint32_t value) {
    assert(value == (id == NOAH_SETTING_DEFAULT_DPI ? 800u : id == NOAH_SETTING_SNIPING_DPI ? 300u : id));
    last_setting = id;
    queued++;
    return true;
}
int main(void) {
    // No EEPROM-writing setter is linked: any regression to one fails to link.
    const struct { uint16_t key; uint8_t setting, normal, shifted; bool press; } cases[] = {
        {POINTER_DEFAULT_DPI_FORWARD, NOAH_SETTING_DEFAULT_DPI, 1u, 2u, true},
        {POINTER_DEFAULT_DPI_REVERSE, NOAH_SETTING_DEFAULT_DPI, 2u, 1u, true},
        {POINTER_SNIPING_DPI_FORWARD, NOAH_SETTING_SNIPING_DPI, 3u, 4u, true},
        {POINTER_SNIPING_DPI_REVERSE, NOAH_SETTING_SNIPING_DPI, 4u, 3u, true},
        {QK_RGB_MATRIX_ON, NOAH_SETTING_RGB_MODE, 5u, 5u, false},
        {QK_RGB_MATRIX_OFF, NOAH_SETTING_RGB_MODE, 6u, 6u, false},
        {QK_RGB_MATRIX_TOGGLE, NOAH_SETTING_RGB_MODE, 7u, 7u, false},
        {QK_RGB_MATRIX_MODE_NEXT, NOAH_SETTING_RGB_MODE, 8u, 9u, false},
        {QK_RGB_MATRIX_MODE_PREVIOUS, NOAH_SETTING_RGB_MODE, 9u, 8u, false},
        {QK_RGB_MATRIX_HUE_UP, NOAH_SETTING_RGB_COLOR, 10u, 11u, false},
        {QK_RGB_MATRIX_HUE_DOWN, NOAH_SETTING_RGB_COLOR, 11u, 10u, false},
        {QK_RGB_MATRIX_SATURATION_UP, NOAH_SETTING_RGB_COLOR, 12u, 13u, false},
        {QK_RGB_MATRIX_SATURATION_DOWN, NOAH_SETTING_RGB_COLOR, 13u, 12u, false},
        {QK_RGB_MATRIX_VALUE_UP, NOAH_SETTING_RGB_COLOR, 14u, 15u, false},
        {QK_RGB_MATRIX_VALUE_DOWN, NOAH_SETTING_RGB_COLOR, 15u, 14u, false},
        {QK_RGB_MATRIX_SPEED_UP, NOAH_SETTING_RGB_MODE, 16u, 17u, false},
        {QK_RGB_MATRIX_SPEED_DOWN, NOAH_SETTING_RGB_MODE, 17u, 16u, false},
        {QK_RGB_MATRIX_FLAG_NEXT, NOAH_SETTING_RGB_MODE, 18u, 19u, false},
        {QK_RGB_MATRIX_FLAG_PREVIOUS, NOAH_SETTING_RGB_MODE, 19u, 18u, false},
        {QK_UNDERGLOW_TOGGLE, NOAH_SETTING_RGB_MODE, 7u, 7u, true},
        {QK_UNDERGLOW_MODE_NEXT, NOAH_SETTING_RGB_MODE, 8u, 9u, true},
        {QK_UNDERGLOW_MODE_PREVIOUS, NOAH_SETTING_RGB_MODE, 9u, 8u, true},
        {QK_UNDERGLOW_HUE_UP, NOAH_SETTING_RGB_COLOR, 10u, 11u, true},
        {QK_UNDERGLOW_HUE_DOWN, NOAH_SETTING_RGB_COLOR, 11u, 10u, true},
        {QK_UNDERGLOW_SATURATION_UP, NOAH_SETTING_RGB_COLOR, 12u, 13u, true},
        {QK_UNDERGLOW_SATURATION_DOWN, NOAH_SETTING_RGB_COLOR, 13u, 12u, true},
        {QK_UNDERGLOW_VALUE_UP, NOAH_SETTING_RGB_COLOR, 14u, 15u, true},
        {QK_UNDERGLOW_VALUE_DOWN, NOAH_SETTING_RGB_COLOR, 15u, 14u, true},
        {QK_UNDERGLOW_SPEED_UP, NOAH_SETTING_RGB_MODE, 16u, 17u, true},
        {QK_UNDERGLOW_SPEED_DOWN, NOAH_SETTING_RGB_MODE, 17u, 16u, true},
    };
    for (uint8_t shift = 0u; shift < 2u; shift++) {
        mods = shift ? MOD_MASK_SHIFT : 0u;
        for (size_t i = 0u; i < sizeof(cases)/sizeof(cases[0]); i++) {
            bool trigger = cases[i].press;
#ifdef RGB_TRIGGER_ON_KEYDOWN
            trigger = true;
#endif
            uint32_t prior = calls;
            for (uint8_t pressed = 0u; pressed < 2u; pressed++) {
                keyrecord_t record = {.event.pressed = pressed};
                assert(!noah_qmk_profile_adjustment_process(cases[i].key, &record));
                if (pressed == trigger) assert(calls == prior + 1u && queued == calls && last_setting == cases[i].setting && last_action == (shift ? cases[i].shifted : cases[i].normal));
                prior = calls;
            }
        }
    }
    uint32_t prior = calls;
    keyrecord_t record = {.event.pressed = true};
    assert(noah_qmk_profile_adjustment_process(CM_TOGG, &record));
    assert(noah_qmk_profile_adjustment_process(SNIPING_MODE_TOGGLE, &record));
    assert(calls == prior && queued == calls);
    mods = 0u; oneshot = MOD_MASK_SHIFT;
    assert(!noah_qmk_profile_adjustment_process(POINTER_DEFAULT_DPI_FORWARD, &record) && last_action == 2u);
    puts("DPI and RGB keys preserve QMK triggers/shift behavior, queue profile edits, and link no EEPROM writers; toggles bypass edits");
}
