#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qmk_stub.h"
#include "send_string.h"
#include "users/noah/lib/action/owned_keycode.h"
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

#define TEST_MAX_OPS 512

static const char *const test_long_delay_heavy_payload = "h{829}e{627}y{665} {249}h{158}a{167}l{424}o{386} {448}h{144}o{111}e{103} {118}i{123}s{118} {125}h{132}e{134}t{158} {133}m{118}e{493}t{156} {503}y{503}u{10}o{695} {382}h{113}e{212}b{149}b{83}e{155}n{60} {102}e{65}w{164} {79} {152}h{109}i{79}e{129}r{146} {143}e{176}e{104}n{98} {148}p{124}r{119}o{165}b{126}l{130}e{172}e{104}m{1032}{+KC_LSFT}{189};{140}{-KC_LSFT}";

static test_op_t test_ops[TEST_MAX_OPS];
static uint16_t  test_op_count;
static uint16_t  test_runtime_diag_heartbeat_count;
static int16_t   test_failed_acquire_keycode;

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
    test_op_count                     = 0;
    test_runtime_diag_heartbeat_count = 0;
    test_failed_acquire_keycode        = -1;
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

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    if (test_failed_acquire_keycode == (int16_t)keycode) {
        return false;
    }
    test_log_op(TEST_OP_REGISTER, keycode);
    *lease = (owned_keycode_lease_t){.active = true, .has_basic = true, .basic = (uint8_t)keycode};
    return true;
}

bool owned_keycode_release(owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    CHECK(lease->active);
    test_log_op(TEST_OP_UNREGISTER, lease->basic);
    *lease = (owned_keycode_lease_t){0};
    return true;
}

bool owned_keycode_tap(uint16_t keycode) {
    test_log_op(TEST_OP_TAP, keycode);
    return true;
}

void noah_runtime_diag_heartbeat(void) {
    test_runtime_diag_heartbeat_count++;
}

static void test_validate_accepts_mixed_payload(void) {
    CHECK(macro_payload_validate("Hi{KC_LCTL,KC_C}{25}{+KC_LSFT}{-KC_LSFT}"));
}

static void test_validate_rejects_invalid_payloads(void) {
    static const char non_ascii_payload[] = {'A', (char)0x80, '\0'};

    CHECK(!macro_payload_validate("{ }"));
    CHECK(!macro_payload_validate("{KC_NOT_A_KEY}"));
    CHECK(!macro_payload_validate("{+KC_LCTL,KC_C}"));
    CHECK(!macro_payload_validate("{+KC_LSFT}"));
    CHECK(!macro_payload_validate("{-KC_LSFT}"));
    CHECK(!macro_payload_validate("{+KC_LSFT}{+KC_LSFT}{-KC_LSFT}"));
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
    CHECK(!macro_payload_compile("{+KC_LGUI}{+KC_A}{120}{-KC_LGUI}{1340}{+KC_LGUI}{+KC_C}{-KC_LGUI}", &ir));
    CHECK(ir.length == 0);
    CHECK(!macro_payload_compile(non_ascii_payload, &ir));
    CHECK(ir.length == 0);
}

static void test_compile_accepts_long_delay_heavy_payload(void) {
    macro_payload_ir_t ir = {0};

    CHECK(macro_payload_compile(test_long_delay_heavy_payload, &ir));
    CHECK(ir.length > 128u);
}

static void test_encode_emits_expected_qmk_sequence(void) {
    uint8_t  buffer[32] = {0};
    uint16_t written    = 0;

    static const uint8_t expected[] = {
        'A', SS_QMK_PREFIX, SS_DELAY_CODE, '1', '2', '|', SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_CTRL), SS_QMK_PREFIX, SS_TAP_CODE, TEST_SEND_STRING_U8(X_C), SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_CTRL), SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_SHIFT), SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_SHIFT),
    };

    CHECK(macro_payload_encode("A{12}{KC_LCTL,KC_C}{+KC_LSFT}{-KC_LSFT}", buffer, sizeof(buffer), &written));
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

    CHECK(macro_payload_compile("A{12}{KC_LCTL,KC_C}{+KC_LSFT}{-KC_LSFT}", &expected));
    CHECK(macro_payload_encode_ir(&expected, buffer, sizeof(buffer), &written));
    CHECK(written > 0);

    buffer[written] = 0;
    CHECK(macro_payload_decode_qmk_stream(&decoded, (uint16_t)(written + 1u), test_qmk_reader_read_byte, &reader));
    CHECK(decoded.length == expected.length);
    CHECK(memcmp(decoded.bytes, expected.bytes, expected.length) == 0);
}

