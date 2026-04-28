// Host PD Fixture
//
// Shared host-test helpers for pd-mode display/view synthesis and split-runtime
// remote packet initialization.
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/split/runtime_sync.h"

static inline split_runtime_sync_remote_t host_runtime_fixture_split_remote_init(void) {
    return (split_runtime_sync_remote_t)SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;
}

static inline uint8_t host_runtime_fixture_pd_mode_index(const pd_mode_def_t *defs, uint8_t count, pd_mode_mask_t mode) {
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

static inline pd_mode_mask_t host_runtime_fixture_first_mode(const pd_mode_def_t *defs, uint8_t count, pd_mode_mask_t flags) {
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

static inline pd_mode_mask_t host_runtime_fixture_display_locked_mode(const pd_mode_def_t *defs, uint8_t count, bool is_master, pd_mode_mask_t local_locked, split_runtime_sync_remote_t remote) {
    (void)defs;
    (void)count;
    return is_master ? local_locked : pd_mode_mask_from_id(remote.locked_mode_id);
}

static inline pd_mode_mask_t host_runtime_fixture_display_active_mode(const pd_mode_def_t *defs, uint8_t count, bool is_master, pd_mode_mask_t local_active, pd_mode_mask_t local_locked, split_runtime_sync_remote_t remote) {
    (void)defs;
    (void)count;

    if (is_master) {
        return local_active;
    }

    pd_mode_mask_t locked_mode = host_runtime_fixture_display_locked_mode(defs, count, false, local_locked, remote);
    return locked_mode ? locked_mode : pd_mode_mask_from_id(remote.active_mode_id);
}

static inline split_side_mask_t host_runtime_fixture_display_owner_sides(bool is_master, split_side_mask_t local_owner_sides, split_runtime_sync_remote_t remote) {
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    return is_master ? local_owner_sides : remote.pd_mode_owner_sides;
#else
    (void)is_master;
    (void)local_owner_sides;
    (void)remote;
    return SPLIT_SIDE_MASK_NONE;
#endif
}

static inline pd_mode_snapshot_view_t host_runtime_fixture_pd_mode_view(const pd_mode_def_t *defs, uint8_t count, pd_mode_mask_t active_mode, pd_mode_mask_t locked_mode) {
    pd_mode_snapshot_view_t view = {
        .active_mode  = active_mode,
        .locked_mode  = locked_mode,
        .active_index = host_runtime_fixture_pd_mode_index(defs, count, active_mode),
        .locked_index = host_runtime_fixture_pd_mode_index(defs, count, locked_mode),
    };

    if (view.active_index < count) {
        view.active_traits = defs[view.active_index].traits;
    }

    return view;
}

static inline pd_mode_snapshot_t host_runtime_fixture_pd_mode_snapshot(const pd_mode_def_t *defs, uint8_t count, bool is_master, pd_mode_mask_t local_active, pd_mode_mask_t local_locked, split_side_mask_t local_owner_sides, split_runtime_sync_remote_t remote) {
    pd_mode_mask_t    display_locked      = host_runtime_fixture_display_locked_mode(defs, count, is_master, local_locked, remote);
    pd_mode_mask_t    display_active      = host_runtime_fixture_display_active_mode(defs, count, is_master, local_active, local_locked, remote);
    split_side_mask_t display_owner_sides = host_runtime_fixture_display_owner_sides(is_master, local_owner_sides, remote);

    pd_mode_snapshot_view_t local_view   = host_runtime_fixture_pd_mode_view(defs, count, local_active, local_locked);
    pd_mode_snapshot_view_t display_view = host_runtime_fixture_pd_mode_view(defs, count, display_active, display_locked);

    local_view.owner_sides   = local_owner_sides;
    display_view.owner_sides = display_owner_sides;

    return (pd_mode_snapshot_t){
        .local   = local_view,
        .display = display_view,
    };
}
