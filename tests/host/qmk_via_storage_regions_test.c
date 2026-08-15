#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "via.h"
#include "users/noah/lib/compat/qmk_via_storage_regions.h"

enum {
    TEST_KEYMAP_SIZE = 41,
    TEST_MACRO_SIZE  = 37,
};

static uint8_t  keymap_bytes[TEST_KEYMAP_SIZE];
static uint8_t  macro_bytes[TEST_MACRO_SIZE];
static bool     via_valid;
static uint32_t layout_options;
static uint8_t  max_read_length;
static uint8_t  valid_write_count;
static bool     first_valid_write;
#ifdef ENCODER_MAP_ENABLE
static uint16_t encoder_keycodes[DYNAMIC_KEYMAP_LAYER_COUNT][NUM_ENCODERS][2];
#endif

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

uint16_t noah_qmk_via_keymap_buffer_capacity(void) {
    return sizeof(keymap_bytes);
}

uint16_t noah_qmk_via_macro_seed_capacity(void) {
    return sizeof(macro_bytes);
}

void noah_qmk_via_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    CHECK((uint32_t)offset + size <= sizeof(macro_bytes));
    memcpy(&macro_bytes[offset], data, size);
}

void dynamic_keymap_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    CHECK((uint32_t)offset + size <= sizeof(keymap_bytes));
    if (size > max_read_length) {
        max_read_length = (uint8_t)size;
    }
    memcpy(data, &keymap_bytes[offset], size);
}

void dynamic_keymap_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    CHECK((uint32_t)offset + size <= sizeof(keymap_bytes));
    memcpy(&keymap_bytes[offset], data, size);
}

void dynamic_keymap_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    CHECK((uint32_t)offset + size <= sizeof(macro_bytes));
    if (size > max_read_length) {
        max_read_length = (uint8_t)size;
    }
    memcpy(data, &macro_bytes[offset], size);
}

bool via_eeprom_is_valid(void) {
    return via_valid;
}

void via_eeprom_set_valid(bool valid) {
    if (valid_write_count == 0u) {
        first_valid_write = valid;
    }
    valid_write_count++;
    via_valid = valid;
}

uint32_t via_get_layout_options(void) {
    return layout_options;
}

void via_set_layout_options(uint32_t value) {
    layout_options = value;
}

#ifdef ENCODER_MAP_ENABLE
uint16_t dynamic_keymap_get_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise) {
    return encoder_keycodes[layer][encoder_id][clockwise ? 0 : 1];
}

void dynamic_keymap_set_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode) {
    encoder_keycodes[layer][encoder_id][clockwise ? 0 : 1] = keycode;
}
#endif

static void test_reset(void) {
    for (uint8_t index = 0u; index < sizeof(keymap_bytes); index++) {
        keymap_bytes[index] = (uint8_t)(index * 3u + 1u);
    }
    for (uint8_t index = 0u; index < sizeof(macro_bytes); index++) {
        macro_bytes[index] = (uint8_t)(index * 5u + 2u);
    }
    via_valid         = true;
    layout_options    = UINT32_C(0x12345678);
    max_read_length   = 0u;
    valid_write_count = 0u;
    first_valid_write = true;
#ifdef ENCODER_MAP_ENABLE
    for (uint8_t layer = 0u; layer < DYNAMIC_KEYMAP_LAYER_COUNT; layer++) {
        for (uint8_t encoder = 0u; encoder < NUM_ENCODERS; encoder++) {
            encoder_keycodes[layer][encoder][0] = (uint16_t)(0x1000u + layer * 0x100u + encoder * 2u);
            encoder_keycodes[layer][encoder][1] = (uint16_t)(0x2000u + layer * 0x100u + encoder * 2u);
        }
    }
#endif
}

static void test_region_sizes_and_bounds(void) {
    uint8_t byte = 0u;

    test_reset();
    CHECK(noah_qmk_via_storage_region_size(NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG) == 1u + VIA_EEPROM_LAYOUT_OPTIONS_SIZE);
    CHECK(noah_qmk_via_storage_region_size(NOAH_QMK_VIA_SYNC_REGION_KEYMAP) == sizeof(keymap_bytes));
    CHECK(noah_qmk_via_storage_region_size(NOAH_QMK_VIA_SYNC_REGION_MACRO) == sizeof(macro_bytes));
    CHECK(noah_qmk_via_storage_region_size(NOAH_QMK_VIA_SYNC_REGION_NONE) == 0u);
    CHECK(!noah_qmk_via_storage_region_read(NOAH_QMK_VIA_SYNC_REGION_KEYMAP, sizeof(keymap_bytes), &byte, 1u));
    CHECK(!noah_qmk_via_storage_region_write(NOAH_QMK_VIA_SYNC_REGION_MACRO, sizeof(macro_bytes), &byte, 1u));
    CHECK(!noah_qmk_via_storage_region_read(NOAH_QMK_VIA_SYNC_REGION_KEYMAP, 0u, NULL, 1u));
    CHECK(noah_qmk_via_storage_region_read(NOAH_QMK_VIA_SYNC_REGION_KEYMAP, sizeof(keymap_bytes), NULL, 0u));
}

