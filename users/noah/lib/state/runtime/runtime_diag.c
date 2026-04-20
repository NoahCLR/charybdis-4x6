// ────────────────────────────────────────────────────────────────────────────
// Runtime Freeze Diagnostic
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_diag.h"

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <string.h>

#if defined(QMK_MCU_RP2040) && !defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
#    include "hardware/structs/watchdog.h"
#    include "hardware/watchdog.h"
#endif

#ifndef NOAH_RUNTIME_DIAG_WATCHDOG_TIMEOUT_MS
#    define NOAH_RUNTIME_DIAG_WATCHDOG_TIMEOUT_MS 750u
#endif

#ifndef NOAH_RUNTIME_DIAG_INDICATOR_MS
#    define NOAH_RUNTIME_DIAG_INDICATOR_MS 5000u
#endif

#ifndef NOAH_RUNTIME_DIAG_STACK_DEPTH
#    define NOAH_RUNTIME_DIAG_STACK_DEPTH 8u
#endif

#define NOAH_RUNTIME_DIAG_SCRATCH_MAGIC 0x4E444947u
#define NOAH_RUNTIME_DIAG_SCRATCH_INDEX_MAGIC 0u
#define NOAH_RUNTIME_DIAG_SCRATCH_INDEX_STAGE 1u
#define NOAH_RUNTIME_DIAG_SCRATCH_INDEX_REBOOT_COUNT 2u

typedef struct {
    bool                      initialized;
    bool                      watchdog_enabled;
    bool                      watchdog_reboot_latched;
    bool                      indicator_active;
    noah_runtime_diag_stage_t latched_stage;
    noah_runtime_diag_stage_t current_stage;
    uint8_t                   reboot_count;
    uint8_t                   depth;
    noah_runtime_diag_stage_t stack[NOAH_RUNTIME_DIAG_STACK_DEPTH];
    uint32_t                  indicator_started_at;
} noah_runtime_diag_state_t;

static noah_runtime_diag_state_t noah_runtime_diag_state;

#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
static uint32_t noah_runtime_diag_test_backend_scratch_regs[8];
static bool     noah_runtime_diag_test_backend_watchdog_reboot_flag;
static bool     noah_runtime_diag_test_backend_watchdog_enabled_flag;
static uint32_t noah_runtime_diag_test_backend_watchdog_enable_calls;
static uint32_t noah_runtime_diag_test_backend_watchdog_update_calls;
#endif

static bool noah_runtime_diag_backend_supported(void) {
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND) || defined(QMK_MCU_RP2040)
    return true;
#else
    return false;
#endif
}

static uint32_t noah_runtime_diag_backend_timer_read32(void) {
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND) || defined(QMK_MCU_RP2040)
    return timer_read32();
#else
    return 0u;
#endif
}

static uint32_t noah_runtime_diag_backend_timer_elapsed32(uint32_t last) {
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND) || defined(QMK_MCU_RP2040)
    return timer_elapsed32(last);
#else
    (void)last;
    return 0u;
#endif
}

static bool noah_runtime_diag_backend_is_master(void) {
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND) || defined(QMK_MCU_RP2040)
    return is_keyboard_master();
#else
    return false;
#endif
}

