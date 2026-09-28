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

static void test_scopes_leave_watchdog_state_alone(void) {
    test_reset();
    noah_runtime_diag_post_init();

    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_KEY_RUNTIME);
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
    CHECK(metadata[0] == NOAH_RUNTIME_CADENCE_WIRE_FORMAT);
    CHECK(metadata[1] == NOAH_RUNTIME_CADENCE_WIRE_PAGES);
    CHECK(metadata[2] == 1u);
    CHECK(read_u32(&metadata[3]) == 1u);
    CHECK(read_u32(&metadata[7]) == 1000000u);
    CHECK(read_u16(&metadata[11]) == 1000u);
    CHECK(read_u16(&metadata[19]) == 5000u);
    CHECK(metadata[21] == 1u);
    CHECK(metadata[22] == NOAH_RUNTIME_DIAG_STAGE_COUNT - 1u);
    CHECK(metadata[23] == NOAH_RUNTIME_CADENCE_STAGES_PER_PAGE);
    CHECK(metadata[24] == NOAH_RUNTIME_CADENCE_STAGE_TOTAL_UNIT_SHIFT);

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

static void at(uint32_t us) {
    noah_runtime_diag_test_backend_set_realtime_counter(us);
}

typedef struct {
    uint8_t  index;
    uint16_t total_units;
    uint16_t max_loop_us;
} stage_read_t;

static stage_read_t read_stage(uint8_t window, noah_runtime_diag_stage_t stage) {
    uint8_t      payload[NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE];
    uint8_t      slot = (uint8_t)(stage - 1u);
    uint8_t      page = (uint8_t)(NOAH_RUNTIME_CADENCE_FIRST_STAGE_PAGE + window * NOAH_RUNTIME_CADENCE_STAGE_PAGES_PER_WINDOW + slot / NOAH_RUNTIME_CADENCE_STAGES_PER_PAGE);
    uint8_t      offset = (uint8_t)(5u + (slot % NOAH_RUNTIME_CADENCE_STAGES_PER_PAGE) * 4u);
    stage_read_t read;

    CHECK(noah_runtime_cadence_wire_page(page, payload));
    read.index       = payload[4];
    read.total_units = read_u16(&payload[offset]);
    read.max_loop_us = read_u16(&payload[offset + 2u]);
    return read;
}

static void check_stage(uint8_t window, noah_runtime_diag_stage_t stage, uint32_t total_us, uint16_t max_loop_us) {
    stage_read_t read = read_stage(window, stage);

    CHECK(read.index == window);
    CHECK(read.total_units == (uint16_t)(total_us >> NOAH_RUNTIME_CADENCE_STAGE_TOTAL_UNIT_SHIFT));
    CHECK(read.max_loop_us == max_loop_us);
}

