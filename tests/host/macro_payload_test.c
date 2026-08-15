#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qmk_stub.h"
#include "send_string.h"
#include "users/noah/lib/macro/macro_payload.h"

static const char *const test_long_delay_heavy_payload = "h{829}e{627}y{665} {249}h{158}a{167}l{424}o{386} {448}h{144}o{111}e{103} {118}i{123}s{118} {125}h{132}e{134}t{158} {133}m{118}e{493}t{156} {503}y{503}u{10}o{695} {382}h{113}e{212}b{149}b{83}e{155}n{60} {102}e{65}w{164} {79} {152}h{109}i{79}e{129}r{146} {143}e{176}e{104}n{98} {148}p{124}r{119}o{165}b{126}l{130}e{172}e{104}m{1032}{+KC_LSFT}{189};{140}{-KC_LSFT}";

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

static void test_compile_rejects_invalid_payloads(void) {
    static const char  non_ascii_payload[] = {'A', (char)0x80, '\0'};
    macro_payload_ir_t ir                  = {0};

    CHECK(!macro_payload_compile("{KC_A", &ir));
    CHECK(ir.length == 0u);
    CHECK(!macro_payload_compile("{+KC_LGUI}{+KC_A}{120}{-KC_LGUI}{1340}{+KC_LGUI}{+KC_C}{-KC_LGUI}", &ir));
    CHECK(ir.length == 0u);
    CHECK(!macro_payload_compile(non_ascii_payload, &ir));
    CHECK(ir.length == 0u);
}

static void test_compile_accepts_long_delay_heavy_payload(void) {
    macro_payload_ir_t ir = {0};

    CHECK(macro_payload_compile(test_long_delay_heavy_payload, &ir));
    CHECK(ir.length > 128u);
}

static void test_encode_emits_expected_qmk_sequence(void) {
    uint8_t              buffer[32] = {0};
    uint16_t             written    = 0;
    static const uint8_t expected[] = {
        'A', SS_QMK_PREFIX, SS_DELAY_CODE, '1', '2', '|', SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_CTRL), SS_QMK_PREFIX, SS_TAP_CODE, TEST_SEND_STRING_U8(X_C), SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_CTRL), SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_SHIFT), SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_SHIFT),
    };

    CHECK(macro_payload_encode("A{12}{KC_LCTL,KC_C}{+KC_LSFT}{-KC_LSFT}", buffer, sizeof(buffer), &written));
    CHECK(written == sizeof(expected));
    CHECK(memcmp(buffer, expected, sizeof(expected)) == 0);
}

static void test_encode_fails_when_buffer_is_too_small(void) {
    uint8_t  buffer[4] = {0};
    uint16_t written   = 99u;

    CHECK(!macro_payload_encode("{123}", buffer, sizeof(buffer), &written));
    CHECK(written == 0u);
}

static void test_encode_and_decode_qmk_round_trip_through_ir(void) {
    macro_payload_ir_t expected   = {0};
    macro_payload_ir_t decoded    = {0};
    uint8_t            buffer[64] = {0};
    uint16_t           written    = 0;
    test_qmk_reader_t  reader     = {.buffer = buffer};

    CHECK(macro_payload_compile("A{12}{KC_LCTL,KC_C}{+KC_LSFT}{-KC_LSFT}", &expected));
    CHECK(macro_payload_encode_ir(&expected, buffer, sizeof(buffer), &written));
    CHECK(written > 0u);
    buffer[written] = 0u;
    CHECK(macro_payload_decode_qmk_stream(&decoded, (uint16_t)(written + 1u), test_qmk_reader_read_byte, &reader));
    CHECK(decoded.length == expected.length);
    CHECK(memcmp(decoded.bytes, expected.bytes, expected.length) == 0);
}

static void test_decode_qmk_stream_rejects_unbalanced_key_downs(void) {
    static const uint8_t unbalanced[] = {
        SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_GUI), SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_A), SS_QMK_PREFIX, SS_DELAY_CODE, '1', '2', '0', '|', SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_GUI), SS_QMK_PREFIX, SS_DELAY_CODE, '1', '3', '4', '0', '|', SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_GUI), SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_C), SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_GUI), 0,
    };
    macro_payload_ir_t ir     = {0};
    test_qmk_reader_t  reader = {.buffer = unbalanced};

    CHECK(!macro_payload_decode_qmk_stream(&ir, (uint16_t)sizeof(unbalanced), test_qmk_reader_read_byte, &reader));
    CHECK(ir.length == 0u);
}