static void test_config_is_canonical_and_commits_validity_last(void) {
    uint8_t bytes[1u + VIA_EEPROM_LAYOUT_OPTIONS_SIZE] = {0};

    test_reset();
    CHECK(noah_qmk_via_storage_region_read(NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG, 0u, bytes, sizeof(bytes)));
    CHECK(bytes[0] == 1u);
    CHECK(bytes[1] == 0x78u);

    bytes[0] = 1u;
    bytes[1] = 0xA5u;
    CHECK(noah_qmk_via_storage_region_write(NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG, 0u, bytes, sizeof(bytes)));
    CHECK(valid_write_count == 2u);
    CHECK(!first_valid_write);
    CHECK(via_valid);
    CHECK((layout_options & 0xFFu) == 0xA5u);
    CHECK(!noah_qmk_via_storage_region_write(NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG, 1u, bytes, sizeof(bytes) - 1u));
    bytes[0] = 2u;
    CHECK(!noah_qmk_via_storage_region_write(NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG, 0u, bytes, sizeof(bytes)));
}

static void test_keymap_and_macro_round_trip(void) {
    uint8_t replacement[]                 = {9u, 8u, 7u, 6u};
    uint8_t readback[sizeof(replacement)] = {0};

    test_reset();
    CHECK(noah_qmk_via_storage_region_write(NOAH_QMK_VIA_SYNC_REGION_KEYMAP, 5u, replacement, sizeof(replacement)));
    CHECK(noah_qmk_via_storage_region_read(NOAH_QMK_VIA_SYNC_REGION_KEYMAP, 5u, readback, sizeof(readback)));
    CHECK(memcmp(replacement, readback, sizeof(replacement)) == 0);

    memset(readback, 0, sizeof(readback));
    CHECK(noah_qmk_via_storage_region_write(NOAH_QMK_VIA_SYNC_REGION_MACRO, 7u, replacement, sizeof(replacement)));
    CHECK(noah_qmk_via_storage_region_read(NOAH_QMK_VIA_SYNC_REGION_MACRO, 7u, readback, sizeof(readback)));
    CHECK(memcmp(replacement, readback, sizeof(replacement)) == 0);
}

#ifdef ENCODER_MAP_ENABLE
static void test_encoder_region_preserves_big_endian_partial_writes(void) {
    uint8_t bytes[4] = {0};
    uint8_t low      = 0xBCu;

    test_reset();
    CHECK(noah_qmk_via_storage_region_size(NOAH_QMK_VIA_SYNC_REGION_ENCODER) == DYNAMIC_KEYMAP_LAYER_COUNT * NUM_ENCODERS * 4u);
    CHECK(noah_qmk_via_storage_region_read(NOAH_QMK_VIA_SYNC_REGION_ENCODER, 0u, bytes, sizeof(bytes)));
    CHECK(bytes[0] == 0x10u);
    CHECK(bytes[1] == 0x00u);
    CHECK(bytes[2] == 0x20u);
    CHECK(bytes[3] == 0x00u);
    CHECK(noah_qmk_via_storage_region_write(NOAH_QMK_VIA_SYNC_REGION_ENCODER, 1u, &low, 1u));
    CHECK(encoder_keycodes[0][0][0] == 0x10BCu);
}
#else
static void test_encoder_region_is_empty_when_disabled(void) {
    test_reset();
    CHECK(noah_qmk_via_storage_region_size(NOAH_QMK_VIA_SYNC_REGION_ENCODER) == 0u);
    CHECK(noah_qmk_via_storage_region_read(NOAH_QMK_VIA_SYNC_REGION_ENCODER, 0u, NULL, 0u));
}
#endif

static uint32_t finish_digest(uint8_t budget) {
    noah_qmk_via_storage_digest_cursor_t cursor;
    uint32_t                             digest = 0u;
    uint16_t                             steps  = 0u;

    noah_qmk_via_storage_digest_init(&cursor);
    while (!cursor.complete) {
        CHECK(noah_qmk_via_storage_digest_step(&cursor, budget, &digest));
        CHECK(++steps < 1000u);
    }
    return digest;
}

static void test_digest_is_incremental_deterministic_and_content_sensitive(void) {
    uint32_t first;
    uint32_t second;

    test_reset();
    first = finish_digest(7u);
    CHECK(max_read_length <= 7u);
    max_read_length = 0u;
    second          = finish_digest(3u);
    CHECK(first == second);
    CHECK(max_read_length <= 3u);

    keymap_bytes[11] ^= 0x80u;
    CHECK(finish_digest(7u) != first);
    keymap_bytes[11] ^= 0x80u;
    macro_bytes[5] ^= 0x40u;
    CHECK(finish_digest(7u) != first);
}

int main(void) {
    test_region_sizes_and_bounds();
    test_config_is_canonical_and_commits_validity_last();
    test_keymap_and_macro_round_trip();
#ifdef ENCODER_MAP_ENABLE
    test_encoder_region_preserves_big_endian_partial_writes();
#else
    test_encoder_region_is_empty_when_disabled();
#endif
    test_digest_is_incremental_deterministic_and_content_sensitive();

    puts("qmk_via_storage_regions host tests passed");
    return 0;
}
