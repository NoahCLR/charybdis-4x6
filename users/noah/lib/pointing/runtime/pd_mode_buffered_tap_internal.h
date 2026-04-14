// ────────────────────────────────────────────────────────────────────────────
// PD Mode Buffered-Tap Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Narrow private seam for querying mode-owned buffered tap replay policy.
// Keep this out of the public pd-mode headers so non-owner modules do not
// treat buffered-tap masking as part of the general pd-mode API.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

uint8_t pd_mode_buffered_tap_masked_real_mods(uint16_t keycode);
