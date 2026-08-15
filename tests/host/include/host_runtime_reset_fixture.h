// Host Runtime Reset Fixture
//
// Shared host-test fixture helpers for common QMK/runtime stub state such as
// time, modifier state, layer state, and master-role selection.
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "users/noah/lib/state/shared/runtime_reset.h"

typedef struct {
    uint32_t time32;
    uint16_t timer_read32_count;
    uint16_t timer_elapsed32_count;
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

static inline void host_runtime_fixture_reset_userspace_runtime(void) {
    noah_runtime_reset_for_test();
}

#define HOST_RUNTIME_FIXTURE_DEFINE_MOD_REPORT_STUBS(fixture_expr) \
    uint8_t get_mods(void) {                                       \
        return (fixture_expr).mods;                                \
    }                                                              \
    uint8_t get_weak_mods(void) {                                  \
        return (fixture_expr).weak_mods;                           \
    }                                                              \
    uint8_t get_oneshot_mods(void) {                               \
        return (fixture_expr).oneshot_mods;                        \
    }                                                              \
    uint8_t get_oneshot_locked_mods(void) {                        \
        return (fixture_expr).oneshot_locked_mods;                 \
    }                                                              \
    void set_mods(uint8_t mods) {                                  \
        (fixture_expr).mods = mods;                                \
    }                                                              \
    void set_weak_mods(uint8_t mods) {                             \
        (fixture_expr).weak_mods = mods;                           \
    }                                                              \
    void set_oneshot_mods(uint8_t mods) {                          \
        (fixture_expr).oneshot_mods = mods;                        \
    }                                                              \
    void set_oneshot_locked_mods(uint8_t mods) {                   \
        (fixture_expr).oneshot_locked_mods = mods;                 \
    }                                                              \
    void clear_mods(void) {                                        \
        (fixture_expr).mods = 0;                                   \
    }                                                              \
    void clear_weak_mods(void) {                                   \
        (fixture_expr).weak_mods = 0;                              \
    }                                                              \
    void clear_oneshot_mods(void) {                                \
        (fixture_expr).oneshot_mods = 0;                           \
    }                                                              \
    void clear_oneshot_locked_mods(void) {                         \
        (fixture_expr).oneshot_locked_mods = 0;                    \
    }                                                              \
    void add_mods(uint8_t mods) {                                  \
        (fixture_expr).mods |= mods;                               \
    }                                                              \
    void del_mods(uint8_t mods) {                                  \
        (fixture_expr).mods &= (uint8_t)~mods;                     \
    }                                                              \
    void send_keyboard_report(void) {                              \
        (fixture_expr).send_keyboard_report_count++;               \
    }

#define HOST_RUNTIME_FIXTURE_DEFINE_TIME_MASTER_STUBS(fixture_expr) \
    uint16_t timer_read(void) {                                     \
        return (uint16_t)((fixture_expr).time32);                   \
    }                                                               \
    uint16_t timer_elapsed(uint16_t last) {                         \
        return (uint16_t)(timer_read() - last);                     \
    }                                                               \
    uint32_t timer_read32(void) {                                   \
        (fixture_expr).timer_read32_count++;                        \
        return (fixture_expr).time32;                               \
    }                                                               \
    uint32_t timer_elapsed32(uint32_t last) {                       \
        (fixture_expr).timer_elapsed32_count++;                     \
        return (fixture_expr).time32 - last;                        \
    }                                                               \
    bool is_keyboard_master(void) {                                 \
        return (fixture_expr).is_master;                            \
    }

#define HOST_RUNTIME_FIXTURE_DEFINE_BASIC_QMK_STUBS(fixture_expr) \
    HOST_RUNTIME_FIXTURE_DEFINE_MOD_REPORT_STUBS(fixture_expr)    \
    HOST_RUNTIME_FIXTURE_DEFINE_TIME_MASTER_STUBS(fixture_expr)

#define HOST_RUNTIME_FIXTURE_DEFINE_LAYER_STUBS()                       \
    layer_state_t layer_state;                                          \
    bool          layer_state_cmp(layer_state_t state, uint8_t layer) { \
        return (state & ((layer_state_t)1u << layer)) != 0;             \
    }                                                                   \
    void layer_on(uint8_t layer) {                                      \
        layer_state |= (layer_state_t)1u << layer;                      \
    }                                                                   \
    void layer_off(uint8_t layer) {                                     \
        layer_state &= (layer_state_t) ~((layer_state_t)1u << layer);   \
    }

#define HOST_RUNTIME_FIXTURE_DEFINE_RESET_QMK_STUBS(fixture_expr) \
    HOST_RUNTIME_FIXTURE_DEFINE_MOD_REPORT_STUBS(fixture_expr)    \
    HOST_RUNTIME_FIXTURE_DEFINE_LAYER_STUBS()
