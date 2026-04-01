// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Mode Flags
// ────────────────────────────────────────────────────────────────────────────
//
// Mode identity constants and read-only state queries. This header carries
// no handler types, activation logic, or QMK pointing-device dependencies,
// so non-pointing modules (RGB, key engine) can depend on it without pulling
// in the full pointing-device mode API.
//
// The full API lives in pointing_device_modes.h (which includes this file).
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>
#include <stdbool.h>

// ─── Mode flag bit constants ────────────────────────────────────────────────

#define PD_MODE_VOLUME (1 << 0)
#define PD_MODE_ARROW (1 << 1)
#define PD_MODE_DRAGSCROLL (1 << 2)
#define PD_MODE_BRIGHTNESS (1 << 3)
#define PD_MODE_ZOOM (1 << 4)
#define PD_MODE_PINCH (1 << 5)
#define PD_MODE_COUNT 6

_Static_assert(PD_MODE_COUNT <= 8, "PD_MODE_COUNT exceeds 8-bit pd-mode storage; widen pd-mode flags and split-sync packet state before adding more modes");

// ─── Read-only state queries ────────────────────────────────────────────────

uint8_t pd_mode_active_snapshot(void);
uint8_t pd_mode_locked_snapshot(void);

bool pd_mode_active(uint8_t mode);
bool pd_mode_locked(uint8_t mode);
bool pd_any_mode_active(void);
bool pd_any_mode_locked(void);
