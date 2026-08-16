#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "via.h"
#include "users/noah/lib/compat/qmk_via_storage_contract.h"

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

uint16_t dynamic_keymap_macro_get_buffer_size(void) {
    return 128u;
}

void dynamic_keymap_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    (void)offset;
    (void)size;
    (void)data;
}

bool via_eeprom_is_valid(void) {
    return true;
}

static void test_complete_mutation_table(void) {
    static const struct {
        uint8_t bytes[8];
        uint8_t length;
        uint8_t required_effects;
    } cases[] = {
        {{id_dynamic_keymap_set_keycode, 0, 0, 0, 0, 4}, 6u, NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR},         {{id_dynamic_keymap_set_buffer, 0, 0, 1, 0xA5}, 5u, NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR}, {{id_dynamic_keymap_reset}, 1u, NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR}, {{id_dynamic_keymap_macro_set_buffer, 0, 0, 1, 0xA5}, 5u, NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR | NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_MACROS}, {{id_dynamic_keymap_macro_reset}, 1u, NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR | NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_MACROS}, {{id_eeprom_reset}, 1u, NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR | NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_MACROS},
        {{id_set_keyboard_value, id_layout_options, 0, 0, 0, 1}, 6u, NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR},
    };

    for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        uint8_t effects = 0u;

        CHECK(noah_qmk_via_classify_mutation(cases[index].bytes, cases[index].length, &effects));
        CHECK((effects & cases[index].required_effects) == cases[index].required_effects);
    }
}

static void test_invalid_or_read_only_shapes_are_rejected(void) {
    uint8_t truncated_keycode[]  = {id_dynamic_keymap_set_keycode, 0, 0, 0, 0};
    uint8_t out_of_range_keycode[] = {id_dynamic_keymap_set_keycode, 0, MATRIX_ROWS, 0, 0, 4};
    uint8_t wrong_nested[]       = {id_set_keyboard_value, 0x7Fu, 0, 0, 0, 1};
    uint8_t read_only[]          = {0x01};
    uint8_t effects              = 0xFFu;

    CHECK(!noah_qmk_via_classify_mutation(truncated_keycode, sizeof(truncated_keycode), &effects));
    // Upstream rejects an out-of-range keycode write outright, so it really is
    // a non-mutation. Buffer writes are the ones that partially apply.
    CHECK(!noah_qmk_via_classify_mutation(out_of_range_keycode, sizeof(out_of_range_keycode), &effects));
    CHECK(!noah_qmk_via_classify_mutation(wrong_nested, sizeof(wrong_nested), &effects));
    CHECK(!noah_qmk_via_classify_mutation(read_only, sizeof(read_only), &effects));
    CHECK(!noah_qmk_via_classify_mutation(NULL, 0u, &effects));
}

// Upstream applies buffer writes byte by byte, keeping every byte whose offset
// lands inside the region and dropping the rest. These shapes were previously
// classified as non-mutations, which meant an over-long write changed storage
// while the advertised digest kept its pre-write value and the split halves
// diverged with no way to notice.
static void test_partially_applied_buffer_writes_are_mutations(void) {
    uint8_t overlong_keymap[] = {id_dynamic_keymap_set_buffer, 0, 0, 2, 0xA5};
    uint8_t straddling_macro[] = {id_dynamic_keymap_macro_set_buffer, 0, 127, 2, 0xA5, 0x5A};
    uint8_t effects            = 0u;

    // Claims more payload than the frame carries; upstream still writes at
    // offset 0, which is inside the region.
    CHECK(noah_qmk_via_classify_mutation(overlong_keymap, sizeof(overlong_keymap), &effects));
    CHECK((effects & NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR) != 0u);

    // Starts one byte before the end of the macro region, so the first byte
    // lands and the second is dropped.
    effects = 0u;
    CHECK(noah_qmk_via_classify_mutation(straddling_macro, sizeof(straddling_macro), &effects));
    CHECK((effects & NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR) != 0u);
    CHECK((effects & NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_MACROS) != 0u);
}

// A write starting past the region touches nothing, so it stays a non-mutation
// and does not cost a digest recompute.
static void test_buffer_writes_past_the_region_are_not_mutations(void) {
    uint8_t past_keymap[] = {id_dynamic_keymap_set_buffer, 0xFFu, 0xFFu, 1, 0xA5};
    uint8_t past_macro[]  = {id_dynamic_keymap_macro_set_buffer, 0xFFu, 0xFFu, 1, 0xA5};
    uint8_t effects       = 0xFFu;

    CHECK(!noah_qmk_via_classify_mutation(past_keymap, sizeof(past_keymap), &effects));
    CHECK(!noah_qmk_via_classify_mutation(past_macro, sizeof(past_macro), &effects));
}

int main(void) {
    test_complete_mutation_table();
    test_invalid_or_read_only_shapes_are_rejected();
    test_partially_applied_buffer_writes_are_mutations();
    test_buffer_writes_past_the_region_are_not_mutations();
    puts("qmk_via_command_classifier host tests passed");
    return 0;
}
