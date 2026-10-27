#pragma once
#include QMK_KEYBOARD_H

// Physical presses during protected playback are ignored until released.
// Releases from before playback and synthetic records remain admitted.
bool noah_record_admission_macro_admit(keyrecord_t *record, bool protected, bool synthetic);
void noah_macro_input_guard_reset(void);
