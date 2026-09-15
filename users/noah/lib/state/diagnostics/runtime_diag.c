// ────────────────────────────────────────────────────────────────────────────
// Runtime Restart Watchdog
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_diag.h"

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <string.h>

#if defined(QMK_MCU_RP2040) && !defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
#    include "hardware/watchdog.h"
#endif

#ifndef NOAH_RUNTIME_DIAG_WATCHDOG_TIMEOUT_MS
#    define NOAH_RUNTIME_DIAG_WATCHDOG_TIMEOUT_MS 750u
#endif

#ifndef NOAH_RUNTIME_DIAG_WATCHDOG_HEARTBEAT_DIVISOR
#    define NOAH_RUNTIME_DIAG_WATCHDOG_HEARTBEAT_DIVISOR 8u
#endif

#if NOAH_RUNTIME_DIAG_WATCHDOG_HEARTBEAT_DIVISOR == 0
#    error "NOAH_RUNTIME_DIAG_WATCHDOG_HEARTBEAT_DIVISOR must be greater than zero"
#endif

#ifndef NOAH_RUNTIME_DIAG_INDICATOR_MS
#    define NOAH_RUNTIME_DIAG_INDICATOR_MS 5000u
#endif

typedef struct {
    bool     watchdog_enabled;
    bool     indicator_active;
    uint32_t watchdog_heartbeat_count;
    uint32_t indicator_started_at;
} noah_runtime_diag_state_t;

static noah_runtime_diag_state_t noah_runtime_diag_state;

#if defined(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE) || defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
enum {
    NOAH_RUNTIME_CADENCE_WINDOW_US = 1000000u,
};

typedef struct {
    uint32_t max_pointing_gap_us;
    uint16_t matrix_scans;
    uint16_t pointing_polls;
    uint16_t gap_histogram[NOAH_RUNTIME_CADENCE_HISTOGRAM_BUCKETS];
} noah_runtime_cadence_window_t;

typedef struct {
    noah_runtime_cadence_window_t completed[NOAH_RUNTIME_CADENCE_WINDOW_COUNT];
    noah_runtime_cadence_window_t current;
    uint32_t                      window_started_at;
    uint32_t                      last_pointing_poll_at;
    uint32_t                      sequence;
    uint8_t                       next_window;
    uint8_t                       completed_count;
    bool                          started;
    bool                          last_pointing_poll_known;
} noah_runtime_cadence_state_t;

static noah_runtime_cadence_state_t noah_runtime_cadence_state;

static const uint16_t noah_runtime_cadence_histogram_upper_us[NOAH_RUNTIME_CADENCE_HISTOGRAM_BUCKETS - 1u] = {
    1000u, 1250u, 1500u, 2000u, 5000u,
};

_Static_assert(sizeof(noah_runtime_cadence_window_t) == 20u, "cadence window wire representation drifted");
#endif

#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
static uint32_t noah_runtime_diag_test_backend_scratch_regs[8];
static uint32_t noah_runtime_diag_test_backend_realtime_counter;
static bool     noah_runtime_diag_test_backend_watchdog_enabled_flag;
static uint32_t noah_runtime_diag_test_backend_watchdog_enable_calls;
static uint32_t noah_runtime_diag_test_backend_watchdog_update_calls;
#endif

#if defined(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE) || defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
static uint32_t noah_runtime_cadence_realtime_counter(void) {
#    if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
    return noah_runtime_diag_test_backend_realtime_counter;
#    elif defined(QMK_MCU_RP2040)
    return chSysGetRealtimeCounterX();
#    else
#        error "performance diagnostics require the RP2040 realtime counter"
#    endif
}

static void noah_runtime_cadence_write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static void noah_runtime_cadence_write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static void noah_runtime_cadence_increment_u16(uint16_t *value) {
    if (*value != UINT16_MAX) {
        (*value)++;
    }
}

