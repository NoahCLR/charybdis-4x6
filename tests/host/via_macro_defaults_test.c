#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/macro/macro_slot_provider.h"
#include "users/noah/lib/macro/macro_payload.h"
#include "users/noah/lib/macro/via_macro_defaults.h"
#include "users/noah/noah_keymap_ids.h"
#include "via.h"

void via_init_kb(void);
bool via_command_kb(uint8_t *data, uint8_t length);

enum {
    TEST_MACRO_BUFFER_CAPACITY = 64,
};

static uint8_t macro_buffer[TEST_MACRO_BUFFER_CAPACITY];
static bool    fake_via_eeprom_valid;
static uint8_t rgb_invalidate_count;
static uint8_t dynamic_keymap_set_buffer_calls;
static uint8_t macro_payload_encode_write_calls;

const char *const via_macro_payloads[VIA_MACRO_SLOT_COUNT] = {
    [0] = "AB",
    [1] = "{KC_C}",
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
    memset(macro_buffer, 0, sizeof(macro_buffer));
    fake_via_eeprom_valid           = true;
    rgb_invalidate_count            = 0;
    dynamic_keymap_set_buffer_calls = 0;
    macro_payload_encode_write_calls = 0;
}

uint16_t dynamic_keymap_macro_get_buffer_size(void) {
    return TEST_MACRO_BUFFER_CAPACITY;
}

uint8_t noah_qmk_via_macro_count(void) {
    return 0;
}

uint16_t noah_qmk_via_macro_buffer_size(void) {
    return 0;
}

void noah_qmk_via_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    (void)offset;
    memset(data, 0, size);
}

void dynamic_keymap_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    CHECK((uint32_t)offset + (uint32_t)size <= sizeof(macro_buffer));
    memcpy(&macro_buffer[offset], data, size);
    dynamic_keymap_set_buffer_calls++;
}

bool via_eeprom_is_valid(void) {
    return fake_via_eeprom_valid;
}

bool macro_payload_validate(const char *payload) {
    return payload != NULL;
}

bool macro_payload_play_ir_with_text_output(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval) {
    (void)ir;
    (void)text_output;
    (void)interval;
    return true;
}

bool macro_payload_encode_write(const char *payload, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written) {
    uint16_t count = 0;

    macro_payload_encode_write_calls++;

    if (written) {
        *written = 0;
    }

    if (!payload || !write_byte) {
        return false;
    }

    while (*payload) {
        if (!write_byte((uint8_t)*payload++, context)) {
            return false;
        }
        count++;
    }

    if (written) {
        *written = count;
    }

    return true;
}

typedef struct {
    uint16_t offset;
    uint8_t  bytes[TEST_MACRO_BUFFER_CAPACITY];
} test_provider_sink_t;

static uint8_t test_provider_load_ir_calls;
static uint8_t test_provider_lookup_calls;

static bool test_provider_load_ir(uint8_t slot, macro_payload_ir_t *ir, void *context) {
    (void)context;

    test_provider_load_ir_calls++;
    if (!ir || slot != 0) {
        return false;
    }

    ir->length = 0;
    return true;
}

static bool test_provider_lookup_payload(uint8_t slot, const char **payload, void *context) {
    (void)context;

    test_provider_lookup_calls++;
    if (!payload || slot != 0) {
        return false;
    }

    *payload = "AB";
    return true;
}

static bool test_provider_sink_write_byte(uint8_t byte, void *context) {
    test_provider_sink_t *sink = (test_provider_sink_t *)context;

    CHECK(sink && sink->offset < sizeof(sink->bytes));
    sink->bytes[sink->offset++] = byte;
    return true;
}

void noah_rgb_runtime_invalidate_layer_maps(void) {
    rgb_invalidate_count++;
}

static void test_post_init_seeds_defaults_when_via_eeprom_is_invalid(void) {
    test_reset_state();
    fake_via_eeprom_valid = false;

    via_init_kb();
    noah_via_macro_defaults_keyboard_post_init();

    CHECK(dynamic_keymap_set_buffer_calls > 0);
    CHECK(macro_buffer[0] == 'A');
    CHECK(macro_buffer[1] == 'B');
    CHECK(macro_buffer[2] == 0);
    CHECK(macro_buffer[3] == '{');
}

