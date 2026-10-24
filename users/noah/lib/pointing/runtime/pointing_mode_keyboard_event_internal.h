// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Keyboard-Event Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Narrow private seam for querying mode-owned real modifiers that should be
// hidden from concurrent keyboard event processing while a pointing mode is
// active. Keep this out of the public pointing-mode headers so non-owner
// modules do not treat keyboard-event masking as part of the general
// pointing-mode API.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

uint8_t pointing_mode_active_keyboard_event_masked_real_mods(void);
