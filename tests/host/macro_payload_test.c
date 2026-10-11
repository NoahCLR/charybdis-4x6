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

static void test_authored_compile_resets_streaming_state(void) {
    macro_payload_ir_t ir = {.more = true};
    uint8_t encoded[16];
    uint16_t written;
    CHECK(macro_payload_compile("abc", &ir));
    CHECK(!ir.more);
    ir.more = true;
    CHECK(!macro_payload_encode_ir(&ir, encoded, sizeof(encoded), &written));
    CHECK(written == 0u);
}

static void test_compile_accepts_long_delay_heavy_payload(void) {
    macro_payload_ir_t ir = {0};

    CHECK(macro_payload_compile(test_long_delay_heavy_payload, &ir));
    CHECK(ir.length > 128u);
}

static void test_decode_qmk_stream_accepts_exact_maximum_ir(void) {
    uint8_t            buffer[509];
    macro_payload_ir_t ir     = {0};
    test_qmk_reader_t  reader = {.buffer = buffer};

    memset(buffer, 'A', sizeof(buffer));
    buffer[sizeof(buffer) - 1u] = 0u;
    CHECK(macro_payload_decode_qmk_stream(&ir, (uint16_t)sizeof(buffer), test_qmk_reader_read_byte, &reader));
    CHECK(ir.length == MACRO_PAYLOAD_IR_MAX_BYTES);
}

