#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qmk_stub.h"
#include "send_string.h"
#include "users/noah/lib/macro/macro_payload.h"

typedef enum {
    TEST_OP_SEND_CHAR = 1,
    TEST_OP_SEND_CHAR_DELAYED,
    TEST_OP_WAIT,
    TEST_OP_REGISTER,
    TEST_OP_UNREGISTER,
    TEST_OP_TAP,
} test_op_kind_t;

typedef struct {
    test_op_kind_t kind;
    uint16_t       value;
} test_op_t;

#define TEST_MAX_OPS 32

static test_op_t test_ops[TEST_MAX_OPS];
static uint8_t   test_op_count;

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

#define TEST_SEND_STRING_U8(value_) TEST_SEND_STRING_U8_IMPL(value_)
#define TEST_SEND_STRING_U8_IMPL(value_) ((uint8_t)(0x##value_))

static void test_log_op(test_op_kind_t kind, uint16_t value) {
    CHECK(test_op_count < TEST_MAX_OPS);
    test_ops[test_op_count++] = (test_op_t){
        .kind  = kind,
        .value = value,
    };
}

static void test_reset_stubs(void) {
    memset(test_ops, 0, sizeof(test_ops));
    test_op_count = 0;
}

void send_char(char ascii_code) {
    test_log_op(TEST_OP_SEND_CHAR, (uint8_t)ascii_code);
}

void send_char_with_delay(char ascii_code, uint8_t interval) {
    test_log_op(TEST_OP_SEND_CHAR_DELAYED, (uint16_t)(((uint16_t)interval << 8) | (uint8_t)ascii_code));
}

void wait_ms(uint16_t ms) {
    test_log_op(TEST_OP_WAIT, ms);
}

bool owned_keycode_register(uint16_t keycode) {
    test_log_op(TEST_OP_REGISTER, keycode);
    return true;
}

bool owned_keycode_unregister(uint16_t keycode) {
    test_log_op(TEST_OP_UNREGISTER, keycode);
    return true;
}

bool owned_keycode_tap(uint16_t keycode) {
    test_log_op(TEST_OP_TAP, keycode);
    return true;
}

static void test_validate_accepts_mixed_payload(void) {
    CHECK(macro_payload_validate("Hi{KC_LCTL,KC_C}{25}{+KC_LSFT}{-KC_LSFT}"));
}

static void test_validate_rejects_invalid_payloads(void) {
    static const char non_ascii_payload[] = {'A', (char)0x80, '\0'};

    CHECK(!macro_payload_validate("{ }"));
    CHECK(!macro_payload_validate("{KC_NOT_A_KEY}"));
    CHECK(!macro_payload_validate("{+KC_LCTL,KC_C}"));
    CHECK(!macro_payload_validate("{KC_A"));
    CHECK(!macro_payload_validate(non_ascii_payload));
}

static void test_play_runs_text_chords_and_delays_in_order(void) {
    test_reset_stubs();

    CHECK(macro_payload_play("A{KC_LCTL,KC_C}{25}{+KC_LSFT}{-KC_LSFT}Z"));
    CHECK(test_op_count == 12);

    CHECK(test_ops[0].kind == TEST_OP_SEND_CHAR);
    CHECK(test_ops[0].value == 'A');
    CHECK(test_ops[1].kind == TEST_OP_REGISTER);
    CHECK(test_ops[1].value == TEST_SEND_STRING_U8(X_LEFT_CTRL));
    CHECK(test_ops[2].kind == TEST_OP_TAP);
    CHECK(test_ops[2].value == TEST_SEND_STRING_U8(X_C));
    CHECK(test_ops[3].kind == TEST_OP_UNREGISTER);
    CHECK(test_ops[3].value == TEST_SEND_STRING_U8(X_LEFT_CTRL));
    CHECK(test_ops[4].kind == TEST_OP_WAIT);
    CHECK(test_ops[4].value == TAP_CODE_DELAY);
    CHECK(test_ops[5].kind == TEST_OP_WAIT);
    CHECK(test_ops[5].value == 25);
    CHECK(test_ops[6].kind == TEST_OP_WAIT);
    CHECK(test_ops[6].value == TAP_CODE_DELAY);
    CHECK(test_ops[7].kind == TEST_OP_REGISTER);
    CHECK(test_ops[7].value == TEST_SEND_STRING_U8(X_LEFT_SHIFT));
    CHECK(test_ops[8].kind == TEST_OP_WAIT);
    CHECK(test_ops[8].value == TAP_CODE_DELAY);
    CHECK(test_ops[9].kind == TEST_OP_UNREGISTER);
    CHECK(test_ops[9].value == TEST_SEND_STRING_U8(X_LEFT_SHIFT));
    CHECK(test_ops[10].kind == TEST_OP_WAIT);
    CHECK(test_ops[10].value == TAP_CODE_DELAY);
    CHECK(test_ops[11].kind == TEST_OP_SEND_CHAR);
    CHECK(test_ops[11].value == 'Z');
}