static void noah_runtime_cadence_store_current(void) {
    noah_runtime_cadence_state.completed[noah_runtime_cadence_state.next_window] = noah_runtime_cadence_state.current;
    noah_runtime_cadence_state.next_window                                       = (uint8_t)((noah_runtime_cadence_state.next_window + 1u) % NOAH_RUNTIME_CADENCE_WINDOW_COUNT);
    if (noah_runtime_cadence_state.completed_count < NOAH_RUNTIME_CADENCE_WINDOW_COUNT) {
        noah_runtime_cadence_state.completed_count++;
    }
    memset(&noah_runtime_cadence_state.current, 0, sizeof(noah_runtime_cadence_state.current));
    noah_runtime_cadence_state.sequence++;
}

static void noah_runtime_cadence_advance(uint32_t now) {
    uint32_t elapsed;
    uint32_t windows;

    if (!noah_runtime_cadence_state.started) {
        noah_runtime_cadence_state.started           = true;
        noah_runtime_cadence_state.window_started_at = now;
        return;
    }
    elapsed = now - noah_runtime_cadence_state.window_started_at;
    windows = elapsed / NOAH_RUNTIME_CADENCE_WINDOW_US;
    if (windows == 0u) {
        return;
    }
    if (windows > NOAH_RUNTIME_CADENCE_WINDOW_COUNT) {
        windows = NOAH_RUNTIME_CADENCE_WINDOW_COUNT;
    }
    noah_runtime_cadence_store_current();
    for (uint32_t skipped = 1u; skipped < windows; skipped++) {
        noah_runtime_cadence_store_current();
    }
    noah_runtime_cadence_state.window_started_at += (elapsed / NOAH_RUNTIME_CADENCE_WINDOW_US) * NOAH_RUNTIME_CADENCE_WINDOW_US;
}

void noah_runtime_cadence_note_matrix_scan(void) {
    noah_runtime_cadence_advance(noah_runtime_cadence_realtime_counter());
    noah_runtime_cadence_increment_u16(&noah_runtime_cadence_state.current.matrix_scans);
}

void noah_runtime_cadence_note_pointing_poll(void) {
    uint32_t now = noah_runtime_cadence_realtime_counter();

    noah_runtime_cadence_advance(now);
    noah_runtime_cadence_increment_u16(&noah_runtime_cadence_state.current.pointing_polls);
    if (noah_runtime_cadence_state.last_pointing_poll_known) {
        uint32_t gap = now - noah_runtime_cadence_state.last_pointing_poll_at;
        uint8_t  bucket;

        if (gap > noah_runtime_cadence_state.current.max_pointing_gap_us) {
            noah_runtime_cadence_state.current.max_pointing_gap_us = gap;
        }
        for (bucket = 0u; bucket < NOAH_RUNTIME_CADENCE_HISTOGRAM_BUCKETS - 1u && gap > noah_runtime_cadence_histogram_upper_us[bucket]; bucket++) {
        }
        noah_runtime_cadence_increment_u16(&noah_runtime_cadence_state.current.gap_histogram[bucket]);
    }
    noah_runtime_cadence_state.last_pointing_poll_at    = now;
    noah_runtime_cadence_state.last_pointing_poll_known = true;
}

