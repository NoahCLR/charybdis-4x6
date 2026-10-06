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

typedef uint32_t pd_mode_mask_t;
typedef uint8_t  pd_mode_id_t;

#define PD_MODE_ID_NONE UINT8_MAX

enum {
#define NOAH_PD_MODE_INDEX(name, keycode) PD_MODE_INDEX_##name,
    NOAH_PD_MODE_LIST(NOAH_PD_MODE_INDEX)
#undef NOAH_PD_MODE_INDEX
        PD_MODE_COUNT,
};

// C11 enumeration constants are ints, and slot 31's flag does not fit one: it
// is written as the int with the same bits (two's complement, as GCC and
// Clang define the conversion) and the assertion below proves every constant
// converts back to its exact flag. Use these constants where a
// pd_mode_mask_t is expected; compare against slot 31 through
// pd_mode_mask_from_id() or a pd_mode_mask_t cast.
enum {
#define NOAH_PD_MODE_FLAG(name, keycode) PD_MODE_##name = (int)((pd_mode_mask_t)1u << PD_MODE_INDEX_##name),
    NOAH_PD_MODE_LIST(NOAH_PD_MODE_FLAG)
#undef NOAH_PD_MODE_FLAG
};

#define NOAH_PD_MODE_FLAG_EXACT(name, keycode) &&((pd_mode_mask_t)PD_MODE_##name == ((pd_mode_mask_t)1u << PD_MODE_INDEX_##name))
_Static_assert(1 NOAH_PD_MODE_LIST(NOAH_PD_MODE_FLAG_EXACT), "a pd-mode flag constant does not convert back to its exact pd_mode_mask_t bit");
#undef NOAH_PD_MODE_FLAG_EXACT

// The flag type alone bounds the slot count. The split runtime sync carries
// the active and locked modes as one-byte pd_mode_id_t ids, not masks, so it
// holds up to 254 slots (PD_MODE_ID_NONE is 255) without changing.
_Static_assert(PD_MODE_COUNT <= (sizeof(pd_mode_mask_t) * 8u), "PD_MODE_COUNT exceeds pd_mode_mask_t storage; widen the flag type before adding more modes");

// One flag names one mode: its bit position, found without a scan.
static inline pd_mode_id_t pd_mode_id_from_mask(pd_mode_mask_t mode) {
    if (mode == 0 || (mode & (mode - 1u)) != 0) {
        return PD_MODE_ID_NONE;
    }

    pd_mode_id_t index = (pd_mode_id_t)__builtin_ctz(mode);
    return index < PD_MODE_COUNT ? index : PD_MODE_ID_NONE;
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
    pd_mode_mask_t    active_mode;
    pd_mode_mask_t    locked_mode;
    pd_mode_traits_t  active_traits;
    uint8_t           active_index;
    uint8_t           locked_index;
    split_side_mask_t owner_sides;
} pd_mode_snapshot_view_t;

typedef struct {
    pd_mode_snapshot_view_t local;
    pd_mode_snapshot_view_t display;
} pd_mode_snapshot_t;

pd_mode_snapshot_t pd_mode_snapshot(void);
// Identity and owner keys from one published generation, for consumers that
// render both together. Taking them from two separate accessors is coherent
// per call but can still pair identity from one publication with an owner
// bitmap from the next.
pd_mode_snapshot_t pd_mode_snapshot_with_owner_bitmap(uint8_t *out_owner_bitmap, bool *out_has_owner_keys);

pd_mode_mask_t pd_mode_local_active_snapshot(void);
pd_mode_mask_t pd_mode_local_locked_snapshot(void);
pd_mode_mask_t pd_mode_display_active_snapshot(void);
pd_mode_mask_t pd_mode_display_locked_snapshot(void);

bool              pd_mode_local_active(pd_mode_mask_t mode);
bool              pd_mode_local_locked(pd_mode_mask_t mode);
bool              pd_mode_display_active(pd_mode_mask_t mode);
bool              pd_mode_display_locked(pd_mode_mask_t mode);
bool              pd_any_local_mode_active(void);
bool              pd_any_local_mode_locked(void);
bool              pd_any_display_mode_locked(void);
split_side_mask_t pd_mode_local_owner_sides_snapshot(void);
split_side_mask_t pd_mode_display_owner_sides_snapshot(void);
