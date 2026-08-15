#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/macro/macro_slot_provider.h"

_Static_assert(sizeof(macro_slot_metadata_t) == 1u, "logical macro slots must store metadata only");

static uint8_t                      source_byte;
static uint8_t                      load_call_count;
static bool                         load_succeeds;
static macro_payload_start_result_t start_result;
static bool                         engine_active;
static macro_payload_finish_fn      captured_finish;
static void                        *captured_context;
static const macro_payload_ir_t     *captured_ir;

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
    load_call_count++;
    if (!load_succeeds) {
        return false;
    }
    ir->length   = 3u;
    ir->bytes[0] = MACRO_PAYLOAD_IR_OP_TEXT;
    ir->bytes[1] = 1u;
    ir->bytes[2] = source_byte;
    return true;
}

macro_payload_start_result_t macro_payload_start_ir(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source, uint8_t slot, macro_payload_finish_fn finish, void *context) {
    if (engine_active) {
        return MACRO_PAYLOAD_START_BUSY;
    }
    CHECK(ir != NULL);
    CHECK(text_output == MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED);
    CHECK(interval == 7u);
    CHECK(source == MACRO_PAYLOAD_SOURCE_VIA);
    CHECK(slot == 0u);
    captured_finish  = finish;
    captured_context = context;
    captured_ir      = ir;
    if (start_result == MACRO_PAYLOAD_START_STARTED) {
        engine_active = true;
    }
    return start_result;
}

static void test_finish_active(void) {
    macro_payload_finish_fn finish  = captured_finish;
    void                   *context = captured_context;

    CHECK(engine_active);
    CHECK(finish != NULL);
    engine_active    = false;
    captured_finish  = NULL;
    captured_context = NULL;
    finish(MACRO_PAYLOAD_FINISH_SUCCESS, context);
}

static void test_active_invalidation_and_busy_start_preserve_shared_ir(void) {
    const macro_slot_provider_t provider = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       first[1] = {0};
    macro_slot_metadata_t       second[1] = {0};

    source_byte      = 'A';
    load_call_count  = 0u;
    load_succeeds    = true;
    start_result     = MACRO_PAYLOAD_START_STARTED;
    engine_active    = false;
    captured_finish  = NULL;
    captured_context = NULL;
    captured_ir      = NULL;

    CHECK(macro_slot_provider_start(&provider, first, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_VIA) == MACRO_PAYLOAD_START_STARTED);
    CHECK(first[0].state == MACRO_SLOT_CACHE_VALID);
    CHECK(load_call_count == 1u);
    CHECK(captured_ir != NULL);
    CHECK(captured_ir->bytes[2] == 'A');

    source_byte = 'B';
    macro_slot_provider_invalidate(&provider, first, 0u);
    CHECK(first[0].state == MACRO_SLOT_CACHE_VALID);
    CHECK(macro_slot_provider_start(&provider, second, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_VIA) == MACRO_PAYLOAD_START_BUSY);
    CHECK(second[0].state == MACRO_SLOT_CACHE_UNCHECKED);
    CHECK(load_call_count == 1u);
    CHECK(captured_ir->bytes[2] == 'A');

    test_finish_active();
    CHECK(first[0].state == MACRO_SLOT_CACHE_UNCHECKED);

    CHECK(macro_slot_provider_start(&provider, first, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_VIA) == MACRO_PAYLOAD_START_STARTED);
    CHECK(load_call_count == 2u);
    CHECK(captured_ir->bytes[2] == 'B');
    test_finish_active();
}

static void test_engine_busy_start_keeps_valid_metadata_without_pinning(void) {
    const macro_slot_provider_t provider = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       metadata[1] = {0};

    source_byte     = 'A';
    load_call_count = 0u;
    load_succeeds   = true;
    start_result    = MACRO_PAYLOAD_START_BUSY;
    engine_active   = false;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_VIA) == MACRO_PAYLOAD_START_BUSY);
    CHECK(metadata[0].state == MACRO_SLOT_CACHE_VALID);
    CHECK(load_call_count == 1u);
}

static void test_invalid_result_is_cached_until_invalidation(void) {
    const macro_slot_provider_t provider = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       metadata[1] = {0};

    load_call_count = 0u;
    load_succeeds   = false;
    CHECK(!macro_slot_provider_validate(&provider, metadata, 0u));
    CHECK(metadata[0].state == MACRO_SLOT_CACHE_INVALID);
    CHECK(load_call_count == 1u);
    CHECK(!macro_slot_provider_validate(&provider, metadata, 0u));
    CHECK(load_call_count == 1u);

    macro_slot_provider_invalidate(&provider, metadata, 0u);
    load_succeeds = true;
    CHECK(macro_slot_provider_validate(&provider, metadata, 0u));
    CHECK(metadata[0].state == MACRO_SLOT_CACHE_VALID);
    CHECK(load_call_count == 2u);
}

int main(void) {
    test_active_invalidation_and_busy_start_preserve_shared_ir();
    test_engine_busy_start_keeps_valid_metadata_without_pinning();
    test_invalid_result_is_cached_until_invalidation();
    puts("macro_slot_provider host tests passed");
    return 0;
}