static void test_eeprom_init_seeds_defaults_immediately(void) {
    test_reset_state();

    noah_via_macro_defaults_eeconfig_init();

    CHECK(dynamic_keymap_set_buffer_calls > 0);
    CHECK(macro_buffer[0] == 'A');
    CHECK(macro_buffer[1] == 'B');
    CHECK(macro_buffer[2] == 0);
}

static void test_macro_reset_command_defers_reseed_to_matrix_scan(void) {
    uint8_t cmd[] = {id_dynamic_keymap_macro_reset};

    test_reset_state();

    CHECK(!via_command_kb(cmd, (uint8_t)sizeof(cmd)));
    CHECK(dynamic_keymap_set_buffer_calls == 0);

    noah_via_macro_defaults_matrix_scan();

    CHECK(dynamic_keymap_set_buffer_calls > 0);
    CHECK(rgb_invalidate_count == 0);
}

static void test_keymap_reset_commands_invalidate_rgb(void) {
    uint8_t set_keycode_cmd[] = {id_dynamic_keymap_set_keycode};
    uint8_t reset_cmd[]       = {id_dynamic_keymap_reset};

    test_reset_state();

    CHECK(!via_command_kb(set_keycode_cmd, (uint8_t)sizeof(set_keycode_cmd)));
    CHECK(!via_command_kb(reset_cmd, (uint8_t)sizeof(reset_cmd)));
    CHECK(rgb_invalidate_count == 2);
    CHECK(dynamic_keymap_set_buffer_calls == 0);
}

static void test_eeprom_reset_invalidates_rgb_and_reseeds_on_scan(void) {
    uint8_t cmd[] = {id_eeprom_reset};

    test_reset_state();

    CHECK(!via_command_kb(cmd, (uint8_t)sizeof(cmd)));
    CHECK(rgb_invalidate_count == 1);
    CHECK(dynamic_keymap_set_buffer_calls == 0);

    noah_via_macro_defaults_matrix_scan();

    CHECK(dynamic_keymap_set_buffer_calls > 0);
}

static void test_provider_encode_write_uses_cached_load_state(void) {
    macro_slot_cache_t           cache[1]  = {0};
    macro_slot_provider_t        provider  = {
        .slot_count     = 1,
        .load_ir        = test_provider_load_ir,
        .lookup_payload = test_provider_lookup_payload,
    };
    test_provider_sink_t         sink      = {0};
    uint16_t                     written   = 0;

    test_provider_load_ir_calls = 0;
    test_provider_lookup_calls  = 0;
    macro_payload_encode_write_calls = 0;

    CHECK(macro_slot_provider_encode_write(&provider, cache, 0, test_provider_sink_write_byte, &sink, &written));
    CHECK(test_provider_load_ir_calls == 1);
    CHECK(test_provider_lookup_calls == 1);
    CHECK(macro_payload_encode_write_calls == 1);
    CHECK(written == 2);
    CHECK(sink.offset == 2);
    CHECK(sink.bytes[0] == 'A');
    CHECK(sink.bytes[1] == 'B');

    CHECK(macro_slot_provider_encode_write(&provider, cache, 0, test_provider_sink_write_byte, &sink, NULL));
    CHECK(test_provider_load_ir_calls == 1);
    CHECK(test_provider_lookup_calls == 2);
    CHECK(macro_payload_encode_write_calls == 2);
}

int main(void) {
    test_post_init_seeds_defaults_when_via_eeprom_is_invalid();
    test_eeprom_init_seeds_defaults_immediately();
    test_macro_reset_command_defers_reseed_to_matrix_scan();
    test_keymap_reset_commands_invalidate_rgb();
    test_eeprom_reset_invalidates_rgb_and_reseeds_on_scan();
    test_provider_encode_write_uses_cached_load_state();

    puts("via_macro_defaults host tests passed");
    return 0;
}
