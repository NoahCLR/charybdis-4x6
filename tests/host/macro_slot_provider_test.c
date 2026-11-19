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
static const macro_payload_ir_t    *captured_ir;
static macro_payload_load_window_fn  captured_load_next;
static uint8_t                      window_count = 1u;
static uint8_t                      preflight_window_calls;
static uint8_t                      preflight_fail_window = UINT8_MAX;
static uint8_t                      cancel_calls;

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

// Window n types source_byte + n; the cursor offset counts windows.
static bool test_load_ir(uint8_t slot, macro_payload_ir_t *ir, macro_payload_stream_cursor_t *cursor, void *context) {
    (void)context;
    CHECK(slot == 0u);
    CHECK(ir != NULL);
    CHECK(cursor != NULL);
    load_call_count++;
    if (!load_succeeds) {
        return false;
    }
    ir->length   = 3u;
    ir->bytes[0] = MACRO_PAYLOAD_IR_OP_TEXT;
    ir->bytes[1] = 1u;
    ir->bytes[2] = (uint8_t)(source_byte + cursor->offset);
    ir->more     = cursor->offset + 1u < window_count;
    if (ir->more) {
        cursor->offset++;
    }
    return true;
}

void macro_payload_preflight_begin(macro_payload_preflight_t *preflight) {
    *preflight = (macro_payload_preflight_t){0};
}

bool macro_payload_preflight_window(macro_payload_preflight_t *preflight, const macro_payload_ir_t *ir) {
    CHECK(preflight != NULL);
    CHECK(ir != NULL);
    return preflight_window_calls++ != preflight_fail_window;
}

bool macro_payload_preflight_end(const macro_payload_preflight_t *preflight) {
    return preflight != NULL;
}

bool macro_payload_engine_idle(void) {
    return !engine_active;
}

bool macro_payload_engine_cancel(void) {
    cancel_calls++;
    return engine_active;
}

macro_payload_start_result_t macro_payload_start_ir(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source, uint8_t slot, macro_payload_finish_fn finish, void *context) {
    if (engine_active) {
        return MACRO_PAYLOAD_START_BUSY;
    }
    CHECK(ir != NULL);
    CHECK(text_output == MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED);
    CHECK(interval == (source == MACRO_PAYLOAD_SOURCE_CORRECTION ? 5u : 7u));
    CHECK(source == MACRO_PAYLOAD_SOURCE_SLOT || source == MACRO_PAYLOAD_SOURCE_CORRECTION);
    CHECK(slot == 0u);
    captured_finish  = finish;
    captured_context = context;
    captured_ir      = ir;
    if (start_result == MACRO_PAYLOAD_START_STARTED) {
        engine_active = true;
    }
    return start_result;
}