static void test_compile_and_play_ir_runs_without_reparsing_source(void) {
    macro_payload_ir_t ir = {0};

    CHECK(macro_payload_compile("A{KC_LCTL,KC_C}{25}{+KC_LSFT}{-KC_LSFT}Z", &ir));
    CHECK(ir.length > 0);

    test_reset_stubs();

    CHECK(macro_payload_play_ir(&ir));
    CHECK(test_op_count == 12);

    CHECK(test_ops[0].kind == TEST_OP_SEND_CHAR);
    CHECK(test_ops[0].value == 'A');
    CHECK(test_ops[1].kind == TEST_OP_REGISTER);
    CHECK(test_ops[1].value == TEST_SEND_STRING_U8(X_LEFT_CTRL));
    CHECK(test_ops[2].kind == TEST_OP_TAP);
    CHECK(test_ops[2].value == TEST_SEND_STRING_U8(X_C));
    CHECK(test_ops[3].kind == TEST_OP_UNREGISTER);
    CHECK(test_ops[3].value == TEST_SEND_STRING_U8(X_LEFT_CTRL));
    CHECK(test_ops[4].kind == TEST_OP_WAIT);
    CHECK(test_ops[4].value == TAP_CODE_DELAY);
    CHECK(test_ops[5].kind == TEST_OP_WAIT);
    CHECK(test_ops[5].value == 25);
    CHECK(test_ops[6].kind == TEST_OP_WAIT);
    CHECK(test_ops[6].value == TAP_CODE_DELAY);
    CHECK(test_ops[7].kind == TEST_OP_REGISTER);
    CHECK(test_ops[7].value == TEST_SEND_STRING_U8(X_LEFT_SHIFT));
    CHECK(test_ops[8].kind == TEST_OP_WAIT);
    CHECK(test_ops[8].value == TAP_CODE_DELAY);
    CHECK(test_ops[9].kind == TEST_OP_UNREGISTER);
    CHECK(test_ops[9].value == TEST_SEND_STRING_U8(X_LEFT_SHIFT));
    CHECK(test_ops[10].kind == TEST_OP_WAIT);
    CHECK(test_ops[10].value == TAP_CODE_DELAY);
    CHECK(test_ops[11].kind == TEST_OP_SEND_CHAR);
    CHECK(test_ops[11].value == 'Z');
}

typedef struct {
    const uint8_t *buffer;
} test_qmk_reader_t;

static bool test_qmk_reader_read_byte(uint16_t offset, uint8_t *byte, void *context) {
    test_qmk_reader_t *reader = (test_qmk_reader_t *)context;

    CHECK(byte != NULL);
    CHECK(reader != NULL);
    *byte = reader->buffer[offset];
    return true;
}

static void test_compile_rejects_invalid_payloads(void) {
    static const char  non_ascii_payload[] = {'A', (char)0x80, '\0'};
    macro_payload_ir_t ir                  = {0};

    CHECK(!macro_payload_compile("{KC_A", &ir));
    CHECK(ir.length == 0);
    CHECK(!macro_payload_compile(non_ascii_payload, &ir));
    CHECK(ir.length == 0);
}

static void test_encode_emits_expected_qmk_sequence(void) {
    uint8_t  buffer[32] = {0};
    uint16_t written    = 0;

    static const uint8_t expected[] = {
        'A', SS_QMK_PREFIX, SS_DELAY_CODE, '1', '2', '|', SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_CTRL), SS_QMK_PREFIX, SS_TAP_CODE, TEST_SEND_STRING_U8(X_C), SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_CTRL), SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_SHIFT),
    };

    CHECK(macro_payload_encode("A{12}{KC_LCTL,KC_C}{-KC_LSFT}", buffer, sizeof(buffer), &written));
    CHECK(written == sizeof(expected));
    CHECK(memcmp(buffer, expected, sizeof(expected)) == 0);
}

static void test_encode_fails_when_buffer_is_too_small(void) {
    uint8_t  buffer[4] = {0};
    uint16_t written   = 99;

    CHECK(!macro_payload_encode("{123}", buffer, sizeof(buffer), &written));
    CHECK(written == 0);
}

static void test_encode_and_decode_qmk_round_trip_through_ir(void) {
    macro_payload_ir_t expected   = {0};
    macro_payload_ir_t decoded    = {0};
    uint8_t            buffer[64] = {0};
    uint16_t           written    = 0;
    test_qmk_reader_t  reader     = {.buffer = buffer};

    CHECK(macro_payload_compile("A{12}{KC_LCTL,KC_C}{-KC_LSFT}", &expected));
    CHECK(macro_payload_encode_ir(&expected, buffer, sizeof(buffer), &written));
    CHECK(written > 0);

    buffer[written] = 0;
    CHECK(macro_payload_decode_qmk_stream(&decoded, (uint16_t)(written + 1u), test_qmk_reader_read_byte, &reader));
    CHECK(decoded.length == expected.length);
    CHECK(memcmp(decoded.bytes, expected.bytes, expected.length) == 0);
}

static void test_play_ir_with_delayed_text_uses_send_char_with_delay(void) {
    macro_payload_ir_t ir = {0};

    CHECK(macro_payload_compile("Hi", &ir));

    test_reset_stubs();

    CHECK(macro_payload_play_ir_with_text_output(&ir, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, TAP_CODE_DELAY));
    CHECK(test_op_count == 2);
    CHECK(test_ops[0].kind == TEST_OP_SEND_CHAR_DELAYED);
    CHECK((uint8_t)(test_ops[0].value & 0xFFu) == 'H');
    CHECK((uint8_t)(test_ops[0].value >> 8) == TAP_CODE_DELAY);
    CHECK(test_ops[1].kind == TEST_OP_SEND_CHAR_DELAYED);
    CHECK((uint8_t)(test_ops[1].value & 0xFFu) == 'i');
    CHECK((uint8_t)(test_ops[1].value >> 8) == TAP_CODE_DELAY);
}

int main(void) {
    test_validate_accepts_mixed_payload();
    test_validate_rejects_invalid_payloads();
    test_play_runs_text_chords_and_delays_in_order();
    test_compile_and_play_ir_runs_without_reparsing_source();
    test_compile_rejects_invalid_payloads();
    test_encode_emits_expected_qmk_sequence();
    test_encode_fails_when_buffer_is_too_small();
    test_encode_and_decode_qmk_round_trip_through_ir();
    test_play_ir_with_delayed_text_uses_send_char_with_delay();

    puts("macro_payload host tests passed");
    return 0;
}
