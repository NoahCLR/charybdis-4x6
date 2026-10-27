#pragma once
#include QMK_KEYBOARD_H

// Consume DPI/RGB adjustment keys before QMK's EEPROM-writing handlers.
bool noah_qmk_profile_adjustment_process(uint16_t keycode, keyrecord_t *record);
