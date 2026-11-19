// ────────────────────────────────────────────────────────────────────────────
// Autocorrect Switch
// ────────────────────────────────────────────────────────────────────────────
//
// Autocorrect on/off is two switches, as behaviours and auto-mouse are
// (D-F66): the stored setting in the profile (keymap option 12), and a runtime
// override in RAM that QMK's AC_ON, AC_OFF and AC_TOGG flip. The override
// starts allowed at boot, is never saved and is cleared by a replug. The
// engine corrects only while both allow, and reads both each time it looks,
// so no push of the settings can undo the override.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>

void autocorrect_switch_set_override(bool allowed);
bool autocorrect_switch_override(void);
