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

#include "../../compat/split_half.h"
#include "pd_mode_manifest.h"

// ─── Mode flag bit constants ────────────────────────────────────────────────

typedef uint16_t pd_mode_mask_t;
typedef uint8_t  pd_mode_id_t;

#define PD_MODE_ID_NONE UINT8_MAX

enum {
#define NOAH_PD_MODE_INDEX(name, keycode, handler, key_handler, reset, dpi, traits, lifecycle) PD_MODE_INDEX_##name,
    NOAH_PD_MODE_LIST(NOAH_PD_MODE_INDEX)
#undef NOAH_PD_MODE_INDEX
        PD_MODE_COUNT,
};

enum {
#define NOAH_PD_MODE_FLAG(name, keycode, handler, key_handler, reset, dpi, traits, lifecycle) PD_MODE_##name = ((pd_mode_mask_t)1u << PD_MODE_INDEX_##name),
    NOAH_PD_MODE_LIST(NOAH_PD_MODE_FLAG)
#undef NOAH_PD_MODE_FLAG
};

_Static_assert(PD_MODE_COUNT <= (sizeof(pd_mode_mask_t) * 8u), "PD_MODE_COUNT exceeds pd_mode_mask_t storage; widen the flag type and the split runtime sync packet before adding more modes");

static inline pd_mode_id_t pd_mode_id_from_mask(pd_mode_mask_t mode) {
    if (mode == 0) {
        return PD_MODE_ID_NONE;
    }

    for (pd_mode_id_t index = 0; index < PD_MODE_COUNT; index++) {
        if (mode == ((pd_mode_mask_t)1u << index)) {
            return index;
        }
    }

    return PD_MODE_ID_NONE;
}

static inline pd_mode_mask_t pd_mode_mask_from_id(pd_mode_id_t id) {
    if (id >= PD_MODE_COUNT) {
        return 0;
    }

    return (pd_mode_mask_t)1u << id;
}

// ─── Read-only state queries ────────────────────────────────────────────────
//
// Local queries report the authoritative runtime state on the current half.
// Display queries report what UI consumers should render: local state on the
// master half, mirrored split-sync state on the slave half.

typedef struct {
    pd_mode_mask_t   active_mode;
    pd_mode_mask_t   locked_mode;
    pd_mode_traits_t active_traits;
    uint8_t          active_index;
    uint8_t          locked_index;
    split_half_t     owner_half;
} pd_mode_snapshot_view_t;

typedef struct {
    pd_mode_snapshot_view_t local;
    pd_mode_snapshot_view_t display;
} pd_mode_snapshot_t;

pd_mode_snapshot_t pd_mode_snapshot(void);

pd_mode_mask_t pd_mode_local_active_snapshot(void);
pd_mode_mask_t pd_mode_local_locked_snapshot(void);
pd_mode_mask_t pd_mode_display_active_snapshot(void);
pd_mode_mask_t pd_mode_display_locked_snapshot(void);

bool pd_mode_local_active(pd_mode_mask_t mode);
bool pd_mode_local_locked(pd_mode_mask_t mode);
bool pd_mode_display_active(pd_mode_mask_t mode);
bool pd_mode_display_locked(pd_mode_mask_t mode);
bool pd_any_local_mode_active(void);
bool pd_any_local_mode_locked(void);
bool pd_any_display_mode_active(void);
bool pd_any_display_mode_locked(void);
split_half_t pd_mode_local_owner_half_snapshot(void);
split_half_t pd_mode_display_owner_half_snapshot(void);
