#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void noah_via_macro_defaults_eeconfig_init(void) {
    test_log_stage("via_macro_defaults_eeconfig_init");
}

bool noah_via_macro_defaults_last_seed_succeeded(void) {
    test_log_stage("via_macro_defaults_last_seed_succeeded");
    return true;
}

void noah_qmk_via_sync_state_reset_after_defaults(bool defaults_committed) {
    CHECK(defaults_committed);
    test_log_stage("qmk_via_sync_state_reset_after_defaults");
}

void noah_via_macro_defaults_matrix_scan(void) {
    test_log_stage("via_macro_defaults_matrix_scan");
}

void noah_qmk_via_split_sync_matrix_scan(void) {
    test_log_stage("qmk_via_split_sync_matrix_scan");
}

void noah_qmk_combo_origin_scan(void) {
    test_log_stage("qmk_combo_origin_scan");
}

void noah_key_runtime_scan(void) {
    test_log_stage("key_runtime_scan");
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

void noah_via_macro_defaults_keyboard_post_init(void) {
    test_log_stage("via_macro_defaults_keyboard_post_init");
}

void noah_qmk_combo_origin_init(void) {
    test_log_stage("qmk_combo_origin_init");
}

void noah_rgb_runtime_post_init(void) {
    test_log_stage("rgb_runtime_post_init");
}

void split_runtime_sync_init(void) {
    test_log_stage("split_runtime_sync_init");
}

void noah_qmk_via_split_sync_init(void) {
    test_log_stage("qmk_via_split_sync_init");
}

void noah_qmk_via_split_mirror_init(void) {
    test_log_stage("qmk_via_split_mirror_init");
}

void noah_runtime_diag_post_init(void) {}

void noah_runtime_diag_scope_enter(noah_runtime_diag_stage_t stage) {
    (void)stage;
}

void noah_runtime_diag_scope_leave(void) {}

void noah_runtime_diag_heartbeat(void) {}

static void test_eeconfig_init_order(void) {
    static const char *const expected[] = {
        "macro_payload_engine_cancel",
        "via_macro_defaults_eeconfig_init",
        "via_macro_defaults_last_seed_succeeded",
        "qmk_via_sync_state_reset_after_defaults",
    };

    test_log_reset();
    noah_eeconfig_init_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_matrix_scan_order(void) {
    static const char *const expected[] = {
        "via_macro_defaults_matrix_scan", "qmk_via_split_sync_matrix_scan", "qmk_combo_origin_scan", "key_runtime_scan", "macro_payload_engine_scan", "split_runtime_sync_tick",
    };

    test_log_reset();
    noah_matrix_scan_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_keyboard_post_init_order(void) {
    static const char *const expected[] = {
        "qmk_combo_origin_init", "macro_payload_engine_init", "via_macro_defaults_keyboard_post_init", "rgb_runtime_post_init", "split_runtime_sync_init", "qmk_via_split_sync_init", "qmk_via_split_mirror_init",
    };

    test_log_reset();
    noah_keyboard_post_init_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_housekeeping_order(void) {
    static const char *const expected[] = {
        "held_repeat_tick",
    };

    test_log_reset();
    noah_housekeeping_task_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

int main(void) {
    test_eeconfig_init_order();
    test_matrix_scan_order();
    test_housekeeping_order();
    test_keyboard_post_init_order();

    puts("runtime_init_order host tests passed");
    return 0;
}
