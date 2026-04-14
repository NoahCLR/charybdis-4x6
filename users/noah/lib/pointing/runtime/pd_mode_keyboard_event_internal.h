// ────────────────────────────────────────────────────────────────────────────
// PD Mode Keyboard-Event Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Narrow private seam for querying mode-owned real modifiers that should be
// hidden from concurrent keyboard event processing while a pd mode is active.
// Keep this out of the public pd-mode headers so non-owner modules do not
// treat keyboard-event masking as part of the general pd-mode API.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

uint8_t pd_mode_active_keyboard_event_masked_real_mods(void);
