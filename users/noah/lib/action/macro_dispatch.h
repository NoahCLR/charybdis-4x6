// ────────────────────────────────────────────────────────────────────────────
// Macro Dispatch
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

bool macro_dispatch(uint16_t keycode);
void macro_dispatch_validate_all(void);
