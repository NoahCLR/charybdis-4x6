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

#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
static uint32_t noah_runtime_diag_test_backend_scratch_regs[8];
static bool     noah_runtime_diag_test_backend_watchdog_enabled_flag;
static uint32_t noah_runtime_diag_test_backend_watchdog_enable_calls;
static uint32_t noah_runtime_diag_test_backend_watchdog_update_calls;
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

void noah_runtime_diag_scope_leave(void) {
}

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
}

void noah_runtime_diag_test_backend_seed_watchdog_reboot(noah_runtime_diag_stage_t stage, uint8_t reboot_count) {
    (void)stage;
    (void)reboot_count;
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
