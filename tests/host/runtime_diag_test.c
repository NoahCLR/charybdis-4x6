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

static void test_post_init_starts_boot_indicator_and_watchdog(void) {
    test_reset();

    noah_runtime_diag_post_init();

    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_indicator_active());
    CHECK(noah_runtime_diag_test_backend_watchdog_enabled());
    CHECK(noah_runtime_diag_test_backend_watchdog_enable_count() == 1u);
    CHECK(noah_runtime_diag_test_backend_scratch(0u) == 0u);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == 0u);
    CHECK(noah_runtime_diag_test_backend_scratch(2u) == 0u);
}

static void test_post_init_starts_boot_indicator_and_watchdog_on_slave(void) {
    test_reset();
    fake_is_master = false;

    noah_runtime_diag_post_init();

    CHECK(noah_runtime_diag_indicator_active());
    CHECK(noah_runtime_diag_test_backend_watchdog_enabled());
    CHECK(noah_runtime_diag_test_backend_watchdog_enable_count() == 1u);
}

static void test_scopes_are_hot_path_noops(void) {
    test_reset();
    noah_runtime_diag_post_init();

    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_KEY_RUNTIME);
    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == 0u);

    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK);
    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == 0u);

    noah_runtime_diag_scope_leave();
    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == 0u);

    noah_runtime_diag_scope_leave();
    CHECK(noah_runtime_diag_current_stage() == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_test_backend_scratch(1u) == 0u);
}

static void test_heartbeat_updates_watchdog_without_reboot_stage(void) {
    test_reset();
    noah_runtime_diag_post_init();

    CHECK(noah_runtime_diag_test_backend_watchdog_update_count() == 0u);
    noah_runtime_diag_heartbeat();
    CHECK(noah_runtime_diag_test_backend_watchdog_update_count() == 1u);
    CHECK(!noah_runtime_diag_watchdog_reboot_latched());
    CHECK(noah_runtime_diag_watchdog_stage() == NOAH_RUNTIME_DIAG_STAGE_IDLE);
    CHECK(noah_runtime_diag_watchdog_reboot_count() == 0u);
}

static void test_watchdog_heartbeat_skips_most_loop_passes(void) {
    test_reset();
    noah_runtime_diag_post_init();

    noah_runtime_diag_heartbeat();
    CHECK(noah_runtime_diag_test_backend_watchdog_update_count() == 1u);

    for (uint8_t i = 0u; i < 7u; i++) {
        noah_runtime_diag_heartbeat();
    }
    CHECK(noah_runtime_diag_test_backend_watchdog_update_count() == 1u);

    noah_runtime_diag_heartbeat();
    CHECK(noah_runtime_diag_test_backend_watchdog_update_count() == 2u);
}

static void test_indicator_expires_after_timeout(void) {
    test_reset();

    noah_runtime_diag_post_init();
    CHECK(noah_runtime_diag_indicator_active());

    fake_time32 += 5001u;
    CHECK(!noah_runtime_diag_indicator_active());
}

int main(void) {
    test_post_init_starts_boot_indicator_and_watchdog();
    test_post_init_starts_boot_indicator_and_watchdog_on_slave();
    test_scopes_are_hot_path_noops();
    test_heartbeat_updates_watchdog_without_reboot_stage();
    test_watchdog_heartbeat_skips_most_loop_passes();
    test_indicator_expires_after_timeout();

    puts("runtime_diag host tests passed");
    return 0;
}