// One loop through every boundary the firmware marks, then a second loop and a
// window roll. Nested scopes take their time out of the stage they interrupt.
static void test_stage_timing_charges_exclusive_time_per_loop(void) {
    uint8_t payload[NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE];

    test_reset();
    at(0u);
    noah_runtime_cadence_loop_begin();
    at(100u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_DURABLE_IO);
    at(130u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_KEY_RUNTIME);
    at(140u);
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD);
    at(165u);
    noah_runtime_diag_scope_leave();
    at(175u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_SPLIT_SYNC);
    at(200u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_QMK_TASKS);
    at(210u);
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD);
    at(230u);
    noah_runtime_diag_scope_leave();
    at(260u);
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER);
    at(300u);
    noah_runtime_diag_scope_leave();
    at(320u);
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_SENSOR_READ);
    at(400u);
    noah_runtime_diag_scope_leave();
    at(410u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK);
    at(450u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_POINTING_REPORT);
    at(500u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_OUTSIDE_KEYBOARD_TASK);
    at(600u);
    noah_runtime_cadence_loop_begin();
    at(1600u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_QMK_TASKS);
    at(1700u);
    noah_runtime_cadence_loop_begin();

    CHECK(read_stage(0u, NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN).index == 0xffu);
    at(1000000u);
    noah_runtime_cadence_loop_begin();

    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN, 1100u, 1000u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_DURABLE_IO, 30u, 30u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_KEY_RUNTIME, 20u, 20u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_SPLIT_SYNC, 25u, 25u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_QMK_TASKS, 170u, 100u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD, 45u, 45u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER, 40u, 40u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_SENSOR_READ, 80u, 80u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK, 40u, 40u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_POINTING_REPORT, 50u, 50u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_OUTSIDE_KEYBOARD_TASK, 100u, 100u);

    // The loop that crossed the boundary went to the next window.
    CHECK(read_stage(1u, NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN).index == 0xffu);
    CHECK(noah_runtime_cadence_wire_page(NOAH_RUNTIME_CADENCE_FIRST_STAGE_PAGE + NOAH_RUNTIME_CADENCE_STAGE_PAGES_PER_WINDOW - 1u, payload));
    for (uint8_t offset = 9u; offset < NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE; offset++) {
        CHECK(payload[offset] == 0u);
    }
    CHECK(noah_runtime_cadence_wire_page(NOAH_RUNTIME_CADENCE_WIRE_PAGES - 1u, payload));
    CHECK(payload[4] == 0xffu);
    CHECK(!noah_runtime_cadence_wire_page(NOAH_RUNTIME_CADENCE_WIRE_PAGES, payload));
}

static void test_stage_timing_tolerates_unbalanced_scopes_and_saturates(void) {
    test_reset();

    // Nothing is timed before the first loop begins.
    at(0u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_DURABLE_IO);
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER);
    at(500u);
    noah_runtime_diag_scope_leave();
    noah_runtime_cadence_loop_begin();

    // A stray leave does not pop the top-level stage; out-of-range stages are
    // ignored.
    at(600u);
    noah_runtime_diag_scope_leave();
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_IDLE);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_COUNT);
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_COUNT);
    at(700u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_QMK_TASKS);

    // A scope deeper than the stack charges the deepest one kept, and the
    // leaves still pair: SENSOR_READ 10 + 20, RGB_RENDER 30, PROCESS_RECORD 40.
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD);
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER);
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_SENSOR_READ);
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK);
    at(710u);
    noah_runtime_diag_scope_leave();
    at(730u);
    noah_runtime_diag_scope_leave();
    at(760u);
    noah_runtime_diag_scope_leave();
    at(800u);
    noah_runtime_diag_scope_leave();
    at(900u);

    // A mark inside a scope ends it, so its leave is ignored.
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER);
    at(950u);
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK);
    at(71000u);
    noah_runtime_diag_scope_leave();
    noah_runtime_cadence_loop_begin();
    at(1000500u);
    noah_runtime_cadence_loop_begin();

    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_DURABLE_IO, 0u, 0u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN, 200u, 200u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD, 40u, 40u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_SENSOR_READ, 30u, 30u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER, 80u, 80u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_QMK_TASKS, 100u, 100u);
    check_stage(0u, NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK, 70050u, UINT16_MAX);
}

int main(void) {
    test_post_init_starts_boot_indicator_and_watchdog();
    test_post_init_starts_boot_indicator_and_watchdog_on_slave();
    test_scopes_leave_watchdog_state_alone();
    test_heartbeat_updates_watchdog_without_reboot_stage();
    test_watchdog_heartbeat_skips_most_loop_passes();
    test_indicator_expires_after_timeout();
    test_cadence_records_one_second_windows_and_histogram();
    test_cadence_uses_wrap_safe_microsecond_gaps();
    test_stage_timing_charges_exclusive_time_per_loop();
    test_stage_timing_tolerates_unbalanced_scopes_and_saturates();

    puts("runtime_diag host tests passed");
    return 0;
}