bool noah_runtime_cadence_wire_page(uint8_t page, uint8_t payload[NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE]) {
    uint32_t sequence;

    if (!payload || page >= NOAH_RUNTIME_CADENCE_WIRE_PAGES) {
        return false;
    }
    memset(payload, 0, NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE);
    sequence = noah_runtime_cadence_state.sequence;
    if (page == 0u) {
        payload[0] = 1u;
        payload[1] = NOAH_RUNTIME_CADENCE_WIRE_PAGES;
        payload[2] = noah_runtime_cadence_state.completed_count;
        noah_runtime_cadence_write_u32(&payload[3], sequence);
        noah_runtime_cadence_write_u32(&payload[7], NOAH_RUNTIME_CADENCE_WINDOW_US);
        for (uint8_t bucket = 0u; bucket < NOAH_RUNTIME_CADENCE_HISTOGRAM_BUCKETS - 1u; bucket++) {
            noah_runtime_cadence_write_u16(&payload[11u + bucket * 2u], noah_runtime_cadence_histogram_upper_us[bucket]);
        }
        payload[21] = noah_runtime_cadence_state.started ? 1u : 0u;
        return true;
    }

    noah_runtime_cadence_write_u32(payload, sequence);
    payload[4] = 0xffu;
    if ((uint8_t)(page - 1u) < noah_runtime_cadence_state.completed_count) {
        uint8_t                              logical_index = (uint8_t)(page - 1u);
        uint8_t                              oldest        = (uint8_t)((noah_runtime_cadence_state.next_window + NOAH_RUNTIME_CADENCE_WINDOW_COUNT - noah_runtime_cadence_state.completed_count) % NOAH_RUNTIME_CADENCE_WINDOW_COUNT);
        uint8_t                              stored        = (uint8_t)((oldest + logical_index) % NOAH_RUNTIME_CADENCE_WINDOW_COUNT);
        const noah_runtime_cadence_window_t *window        = &noah_runtime_cadence_state.completed[stored];

        payload[4] = logical_index;
        noah_runtime_cadence_write_u32(&payload[5], window->max_pointing_gap_us);
        noah_runtime_cadence_write_u16(&payload[9], window->matrix_scans);
        noah_runtime_cadence_write_u16(&payload[11], window->pointing_polls);
        for (uint8_t bucket = 0u; bucket < NOAH_RUNTIME_CADENCE_HISTOGRAM_BUCKETS; bucket++) {
            noah_runtime_cadence_write_u16(&payload[13u + bucket * 2u], window->gap_histogram[bucket]);
        }
    }
    return true;
}
#endif

static uint32_t noah_runtime_diag_backend_timer_read32(void) {
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND) || defined(QMK_MCU_RP2040) || defined(__arm__) || defined(__AVR__)
    return timer_read32();
#else
    return 0u;
#endif
}

static uint32_t noah_runtime_diag_backend_timer_elapsed32(uint32_t last) {
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND) || defined(QMK_MCU_RP2040) || defined(__arm__) || defined(__AVR__)
    return timer_elapsed32(last);
#else
    (void)last;
    return 0u;
#endif
}

static void noah_runtime_diag_refresh_indicator(void) {
    if (noah_runtime_diag_state.indicator_active && noah_runtime_diag_backend_timer_elapsed32(noah_runtime_diag_state.indicator_started_at) >= NOAH_RUNTIME_DIAG_INDICATOR_MS) {
        noah_runtime_diag_state.indicator_active = false;
    }
}

static void noah_runtime_diag_backend_watchdog_enable(void) {
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
    noah_runtime_diag_test_backend_watchdog_enabled_flag = true;
    noah_runtime_diag_test_backend_watchdog_enable_calls++;
#elif defined(QMK_MCU_RP2040)
    watchdog_enable(NOAH_RUNTIME_DIAG_WATCHDOG_TIMEOUT_MS, false);
#endif
}

static void noah_runtime_diag_backend_watchdog_update(void) {
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
    if (noah_runtime_diag_test_backend_watchdog_enabled_flag) {
        noah_runtime_diag_test_backend_watchdog_update_calls++;
    }
#elif defined(QMK_MCU_RP2040)
    watchdog_update();
#endif
}

static void noah_runtime_diag_enable_watchdog_if_needed(void) {
    if (noah_runtime_diag_state.watchdog_enabled) {
        return;
    }

    noah_runtime_diag_backend_watchdog_enable();
    noah_runtime_diag_state.watchdog_enabled         = true;
    noah_runtime_diag_state.watchdog_heartbeat_count = NOAH_RUNTIME_DIAG_WATCHDOG_HEARTBEAT_DIVISOR - 1u;
}

