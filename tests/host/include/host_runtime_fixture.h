// Host Runtime Fixture
//
// Shared host-test fixture helpers for common QMK/runtime stub state such as
// time, modifier state, layer state, master-role selection, and pd-mode
// snapshot synthesis. Individual suites can keep their domain-specific logs
// and behaviors while sharing one baseline runtime model.
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/state/runtime/split_runtime_sync.h"

typedef struct {
    uint32_t time32;
    uint8_t  mods;
    uint8_t  weak_mods;
    uint8_t  oneshot_mods;
    uint8_t  oneshot_locked_mods;
    uint8_t  send_keyboard_report_count;
    bool     is_master;
} host_runtime_fixture_t;

#define HOST_RUNTIME_FIXTURE_INIT \
    {                             \
        .time32    = 1000u,       \
        .is_master = true,        \
    }

static inline void host_runtime_fixture_reset(host_runtime_fixture_t *fixture) {
    if (!fixture) {
        return;
    }

    *fixture = (host_runtime_fixture_t)HOST_RUNTIME_FIXTURE_INIT;
}

static inline split_runtime_sync_packet_t host_runtime_fixture_split_remote_init(void) {
    return (split_runtime_sync_packet_t){
        .key_preview_layer = UINT8_MAX,
    };
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

static inline pd_mode_mask_t host_runtime_fixture_display_locked_mode(const pd_mode_def_t *defs, uint8_t count, bool is_master, pd_mode_mask_t local_locked,
                                                                      split_runtime_sync_packet_t remote) {
    return is_master ? local_locked : host_runtime_fixture_first_mode(defs, count, remote.pd_mode_locked_flags);
}

static inline pd_mode_mask_t host_runtime_fixture_display_active_mode(const pd_mode_def_t *defs, uint8_t count, bool is_master, pd_mode_mask_t local_active,
                                                                      pd_mode_mask_t local_locked, split_runtime_sync_packet_t remote) {
    if (is_master) {
        return local_active;
    }

    pd_mode_mask_t locked_mode = host_runtime_fixture_display_locked_mode(defs, count, false, local_locked, remote);
    return locked_mode ? locked_mode : host_runtime_fixture_first_mode(defs, count, remote.pd_mode_flags);
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

static inline pd_mode_snapshot_t host_runtime_fixture_pd_mode_snapshot(const pd_mode_def_t *defs, uint8_t count, bool is_master, pd_mode_mask_t local_active,
                                                                       pd_mode_mask_t local_locked, split_runtime_sync_packet_t remote) {
    pd_mode_mask_t display_locked = host_runtime_fixture_display_locked_mode(defs, count, is_master, local_locked, remote);
    pd_mode_mask_t display_active = host_runtime_fixture_display_active_mode(defs, count, is_master, local_active, local_locked, remote);

    return (pd_mode_snapshot_t){
        .local   = host_runtime_fixture_pd_mode_view(defs, count, local_active, local_locked),
        .display = host_runtime_fixture_pd_mode_view(defs, count, display_active, display_locked),
    };
}

#define HOST_RUNTIME_FIXTURE_DEFINE_BASIC_QMK_STUBS(fixture_expr)                                     \
    uint16_t timer_read(void) {                                                                       \
        return (uint16_t)((fixture_expr).time32);                                                     \
    }                                                                                                 \
    uint16_t timer_elapsed(uint16_t last) {                                                           \
        return (uint16_t)(timer_read() - last);                                                       \
    }                                                                                                 \
    uint32_t timer_read32(void) {                                                                     \
        return (fixture_expr).time32;                                                                 \
    }                                                                                                 \
    uint32_t timer_elapsed32(uint32_t last) {                                                         \
        return (fixture_expr).time32 - last;                                                          \
    }                                                                                                 \
    uint8_t get_mods(void) {                                                                          \
        return (fixture_expr).mods;                                                                   \
    }                                                                                                 \
    uint8_t get_weak_mods(void) {                                                                     \
        return (fixture_expr).weak_mods;                                                              \
    }                                                                                                 \
    uint8_t get_oneshot_mods(void) {                                                                  \
        return (fixture_expr).oneshot_mods;                                                           \
    }                                                                                                 \
    uint8_t get_oneshot_locked_mods(void) {                                                           \
        return (fixture_expr).oneshot_locked_mods;                                                    \
    }                                                                                                 \
    void set_mods(uint8_t mods) {                                                                     \
        (fixture_expr).mods = mods;                                                                   \
    }                                                                                                 \
    void set_weak_mods(uint8_t mods) {                                                                \
        (fixture_expr).weak_mods = mods;                                                              \
    }                                                                                                 \
    void set_oneshot_mods(uint8_t mods) {                                                             \
        (fixture_expr).oneshot_mods = mods;                                                           \
    }                                                                                                 \
    void set_oneshot_locked_mods(uint8_t mods) {                                                      \
        (fixture_expr).oneshot_locked_mods = mods;                                                    \
    }                                                                                                 \
    void clear_mods(void) {                                                                           \
        (fixture_expr).mods = 0;                                                                      \
    }                                                                                                 \
    void clear_weak_mods(void) {                                                                      \
        (fixture_expr).weak_mods = 0;                                                                 \
    }                                                                                                 \
    void clear_oneshot_mods(void) {                                                                   \
        (fixture_expr).oneshot_mods = 0;                                                              \
    }                                                                                                 \
    void clear_oneshot_locked_mods(void) {                                                            \
        (fixture_expr).oneshot_locked_mods = 0;                                                       \
    }                                                                                                 \
    void add_mods(uint8_t mods) {                                                                     \
        (fixture_expr).mods |= mods;                                                                  \
    }                                                                                                 \
    void del_mods(uint8_t mods) {                                                                     \
        (fixture_expr).mods &= (uint8_t)~mods;                                                        \
    }                                                                                                 \
    void send_keyboard_report(void) {                                                                 \
        (fixture_expr).send_keyboard_report_count++;                                                  \
    }                                                                                                 \
    bool is_keyboard_master(void) {                                                                   \
        return (fixture_expr).is_master;                                                              \
    }

#define HOST_RUNTIME_FIXTURE_DEFINE_LAYER_STUBS()                                                     \
    layer_state_t layer_state;                                                                        \
    bool layer_state_cmp(layer_state_t state, uint8_t layer) {                                        \
        return (state & ((layer_state_t)1u << layer)) != 0;                                           \
    }                                                                                                 \
    void layer_on(uint8_t layer) {                                                                    \
        layer_state |= ((layer_state_t)1u << layer);                                                  \
    }                                                                                                 \
    void layer_off(uint8_t layer) {                                                                   \
        layer_state &= (layer_state_t)~((layer_state_t)1u << layer);                                  \
    }