static void test_decode_qmk_stream_rejects_orphan_key_up(void) {
    static const uint8_t orphan[] = {SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_SHIFT), 0};
    macro_payload_ir_t   ir       = {0};
    test_qmk_reader_t    reader   = {.buffer = orphan};

    CHECK(!macro_payload_decode_qmk_stream(&ir, (uint16_t)sizeof(orphan), test_qmk_reader_read_byte, &reader));
    CHECK(ir.length == 0u);
}

static void test_decode_qmk_stream_rejects_every_high_text_byte(void) {
    for (uint16_t value = 0x80u; value <= UINT8_MAX; value++) {
        uint8_t direct[]        = {(uint8_t)value, 0};
        uint8_t surrounded[]    = {'A', (uint8_t)value, 'B', 0};
        uint8_t after_command[] = {SS_QMK_PREFIX, SS_TAP_CODE, (uint8_t)value, (uint8_t)value, 0};
        const struct {
            const uint8_t *bytes;
            uint16_t       length;
        } cases[] = {
            {.bytes = direct, .length = sizeof(direct)},
            {.bytes = surrounded, .length = sizeof(surrounded)},
            {.bytes = after_command, .length = sizeof(after_command)},
        };

        for (size_t index = 0; index < ARRAY_SIZE(cases); index++) {
            macro_payload_ir_t ir     = {.length = 1u, .bytes = {0xA5u}};
            test_qmk_reader_t  reader = {.buffer = cases[index].bytes};

            CHECK(!macro_payload_decode_qmk_stream(&ir, cases[index].length, test_qmk_reader_read_byte, &reader));
            CHECK(ir.length == 0u);
        }
    }
}

static void test_decode_qmk_stream_keeps_high_command_operands(void) {
    static const uint8_t keycodes[] = {0x80u, 0xFEu, 0xFFu};

    for (size_t index = 0; index < ARRAY_SIZE(keycodes); index++) {
        uint8_t            tap[]       = {SS_QMK_PREFIX, SS_TAP_CODE, keycodes[index], 0};
        uint8_t            held[]      = {SS_QMK_PREFIX, SS_DOWN_CODE, keycodes[index], SS_QMK_PREFIX, SS_UP_CODE, keycodes[index], 0};
        macro_payload_ir_t tap_ir      = {0};
        macro_payload_ir_t held_ir     = {0};
        test_qmk_reader_t  tap_reader  = {.buffer = tap};
        test_qmk_reader_t  held_reader = {.buffer = held};

        CHECK(macro_payload_decode_qmk_stream(&tap_ir, sizeof(tap), test_qmk_reader_read_byte, &tap_reader));
        CHECK(tap_ir.length == 3u);
        CHECK(tap_ir.bytes[0] == MACRO_PAYLOAD_IR_OP_TAP_LIST);
        CHECK(tap_ir.bytes[1] == 1u);
        CHECK(tap_ir.bytes[2] == keycodes[index]);

        CHECK(macro_payload_decode_qmk_stream(&held_ir, sizeof(held), test_qmk_reader_read_byte, &held_reader));
        CHECK(held_ir.length == 4u);
        CHECK(held_ir.bytes[0] == MACRO_PAYLOAD_IR_OP_KEY_DOWN);
        CHECK(held_ir.bytes[1] == keycodes[index]);
        CHECK(held_ir.bytes[2] == MACRO_PAYLOAD_IR_OP_KEY_UP);
        CHECK(held_ir.bytes[3] == keycodes[index]);
    }
}

int main(void) {
    test_validate_accepts_mixed_payload();
    test_validate_rejects_invalid_payloads();
    test_compile_rejects_invalid_payloads();
    test_compile_accepts_long_delay_heavy_payload();
    test_encode_emits_expected_qmk_sequence();
    test_encode_fails_when_buffer_is_too_small();
    test_encode_and_decode_qmk_round_trip_through_ir();
    test_decode_qmk_stream_rejects_unbalanced_key_downs();
    test_decode_qmk_stream_rejects_orphan_key_up();
    test_decode_qmk_stream_rejects_every_high_text_byte();
    test_decode_qmk_stream_keeps_high_command_operands();

    puts("macro_payload host tests passed");
    return 0;
}
