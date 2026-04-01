// ────────────────────────────────────────────────────────────────────────────
// Keyboard Modifier State
// ────────────────────────────────────────────────────────────────────────────
//
// Snapshot/apply helpers for QMK modifier state. Useful when a helper needs to
// temporarily clear mods, send an action, then restore the prior mod context.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

typedef struct {
    uint8_t real;
    uint8_t weak;
    uint8_t oneshot;
    uint8_t oneshot_locked;
} keyboard_mod_state_t;

keyboard_mod_state_t keyboard_mod_state_suspend(void);
void                 keyboard_mod_state_apply(keyboard_mod_state_t state);
