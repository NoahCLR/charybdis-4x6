// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse Switch
// ────────────────────────────────────────────────────────────────────────────
//
// Auto-mouse on/off is two switches, as behaviours are (D-F32, D-F66): the
// stored setting in the profile, and a runtime override in RAM that AM_ON,
// AM_OFF and AM_TOGG flip. The override starts allowed at boot, is never
// saved and is cleared by a replug. QMK's auto-mouse flag holds both: it is on
// only while the stored setting and the override allow it.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>

// Sets the override and puts the stored setting AND it into QMK's flag.
void auto_mouse_switch_set_override(bool allowed);
bool auto_mouse_switch_override(void);

// Puts `stored` AND the override into QMK's flag: every push of the stored
// setting goes through here (boot, activation, a settings edit). Switching
// auto-mouse off turns its pointer layer off unless a layer owner keeps it,
// since QMK leaves it on and, while off, never times it out.
void auto_mouse_switch_apply(bool stored);
