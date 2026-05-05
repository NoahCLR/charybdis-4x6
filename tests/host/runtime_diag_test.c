#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/state/diagnostics/runtime_diag.h"

static uint32_t fake_time32;
static bool     fake_is_master;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

uint32_t timer_read32(void) {
    return fake_time32;
}

uint32_t timer_elapsed32(uint32_t last) {
    return timer_read32() - last;
}

bool is_keyboard_master(void) {
    return fake_is_master;
}

static void test_reset(void) {
    fake_time32    = 1000u;
    fake_is_master = true;
    noah_runtime_diag_reset_for_test();
}

static void test_post_init_enables_watchdog_on_master(void) {
    test_reset();

    noah_runtime_diag_post_init();

    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_test_backend_watchdog_enabled());
    CHECK(noah_runtime_diag_test_backend_watchdog_enable_count() == 1u);
    CHECK(noah_runtime_diag_test_backend_scratch(0u) == 0x4E444947u);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_test_backend_scratch(2u) == 0u);
}

static void test_post_init_skips_watchdog_on_slave(void) {
    test_reset();
    fake_is_master = false;

    noah_runtime_diag_post_init();

    CHECK(!noah_runtime_diag_test_backend_watchdog_enabled());
    CHECK(noah_runtime_diag_test_backend_watchdog_enable_count() == 0u);
}

static void test_nested_scopes_restore_parent_stage(void) {
    test_reset();
    noah_runtime_diag_post_init();

    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_KEY_RUNTIME);
    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_KEY_RUNTIME);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_KEY_RUNTIME);

    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK);
    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK);

    noah_runtime_diag_scope_leave();
    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_KEY_RUNTIME);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_KEY_RUNTIME);

    noah_runtime_diag_scope_leave();
    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == NOAH_RUNTIME_DIAG_STAGE_IDLE);
}

static void test_heartbeat_updates_watchdog(void) {
    test_reset();
    noah_runtime_diag_post_init();

    CHECK(noah_runtime_diag_test_backend_watchdog_update_count() == 0u);
    noah_runtime_diag_heartbeat();
    CHECK(noah_runtime_diag_test_backend_watchdog_update_count() == 1u);
}

static void test_watchdog_reboot_latches_previous_stage(void) {
    test_reset();
    noah_runtime_diag_test_backend_seed_watchdog_reboot(NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_SPLIT_SYNC, 4u);

    noah_runtime_diag_post_init();

    CHECK(noah_runtime_diag_watchdog_reboot_latched());
    CHECK(noah_runtime_diag_watchdog_stage() == NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_SPLIT_SYNC);
    CHECK(noah_runtime_diag_watchdog_reboot_count() == 5u);
    CHECK(noah_runtime_diag_indicator_active());
    CHECK(noah_runtime_diag_test_backend_watchdog_enabled());
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_test_backend_scratch(2u) == 5u);
}

static void test_indicator_expires_after_timeout(void) {
    test_reset();
    noah_runtime_diag_test_backend_seed_watchdog_reboot(NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER, 0u);

    noah_runtime_diag_post_init();
    CHECK(noah_runtime_diag_indicator_active());

    fake_time32 += 5001u;
    CHECK(!noah_runtime_diag_indicator_active());
}

int main(void) {
    test_post_init_enables_watchdog_on_master();
    test_post_init_skips_watchdog_on_slave();
    test_nested_scopes_restore_parent_stage();
    test_heartbeat_updates_watchdog();
    test_watchdog_reboot_latches_previous_stage();
    test_indicator_expires_after_timeout();

    puts("runtime_diag host tests passed");
    return 0;
}