static void test_decode_qmk_stream_rejects_unbalanced_key_downs(void) {
    static const uint8_t unbalanced[] = {
        SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_GUI),
        SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_A),
        SS_QMK_PREFIX, SS_DELAY_CODE, '1', '2', '0', '|',
        SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_GUI),
        SS_QMK_PREFIX, SS_DELAY_CODE, '1', '3', '4', '0', '|',
        SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_GUI),
        SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_C),
        SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_GUI),
        0,
    };
    macro_payload_ir_t ir     = {0};
    test_qmk_reader_t  reader = {.buffer = unbalanced};

    CHECK(!macro_payload_decode_qmk_stream(&ir, (uint16_t)sizeof(unbalanced), test_qmk_reader_read_byte, &reader));
    CHECK(ir.length == 0);
}

static void test_decode_and_play_reject_orphan_key_up_without_side_effects(void) {
    static const uint8_t orphan_stream[] = {
        SS_QMK_PREFIX,
        SS_UP_CODE,
        TEST_SEND_STRING_U8(X_LEFT_SHIFT),
        0,
    };
    macro_payload_ir_t decoded   = {0};
    macro_payload_ir_t orphan_ir = {
        .length = 2u,
        .bytes  = {MACRO_PAYLOAD_IR_OP_KEY_UP, KC_LEFT_SHIFT},
    };
    test_qmk_reader_t reader = {.buffer = orphan_stream};

    CHECK(!macro_payload_decode_qmk_stream(&decoded, (uint16_t)sizeof(orphan_stream), test_qmk_reader_read_byte, &reader));
    CHECK(decoded.length == 0u);

    test_reset_stubs();
    CHECK(!macro_payload_play_ir(&orphan_ir));
    CHECK(test_op_count == 0u);
}

static void test_decode_qmk_stream_rejects_every_high_text_byte(void) {
    for (uint16_t value = 0x80u; value <= UINT8_MAX; value++) {
        uint8_t direct[] = {(uint8_t)value, 0};
        uint8_t surrounded[] = {'A', (uint8_t)value, 'B', 0};
        uint8_t after_command[] = {SS_QMK_PREFIX, SS_TAP_CODE, (uint8_t)value, (uint8_t)value, 0};
        const struct {
            const uint8_t *bytes;
            uint16_t       length;
        } cases[] = {
            {.bytes = direct, .length = sizeof(direct)},
            {.bytes = surrounded, .length = sizeof(surrounded)},
            {.bytes = after_command, .length = sizeof(after_command)},
        };

        for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
            macro_payload_ir_t ir     = {.length = 1u, .bytes = {0xA5}};
            test_qmk_reader_t  reader = {.buffer = cases[index].bytes};

            CHECK(!macro_payload_decode_qmk_stream(&ir, cases[index].length, test_qmk_reader_read_byte, &reader));
            CHECK(ir.length == 0u);
        }
    }
}

