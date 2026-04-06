// ────────────────────────────────────────────────────────────────────────────
// PD Mode Flags
// ────────────────────────────────────────────────────────────────────────────
//
// Mode identity constants and read-only state queries. This header carries
// no handler types, activation logic, or QMK pointing-device dependencies,
// so non-pointing modules (RGB, key engine) can depend on it without pulling
// in the full pd-mode API.
//
// The full API lives in pd_modes.h (which includes this file).
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "pd_mode_manifest.h"

// ─── Mode flag bit constants ────────────────────────────────────────────────

typedef uint16_t pd_mode_mask_t;

enum {
#define NOAH_PD_MODE_INDEX(name, keycode, handler, key_handler, reset, dpi) PD_MODE_INDEX_##name,
    NOAH_PD_MODE_LIST(NOAH_PD_MODE_INDEX)
#undef NOAH_PD_MODE_INDEX
    PD_MODE_COUNT,
};

enum {
#define NOAH_PD_MODE_FLAG(name, keycode, handler, key_handler, reset, dpi) PD_MODE_##name = ((pd_mode_mask_t)1u << PD_MODE_INDEX_##name),
    NOAH_PD_MODE_LIST(NOAH_PD_MODE_FLAG)
#undef NOAH_PD_MODE_FLAG
};

_Static_assert(PD_MODE_COUNT <= (sizeof(pd_mode_mask_t) * 8u), "PD_MODE_COUNT exceeds pd_mode_mask_t storage; widen the flag type and the split runtime sync packet before adding more modes");

// ─── Read-only state queries ────────────────────────────────────────────────

pd_mode_mask_t pd_mode_active_snapshot(void);
pd_mode_mask_t pd_mode_locked_snapshot(void);

bool pd_mode_active(pd_mode_mask_t mode);
bool pd_mode_locked(pd_mode_mask_t mode);
bool pd_any_mode_active(void);
bool pd_any_mode_locked(void);
