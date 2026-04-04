// ────────────────────────────────────────────────────────────────────────────
// Synthetic Record Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Bridges authored key_behavior actions back into process_record_user() for
// keymap-local custom keycodes without confusing the physical key runtime.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

bool noah_synthetic_record_active(void);
bool noah_dispatch_synthetic_record(uint16_t keycode, bool pressed);
void noah_dispatch_synthetic_tap(uint16_t keycode);