static bool noah_runtime_diag_watchdog_heartbeat_due(void) {
    noah_runtime_diag_state.watchdog_heartbeat_count++;

    if (noah_runtime_diag_state.watchdog_heartbeat_count < NOAH_RUNTIME_DIAG_WATCHDOG_HEARTBEAT_DIVISOR) {
        return false;
    }

    noah_runtime_diag_state.watchdog_heartbeat_count = 0u;
    return true;
}

void noah_runtime_diag_post_init(void) {
    noah_runtime_diag_state.indicator_active     = true;
    noah_runtime_diag_state.indicator_started_at = noah_runtime_diag_backend_timer_read32();
    noah_runtime_diag_enable_watchdog_if_needed();
}

void noah_runtime_diag_scope_enter(noah_runtime_diag_stage_t stage) {
    (void)stage;
}

void noah_runtime_diag_scope_leave(void) {}

void noah_runtime_diag_heartbeat(void) {
    noah_runtime_diag_refresh_indicator();
    if (noah_runtime_diag_state.watchdog_enabled && noah_runtime_diag_watchdog_heartbeat_due()) {
        noah_runtime_diag_backend_watchdog_update();
    }
}

noah_runtime_diag_stage_t noah_runtime_diag_current_stage(void) {
    return NOAH_RUNTIME_DIAG_STAGE_IDLE;
}

bool noah_runtime_diag_watchdog_reboot_latched(void) {
    noah_runtime_diag_refresh_indicator();
    return false;
}

noah_runtime_diag_stage_t noah_runtime_diag_watchdog_stage(void) {
    noah_runtime_diag_refresh_indicator();
    return NOAH_RUNTIME_DIAG_STAGE_IDLE;
}

uint8_t noah_runtime_diag_watchdog_reboot_count(void) {
    noah_runtime_diag_refresh_indicator();
    return 0u;
}

bool noah_runtime_diag_indicator_active(void) {
    noah_runtime_diag_refresh_indicator();
    return noah_runtime_diag_state.indicator_active;
}

void noah_runtime_diag_reset_for_test(void) {
    memset(&noah_runtime_diag_state, 0, sizeof(noah_runtime_diag_state));
#if defined(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE) || defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
    memset(&noah_runtime_cadence_state, 0, sizeof(noah_runtime_cadence_state));
#endif
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
    noah_runtime_diag_test_backend_reset();
#endif
}

#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
void noah_runtime_diag_test_backend_reset(void) {
    memset(noah_runtime_diag_test_backend_scratch_regs, 0, sizeof(noah_runtime_diag_test_backend_scratch_regs));
    noah_runtime_diag_test_backend_watchdog_enabled_flag = false;
    noah_runtime_diag_test_backend_watchdog_enable_calls = 0u;
    noah_runtime_diag_test_backend_watchdog_update_calls = 0u;
    noah_runtime_diag_test_backend_realtime_counter      = 0u;
}

void noah_runtime_diag_test_backend_set_realtime_counter(uint32_t value) {
    noah_runtime_diag_test_backend_realtime_counter = value;
}

uint32_t noah_runtime_diag_test_backend_scratch(uint8_t index) {
    return index < 8u ? noah_runtime_diag_test_backend_scratch_regs[index] : 0u;
}

bool noah_runtime_diag_test_backend_watchdog_enabled(void) {
    return noah_runtime_diag_test_backend_watchdog_enabled_flag;
}

uint32_t noah_runtime_diag_test_backend_watchdog_enable_count(void) {
    return noah_runtime_diag_test_backend_watchdog_enable_calls;
}

uint32_t noah_runtime_diag_test_backend_watchdog_update_count(void) {
    return noah_runtime_diag_test_backend_watchdog_update_calls;
}
#endif