static bool noah_runtime_diag_backend_watchdog_enable_caused_reboot(void) {
#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
    return noah_runtime_diag_test_backend_watchdog_reboot_flag;
#elif defined(QMK_MCU_RP2040)
    return watchdog_enable_caused_reboot();
#else
    return false;
#endif
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

static uint32_t noah_runtime_diag_backend_scratch_get(uint8_t index) {
    if (index >= 8u) {
        return 0u;
    }

#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
    return noah_runtime_diag_test_backend_scratch_regs[index];
#elif defined(QMK_MCU_RP2040)
    return watchdog_hw->scratch[index];
#else
    return 0u;
#endif
}

static void noah_runtime_diag_backend_scratch_set(uint8_t index, uint32_t value) {
    if (index >= 8u) {
        return;
    }

#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
    noah_runtime_diag_test_backend_scratch_regs[index] = value;
#elif defined(QMK_MCU_RP2040)
    watchdog_hw->scratch[index] = value;
#else
    (void)value;
#endif
}

static void noah_runtime_diag_backend_store_stage(noah_runtime_diag_stage_t stage) {
    if (!noah_runtime_diag_backend_supported()) {
        return;
    }

    noah_runtime_diag_backend_scratch_set(NOAH_RUNTIME_DIAG_SCRATCH_INDEX_MAGIC, NOAH_RUNTIME_DIAG_SCRATCH_MAGIC);
    noah_runtime_diag_backend_scratch_set(NOAH_RUNTIME_DIAG_SCRATCH_INDEX_STAGE, (uint32_t)stage);
    noah_runtime_diag_backend_scratch_set(NOAH_RUNTIME_DIAG_SCRATCH_INDEX_REBOOT_COUNT, noah_runtime_diag_state.reboot_count);
}

static void noah_runtime_diag_refresh_indicator(void) {
    if (noah_runtime_diag_state.indicator_active && noah_runtime_diag_backend_timer_elapsed32(noah_runtime_diag_state.indicator_started_at) >= NOAH_RUNTIME_DIAG_INDICATOR_MS) {
        noah_runtime_diag_state.indicator_active = false;
    }
}

static void noah_runtime_diag_enable_watchdog_if_needed(void) {
    if (!noah_runtime_diag_backend_supported() || noah_runtime_diag_state.watchdog_enabled || !noah_runtime_diag_backend_is_master()) {
        return;
    }

    noah_runtime_diag_backend_store_stage(noah_runtime_diag_state.current_stage);
    noah_runtime_diag_backend_watchdog_enable();
    noah_runtime_diag_state.watchdog_enabled = true;
}

void noah_runtime_diag_post_init(void) {
    bool     reboot_latched = false;
    uint32_t reboot_count   = 0u;
    uint32_t stage          = 0u;

    noah_runtime_diag_state.initialized = true;

    if (noah_runtime_diag_backend_supported() && noah_runtime_diag_backend_watchdog_enable_caused_reboot() && noah_runtime_diag_backend_scratch_get(NOAH_RUNTIME_DIAG_SCRATCH_INDEX_MAGIC) == NOAH_RUNTIME_DIAG_SCRATCH_MAGIC) {
        reboot_latched = true;
        stage          = noah_runtime_diag_backend_scratch_get(NOAH_RUNTIME_DIAG_SCRATCH_INDEX_STAGE);
        reboot_count   = noah_runtime_diag_backend_scratch_get(NOAH_RUNTIME_DIAG_SCRATCH_INDEX_REBOOT_COUNT);
    }

    noah_runtime_diag_state.watchdog_reboot_latched = reboot_latched;
    noah_runtime_diag_state.latched_stage           = reboot_latched ? (noah_runtime_diag_stage_t)stage : NOAH_RUNTIME_DIAG_STAGE_IDLE;
    noah_runtime_diag_state.reboot_count            = reboot_latched && reboot_count < UINT8_MAX ? (uint8_t)(reboot_count + 1u) : 0u;
    noah_runtime_diag_state.indicator_active        = reboot_latched;
    noah_runtime_diag_state.indicator_started_at    = reboot_latched ? noah_runtime_diag_backend_timer_read32() : 0u;
    noah_runtime_diag_state.current_stage           = NOAH_RUNTIME_DIAG_STAGE_IDLE;
    noah_runtime_diag_state.depth                   = 0u;

    noah_runtime_diag_backend_store_stage(NOAH_RUNTIME_DIAG_STAGE_IDLE);
    noah_runtime_diag_enable_watchdog_if_needed();
}

void noah_runtime_diag_scope_enter(noah_runtime_diag_stage_t stage) {
    if (noah_runtime_diag_state.depth < NOAH_RUNTIME_DIAG_STACK_DEPTH) {
        noah_runtime_diag_state.stack[noah_runtime_diag_state.depth++] = noah_runtime_diag_state.current_stage;
    }

    noah_runtime_diag_state.current_stage = stage;
    noah_runtime_diag_backend_store_stage(stage);
}

void noah_runtime_diag_scope_leave(void) {
    noah_runtime_diag_state.current_stage = noah_runtime_diag_state.depth != 0u ? noah_runtime_diag_state.stack[--noah_runtime_diag_state.depth] : NOAH_RUNTIME_DIAG_STAGE_IDLE;
    noah_runtime_diag_backend_store_stage(noah_runtime_diag_state.current_stage);
}

void noah_runtime_diag_heartbeat(void) {
    noah_runtime_diag_refresh_indicator();
    noah_runtime_diag_enable_watchdog_if_needed();
    if (noah_runtime_diag_state.watchdog_enabled) {
        noah_runtime_diag_backend_watchdog_update();
    }
}

noah_runtime_diag_stage_t noah_runtime_diag_current_stage(void) {
    return noah_runtime_diag_state.current_stage;
}

bool noah_runtime_diag_watchdog_reboot_latched(void) {
    noah_runtime_diag_refresh_indicator();
    return noah_runtime_diag_state.watchdog_reboot_latched;
}

noah_runtime_diag_stage_t noah_runtime_diag_watchdog_stage(void) {
    noah_runtime_diag_refresh_indicator();
    return noah_runtime_diag_state.latched_stage;
}

uint8_t noah_runtime_diag_watchdog_reboot_count(void) {
    noah_runtime_diag_refresh_indicator();
    return noah_runtime_diag_state.reboot_count;
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
    noah_runtime_diag_test_backend_watchdog_reboot_flag  = false;
    noah_runtime_diag_test_backend_watchdog_enabled_flag = false;
    noah_runtime_diag_test_backend_watchdog_enable_calls = 0u;
    noah_runtime_diag_test_backend_watchdog_update_calls = 0u;
}

void noah_runtime_diag_test_backend_seed_watchdog_reboot(noah_runtime_diag_stage_t stage, uint8_t reboot_count) {
    noah_runtime_diag_test_backend_watchdog_reboot_flag                                       = true;
    noah_runtime_diag_test_backend_scratch_regs[NOAH_RUNTIME_DIAG_SCRATCH_INDEX_MAGIC]        = NOAH_RUNTIME_DIAG_SCRATCH_MAGIC;
    noah_runtime_diag_test_backend_scratch_regs[NOAH_RUNTIME_DIAG_SCRATCH_INDEX_STAGE]        = (uint32_t)stage;
    noah_runtime_diag_test_backend_scratch_regs[NOAH_RUNTIME_DIAG_SCRATCH_INDEX_REBOOT_COUNT] = reboot_count;
}

void noah_runtime_diag_test_backend_set_watchdog_reboot(bool caused_reboot) {
    noah_runtime_diag_test_backend_watchdog_reboot_flag = caused_reboot;
}

void noah_runtime_diag_test_backend_set_scratch(uint8_t index, uint32_t value) {
    if (index >= 8u) {
        return;
    }

    noah_runtime_diag_test_backend_scratch_regs[index] = value;
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
