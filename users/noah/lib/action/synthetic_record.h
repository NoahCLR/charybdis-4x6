// ────────────────────────────────────────────────────────────────────────────
// Synthetic Record Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Bridges authored key_behavior actions into synthetic records without
// confusing the physical key runtime.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

bool noah_synthetic_record_active(void);
bool noah_dispatch_synthetic_record(uint16_t keycode, bool pressed);
void noah_dispatch_synthetic_tap(uint16_t keycode);
void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count);
void noah_dispatch_synthetic_qmk_tap(uint16_t keycode);
