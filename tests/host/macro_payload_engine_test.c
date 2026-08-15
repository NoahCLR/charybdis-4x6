#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qmk_stub.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/macro/macro_payload.h"

typedef enum {
    TEST_OP_ACQUIRE = 1,
    TEST_OP_RELEASE,
} test_op_kind_t;

typedef struct {
    test_op_kind_t kind;
    uint8_t        keycode;
} test_op_t;

#define TEST_MAX_OPS 128

static uint32_t                      fake_time;
static uint16_t                      wait_call_count;
static test_op_t                     test_ops[TEST_MAX_OPS];
static uint16_t                      test_op_count;
static int16_t                       fail_acquire_keycode;
static macro_payload_finish_result_t callback_result;
static uint16_t                      callback_count;

const uint8_t ascii_to_shift_lut[16];
const uint8_t ascii_to_altgr_lut[16];
const uint8_t ascii_to_dead_lut[16];
const uint8_t ascii_to_keycode_lut[128] = {
    ['a'] = KC_A,
    ['b'] = KC_B,
};

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

static void test_log_op(test_op_kind_t kind, uint8_t keycode) {
    CHECK(test_op_count < TEST_MAX_OPS);
    test_ops[test_op_count++] = (test_op_t){.kind = kind, .keycode = keycode};
}

static void test_finish_callback(macro_payload_finish_result_t result, void *context) {
    CHECK(context == &callback_count);
    callback_result = result;
    callback_count++;
}

static void test_reset(void) {
    macro_payload_debug_snapshot_t snapshot;

    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE);
    macro_payload_engine_init();
    fake_time            = 1000u;
    wait_call_count      = 0u;
    test_op_count        = 0u;
    fail_acquire_keycode = -1;
    callback_result      = MACRO_PAYLOAD_FINISH_NONE;
    callback_count       = 0u;
    memset(test_ops, 0, sizeof(test_ops));
}

static void test_scan_after(uint32_t elapsed_ms) {
    fake_time += elapsed_ms;
    macro_payload_engine_scan();
}

static void test_drain_cleanup(void) {
    macro_payload_debug_snapshot_t snapshot;

    for (uint8_t scan = 0u; scan < 32u; scan++) {
        macro_payload_debug_snapshot(&snapshot);
        if (snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE) {
            return;
        }
        macro_payload_engine_scan();
    }
    CHECK(false);
}

uint32_t timer_read32(void) {
    return fake_time;
}

uint32_t timer_elapsed32(uint32_t last) {
    return fake_time - last;
}

void wait_ms(uint16_t delay_ms) {
    (void)delay_ms;
    wait_call_count++;
}

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    if (fail_acquire_keycode == (int16_t)keycode) {
        return false;
    }
    CHECK(keycode <= UINT8_MAX);
    test_log_op(TEST_OP_ACQUIRE, (uint8_t)keycode);
    *lease = (owned_keycode_lease_t){.active = true, .has_basic = true, .basic = (uint8_t)keycode};
    return true;
}

bool owned_keycode_release(owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    CHECK(lease->active);
    test_log_op(TEST_OP_RELEASE, lease->basic);
    *lease = (owned_keycode_lease_t){0};
    return true;
}

static void test_long_delay_start_is_nonblocking_and_wrap_safe(void) {
    macro_payload_ir_t             ir = {.length = 3u, .bytes = {MACRO_PAYLOAD_IR_OP_DELAY, 0xFFu, 0xFFu}};
    macro_payload_debug_snapshot_t snapshot;

    test_reset();
    fake_time = 0xFFFFFFF0u;
    CHECK(macro_payload_start_ir(&ir, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0u, MACRO_PAYLOAD_SOURCE_DIRECT, 0u, test_finish_callback, &callback_count) == MACRO_PAYLOAD_START_STARTED);
    CHECK(wait_call_count == 0u);
    CHECK(test_op_count == 0u);

    macro_payload_engine_scan();
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_WAITING);

    test_scan_after(65535u + TAP_CODE_DELAY - 1u);
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_WAITING);

    test_scan_after(1u);
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_READY);
    CHECK(callback_count == 0u);

    macro_payload_engine_scan();
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE);
    CHECK(snapshot.last_finish == MACRO_PAYLOAD_FINISH_SUCCESS);
    CHECK(callback_count == 1u);
    CHECK(callback_result == MACRO_PAYLOAD_FINISH_SUCCESS);
    CHECK(wait_call_count == 0u);
}

