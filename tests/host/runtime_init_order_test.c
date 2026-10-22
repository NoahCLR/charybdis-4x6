#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/runtime/effective_pd_runtime.h"
#include "users/noah/lib/state/diagnostics/runtime_diag.h"
#include "users/noah/noah_runtime.h"

enum {
    TEST_LOG_CAPACITY = 16,
};

static const char *test_log_entries[TEST_LOG_CAPACITY];
static uint8_t     test_log_count;

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

static void test_log_reset(void) {
    memset(test_log_entries, 0, sizeof(test_log_entries));
    test_log_count = 0;
}

static void test_log_stage(const char *stage) {
    CHECK(test_log_count < ARRAY_SIZE(test_log_entries));
    test_log_entries[test_log_count++] = stage;
}

static void test_expect_sequence(const char *const *expected, uint8_t count) {
    if (test_log_count != count) {
        fprintf(stderr, "expected %u stages, saw %u\n", (unsigned int)count, (unsigned int)test_log_count);
        for (uint8_t index = 0; index < test_log_count; index++) {
            fprintf(stderr, "  actual[%u] = %s\n", (unsigned int)index, test_log_entries[index]);
        }
        CHECK(test_log_count == count);
    }

    for (uint8_t index = 0; index < count; index++) {
        CHECK(strcmp(test_log_entries[index], expected[index]) == 0);
    }
}

void noah_profile_store_runtime_matrix_scan(void) {
    test_log_stage("profile_store_runtime_matrix_scan");
}

void noah_qmk_combo_origin_scan(void) {
    test_log_stage("qmk_combo_origin_scan");
}

void noah_key_runtime_scan(void) {
    test_log_stage("key_runtime_scan");
}

void noah_record_admission_task(void) {
    test_log_stage("record_admission_task");
}

const noah_pd_config_t noah_pd_defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT];

// An image without a profile owner warms the pointing slots from the compiled
// defaults; with one, the owner publishes them.
void noah_effective_pd_load_compiled_defaults(const noah_pd_config_t defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT]) {
    CHECK(defaults == noah_pd_defaults);
    test_log_stage("effective_pd_load_compiled_defaults");
}

void noah_record_admission_reset(void) {
    test_log_stage("record_admission_reset");
}

void macro_payload_engine_scan(void) {
    test_log_stage("macro_payload_engine_scan");
}

bool macro_payload_engine_cancel(void) {
    test_log_stage("macro_payload_engine_cancel");
    return false;
}

void macro_payload_engine_init(void) {
    test_log_stage("macro_payload_engine_init");
}

void split_runtime_sync_tick(void) {
    test_log_stage("split_runtime_sync_tick");
}

void held_repeat_tick(void) {
    test_log_stage("held_repeat_tick");
}

void noah_qmk_profile_flash_pre_init(void) {
    test_log_stage("qmk_profile_flash_pre_init");
}

void noah_profile_store_runtime_init(void) {
    test_log_stage("profile_store_runtime_init");
}

void noah_qmk_combo_origin_init(void) {
    test_log_stage("qmk_combo_origin_init");
}

void noah_runtime_shared_state_post_init(void) {
    test_log_stage("runtime_shared_state_post_init");
}

void noah_rgb_runtime_post_init(void) {
    test_log_stage("rgb_runtime_post_init");
}

void split_runtime_sync_init(void) {
    test_log_stage("split_runtime_sync_init");
}

void noah_runtime_diag_post_init(void) {}

void noah_qmk_usb_product_name_pre_init(void) {
    test_log_stage("qmk_usb_product_name_pre_init");
}

void noah_qmk_usb_product_name_task(void) {
    test_log_stage("qmk_usb_product_name_task");
}

void noah_qmk_profile_flash_housekeeping(void) {
    test_log_stage("qmk_profile_flash_housekeeping");
}

void noah_qmk_keyboard_restart_task(void) {
    test_log_stage("qmk_keyboard_restart_task");
}

void noah_runtime_diag_scope_enter(noah_runtime_diag_stage_t stage) {
    (void)stage;
}

void noah_runtime_diag_scope_leave(void) {}

void noah_runtime_diag_heartbeat(void) {
    test_log_stage("runtime_diag_heartbeat");
}

static void test_eeconfig_init_order(void) {
    static const char *const expected[] = {
        "macro_payload_engine_cancel",
    };

    test_log_reset();
    noah_eeconfig_init_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_matrix_scan_order(void) {
    static const char *const expected[] = {
        "profile_store_runtime_matrix_scan", "qmk_combo_origin_scan", "key_runtime_scan", "record_admission_task", "macro_payload_engine_scan", "split_runtime_sync_tick",
    };

    test_log_reset();
    noah_matrix_scan_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_matrix_slave_scan_order(void) {
    static const char *const expected[] = {
        "profile_store_runtime_matrix_scan",
    };

    test_log_reset();
    noah_matrix_slave_scan_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_keyboard_post_init_order(void) {
    static const char *const expected[] = {
        // Core defaults must land before any stage can observe key runtime
        // core state.
        "runtime_shared_state_post_init", "qmk_combo_origin_init", "record_admission_reset", "macro_payload_engine_init", "profile_store_runtime_init",
#ifndef NOAH_LIVE_PROFILE_OWNER_ENABLE
        "effective_pd_load_compiled_defaults",
#endif
        "rgb_runtime_post_init", "split_runtime_sync_init",
    };

    test_log_reset();
    noah_keyboard_post_init_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

// Before USB starts: an EEPROM reset forgets the saved profiles (D-F29),
// then the USB product name (D-F25) is read from what remains.
static void test_keyboard_pre_init_order(void) {
    static const char *const expected[] = {
        "qmk_profile_flash_pre_init",
        "qmk_usb_product_name_pre_init",
    };

    test_log_reset();
    noah_keyboard_pre_init_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_housekeeping_order(void) {
    static const char *const expected[] = {
        "held_repeat_tick",
        "runtime_diag_heartbeat",
        "qmk_usb_product_name_task",
        "qmk_profile_flash_housekeeping",
        "qmk_keyboard_restart_task",
    };

    test_log_reset();
    noah_housekeeping_task_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_suspend_power_down_feeds_watchdog_only(void) {
    // QMK's USB-suspend loop never reaches housekeeping_task(); the suspend
    // hook must keep the restart watchdog fed without running key work.
    static const char *const expected[] = {
        "runtime_diag_heartbeat",
    };

    test_log_reset();
    noah_suspend_power_down_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

int main(void) {
    test_eeconfig_init_order();
    test_matrix_scan_order();
    test_matrix_slave_scan_order();
    test_keyboard_pre_init_order();
    test_housekeeping_order();
    test_suspend_power_down_feeds_watchdog_only();
    test_keyboard_post_init_order();

    puts("runtime_init_order host tests passed");
    return 0;
}
