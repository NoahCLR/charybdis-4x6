#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/compat/qmk_via_sync_metadata.h"

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

static void test_clean_and_dirty_round_trip(void) {
    static const uint32_t generations[] = {1u, 2u, NOAH_QMK_VIA_SYNC_METADATA_SERIAL_HALF - 1u, NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK};

    for (size_t index = 0u; index < sizeof(generations) / sizeof(generations[0]); index++) {
        for (uint8_t dirty = 0u; dirty <= 1u; dirty++) {
            noah_qmk_via_sync_metadata_t decoded;
            noah_qmk_via_sync_metadata_t source = {.generation = generations[index], .dirty = dirty != 0u};
            uint32_t                     word   = noah_qmk_via_sync_metadata_encode(source);

            CHECK(word != 0u);
            CHECK(noah_qmk_via_sync_metadata_decode(word, &decoded));
            CHECK(decoded.generation == source.generation);
            CHECK(decoded.dirty == source.dirty);
        }
    }
}

static void test_invalid_schema_and_generation_are_rejected(void) {
    noah_qmk_via_sync_metadata_t decoded = {0};

    CHECK(noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){0}) == 0u);
    CHECK(!noah_qmk_via_sync_metadata_decode(0u, &decoded));
    CHECK(!noah_qmk_via_sync_metadata_decode(UINT32_C(0x20000001), &decoded));
    CHECK(!noah_qmk_via_sync_metadata_decode(UINT32_C(0x10000000), &decoded));
    CHECK(!noah_qmk_via_sync_metadata_decode(UINT32_C(0x10000001), NULL));
}

static void test_next_skips_reserved_zero(void) {
    CHECK(noah_qmk_via_sync_generation_next(0u) == 1u);
    CHECK(noah_qmk_via_sync_generation_next(1u) == 2u);
    CHECK(noah_qmk_via_sync_generation_next(NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK) == 1u);
}

static void test_serial_order_handles_wrap_and_ambiguity(void) {
    CHECK(noah_qmk_via_sync_generation_compare(10u, 10u) == NOAH_QMK_VIA_SYNC_SERIAL_EQUAL);
    CHECK(noah_qmk_via_sync_generation_compare(11u, 10u) == NOAH_QMK_VIA_SYNC_SERIAL_NEWER);
    CHECK(noah_qmk_via_sync_generation_compare(10u, 11u) == NOAH_QMK_VIA_SYNC_SERIAL_OLDER);
    CHECK(noah_qmk_via_sync_generation_compare(1u, NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK) == NOAH_QMK_VIA_SYNC_SERIAL_NEWER);
    CHECK(noah_qmk_via_sync_generation_compare(NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK, 1u) == NOAH_QMK_VIA_SYNC_SERIAL_OLDER);
    CHECK(noah_qmk_via_sync_generation_compare(NOAH_QMK_VIA_SYNC_METADATA_SERIAL_HALF + 1u, 1u) == NOAH_QMK_VIA_SYNC_SERIAL_AMBIGUOUS);
    CHECK(noah_qmk_via_sync_generation_compare(0u, 1u) == NOAH_QMK_VIA_SYNC_SERIAL_AMBIGUOUS);
}

int main(void) {
    test_clean_and_dirty_round_trip();
    test_invalid_schema_and_generation_are_rejected();
    test_next_skips_reserved_zero();
    test_serial_order_handles_wrap_and_ambiguity();

    puts("qmk_via_sync_metadata host tests passed");
    return 0;
}
