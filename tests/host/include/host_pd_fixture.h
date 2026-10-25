// Host PD Fixture
//
// Shared host-test helpers for pointing-mode display/view synthesis and
// split-runtime remote packet initialization.
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "users/noah/lib/pointing/defs/pointing_modes.h"
#include "users/noah/lib/split/runtime_sync.h"

static inline split_runtime_sync_remote_t host_runtime_fixture_split_remote_init(void) {
    return (split_runtime_sync_remote_t)SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;
}

static inline uint8_t host_runtime_fixture_pointing_mode_index(const pointing_mode_def_t *defs, uint8_t count, pointing_mode_mask_t mode) {
    if (!defs || !mode) {
        return count;
    }

    for (uint8_t index = 0; index < count; index++) {
        if (defs[index].mode_flag == mode) {
            return index;
        }
    }

    return count;
}

static inline pointing_mode_mask_t host_runtime_fixture_first_mode(const pointing_mode_def_t *defs, uint8_t count, pointing_mode_mask_t flags) {
    if (!defs || !flags) {
        return 0;
    }

    for (uint8_t index = 0; index < count; index++) {
        if ((flags & defs[index].mode_flag) != 0) {
            return defs[index].mode_flag;
        }
    }

    return 0;
}

static inline pointing_mode_mask_t host_runtime_fixture_display_locked_mode(const pointing_mode_def_t *defs, uint8_t count, bool is_master, pointing_mode_mask_t local_locked, split_runtime_sync_remote_t remote) {
    (void)defs;
    (void)count;
    return is_master ? local_locked : pointing_mode_mask_from_id(remote.locked_mode_id);
}

static inline pointing_mode_mask_t host_runtime_fixture_display_active_mode(const pointing_mode_def_t *defs, uint8_t count, bool is_master, pointing_mode_mask_t local_active, pointing_mode_mask_t local_locked, split_runtime_sync_remote_t remote) {
    (void)defs;
    (void)count;

    if (is_master) {
        return local_active;
    }

    pointing_mode_mask_t locked_mode = host_runtime_fixture_display_locked_mode(defs, count, false, local_locked, remote);
    return locked_mode ? locked_mode : pointing_mode_mask_from_id(remote.active_mode_id);
}

static inline split_side_mask_t host_runtime_fixture_display_owner_sides(bool is_master, split_side_mask_t local_owner_sides, split_runtime_sync_remote_t remote) {
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    return is_master ? local_owner_sides : remote.pointing_mode_owner_sides;
#else
    (void)is_master;
    (void)local_owner_sides;
    (void)remote;
    return SPLIT_SIDE_MASK_NONE;
#endif
}

// Traits come from a mode's effective slot, which the fixture does not model:
// its modes carry none.
static inline pointing_mode_snapshot_view_t host_runtime_fixture_pointing_mode_view(const pointing_mode_def_t *defs, uint8_t count, pointing_mode_mask_t active_mode, pointing_mode_mask_t locked_mode) {
    pointing_mode_snapshot_view_t view = {
        .active_mode  = active_mode,
        .locked_mode  = locked_mode,
        .active_index = host_runtime_fixture_pointing_mode_index(defs, count, active_mode),
        .locked_index = host_runtime_fixture_pointing_mode_index(defs, count, locked_mode),
    };

    return view;
}

static inline pointing_mode_snapshot_t host_runtime_fixture_pointing_mode_snapshot(const pointing_mode_def_t *defs, uint8_t count, bool is_master, pointing_mode_mask_t local_active, pointing_mode_mask_t local_locked, split_side_mask_t local_owner_sides, split_runtime_sync_remote_t remote) {
    pointing_mode_mask_t display_locked      = host_runtime_fixture_display_locked_mode(defs, count, is_master, local_locked, remote);
    pointing_mode_mask_t display_active      = host_runtime_fixture_display_active_mode(defs, count, is_master, local_active, local_locked, remote);
    split_side_mask_t    display_owner_sides = host_runtime_fixture_display_owner_sides(is_master, local_owner_sides, remote);

    pointing_mode_snapshot_view_t local_view   = host_runtime_fixture_pointing_mode_view(defs, count, local_active, local_locked);
    pointing_mode_snapshot_view_t display_view = host_runtime_fixture_pointing_mode_view(defs, count, display_active, display_locked);

    local_view.owner_sides   = local_owner_sides;
    display_view.owner_sides = display_owner_sides;
    // The fixture's local modes run from their keys, so they show their own
    // light; the slave half shows what the master mirrored.
    local_view.active_source  = local_active != 0 ? POINTING_MODE_SOURCE_KEY : POINTING_MODE_SOURCE_NONE;
    local_view.active_light   = local_active != 0;
    display_view.active_source = is_master ? local_view.active_source : POINTING_MODE_SOURCE_NONE;
    display_view.active_light  = display_active != 0 && (is_master ? local_view.active_light : remote.active_mode_light != 0u);

    return (pointing_mode_snapshot_t){
        .local   = local_view,
        .display = display_view,
    };
}
