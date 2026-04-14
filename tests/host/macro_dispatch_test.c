#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/macro/macro_dispatch.h"
#include "users/noah/lib/macro/macro_payload.h"
#include "users/noah/noah_keymap_ids.h"

static uint8_t compile_call_count[HARDCODED_MACRO_SLOT_COUNT];
static uint8_t play_call_count;
static uint8_t last_play_program_id;
static bool    fail_compile_for_slot_1;
static bool    fail_playback;

const char *const hardcoded_macro_payloads[HARDCODED_MACRO_SLOT_COUNT] = {
    [0] = "A{KC_C}", [1] = "{KC_NOT_A_KEY}", [2] = "", [3] = "Z", [4] = "B", [5] = "{KC_BAD}", [6] = "C",
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

static void test_reset_state(void) {
    memset(compile_call_count, 0, sizeof(compile_call_count));
    play_call_count         = 0;
    last_play_program_id    = 0;
    fail_compile_for_slot_1 = true;
    fail_playback           = false;
}

static int test_payload_slot(const char *payload) {
    for (uint8_t slot = 0; slot < HARDCODED_MACRO_SLOT_COUNT; slot++) {
        if (hardcoded_macro_payloads[slot] == payload) {
            return slot;
        }
    }

    return -1;
}

bool macro_payload_compile(const char *payload, macro_payload_ir_t *ir) {
    int slot = test_payload_slot(payload);

    CHECK(slot >= 0);
    CHECK(ir != NULL);
    compile_call_count[slot]++;

    ir->length = 0;
    if ((slot == 1 || slot == 5) && fail_compile_for_slot_1) {
        return false;
    }

    ir->length   = 1;
    ir->bytes[0] = (uint8_t)(slot + 1);
    return true;
}

bool macro_payload_play_ir(const macro_payload_ir_t *ir) {
    CHECK(ir != NULL);

    play_call_count++;
    last_play_program_id = ir->length ? ir->bytes[0] : 0;
    return !fail_playback;
}

bool macro_payload_play_ir_with_text_output(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval) {
    CHECK(text_output == MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN);
    CHECK(interval == 0);
    return macro_payload_play_ir(ir);
}

static void test_dispatch_compiles_valid_slot_once_and_reuses_ir(void) {
    test_reset_state();

    CHECK(macro_dispatch(MACRO_4));
    CHECK(compile_call_count[4] == 1);
    CHECK(play_call_count == 1);
    CHECK(last_play_program_id == 5);

    CHECK(macro_dispatch(MACRO_4));
    CHECK(compile_call_count[4] == 1);
    CHECK(play_call_count == 2);
    CHECK(last_play_program_id == 5);
}

static void test_dispatch_caches_invalid_compile_result(void) {
    test_reset_state();

    CHECK(macro_dispatch(MACRO_5));
    CHECK(compile_call_count[5] == 1);
    CHECK(play_call_count == 0);

    CHECK(macro_dispatch(MACRO_5));
    CHECK(compile_call_count[5] == 1);
    CHECK(play_call_count == 0);
}

static void test_dispatch_skips_empty_payload_slots(void) {
    test_reset_state();

    CHECK(macro_dispatch(MACRO_2));
    CHECK(compile_call_count[2] == 0);
    CHECK(play_call_count == 0);
}

static void test_validate_all_compiles_each_non_empty_slot_once(void) {
    test_reset_state();

    macro_dispatch_validate_all();
    CHECK(compile_call_count[0] == 1);
    CHECK(compile_call_count[1] == 1);
    CHECK(compile_call_count[2] == 0);
    CHECK(compile_call_count[3] == 1);
    CHECK(compile_call_count[4] == 0);
    CHECK(compile_call_count[5] == 0);
    CHECK(compile_call_count[6] == 0);
    CHECK(play_call_count == 0);

    macro_dispatch_validate_all();
    CHECK(compile_call_count[0] == 1);
    CHECK(compile_call_count[1] == 1);
    CHECK(compile_call_count[3] == 1);
}

static void test_playback_failure_invalidates_cached_ir(void) {
    test_reset_state();
    fail_playback = true;

    CHECK(macro_dispatch(MACRO_6));
    CHECK(compile_call_count[6] == 1);
    CHECK(play_call_count == 1);

    fail_playback = false;
    CHECK(macro_dispatch(MACRO_6));
    CHECK(compile_call_count[6] == 1);
    CHECK(play_call_count == 1);
}

static void test_non_macro_keycodes_are_not_handled(void) {
    test_reset_state();

    CHECK(!macro_dispatch(0));
    CHECK(play_call_count == 0);
}

int main(void) {
    test_dispatch_compiles_valid_slot_once_and_reuses_ir();
    test_dispatch_caches_invalid_compile_result();
    test_dispatch_skips_empty_payload_slots();
    test_playback_failure_invalidates_cached_ir();
    test_validate_all_compiles_each_non_empty_slot_once();
    test_non_macro_keycodes_are_not_handled();

    puts("macro_dispatch host tests passed");
    return 0;
}
