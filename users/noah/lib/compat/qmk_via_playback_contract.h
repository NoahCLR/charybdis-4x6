// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Playback Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Centralizes this userspace's dependence on the current fork's VIA dynamic
// macro playback encoding.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

bool noah_qmk_contract_try_play_via_macro(uint16_t action);
