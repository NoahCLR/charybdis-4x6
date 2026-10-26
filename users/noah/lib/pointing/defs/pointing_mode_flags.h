// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Flags
// ────────────────────────────────────────────────────────────────────────────
//
// Mode identity constants and read-only state queries. This header carries
// no handler types, activation logic, or QMK pointing-device dependencies,
// so non-pointing modules (RGB, key engine) can depend on it without pulling
// in the full pointing-mode API.
//
// The full API lives in pointing_modes.h (which includes this file).
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../../compat/split_half.h"
#include "pointing_mode_manifest.h"

// ─── Mode flag bit constants ────────────────────────────────────────────────

typedef uint32_t pointing_mode_mask_t;
typedef uint8_t  pointing_mode_id_t;

#define POINTING_MODE_ID_NONE UINT8_MAX

enum {
#define NOAH_POINTING_MODE_INDEX(name, keycode) POINTING_MODE_INDEX_##name,
    NOAH_POINTING_MODE_LIST(NOAH_POINTING_MODE_INDEX)
#undef NOAH_POINTING_MODE_INDEX
        POINTING_MODE_COUNT,
};

// C11 enumeration constants are ints, and slot 31's flag does not fit one: it
// is written as the int with the same bits (two's complement, as GCC and
// Clang define the conversion) and the assertion below proves every constant
// converts back to its exact flag. Use these constants where a
// pointing_mode_mask_t is expected; compare against slot 31 through
// pointing_mode_mask_from_id() or a pointing_mode_mask_t cast.
enum {
#define NOAH_POINTING_MODE_FLAG(name, keycode) POINTING_MODE_##name = (int)((pointing_mode_mask_t)1u << POINTING_MODE_INDEX_##name),
    NOAH_POINTING_MODE_LIST(NOAH_POINTING_MODE_FLAG)
#undef NOAH_POINTING_MODE_FLAG
};

#define NOAH_POINTING_MODE_FLAG_EXACT(name, keycode) &&((pointing_mode_mask_t)POINTING_MODE_##name == ((pointing_mode_mask_t)1u << POINTING_MODE_INDEX_##name))
_Static_assert(1 NOAH_POINTING_MODE_LIST(NOAH_POINTING_MODE_FLAG_EXACT), "a pointing-mode flag constant does not convert back to its exact pointing_mode_mask_t bit");
#undef NOAH_POINTING_MODE_FLAG_EXACT

// The flag type alone bounds the slot count. The split runtime sync carries
// the active and locked modes as one-byte pointing_mode_id_t ids, not masks, so
// it holds up to 254 slots (POINTING_MODE_ID_NONE is 255) without changing.
_Static_assert(POINTING_MODE_COUNT <= (sizeof(pointing_mode_mask_t) * 8u), "POINTING_MODE_COUNT exceeds pointing_mode_mask_t storage; widen the flag type before adding more modes");

// One flag names one mode: its bit position, found without a scan.
static inline pointing_mode_id_t pointing_mode_id_from_mask(pointing_mode_mask_t mode) {
    if (mode == 0 || (mode & (mode - 1u)) != 0) {
        return POINTING_MODE_ID_NONE;
    }

    pointing_mode_id_t index = (pointing_mode_id_t)__builtin_ctz(mode);
    return index < POINTING_MODE_COUNT ? index : POINTING_MODE_ID_NONE;
}

static inline pointing_mode_mask_t pointing_mode_mask_from_id(pointing_mode_id_t id) {
    if (id >= POINTING_MODE_COUNT) {
        return 0;
    }

    return (pointing_mode_mask_t)1u << id;
}

// No layer, as a running mode paints beneath none.
#define POINTING_MODE_NO_LAYER UINT8_MAX

// What a running mode runs from (D-F62): its key, held or locked, or its
// linked layers. A key source beats a layer source.
typedef enum {
    POINTING_MODE_SOURCE_NONE = 0,
    POINTING_MODE_SOURCE_KEY,
    POINTING_MODE_SOURCE_LAYER,
} pointing_mode_source_t;

// ─── Read-only state queries ────────────────────────────────────────────────
//
// Local queries report the authoritative runtime state on the current half.
// Display queries report what UI consumers should render: local state on the
// master half, mirrored split-sync state on the slave half.

// active_source is this half's own; the slave half's display view has none.
typedef struct {
    pointing_mode_mask_t   active_mode;
    pointing_mode_mask_t   locked_mode;
    pointing_mode_traits_t active_traits;
    uint8_t                active_index;
    uint8_t                locked_index;
    split_side_mask_t      owner_sides;
    pointing_mode_source_t active_source;
} pointing_mode_snapshot_view_t;

typedef struct {
    pointing_mode_snapshot_view_t local;
    pointing_mode_snapshot_view_t display;
} pointing_mode_snapshot_t;

pointing_mode_snapshot_t pointing_mode_snapshot(void);
// Identity and owner keys from one published generation, for consumers that
// render both together. Taking them from two separate accessors is coherent
// per call but can still pair identity from one publication with an owner
// bitmap from the next.
pointing_mode_snapshot_t pointing_mode_snapshot_with_owner_bitmap(uint8_t *out_owner_bitmap, bool *out_has_owner_keys);

pointing_mode_mask_t pointing_mode_local_active_snapshot(void);
pointing_mode_mask_t pointing_mode_local_locked_snapshot(void);
pointing_mode_mask_t pointing_mode_display_active_snapshot(void);
pointing_mode_mask_t pointing_mode_display_locked_snapshot(void);

bool              pointing_mode_local_active(pointing_mode_mask_t mode);
bool              pointing_mode_local_locked(pointing_mode_mask_t mode);
bool              pointing_mode_display_active(pointing_mode_mask_t mode);
bool              pointing_mode_display_locked(pointing_mode_mask_t mode);
bool              pd_any_local_mode_active(void);
bool              pd_any_local_mode_locked(void);
// A mode runs from its key, held or locked: what holds up a profile
// activation. A mode its layer started does not (D-F62).
bool              pointing_mode_local_key_source_active(void);
// Every live layer of `mode`'s link, its linked layers and the layers its key
// turns on, bit n layer n, from this half's own pointing slots. A running mode
// paints beneath the lowest of them that is on (D-F62).
uint16_t          pointing_mode_link_layers(pointing_mode_mask_t mode);
pointing_mode_source_t pointing_mode_local_source_snapshot(void);
bool              pd_any_display_mode_locked(void);
split_side_mask_t pointing_mode_local_owner_sides_snapshot(void);
split_side_mask_t pointing_mode_display_owner_sides_snapshot(void);
