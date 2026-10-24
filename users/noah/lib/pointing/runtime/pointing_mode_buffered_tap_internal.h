// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Buffered-Tap Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Narrow private seam for querying mode-owned buffered tap replay policy.
// Keep this out of the public pointing-mode headers so non-owner modules do not
// treat buffered-tap masking as part of the general pointing-mode API.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

uint8_t pointing_mode_buffered_tap_masked_real_mods(uint16_t keycode);
