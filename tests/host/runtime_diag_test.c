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

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

static void test_cadence_records_one_second_windows_and_histogram(void) {
    uint8_t metadata[NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE];
    uint8_t window[NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE];

    test_reset();
    noah_runtime_diag_test_backend_set_realtime_counter(0u);
    noah_runtime_cadence_note_matrix_scan();
    noah_runtime_cadence_note_pointing_poll();
    noah_runtime_diag_test_backend_set_realtime_counter(900u);
    noah_runtime_cadence_note_matrix_scan();
    noah_runtime_cadence_note_pointing_poll();
    noah_runtime_diag_test_backend_set_realtime_counter(1100u);
    noah_runtime_cadence_note_pointing_poll();
    noah_runtime_diag_test_backend_set_realtime_counter(1000000u);
    noah_runtime_cadence_note_matrix_scan();

    CHECK(noah_runtime_cadence_wire_page(0u, metadata));
    CHECK(metadata[0] == 1u);
    CHECK(metadata[1] == NOAH_RUNTIME_CADENCE_WIRE_PAGES);
    CHECK(metadata[2] == 1u);
    CHECK(read_u32(&metadata[3]) == 1u);
    CHECK(read_u32(&metadata[7]) == 1000000u);
    CHECK(read_u16(&metadata[11]) == 1000u);
    CHECK(read_u16(&metadata[19]) == 5000u);
    CHECK(metadata[21] == 1u);

    CHECK(noah_runtime_cadence_wire_page(1u, window));
    CHECK(read_u32(window) == 1u);
    CHECK(window[4] == 0u);
    CHECK(read_u32(&window[5]) == 900u);
    CHECK(read_u16(&window[9]) == 2u);
    CHECK(read_u16(&window[11]) == 3u);
    CHECK(read_u16(&window[13]) == 2u);
    for (uint8_t bucket = 1u; bucket < NOAH_RUNTIME_CADENCE_HISTOGRAM_BUCKETS; bucket++) {
        CHECK(read_u16(&window[13u + bucket * 2u]) == 0u);
    }
    CHECK(noah_runtime_cadence_wire_page(2u, window));
    CHECK(window[4] == 0xffu);
    CHECK(!noah_runtime_cadence_wire_page(NOAH_RUNTIME_CADENCE_WIRE_PAGES, window));
}

static void test_cadence_uses_wrap_safe_microsecond_gaps(void) {
    uint8_t window[NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE];

    test_reset();
    noah_runtime_diag_test_backend_set_realtime_counter(UINT32_MAX - 500u);
    noah_runtime_cadence_note_pointing_poll();
    noah_runtime_diag_test_backend_set_realtime_counter(700u);
    noah_runtime_cadence_note_pointing_poll();
    noah_runtime_diag_test_backend_set_realtime_counter(999499u);
    noah_runtime_cadence_note_matrix_scan();

    CHECK(noah_runtime_cadence_wire_page(1u, window));
    CHECK(read_u32(&window[5]) == 1201u);
    CHECK(read_u16(&window[15]) == 1u);
}

int main(void) {
    test_post_init_starts_boot_indicator_and_watchdog();
    test_post_init_starts_boot_indicator_and_watchdog_on_slave();
    test_scopes_are_hot_path_noops();
    test_heartbeat_updates_watchdog_without_reboot_stage();
    test_watchdog_heartbeat_skips_most_loop_passes();
    test_indicator_expires_after_timeout();
    test_cadence_records_one_second_windows_and_histogram();
    test_cadence_uses_wrap_safe_microsecond_gaps();

    puts("runtime_diag host tests passed");
    return 0;
}
