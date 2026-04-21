#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/state/runtime/runtime_diag.h"
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

void eeconfig_update_user(uint32_t value) {
    CHECK(value == 0u);
    test_log_stage("eeconfig_update_user");
}

void noah_via_macro_defaults_eeconfig_init(void) {
    test_log_stage("via_macro_defaults_eeconfig_init");
}

void noah_via_macro_defaults_matrix_scan(void) {
    test_log_stage("via_macro_defaults_matrix_scan");
}

void noah_key_runtime_scan(void) {
    test_log_stage("key_runtime_scan");
}

void split_runtime_sync_tick(void) {
    test_log_stage("split_runtime_sync_tick");
}

void held_repeat_tick(void) {
    test_log_stage("held_repeat_tick");
}

void macro_dispatch_validate_all(void) {
    test_log_stage("macro_dispatch_validate_all");
}

void noah_keymap_validate(void) {
    test_log_stage("keymap_validate");
}

void noah_via_macro_defaults_keyboard_post_init(void) {
    test_log_stage("via_macro_defaults_keyboard_post_init");
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

void noah_runtime_diag_post_init(void) {}

void noah_runtime_diag_scope_enter(noah_runtime_diag_stage_t stage) {
    (void)stage;
}

void noah_runtime_diag_scope_leave(void) {}

void noah_runtime_diag_heartbeat(void) {}

static void test_eeconfig_init_order(void) {
    static const char *const expected[] = {
        "eeconfig_update_user",
        "via_macro_defaults_eeconfig_init",
    };

    test_log_reset();
    noah_eeconfig_init_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_matrix_scan_order(void) {
    static const char *const expected[] = {
        "via_macro_defaults_matrix_scan",
        "key_runtime_scan",
        "split_runtime_sync_tick",
    };

    test_log_reset();
    noah_matrix_scan_user();
    test_expect_sequence(expected, ARRAY_SIZE(expected));
}

static void test_keyboard_post_init_order(void) {
    static const char *const expected[] = {
        "macro_dispatch_validate_all", "keymap_validate", "via_macro_defaults_keyboard_post_init", "rgb_runtime_post_init", "split_runtime_sync_init", "qmk_via_split_sync_init",
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