static void test_text_uses_lease_backed_press_and_release_scans(void) {
    macro_payload_ir_t             ir = {.length = 4u, .bytes = {MACRO_PAYLOAD_IR_OP_TEXT, 2u, 'a', 'b'}};
    macro_payload_debug_snapshot_t snapshot;

    test_reset();
    CHECK(macro_payload_start_ir(&ir, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_DIRECT, 0u, NULL, NULL) == MACRO_PAYLOAD_START_STARTED);

    macro_payload_engine_scan();
    CHECK(test_op_count == 1u);
    CHECK(test_ops[0].kind == TEST_OP_ACQUIRE && test_ops[0].keycode == KC_A);

    test_scan_after(6u);
    CHECK(test_op_count == 1u);
    test_scan_after(1u);
    CHECK(test_op_count == 1u);
    macro_payload_engine_scan();
    CHECK(test_op_count == 2u);
    CHECK(test_ops[1].kind == TEST_OP_RELEASE && test_ops[1].keycode == KC_A);

    test_scan_after(7u);
    macro_payload_engine_scan();
    CHECK(test_op_count == 3u);
    CHECK(test_ops[2].kind == TEST_OP_ACQUIRE && test_ops[2].keycode == KC_B);
    test_scan_after(7u);
    macro_payload_engine_scan();
    CHECK(test_op_count == 4u);
    CHECK(test_ops[3].kind == TEST_OP_RELEASE && test_ops[3].keycode == KC_B);
    test_scan_after(7u);
    macro_payload_engine_scan();
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE);
    CHECK(snapshot.completed_count == 1u);
    CHECK(wait_call_count == 0u);
}

static void test_busy_start_is_rejected_without_restarting_active_ir(void) {
    macro_payload_ir_t             active = {.length = 3u, .bytes = {MACRO_PAYLOAD_IR_OP_DELAY, 100u, 0u}};
    macro_payload_ir_t             other  = {.length = 3u, .bytes = {MACRO_PAYLOAD_IR_OP_DELAY, 1u, 0u}};
    macro_payload_debug_snapshot_t snapshot;

    test_reset();
    CHECK(macro_payload_start_ir(&active, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0u, MACRO_PAYLOAD_SOURCE_HARDCODED, 3u, NULL, NULL) == MACRO_PAYLOAD_START_STARTED);
    CHECK(macro_payload_start_ir(&other, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0u, MACRO_PAYLOAD_SOURCE_VIA, 2u, NULL, NULL) == MACRO_PAYLOAD_START_BUSY);
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.busy_rejection_count == 1u);
    CHECK(snapshot.active_source == MACRO_PAYLOAD_SOURCE_HARDCODED);
    CHECK(snapshot.active_slot == 3u);
    CHECK(macro_payload_engine_cancel());
    test_drain_cleanup();
}