static void test_decode_qmk_stream_rejects_ir_over_capacity(void) {
    uint8_t            buffer[511];
    macro_payload_ir_t ir     = {0};
    test_qmk_reader_t  reader = {.buffer = buffer};

    memset(buffer, 'A', sizeof(buffer));
    buffer[sizeof(buffer) - 1u] = 0u;
    CHECK(!macro_payload_decode_qmk_stream(&ir, (uint16_t)sizeof(buffer), test_qmk_reader_read_byte, &reader));
    CHECK(ir.length == 0u);
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

// A held modifier around two taps plays each tap once: the tap that follows
// the first must not leave that first one pending for the release to replay.
static void test_decode_qmk_stream_plays_each_tap_under_a_held_key_once(void) {
    static const uint8_t held[] = {
        SS_QMK_PREFIX, SS_DOWN_CODE, TEST_SEND_STRING_U8(X_LEFT_SHIFT), SS_QMK_PREFIX, SS_TAP_CODE, TEST_SEND_STRING_U8(X_A), SS_QMK_PREFIX, SS_TAP_CODE, TEST_SEND_STRING_U8(X_B), SS_QMK_PREFIX, SS_UP_CODE, TEST_SEND_STRING_U8(X_LEFT_SHIFT), 0,
    };
    static const uint8_t expected[] = {
        MACRO_PAYLOAD_IR_OP_KEY_DOWN, TEST_SEND_STRING_U8(X_LEFT_SHIFT), MACRO_PAYLOAD_IR_OP_TAP_LIST, 1, TEST_SEND_STRING_U8(X_A), MACRO_PAYLOAD_IR_OP_TAP_LIST, 1, TEST_SEND_STRING_U8(X_B), MACRO_PAYLOAD_IR_OP_KEY_UP, TEST_SEND_STRING_U8(X_LEFT_SHIFT),
    };
    macro_payload_ir_t ir     = {0};
    test_qmk_reader_t  reader = {.buffer = held};

    CHECK(macro_payload_decode_qmk_stream(&ir, (uint16_t)sizeof(held), test_qmk_reader_read_byte, &reader));
    CHECK(ir.length == sizeof(expected));
    CHECK(memcmp(ir.bytes, expected, sizeof(expected)) == 0);
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

static void test_unicode_round_trip_and_rejection(void) {
    static const char *texts[] = {"café", "“hello” € 🙂", "e\xCC\x81", "👩‍💻"};
    for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
        macro_payload_ir_t authored = {0}, decoded = {0};
        uint8_t bytes[512] = {0};
        uint16_t written = 0;
        CHECK(macro_payload_compile(texts[i], &authored));
        CHECK(macro_payload_encode_ir(&authored, bytes, sizeof(bytes), &written));
        CHECK(written == strlen(texts[i]));
        CHECK(memcmp(bytes, texts[i], written) == 0);
        test_qmk_reader_t reader = {.buffer = bytes};
        CHECK(macro_payload_decode_qmk_stream(&decoded, written + 1u, test_qmk_reader_read_byte, &reader));
        CHECK(decoded.length == authored.length);
        CHECK(memcmp(decoded.bytes, authored.bytes, authored.length) == 0);
    }
    static const uint8_t bad[][6] = {{0xC0,0xAF,0}, {0xE0,0x80,0xAF,0}, {0xF0,0x80,0x80,0xAF,0}, {0xED,0xA0,0x80,0}, {0xF4,0x90,0x80,0x80,0}, {0xC3,0}, {0xE2,0x82,1,1,4,0}, {0xF0,0x9F,0x99,0}, {0x80,0}, {0xFF,0}};
    for (size_t i = 0; i < ARRAY_SIZE(bad); i++) {
        macro_payload_ir_t ir = {0};
        test_qmk_reader_t reader = {.buffer = bad[i]};
        CHECK(!macro_payload_decode_qmk_stream(&ir, sizeof(bad[i]), test_qmk_reader_read_byte, &reader));
        CHECK(ir.length == 0u);
        CHECK(!macro_payload_compile((const char *)bad[i], &ir));
        CHECK(ir.length == 0u);
    }
    char boundary[260]; memset(boundary, 'a', 254); memcpy(boundary + 254, "🙂", 5);
    macro_payload_ir_t ir = {0}; CHECK(macro_payload_compile(boundary, &ir));
    CHECK(ir.length == 260u && ir.bytes[256] == MACRO_PAYLOAD_IR_OP_UNICODE);
    // Non-ASCII scalars cost four IR bytes, including supplementary scalars.
    char maximum[257]; for (size_t i = 0; i < 128; i++) memcpy(maximum + 2*i, "é", 2); maximum[256] = 0;
    CHECK(macro_payload_compile(maximum, &ir)); CHECK(ir.length == 512u);
    char overflow[259]; memcpy(overflow, maximum, 256); memcpy(overflow + 256, "é", 3);
    CHECK(!macro_payload_compile(overflow, &ir)); CHECK(ir.length == 0u);
}

static void test_protection_prefix_round_trip(void) {
    for (uint8_t policy = 1u; policy <= 2u; policy++) {
        macro_payload_ir_t original = {0}, decoded = {0};
        uint8_t bytes[32] = {0}; uint16_t written = 0u;
        CHECK(macro_payload_compile("é{KC_A}", &original));
        original.protection = policy;
        CHECK(macro_payload_encode_ir(&original, bytes, sizeof(bytes), &written));
        CHECK(bytes[0] == 1u && bytes[1] == 5u && bytes[2] == policy);
        test_qmk_reader_t reader = {.buffer = bytes};
        CHECK(macro_payload_decode_qmk_stream(&decoded, written + 1u, test_qmk_reader_read_byte, &reader));
        CHECK(decoded.protection == policy && decoded.length == original.length);
        CHECK(memcmp(decoded.bytes, original.bytes, original.length) == 0);
    }
    const uint8_t bad[][7] = {{1,5,0,0}, {1,5,3,0}, {'a',1,5,1,0}, {1,5,1,1,5,2,0}};
    for (uint8_t i = 0u; i < sizeof(bad)/sizeof(bad[0]); i++) {
        macro_payload_ir_t decoded = {0}; test_qmk_reader_t reader = {.buffer = bad[i]};
        CHECK(!macro_payload_decode_qmk_stream(&decoded, sizeof(bad[i]), test_qmk_reader_read_byte, &reader));
    }
    macro_payload_ir_t decoded = {0}; const uint8_t truncated[] = {1,5}; test_qmk_reader_t reader = {.buffer = truncated};
    CHECK(!macro_payload_decode_qmk_stream(&decoded, sizeof(truncated), test_qmk_reader_read_byte, &reader));
}

// Windows of one stored macro, flattened to one step per text byte so that
// where a window splits a text run does not matter.
#define TEST_STREAM_MAX 40000u
#define TEST_EVENTS_MAX 120000u

typedef struct {
    uint8_t  bytes[TEST_EVENTS_MAX];
    uint32_t length;
    uint16_t windows;
    uint8_t  protection;
} test_events_t;

static void test_events_append_ir(test_events_t *events, const macro_payload_ir_t *ir) {
    uint16_t at = 0u;

    while (at < ir->length) {
        uint8_t  opcode = ir->bytes[at];
        uint16_t size   = 0u;

        switch (opcode) {
            case MACRO_PAYLOAD_IR_OP_TEXT:
                CHECK(at + 1u < ir->length && ir->bytes[at + 1u] != 0u);
                for (uint8_t i = 0u; i < ir->bytes[at + 1u]; i++) {
                    CHECK(events->length + 3u <= sizeof(events->bytes));
                    events->bytes[events->length++] = MACRO_PAYLOAD_IR_OP_TEXT;
                    events->bytes[events->length++] = 1u;
                    events->bytes[events->length++] = ir->bytes[at + 2u + i];
                }
                at = (uint16_t)(at + 2u + ir->bytes[at + 1u]);
                continue;
            case MACRO_PAYLOAD_IR_OP_UNICODE:
                size = 4u;
                break;
            case MACRO_PAYLOAD_IR_OP_DELAY:
                size = 3u;
                break;
            case MACRO_PAYLOAD_IR_OP_KEY_DOWN:
            case MACRO_PAYLOAD_IR_OP_KEY_UP:
                size = 2u;
                break;
            case MACRO_PAYLOAD_IR_OP_TAP_LIST:
                size = (uint16_t)(2u + ir->bytes[at + 1u]);
                break;
            default:
                CHECK(false);
        }
        CHECK(at + size <= ir->length && events->length + size <= sizeof(events->bytes));
        memcpy(&events->bytes[events->length], &ir->bytes[at], size);
        events->length += size;
        at = (uint16_t)(at + size);
    }
}

// Decodes every window, as playback does; false if any window fails.
static bool test_decode_windows(const uint8_t *stream, uint16_t length, test_events_t *events) {
    macro_payload_stream_cursor_t cursor = {0};
    macro_payload_ir_t            ir     = {0};
    test_qmk_reader_t             reader = {.buffer = stream};

    events->length  = 0u;
    events->windows = 0u;
    do {
        uint16_t from = cursor.offset;

        if (!macro_payload_decode_qmk_window(&ir, &cursor, length, test_qmk_reader_read_byte, &reader)) {
            return false;
        }
        CHECK(ir.length <= MACRO_PAYLOAD_IR_MAX_BYTES);
        CHECK(ir.length != 0u || !ir.more);
        CHECK(!ir.more || cursor.offset > from);
        if (events->windows++ == 0u) {
            events->protection = ir.protection;
        }
        CHECK(ir.protection == events->protection);
        test_events_append_ir(events, &ir);
    } while (ir.more);
    return true;
}

static uint16_t test_append_text(uint8_t *stream, uint16_t at, char c, uint16_t count) {
    for (uint16_t i = 0u; i < count; i++) {
        stream[at++] = (uint8_t)c;
    }
    return at;
}

// Three held keys around a tap (one chord), Shift held across text, a delay.
static uint16_t test_append_chords(uint8_t *stream, uint16_t at) {
    static const uint8_t chords[] = {
        SS_QMK_PREFIX, SS_DOWN_CODE, 0xE0u, SS_QMK_PREFIX, SS_DOWN_CODE, 0xE2u, SS_QMK_PREFIX, SS_DOWN_CODE, 0xE3u,
        SS_QMK_PREFIX, SS_TAP_CODE, 0x04u,
        SS_QMK_PREFIX, SS_UP_CODE, 0xE3u, SS_QMK_PREFIX, SS_UP_CODE, 0xE2u, SS_QMK_PREFIX, SS_UP_CODE, 0xE0u,
        SS_QMK_PREFIX, SS_DOWN_CODE, 0xE1u, 'x', 'y', SS_QMK_PREFIX, SS_UP_CODE, 0xE1u,
        SS_QMK_PREFIX, SS_DELAY_CODE, '2', '5', '|',
    };

    memcpy(&stream[at], chords, sizeof(chords));
    return (uint16_t)(at + sizeof(chords));
}

static void test_long_stream_decodes_in_windows_that_split_only_whole_steps(void) {
    static uint8_t       stream[TEST_STREAM_MAX];
    static uint8_t       one[64];
    static test_events_t whole, reference;
    uint16_t             one_length = (uint16_t)(test_append_chords(one, 0u) + 1u);
    macro_payload_ir_t   chord_ir   = {0};
    test_qmk_reader_t    reader     = {.buffer = one};

    one[one_length - 1u] = 0u;
    CHECK(macro_payload_decode_qmk_stream(&chord_ir, one_length, test_qmk_reader_read_byte, &reader));

    // Every offset of the chords relative to a window's end.
    for (uint16_t lead = 0u; lead < 600u; lead++) {
        uint16_t at = test_append_text(stream, 0u, 'a', lead);

        for (uint8_t i = 0u; i < 60u; i++) {
            at = test_append_chords(stream, at);
        }
        stream[at++] = 0u;

        CHECK(test_decode_windows(stream, at, &whole));
        CHECK(whole.windows > 1u);

        reference.length = 0u;
        for (uint16_t i = 0u; i < lead; i++) {
            reference.bytes[reference.length++] = MACRO_PAYLOAD_IR_OP_TEXT;
            reference.bytes[reference.length++] = 1u;
            reference.bytes[reference.length++] = 'a';
        }
        for (uint8_t i = 0u; i < 60u; i++) {
            test_events_append_ir(&reference, &chord_ir);
        }
        CHECK(whole.length == reference.length && memcmp(whole.bytes, reference.bytes, whole.length) == 0);
    }
}

static void test_whole_bank_text_with_a_held_key_and_protection(void) {
    static uint8_t       stream[TEST_STREAM_MAX];
    static test_events_t events;
    uint16_t             at = 0u;

    stream[at++] = SS_QMK_PREFIX;
    stream[at++] = 5u;
    stream[at++] = 1u;
    stream[at++] = SS_QMK_PREFIX;
    stream[at++] = SS_DOWN_CODE;
    stream[at++] = 0xE1u;
    at           = test_append_text(stream, at, 'q', 34000u);
    stream[at++] = SS_QMK_PREFIX;
    stream[at++] = SS_UP_CODE;
    stream[at++] = 0xE1u;
    stream[at++] = 0u;

    CHECK(test_decode_windows(stream, at, &events));
    CHECK(events.protection == 1u);
    CHECK(events.windows > 60u);
    CHECK(events.length == 2u + 34000u * 3u + 2u);
    CHECK(events.bytes[0] == MACRO_PAYLOAD_IR_OP_KEY_DOWN && events.bytes[events.length - 2u] == MACRO_PAYLOAD_IR_OP_KEY_UP);

    // One window is not the whole macro.
    {
        macro_payload_ir_t ir     = {0};
        test_qmk_reader_t  reader = {.buffer = stream};

        CHECK(!macro_payload_decode_qmk_stream(&ir, at, test_qmk_reader_read_byte, &reader));
        CHECK(ir.length == 0u && !ir.more);
    }

    // A key still held at the end fails the last window, however far it is.
    stream[at - 4u] = 0u;
    CHECK(!test_decode_windows(stream, (uint16_t)(at - 3u), &events));
}

static void test_invalid_byte_far_into_a_macro_fails_its_window(void) {
    static uint8_t       stream[TEST_STREAM_MAX];
    static test_events_t events;
    uint16_t             at = test_append_text(stream, 0u, 'a', 5000u);

    stream[at++] = 0x80u;
    at           = test_append_text(stream, at, 'b', 10u);
    stream[at++] = 0u;
    CHECK(!test_decode_windows(stream, at, &events));

    // Unterminated: the bank ends without the macro's terminator.
    at = test_append_text(stream, 0u, 'a', 5000u);
    CHECK(!test_decode_windows(stream, at, &events));
}

int main(void) {
    test_protection_prefix_round_trip();
    test_unicode_round_trip_and_rejection();
    test_validate_accepts_mixed_payload();
    test_validate_rejects_invalid_payloads();
    test_compile_rejects_invalid_payloads();
    test_compile_accepts_long_delay_heavy_payload();
    test_authored_compile_resets_streaming_state();
    test_decode_qmk_stream_accepts_exact_maximum_ir();
    test_decode_qmk_stream_rejects_ir_over_capacity();
    test_encode_emits_expected_qmk_sequence();
    test_encode_fails_when_buffer_is_too_small();
    test_encode_and_decode_qmk_round_trip_through_ir();
    test_decode_qmk_stream_rejects_unbalanced_key_downs();
    test_decode_qmk_stream_rejects_orphan_key_up();
    test_decode_qmk_stream_plays_each_tap_under_a_held_key_once();
    test_decode_qmk_stream_rejects_every_high_text_byte();
    test_decode_qmk_stream_keeps_high_command_operands();
    test_long_stream_decodes_in_windows_that_split_only_whole_steps();
    test_whole_bank_text_with_a_held_key_and_protection();
    test_invalid_byte_far_into_a_macro_fails_its_window();

    puts("macro_payload host tests passed");
    return 0;
}