static void test_decode_qmk_stream_keeps_high_command_operands_in_keycode_grammar(void) {
    static const uint8_t keycodes[] = {0x80u, 0xFEu, 0xFFu};

    for (size_t index = 0; index < sizeof(keycodes) / sizeof(keycodes[0]); index++) {
        uint8_t tap[] = {SS_QMK_PREFIX, SS_TAP_CODE, keycodes[index], 0};
        uint8_t held[] = {
            SS_QMK_PREFIX, SS_DOWN_CODE, keycodes[index],
            SS_QMK_PREFIX, SS_UP_CODE, keycodes[index],
            0,
        };
        macro_payload_ir_t tap_ir     = {0};
        macro_payload_ir_t held_ir    = {0};
        test_qmk_reader_t  tap_reader = {.buffer = tap};
        test_qmk_reader_t  held_reader = {.buffer = held};

        CHECK(macro_payload_decode_qmk_stream(&tap_ir, sizeof(tap), test_qmk_reader_read_byte, &tap_reader));
        CHECK(macro_payload_decode_qmk_stream(&held_ir, sizeof(held), test_qmk_reader_read_byte, &held_reader));

        test_reset_stubs();
        CHECK(macro_payload_play_ir(&tap_ir));
        CHECK(test_op_count == 2u);
        CHECK(test_ops[0].kind == TEST_OP_TAP);
        CHECK(test_ops[0].value == keycodes[index]);

        test_reset_stubs();
        CHECK(macro_payload_play_ir(&held_ir));
        CHECK(test_op_count == 4u);
        CHECK(test_ops[0].kind == TEST_OP_REGISTER);
        CHECK(test_ops[0].value == keycodes[index]);
        CHECK(test_ops[2].kind == TEST_OP_UNREGISTER);
        CHECK(test_ops[2].value == keycodes[index]);
    }
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

static void test_play_ir_preflight_rejects_malformed_ir_without_side_effects(void) {
    macro_payload_ir_t ir = {
        .length = 2,
        .bytes  = {MACRO_PAYLOAD_IR_OP_KEY_DOWN, TEST_SEND_STRING_U8(X_LEFT_CTRL)},
    };
    macro_payload_ir_t high_text = {
        .length = 3,
        .bytes  = {MACRO_PAYLOAD_IR_OP_TEXT, 1, 0x80},
    };
    macro_payload_ir_t valid_then_truncated = {
        .length = 5,
        .bytes  = {MACRO_PAYLOAD_IR_OP_TEXT, 1, 'A', MACRO_PAYLOAD_IR_OP_DELAY, 1},
    };

    test_reset_stubs();

    CHECK(!macro_payload_play_ir(&ir));
    CHECK(test_op_count == 0u);

    CHECK(!macro_payload_play_ir(&high_text));
    CHECK(test_op_count == 0u);

    CHECK(!macro_payload_play_ir(&valid_then_truncated));
    CHECK(test_op_count == 0u);
}

static void test_playback_failure_releases_only_successfully_acquired_holds(void) {
    macro_payload_ir_t ir = {0};

    CHECK(macro_payload_compile("{+KC_LCTL}{+KC_LSFT}{-KC_LSFT}{-KC_LCTL}", &ir));
    test_reset_stubs();
    test_failed_acquire_keycode = TEST_SEND_STRING_U8(X_LEFT_SHIFT);

    CHECK(!macro_payload_play_ir(&ir));
    CHECK(test_op_count == 3u);
    CHECK(test_ops[0].kind == TEST_OP_REGISTER);
    CHECK(test_ops[0].value == TEST_SEND_STRING_U8(X_LEFT_CTRL));
    CHECK(test_ops[1].kind == TEST_OP_WAIT);
    CHECK(test_ops[2].kind == TEST_OP_UNREGISTER);
    CHECK(test_ops[2].value == TEST_SEND_STRING_U8(X_LEFT_CTRL));
}

static void test_play_long_delay_heavy_payload_keeps_runtime_heartbeat_alive(void) {
    macro_payload_ir_t ir            = {0};
    uint32_t           total_wait_ms = 0;
    uint16_t           delayed_chars = 0;
    uint16_t           shift_downs   = 0;
    uint16_t           shift_ups     = 0;
    uint16_t           max_wait_ms   = 0;

    CHECK(macro_payload_compile(test_long_delay_heavy_payload, &ir));

    test_reset_stubs();

    CHECK(macro_payload_play_ir_with_text_output(&ir, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, TAP_CODE_DELAY));

    for (uint16_t index = 0; index < test_op_count; index++) {
        if (test_ops[index].kind == TEST_OP_WAIT) {
            total_wait_ms += test_ops[index].value;
            if (test_ops[index].value > max_wait_ms) {
                max_wait_ms = test_ops[index].value;
            }
        } else if (test_ops[index].kind == TEST_OP_SEND_CHAR_DELAYED) {
            delayed_chars++;
        } else if (test_ops[index].kind == TEST_OP_REGISTER && test_ops[index].value == TEST_SEND_STRING_U8(X_LEFT_SHIFT)) {
            shift_downs++;
        } else if (test_ops[index].kind == TEST_OP_UNREGISTER && test_ops[index].value == TEST_SEND_STRING_U8(X_LEFT_SHIFT)) {
            shift_ups++;
        }
    }

    CHECK(delayed_chars == 57u);
    CHECK(shift_downs == 1u);
    CHECK(shift_ups == 1u);
    CHECK(total_wait_ms == 13579u);
    CHECK(max_wait_ms <= 50u);
    CHECK(test_runtime_diag_heartbeat_count > 0u);
}

int main(void) {
    test_validate_accepts_mixed_payload();
    test_validate_rejects_invalid_payloads();
    test_play_runs_text_chords_and_delays_in_order();
    test_compile_and_play_ir_runs_without_reparsing_source();
    test_compile_rejects_invalid_payloads();
    test_compile_accepts_long_delay_heavy_payload();
    test_encode_emits_expected_qmk_sequence();
    test_encode_fails_when_buffer_is_too_small();
    test_encode_and_decode_qmk_round_trip_through_ir();
    test_decode_qmk_stream_rejects_unbalanced_key_downs();
    test_decode_and_play_reject_orphan_key_up_without_side_effects();
    test_decode_qmk_stream_rejects_every_high_text_byte();
    test_decode_qmk_stream_keeps_high_command_operands_in_keycode_grammar();
    test_play_ir_with_delayed_text_uses_send_char_with_delay();
    test_play_ir_preflight_rejects_malformed_ir_without_side_effects();
    test_playback_failure_releases_only_successfully_acquired_holds();
    test_play_long_delay_heavy_payload_keeps_runtime_heartbeat_alive();

    puts("macro_payload host tests passed");
    return 0;
}