static void test_cancel_releases_one_persistent_hold_per_scan(void) {
    macro_payload_ir_t ir = {
        .length = 11u,
        .bytes  = {MACRO_PAYLOAD_IR_OP_KEY_DOWN, KC_A, MACRO_PAYLOAD_IR_OP_KEY_DOWN, KC_B, MACRO_PAYLOAD_IR_OP_DELAY, 0xE8u, 0x03u, MACRO_PAYLOAD_IR_OP_KEY_UP, KC_B, MACRO_PAYLOAD_IR_OP_KEY_UP, KC_A},
    };

    test_reset();
    CHECK(macro_payload_start_ir(&ir, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0u, MACRO_PAYLOAD_SOURCE_DIRECT, 0u, test_finish_callback, &callback_count) == MACRO_PAYLOAD_START_STARTED);
    macro_payload_engine_scan();
    test_scan_after(TAP_CODE_DELAY);
    macro_payload_engine_scan();
    CHECK(test_op_count == 2u);
    CHECK(macro_payload_engine_cancel());

    macro_payload_engine_scan();
    CHECK(test_op_count == 3u);
    CHECK(test_ops[2].kind == TEST_OP_RELEASE && test_ops[2].keycode == KC_B);
    macro_payload_engine_scan();
    CHECK(test_op_count == 4u);
    CHECK(test_ops[3].kind == TEST_OP_RELEASE && test_ops[3].keycode == KC_A);
    macro_payload_engine_scan();
    CHECK(callback_result == MACRO_PAYLOAD_FINISH_CANCELLED);
    CHECK(callback_count == 1u);
}

static void test_runtime_failure_cleans_only_acquired_holds(void) {
    macro_payload_ir_t ir = {
        .length = 8u,
        .bytes  = {MACRO_PAYLOAD_IR_OP_KEY_DOWN, KC_A, MACRO_PAYLOAD_IR_OP_KEY_DOWN, KC_B, MACRO_PAYLOAD_IR_OP_KEY_UP, KC_B, MACRO_PAYLOAD_IR_OP_KEY_UP, KC_A},
    };
    macro_payload_debug_snapshot_t snapshot;

    test_reset();
    fail_acquire_keycode = KC_B;
    CHECK(macro_payload_start_ir(&ir, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0u, MACRO_PAYLOAD_SOURCE_DIRECT, 0u, NULL, NULL) == MACRO_PAYLOAD_START_STARTED);
    macro_payload_engine_scan();
    test_scan_after(TAP_CODE_DELAY);
    macro_payload_engine_scan();
    CHECK(test_op_count == 1u);
    macro_payload_engine_scan();
    CHECK(test_op_count == 2u);
    CHECK(test_ops[1].kind == TEST_OP_RELEASE && test_ops[1].keycode == KC_A);
    macro_payload_engine_scan();
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE);
    CHECK(snapshot.last_finish == MACRO_PAYLOAD_FINISH_RUNTIME_ERROR);
    CHECK(snapshot.runtime_error_count == 1u);
}

static void test_preflight_rejects_malformed_ir_without_side_effects(void) {
    macro_payload_ir_t orphan    = {.length = 2u, .bytes = {MACRO_PAYLOAD_IR_OP_KEY_UP, KC_A}};
    macro_payload_ir_t truncated = {.length = 2u, .bytes = {MACRO_PAYLOAD_IR_OP_DELAY, 1u}};

    test_reset();
    CHECK(macro_payload_start_ir(&orphan, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0u, MACRO_PAYLOAD_SOURCE_DIRECT, 0u, NULL, NULL) == MACRO_PAYLOAD_START_INVALID);
    CHECK(macro_payload_start_ir(&truncated, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0u, MACRO_PAYLOAD_SOURCE_DIRECT, 0u, NULL, NULL) == MACRO_PAYLOAD_START_INVALID);
    CHECK(test_op_count == 0u);
    CHECK(wait_call_count == 0u);
}

int main(void) {
    macro_payload_engine_init();
    test_long_delay_start_is_nonblocking_and_wrap_safe();
    test_text_uses_lease_backed_press_and_release_scans();
    test_busy_start_is_rejected_without_restarting_active_ir();
    test_cancel_releases_one_persistent_hold_per_scan();
    test_runtime_failure_cleans_only_acquired_holds();
    test_preflight_rejects_malformed_ir_without_side_effects();

    puts("macro_payload_engine host tests passed");
    return 0;
}