macro_payload_start_result_t macro_payload_start_windows(const macro_payload_ir_t *ir, const macro_payload_preflight_t *preflight, macro_payload_load_window_fn load_next, void *load_context, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source, uint8_t slot, macro_payload_finish_fn finish, void *context) {
    (void)load_context;
    CHECK(preflight != NULL);
    CHECK((load_next != NULL) == (source == MACRO_PAYLOAD_SOURCE_SLOT));
    captured_load_next = load_next;
    return macro_payload_start_ir(ir, text_output, interval, source, slot, finish, context);
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
    const macro_slot_provider_t provider  = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       first[1]  = {0};
    macro_slot_metadata_t       second[1] = {0};

    source_byte      = 'A';
    load_call_count  = 0u;
    load_succeeds    = true;
    start_result     = MACRO_PAYLOAD_START_STARTED;
    engine_active    = false;
    captured_finish  = NULL;
    captured_context = NULL;
    captured_ir      = NULL;

    CHECK(macro_slot_provider_start(&provider, first, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    CHECK(first[0].state == MACRO_SLOT_CACHE_VALID);
    CHECK(load_call_count == 1u);
    CHECK(captured_ir != NULL);
    CHECK(captured_ir->bytes[2] == 'A');

    source_byte = 'B';
    macro_slot_provider_invalidate(&provider, first, 0u);
    CHECK(first[0].state == MACRO_SLOT_CACHE_VALID);
    CHECK(macro_slot_provider_start(&provider, second, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_BUSY);
    CHECK(second[0].state == MACRO_SLOT_CACHE_UNCHECKED);
    CHECK(load_call_count == 1u);
    CHECK(captured_ir->bytes[2] == 'A');

    test_finish_active();
    CHECK(first[0].state == MACRO_SLOT_CACHE_UNCHECKED);

    CHECK(macro_slot_provider_start(&provider, first, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    CHECK(load_call_count == 2u);
    CHECK(captured_ir->bytes[2] == 'B');
    test_finish_active();
}

static void test_engine_busy_start_keeps_valid_metadata_without_pinning(void) {
    const macro_slot_provider_t provider    = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       metadata[1] = {0};

    source_byte     = 'A';
    load_call_count = 0u;
    load_succeeds   = true;
    start_result    = MACRO_PAYLOAD_START_BUSY;
    engine_active   = false;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_BUSY);
    CHECK(metadata[0].state == MACRO_SLOT_CACHE_VALID);
    CHECK(load_call_count == 1u);
}

static void test_playback_preflight_rejection_is_retried(void) {
    const macro_slot_provider_t provider = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t metadata[1] = {0};
    source_byte = 'A'; load_succeeds = true; load_call_count = 0; engine_active = false;
    start_result = MACRO_PAYLOAD_START_INVALID;
    CHECK(macro_slot_provider_start(&provider, metadata, 0, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_INVALID);
    CHECK(metadata[0].state == MACRO_SLOT_CACHE_VALID);
    start_result = MACRO_PAYLOAD_START_STARTED;
    CHECK(macro_slot_provider_start(&provider, metadata, 0, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    CHECK(load_call_count == 2);
    test_finish_active();
}

static void test_reset(void) {
    source_byte            = 'A';
    load_call_count        = 0u;
    load_succeeds          = true;
    start_result           = MACRO_PAYLOAD_START_STARTED;
    engine_active          = false;
    captured_finish        = NULL;
    captured_context       = NULL;
    captured_ir            = NULL;
    captured_load_next     = NULL;
    window_count           = 1u;
    preflight_window_calls = 0u;
    preflight_fail_window  = UINT8_MAX;
    cancel_calls           = 0u;
}

static void test_invalid_result_is_cached_until_invalidation(void) {
    const macro_slot_provider_t provider    = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       metadata[1] = {0};

    test_reset();
    load_succeeds = false;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_INVALID);
    CHECK(metadata[0].state == MACRO_SLOT_CACHE_INVALID);
    CHECK(load_call_count == 1u);
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_INVALID);
    CHECK(load_call_count == 1u);
    CHECK(captured_ir == NULL);

    macro_slot_provider_invalidate(&provider, metadata, 0u);
    load_succeeds = true;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    CHECK(metadata[0].state == MACRO_SLOT_CACHE_VALID);
    CHECK(load_call_count == 2u);
    test_finish_active();
}

static void test_long_macro_checks_every_window_then_plays_from_the_first(void) {
    const macro_slot_provider_t provider    = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       metadata[1] = {0};

    test_reset();
    window_count = 3u;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    CHECK(preflight_window_calls == 3u);
    CHECK(load_call_count == 4u);
    CHECK(metadata[0].state == MACRO_SLOT_CACHE_VALID);
    CHECK(captured_ir->bytes[2] == 'A' && captured_ir->more);

    CHECK(captured_load_next(NULL));
    CHECK(captured_ir->bytes[2] == 'B' && captured_ir->more);
    CHECK(captured_load_next(NULL));
    CHECK(captured_ir->bytes[2] == 'C' && !captured_ir->more);
    test_finish_active();
}

static void test_rejected_later_window_types_nothing(void) {
    const macro_slot_provider_t provider    = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       metadata[1] = {0};

    test_reset();
    window_count          = 3u;
    preflight_fail_window = 2u;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_INVALID);
    CHECK(captured_ir == NULL);
    CHECK(metadata[0].state != MACRO_SLOT_CACHE_INVALID);

    // Playback checks depend on host settings, so the next press checks again.
    preflight_fail_window  = UINT8_MAX;
    preflight_window_calls = 0u;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    test_finish_active();
}

static void test_storage_change_cancels_any_active_stored_macro(void) {
    const macro_slot_provider_t provider    = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       metadata[1] = {0};

    test_reset();
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    macro_slot_provider_storage_changing();
    CHECK(cancel_calls == 1u);
    test_finish_active();

    test_reset();
    window_count = 2u;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    macro_slot_provider_storage_changing();
    CHECK(cancel_calls == 1u);
    // Cleanup may still scan before finish; it must never read the new bytes.
    CHECK(!captured_load_next(NULL));
    test_finish_active();
    CHECK(metadata[0].state == MACRO_SLOT_CACHE_UNCHECKED);

    // Deferred invalidation reaching a macro still reading storage also stops it.
    test_reset();
    window_count = 2u;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    macro_slot_provider_invalidate_all(&provider, metadata);
    CHECK(cancel_calls == 1u);
    test_finish_active();
}

static uint8_t                       correction_finishes;
static macro_payload_finish_result_t correction_result;

static void test_correction_finished(macro_payload_finish_result_t result, void *context) {
    (void)context;
    correction_finishes++;
    correction_result = result;
}

// A correction plays from the shared window while no macro holds it, and a
// macro finds the window busy until the correction ends.
static void test_a_correction_borrows_the_shared_window(void) {
    static const uint16_t       characters[] = {'h', 0x00E9u, 'e', 'y'};
    static const uint8_t        program[]    = {MACRO_PAYLOAD_IR_OP_ERASE, 2u, MACRO_PAYLOAD_IR_OP_TEXT, 1u, 'h', MACRO_PAYLOAD_IR_OP_UNICODE, 0xE9u, 0u, 0u, MACRO_PAYLOAD_IR_OP_TEXT, 2u, 'e', 'y'};
    const macro_slot_provider_t provider     = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       metadata[1]  = {0};

    test_reset();
    correction_finishes = 0u;
    // Nothing to erase or type is nothing to play.
    CHECK(macro_slot_provider_start_correction(0u, characters, 0u, 5u, test_correction_finished) == MACRO_PAYLOAD_START_INVALID);
    CHECK(macro_slot_provider_start_correction(2u, characters, 4u, 5u, NULL) == MACRO_PAYLOAD_START_INVALID);

    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    CHECK(macro_slot_provider_start_correction(2u, characters, 4u, 5u, test_correction_finished) == MACRO_PAYLOAD_START_BUSY);
    CHECK(captured_ir->bytes[0] == MACRO_PAYLOAD_IR_OP_TEXT);
    test_finish_active();

    CHECK(macro_slot_provider_start_correction(2u, characters, 4u, 5u, test_correction_finished) == MACRO_PAYLOAD_START_STARTED);
    CHECK(captured_ir->protection == 1u && !captured_ir->more && captured_ir->length == sizeof(program));
    for (uint8_t index = 0u; index < sizeof(program); index++)
        CHECK(captured_ir->bytes[index] == program[index]);
    load_call_count = 0u;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_BUSY);
    CHECK(load_call_count == 0u && captured_ir->bytes[0] == MACRO_PAYLOAD_IR_OP_ERASE);
    test_finish_active();
    CHECK(correction_finishes == 1u && correction_result == MACRO_PAYLOAD_FINISH_SUCCESS);

    // A character the layout cannot type is refused by preflight: nothing
    // starts and the window is free again.
    preflight_window_calls = 0u;
    preflight_fail_window  = 0u;
    CHECK(macro_slot_provider_start_correction(2u, characters, 4u, 5u, test_correction_finished) == MACRO_PAYLOAD_START_INVALID);
    preflight_fail_window = UINT8_MAX;
    CHECK(macro_slot_provider_start(&provider, metadata, 0u, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 7u, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED);
    test_finish_active();
    CHECK(correction_finishes == 1u);
}

// A correction that plays a macro asks first whether the macro would start
// (D-F69): nothing is sent, the window is left free, and the erase that
// follows has nothing to type.
static void test_can_start_checks_without_playing(void) {
    static const uint8_t        erase[]     = {MACRO_PAYLOAD_IR_OP_ERASE, 3u};
    const macro_slot_provider_t provider    = {.slot_count = 1u, .load_ir = test_load_ir};
    macro_slot_metadata_t       metadata[1] = {0};

    test_reset();
    CHECK(macro_slot_provider_can_start(&provider, metadata, 0u));
    CHECK(!engine_active && load_call_count == 1u);
    // An erase alone borrows the window the check left free.
    CHECK(macro_slot_provider_start_correction(3u, NULL, 0u, 5u, test_correction_finished) == MACRO_PAYLOAD_START_STARTED);
    CHECK(captured_ir->length == sizeof(erase) && captured_ir->bytes[0] == erase[0] && captured_ir->bytes[1] == erase[1]);
    // Nothing can start while it plays.
    CHECK(!macro_slot_provider_can_start(&provider, metadata, 0u));
    test_finish_active();

    // A slot that does not decode, one past the slots, and one preflight
    // refuses cannot start.
    load_succeeds = false;
    macro_slot_provider_invalidate(&provider, metadata, 0u);
    CHECK(!macro_slot_provider_can_start(&provider, metadata, 0u));
    load_succeeds = true;
    macro_slot_provider_invalidate(&provider, metadata, 0u);
    CHECK(!macro_slot_provider_can_start(&provider, metadata, 1u));
    preflight_window_calls = 0u;
    preflight_fail_window  = 0u;
    CHECK(!macro_slot_provider_can_start(&provider, metadata, 0u));
    preflight_fail_window = UINT8_MAX;
    CHECK(macro_slot_provider_can_start(&provider, metadata, 0u));
}

int main(void) {
    test_active_invalidation_and_busy_start_preserve_shared_ir();
    test_engine_busy_start_keeps_valid_metadata_without_pinning();
    test_invalid_result_is_cached_until_invalidation();
    test_playback_preflight_rejection_is_retried();
    test_long_macro_checks_every_window_then_plays_from_the_first();
    test_rejected_later_window_types_nothing();
    test_storage_change_cancels_any_active_stored_macro();
    test_a_correction_borrows_the_shared_window();
    test_can_start_checks_without_playing();
    puts("macro_slot_provider host tests passed");
    return 0;
}
