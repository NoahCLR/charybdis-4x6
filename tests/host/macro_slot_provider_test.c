#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/macro/macro_slot_provider.h"

static uint8_t                      source_byte;
static macro_payload_start_result_t start_result;
static macro_payload_finish_fn      captured_finish;
static void                        *captured_context;

bool macro_payload_encode_ir_write(const macro_payload_ir_t *ir, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written) {
    (void)ir;
    (void)write_byte;
    (void)context;
    if (written) {
        *written = 0u;
    }
    return false;
}

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

static bool test_load_ir(uint8_t slot, macro_payload_ir_t *ir, void *context) {
    (void)context;
    CHECK(slot == 0u);
    CHECK(ir != NULL);
    ir->length   = 3u;
    ir->bytes[0] = MACRO_PAYLOAD_IR_OP_TEXT;
    ir->bytes[1] = 1u;
    ir->bytes[2] = source_byte;
    return true;
}

macro_payload_start_result_t macro_payload_start_ir(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source, uint8_t slot, macro_payload_finish_fn finish, void *context) {
    CHECK(ir != NULL);
    CHECK(text_output == MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED);
    CHECK(interval == 7u);
    CHECK(source == MACRO_PAYLOAD_SOURCE_VIA);
    CHECK(slot == 0u);
    captured_finish  = finish;
    captured_context = context;
    return start_result;
}

static void test_pinned_invalidation_defers_byte_reuse(void) {
    const macro_slot_provider_t provider = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_cache_t          cache[1] = {0};

    source_byte      = 'A';
    start_result     = MACRO_PAYLOAD_START_STARTED;
    captured_finish  = NULL;
    captured_context = NULL;

    CHECK(macro_slot_provider_start(&provider, cache, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_VIA) == MACRO_PAYLOAD_START_STARTED);
    CHECK(cache[0].pinned);
    CHECK(cache[0].ir.bytes[2] == 'A');

    source_byte = 'B';
    macro_slot_provider_invalidate(&provider, cache, 0u);
    CHECK(cache[0].pinned);
    CHECK(cache[0].stale);
    CHECK(cache[0].ir.bytes[2] == 'A');

    CHECK(captured_finish != NULL);
    captured_finish(MACRO_PAYLOAD_FINISH_SUCCESS, captured_context);
    CHECK(!cache[0].pinned);
    CHECK(!cache[0].stale);
    CHECK(cache[0].state == MACRO_SLOT_CACHE_UNCHECKED);
    CHECK(cache[0].ir.length == 0u);

    CHECK(macro_slot_provider_load(&provider, cache, 0u));
    CHECK(cache[0].ir.bytes[2] == 'B');
}

static void test_busy_start_does_not_pin_slot(void) {
    const macro_slot_provider_t provider = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_cache_t          cache[1] = {0};

    source_byte  = 'A';
    start_result = MACRO_PAYLOAD_START_BUSY;
    CHECK(macro_slot_provider_start(&provider, cache, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_VIA) == MACRO_PAYLOAD_START_BUSY);
    CHECK(!cache[0].pinned);
    CHECK(cache[0].state == MACRO_SLOT_CACHE_VALID);
}

int main(void) {
    test_pinned_invalidation_defers_byte_reuse();
    test_busy_start_does_not_pin_slot();
    puts("macro_slot_provider host tests passed");
    return 0;
}
