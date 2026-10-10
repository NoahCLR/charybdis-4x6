#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "users/noah/lib/compat/qmk_host.h"
#include "users/noah/lib/profile/schema/profile_settings_v1.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"
#include "profile_test_blob.h"
static unsigned applied;
void            noah_qmk_portable_apply(void) {
    applied++;
}
// The runtime falls back to compiled defaults before any settings are live;
// this test publishes its own domain, so the defaults are never consulted.
uint8_t noah_profile_settings_defaults_byte(uint16_t offset) {
    (void)offset;
    return 0xA5u;
}
enum { POSITIONS = 60 };
static uint8_t  bytes[NOAH_SETTINGS_MAX_SIZE + 2];
static uint16_t length;
static uint8_t  blob[NOAH_PROFILE_TEST_BLOB_PAYLOAD_OFFSET + sizeof(bytes)];
// The current bytes as a validated profile's settings domain.
static noah_effective_profile_snapshot_t settings_view(void) {
    noah_effective_profile_snapshot_t view  = {0};
    size_t                            total = noah_profile_test_blob_wrap(blob, sizeof(blob), NOAH_PROFILE_DOMAIN_V1_SETTINGS, bytes, length);
    assert(total);
    view.reader               = noah_profile_reader_from_memory(blob, total);
    view.profile.domain_mask  = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS;
    view.profile.domain_count = 1;
    view.profile.byte_length  = (uint16_t)total;
    return view;
}
static void set_scalar(uint8_t id, uint32_t value) {
    for (uint8_t j = 0; j < 4; j++)
        bytes[NOAH_SETTINGS_SCALARS_OFFSET + id * 4 + j] = (uint8_t)(value >> (j * 8));
}
static uint16_t layer_record(uint8_t layer, uint8_t field) {
    return (uint16_t)(NOAH_SETTINGS_LAYER_RECORDS_OFFSET + layer * NOAH_SETTINGS_LAYER_RECORD_SIZE + field);
}
// A canonical domain: every name `size` bytes of `fill`.
static void named(uint8_t size, char fill) {
    const uint32_t values[NOAH_SETTINGS_COUNT] = {200, 150, 400, 150, 1, 4, 1200, 25, 1, 3, 0, 0, 0, 0, 0, 200, 400, 900000, 1200, 200, 1, 257, 0xc8ff00, 1, 0, 200, 10, 0, 1, 0xffff, 0xffff};
    memset(bytes, 0, sizeof(bytes));
    const uint8_t header[NOAH_SETTINGS_HEADER_SIZE] = {6, NOAH_SETTINGS_LAYERS, NOAH_SETTINGS_COUNT, NOAH_SETTINGS_MACRO_NAMES, NOAH_SETTINGS_CUSTOM_KEY_NAMES, 0, 0, 0};
    memcpy(bytes, header, sizeof(header));
    for (uint8_t i = 0; i < NOAH_SETTINGS_COUNT; i++)
        set_scalar(i, values[i]);
    for (uint8_t layer = 0; layer < NOAH_SETTINGS_LAYERS; layer++)
        bytes[layer_record(layer, NOAH_SETTINGS_LAYER_REFERENCE)] = layer;
    length = NOAH_SETTINGS_FIXED_SIZE;
    for (uint16_t name = 0; name < NOAH_SETTINGS_NAME_COUNT; name++) {
        bytes[length++] = size;
        memset(bytes + length, fill, size);
        length += size;
    }
}
static void defaults(void) {
    named(0, 0);
}
static bool valid_with(uint8_t layers, uint8_t positions) {
    noah_profile_settings_v1_validation_t state = {0};
    for (size_t i = 0; i < length; i++)
        if (!noah_profile_settings_v1_consume(&state, bytes[i], length, layers, positions)) return false;
    return noah_profile_settings_v1_complete(&state, length);
}
static bool valid(void) {
    return valid_with(NOAH_SETTINGS_LAYERS, POSITIONS);
}
// The first byte of the name record `index` in a domain of empty names.
static uint16_t empty_name(uint16_t index) {
    return (uint16_t)(NOAH_SETTINGS_FIXED_SIZE + index);
}
// Replaces empty name `index` with `text` of `size` bytes.
static void put_name(uint16_t index, const char *text, uint8_t size) {
    uint16_t at = empty_name(index);
    memmove(bytes + at + 1 + size, bytes + at + 1, length - at - 1);
    bytes[at] = size;
    memcpy(bytes + at + 1, text, size);
    length += size;
}
static void test_header_and_versions(void) {
    defaults();
    assert(valid());
    for (uint8_t version = 0; version <= 8; version++) {
        defaults();
        bytes[0] = version;
        assert(valid() == (version == NOAH_SETTINGS_VERSION));
    }
    assert(NOAH_SETTINGS_VERSION == 6);
    for (uint8_t at = 1; at < NOAH_SETTINGS_HEADER_SIZE; at++) {
        defaults();
        bytes[at]++;
        assert(!valid());
    }
    // The bank is the firmware's sixteen layers.
    defaults();
    assert(!valid_with(8, POSITIONS));
}
static void test_scalars(void) {
    defaults();
    for (uint8_t id = NOAH_SETTING_DRAGSCROLL_DPI; id <= NOAH_SETTING_ARROW_DPI; id++) {
        set_scalar(id, 1);
        assert(!valid());
        set_scalar(id, 0);
    }
    // The v5 combo reference scalar is retired.
    set_scalar(NOAH_SETTING_RETIRED_COMBO_REFERENCES, 0x76543210u);
    assert(!valid());
    defaults();
    set_scalar(NOAH_SETTING_BEHAVIORS_ENABLED, 0);
    assert(valid());
    set_scalar(NOAH_SETTING_BEHAVIORS_ENABLED, 2);
    assert(!valid());
    defaults();
    // Layer masks name layers 0..15 only; an empty mask is allowed.
    for (uint8_t id = NOAH_SETTING_LAYER_BEHAVIORS; id <= NOAH_SETTING_LAYER_COMBOS; id++) {
        set_scalar(id, 0);
        assert(valid());
        set_scalar(id, 0x8000);
        assert(valid());
        set_scalar(id, 0x10000);
        assert(!valid());
        set_scalar(id, 0x80000000u);
        assert(!valid());
        set_scalar(id, 0xffff);
    }
    // The startup layers reach layer 15 and must name one.
    set_scalar(NOAH_SETTING_DEFAULT_LAYERS, 0x8000);
    assert(valid());
    set_scalar(NOAH_SETTING_DEFAULT_LAYERS, 0);
    assert(!valid());
    set_scalar(NOAH_SETTING_DEFAULT_LAYERS, 0x10000);
    assert(!valid());
    defaults();
    set_scalar(NOAH_SETTING_AUTO_MOUSE_LAYER, 15);
    assert(valid());
    set_scalar(NOAH_SETTING_AUTO_MOUSE_LAYER, 16);
    assert(!valid());
    defaults();
    set_scalar(NOAH_SETTING_AUTO_MOUSE_ENABLED, 2);
    assert(!valid());
    assert(!noah_profile_setting_v1_valid(NOAH_SETTING_DEFAULT_DPI, 401, 16));
    assert(!noah_profile_setting_v1_valid(NOAH_SETTING_AUTO_SNIPING_LAYER, 16, 16));
    assert(noah_profile_setting_v1_valid(NOAH_SETTING_AUTO_SNIPING_LAYER, 15, 16));
}
static void test_layer_records(void) {
    defaults();
    bytes[layer_record(15, NOAH_SETTINGS_LAYER_REFERENCE)] = 0;
    assert(valid());
    bytes[layer_record(0, NOAH_SETTINGS_LAYER_REFERENCE)] = 15;
    assert(valid());
    bytes[layer_record(0, NOAH_SETTINGS_LAYER_REFERENCE)] = 16;
    assert(!valid());
    // Positions 0..59 exist on this 10 x 6 matrix; bits 60..63 are zero.
    for (uint8_t field = NOAH_SETTINGS_LAYER_BYPASS; field <= NOAH_SETTINGS_LAYER_EXCLUDE; field += NOAH_SETTINGS_PLACEMENT_BYTES) {
        defaults();
        bytes[layer_record(15, field)] = 1;
        bytes[layer_record(15, field + 7)] = 0x08; // position 59
        assert(valid());
        bytes[layer_record(15, field + 7)] = 0x10; // position 60
        assert(!valid());
        assert(valid_with(NOAH_SETTINGS_LAYERS, 64));
        bytes[layer_record(15, field + 7)] = 0x80; // position 63
        assert(!valid() && valid_with(NOAH_SETTINGS_LAYERS, 64));
    }
    defaults();
    assert(!valid_with(NOAH_SETTINGS_LAYERS, 65));
}
static void test_names(void) {
    // 32 bytes is the limit for every name, layers, macros and custom keys.
    named(NOAH_SETTINGS_NAME_MAX, 'A');
    assert(length == NOAH_SETTINGS_MAX_SIZE);
    assert(valid());
    named(NOAH_SETTINGS_NAME_MAX, 'A');
    bytes[NOAH_SETTINGS_FIXED_SIZE] = NOAH_SETTINGS_NAME_MAX + 1;
    assert(!valid());
    // Multibyte text is UTF-8 counted in bytes.
    static const char *const accepted[] = {"\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80", "~ !", "\xef\xbf\xbd"};
    static const char *const refused[]  = {"\x00", "\x1f", "\x7f", "\xc3", "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xff", "\xe2\x82", "\x80"};
    for (uint16_t index = 0; index < NOAH_SETTINGS_NAME_COUNT; index += NOAH_SETTINGS_NAME_COUNT - 1) {
        for (size_t i = 0; i < sizeof(accepted) / sizeof(accepted[0]); i++) {
            defaults();
            put_name(index, accepted[i], (uint8_t)strlen(accepted[i]));
            assert(valid());
        }
        for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
            defaults();
            put_name(index, refused[i], (uint8_t)(refused[i][0] ? strlen(refused[i]) : 1));
            assert(!valid());
        }
    }
    // A sequence cut by its length is refused.
    defaults();
    put_name(3, "ab\xe2\x82\xac", 4);
    assert(!valid());
    // A missing or extra name is refused.
    defaults();
    length--;
    assert(!valid());
    defaults();
    bytes[length++] = 0;
    assert(!valid());
    // 272 names (D-F14): entry 255 is past any byte counter, entry 271 the
    // last; a name in either is read in order.
    assert(NOAH_SETTINGS_NAME_COUNT == 272);
    defaults();
    put_name(271, "Last custom", 11);
    put_name(255, "Two-five-five", 13);
    assert(valid());
    defaults();
    put_name(255, "\xff", 1);
    assert(!valid());
    // The last of the names closes the domain.
    defaults();
    put_name(NOAH_SETTINGS_NAME_COUNT - 1, "End", 3);
    assert(valid());
    length--;
    assert(!valid());
}
static void test_publication(void) {
    defaults();
    put_name(NOAH_SETTINGS_LAYERS + 5, "\xc3\x89t\xc3\xa9", 5);
    put_name(0, "Base", 4);
    bytes[layer_record(9, NOAH_SETTINGS_LAYER_REFERENCE)] = 2;
    bytes[layer_record(9, NOAH_SETTINGS_LAYER_BYPASS + 7)] = 0x08;
    assert(valid());
    noah_effective_profile_snapshot_t view = settings_view();
    noah_effective_settings_invalidate(NULL, 1, view.identity, view.identity, &view);
    assert(applied == 1 && noah_effective_settings_length() == length);
    assert(noah_setting(NOAH_SETTING_TAPPING_TERM, 99) == 200);
    assert(noah_setting(NOAH_SETTING_LAYER_COMBOS, 0) == 0xffff);
    assert(noah_setting_layer_record(9, NOAH_SETTINGS_LAYER_REFERENCE) == 2);
    assert(noah_setting_layer_record(9, NOAH_SETTINGS_LAYER_BYPASS + 7) == 0x08);
    assert(noah_setting_layer_record(15, NOAH_SETTINGS_LAYER_REFERENCE) == 15);
    assert(noah_setting_layer_record(16, NOAH_SETTINGS_LAYER_REFERENCE) == 0);
    for (uint16_t i = 0; i < length; i++)
        assert(noah_effective_settings_byte(i) == bytes[i]);
    assert(noah_effective_settings_byte(length) == 0);
    // A profile without settings keeps the compiled snapshot's cache.
    noah_effective_profile_snapshot_t compiled = view;
    compiled.identity.kind = NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS;
    noah_effective_profile_snapshot_t partial = {0};
    partial.identity.kind = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE;
    noah_effective_settings_invalidate(&compiled, 2, partial.identity, partial.identity, &partial);
    assert(applied == 1 && noah_effective_settings_length() == length && noah_setting(NOAH_SETTING_TAPPING_TERM, 0) == 200);
    // A missing domain returns ownership to defaults.
    view.profile.domain_mask = 0;
    noah_effective_settings_invalidate(NULL, 3, view.identity, view.identity, &view);
    assert(noah_setting(NOAH_SETTING_TAPPING_TERM, 99) == 99);
    assert(!noah_effective_settings_length() && !noah_effective_settings_byte(0));
    assert(noah_setting_layer_record(9, NOAH_SETTINGS_LAYER_REFERENCE) == 0xA5u);
    // The largest domain publishes whole.
    named(NOAH_SETTINGS_NAME_MAX, 'z');
    view = settings_view();
    noah_effective_settings_invalidate(NULL, 4, view.identity, view.identity, &view);
    assert(noah_effective_settings_length() == NOAH_SETTINGS_MAX_SIZE && noah_effective_settings_byte(NOAH_SETTINGS_MAX_SIZE - 1) == 'z');
}
int main(void) {
    noah_effective_settings_set_apply(noah_qmk_portable_apply);
    test_header_and_versions();
    test_scalars();
    for (uint8_t selected = 0; selected <= 3; selected++) {
        assert(noah_profile_setting_v1_valid(27, selected, 16));
        assert(noah_profile_setting_v1_valid(27, selected | NOAH_HOST_UNICODE_ENABLED, 16));
        for (uint8_t detected = 0; detected <= 4; detected++)
            assert(noah_host_effective(selected, detected) == (selected ? selected : detected <= 3 ? detected : 0));
    }
    assert(!noah_profile_setting_v1_valid(27, 4, 16));
    assert(!noah_profile_setting_v1_valid(27, 0x200, 16));
    // Bits 16..23 name a host layout and bit 24 marks a macOS ISO keyboard.
    for (uint32_t layout = 0; layout < NOAH_HOST_LAYOUT_LIMIT; layout++)
        assert(noah_profile_setting_v1_valid(27, NOAH_HOST_MACOS | NOAH_HOST_UNICODE_ENABLED | NOAH_HOST_MACOS_ISO | (layout << NOAH_HOST_LAYOUT_SHIFT), 16));
    assert(!noah_profile_setting_v1_valid(27, (uint32_t)NOAH_HOST_LAYOUT_LIMIT << NOAH_HOST_LAYOUT_SHIFT, 16));
    assert(!noah_profile_setting_v1_valid(27, UINT32_C(0xFF) << NOAH_HOST_LAYOUT_SHIFT, 16));
    assert(!noah_profile_setting_v1_valid(27, UINT32_C(0x02000000), 16));
    assert(!noah_profile_setting_v1_valid(27, UINT32_C(0x00008000), 16));
    assert(noah_host_layout_id(UINT32_C(0x01090103)) == 9u);
    test_layer_records();
    test_names();
    test_publication();
    puts("portable settings validation and publication tests passed");
}
