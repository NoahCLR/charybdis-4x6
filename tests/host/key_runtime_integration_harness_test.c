#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_integration_harness.h"

enum {
    TEST_TRUE_KEYCODE  = 0x0004u,
    TEST_FALSE_KEYCODE = 0x0005u,
};

static unsigned     finalize_calls;
static uint16_t     finalize_keycode;
static keyrecord_t *finalize_record;
static bool         finalize_keep_processing;
static bool         process_return_value;

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

void noah_key_runtime_scan(void) {}

bool noah_process_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return process_return_value;
}

void noah_process_record_user_finalize(uint16_t keycode, keyrecord_t *record, bool keep_processing) {
    finalize_calls++;
    finalize_keycode         = keycode;
    finalize_record          = record;
    finalize_keep_processing = keep_processing;
}

static void test_reset(void) {
    finalize_calls           = 0;
    finalize_keycode         = KC_NO;
    finalize_record          = NULL;
    finalize_keep_processing = false;
    process_return_value     = true;
}

static void test_true_path_uses_default_post_finalize(void) {
    keypos_t key_pos = {.row = 1, .col = 2};

    test_reset();

    CHECK(key_runtime_integration_process_record(TEST_TRUE_KEYCODE, key_pos, true));
    CHECK(finalize_calls == 1);
    CHECK(finalize_keycode == TEST_TRUE_KEYCODE);
    CHECK(finalize_record != NULL);
    CHECK(finalize_record->event.pressed);
    CHECK(finalize_record->event.key.row == key_pos.row);
    CHECK(finalize_record->event.key.col == key_pos.col);
    CHECK(finalize_keep_processing);
}

static void test_false_path_finalizes_immediately(void) {
    keypos_t key_pos = {.row = 3, .col = 4};

    test_reset();
    process_return_value = false;

    CHECK(!key_runtime_integration_process_record(TEST_FALSE_KEYCODE, key_pos, false));
    CHECK(finalize_calls == 1);
    CHECK(finalize_keycode == TEST_FALSE_KEYCODE);
    CHECK(finalize_record != NULL);
    CHECK(!finalize_record->event.pressed);
    CHECK(finalize_record->event.key.row == key_pos.row);
    CHECK(finalize_record->event.key.col == key_pos.col);
    CHECK(!finalize_keep_processing);
}

static void test_key_runtime_core_event_adapter_routes_key_and_timer_events(void) {
    uint16_t        time    = 100u;
    keypos_t        key_pos = {.row = 5, .col = 6};
    runtime_event_t down    = {
        .kind = RUNTIME_EVENT_KIND_KEY_DOWN,
        .data.key_event =
            {
                .keycode = TEST_TRUE_KEYCODE,
                .key_pos = key_pos,
            },
    };
    runtime_event_t advance = {
        .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
        .data.timer_advance =
            {
                .advance_ms = 17u,
            },
    };

    test_reset();

    CHECK(key_runtime_integration_apply_core_event(&time, &down));
    CHECK(finalize_calls == 1);
    CHECK(finalize_keycode == TEST_TRUE_KEYCODE);
    CHECK(finalize_record != NULL);
    CHECK(finalize_record->event.pressed);
    CHECK(finalize_record->event.key.row == key_pos.row);
    CHECK(finalize_record->event.key.col == key_pos.col);

    CHECK(key_runtime_integration_apply_core_event(&time, &advance));
    CHECK(time == 117u);
}

int main(void) {
    test_true_path_uses_default_post_finalize();
    test_false_path_finalizes_immediately();
    test_key_runtime_core_event_adapter_routes_key_and_timer_events();

    puts("key runtime integration harness host tests passed");
    return 0;
}
