#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "users/noah/lib/profile/schema/profile_settings_v1.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"
#include "profile_test_blob.h"
static unsigned applied;
void            noah_qmk_portable_apply(void) {
    applied++;
}
static uint8_t  bytes[NOAH_SETTINGS_V5_MAX_SIZE + 1];
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
// A legacy domain carries 16 empty user-macro records; v3 carries 64 empty
// VIA macro names in their place, and v5 64 empty custom-key names after them.
static void defaults_as(uint8_t version) {
    const uint32_t values[28] = {200, 150, 400, 150, 1, 4, 1200, 25, 1, 3, 100, 0, 0, 400, 400, 200, 400, 900000, 1200, 200, 1, 257, 0xc8ff00, 1, 0, 200, 10, 0x76543210};
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = version;
    bytes[1] = 8;
    bytes[2] = 28;
    bytes[3] = version >= 3 ? 64 : 16;
    bytes[4] = version >= 5 ? NOAH_SETTINGS_CUSTOM_KEY_NAMES : 0;
    length   = version >= 5 ? NOAH_SETTINGS_FIXED_SIZE + 128 : version >= 3 ? NOAH_SETTINGS_FIXED_SIZE + 64 : NOAH_SETTINGS_FIXED_SIZE + 32;
    for (uint8_t i = 0; i < 28; i++)
        for (uint8_t j = 0; j < 4; j++)
            bytes[8 + i * 4 + j] = values[i] >> (j * 8);
    memset(bytes + 8 + NOAH_SETTING_DRAGSCROLL_DPI * 4, 0, 5 * 4);
}
static void defaults(void) {
    defaults_as(5);
}
static bool valid(void) {
    noah_profile_settings_v1_validation_t state = {0};
    for (size_t i = 0; i < length; i++)
        if (!noah_profile_settings_v1_consume(&state, bytes[i], length, 8)) return false;
    return noah_profile_settings_v1_complete(&state, length);
}
// Every name (64, or 128 with v5's custom keys) at `size` bytes of `fill`.
static void names_of(uint8_t version, uint8_t size, char fill) {
    defaults_as(version);
    length = NOAH_SETTINGS_FIXED_SIZE;
    for (uint8_t slot = 0; slot < (version >= 5 ? 128 : 64); slot++) {
        bytes[length++] = size;
        memset(bytes + length, fill, size);
        length += size;
    }
}
static void test_rejects_retired_versions(void) {
    for (uint8_t version = 0; version <= 6; version++) {
        defaults_as(version);
        assert(valid() == (version == NOAH_SETTINGS_VERSION));
    }
}
static void test_macro_name_limits(void) {
    names_of(5, 20, 'A');
    assert(valid());
    bytes[NOAH_SETTINGS_FIXED_SIZE + 1] = '~';
    bytes[NOAH_SETTINGS_FIXED_SIZE + 2] = ' ';
    assert(valid());
    const uint8_t rejected[] = {0, 0x1f, 0x7f, 0xc3, 0xff};
    for (size_t i = 0; i < sizeof(rejected); i++) {
        bytes[NOAH_SETTINGS_FIXED_SIZE + 1] = rejected[i];
        assert(!valid());
    }
    names_of(5, 20, 'A');
    bytes[NOAH_SETTINGS_FIXED_SIZE] = 21;
    assert(!valid());
    defaults_as(5);
    length--;
    assert(!valid());
}
// v5 adds 64 custom-key names after the macro names, each under the v4 rule,
// so the domain's worst case is 128 names of 20 characters.
static void test_custom_key_names_v5(void) {
    defaults_as(5);
    assert(valid());
    bytes[4] = 0; // v5 names its custom-key count
    assert(!valid());
    defaults_as(4);
    bytes[4] = NOAH_SETTINGS_CUSTOM_KEY_NAMES; // v4 has none
    assert(!valid());
    defaults_as(5);
    length = NOAH_SETTINGS_FIXED_SIZE + 64; // the macro names alone
    assert(!valid());
    names_of(5, 20, 'A');
    assert(length == NOAH_SETTINGS_V5_MAX_SIZE && length == 3000);
    assert(valid());
    uint16_t custom = NOAH_SETTINGS_FIXED_SIZE + 64 * 21; // the first custom-key record
    bytes[custom + 1] = 0xc3;
    assert(!valid());
    bytes[custom + 1] = 0x07;
    assert(!valid());
    names_of(5, 20, 'A');
    bytes[custom] = 21; // one name past 20
    assert(!valid());
    // A v5 domain publishes and reads back whole.
    defaults_as(5);
    bytes[NOAH_SETTINGS_FIXED_SIZE + 64] = 2;
    memmove(bytes + NOAH_SETTINGS_FIXED_SIZE + 67, bytes + NOAH_SETTINGS_FIXED_SIZE + 65, 63);
    bytes[NOAH_SETTINGS_FIXED_SIZE + 65] = 'O';
    bytes[NOAH_SETTINGS_FIXED_SIZE + 66] = 'K';
    length += 2;
    assert(valid());
    noah_effective_profile_snapshot_t view = settings_view();
    noah_effective_settings_invalidate(NULL, 4, view.identity, view.identity, &view);
    assert(noah_effective_settings_length() == length);
    assert(noah_effective_settings_byte(NOAH_SETTINGS_FIXED_SIZE + 64) == 2 && noah_effective_settings_byte(NOAH_SETTINGS_FIXED_SIZE + 66) == 'K');
}
int main(void) {
    noah_effective_settings_set_apply(noah_qmk_portable_apply);
    defaults();
    assert(valid());
    bytes[0] = NOAH_SETTINGS_VERSION == 1 ? 2 : 1;
    assert(!valid());
    defaults();
    for (uint8_t id = NOAH_SETTING_DRAGSCROLL_DPI; id <= NOAH_SETTING_ARROW_DPI; id++) {
        bytes[8 + id * 4] = 1;
        assert(!valid());
        bytes[8 + id * 4] = 0;
    }
    bytes[8 + 4 * 4] = 2;
    assert(!valid());
    defaults();
    bytes[143] = 1;
    assert(!valid());
    defaults();
    bytes[120] = 0xed;
    bytes[121] = 0xa0;
    bytes[122] = 0x80;
    assert(!valid());
    defaults();
    bytes[120] = 0xc3;
    bytes[121] = 0xa9;
    assert(valid());
    defaults();
    bytes[312] = 1;
    assert(!valid());
    defaults();
    assert(!noah_profile_setting_v1_valid(NOAH_SETTING_DEFAULT_DPI, 401, 8));
    assert(!noah_profile_setting_v1_valid(NOAH_SETTING_AUTO_MOUSE_LAYER, 8, 8));
    noah_effective_profile_snapshot_t view = settings_view();
    noah_effective_settings_invalidate(NULL, 1, view.identity, view.identity, &view);
    assert(noah_setting(NOAH_SETTING_TAPPING_TERM, 99) == 200);
    // The whole stored domain stays readable, retired user macros included.
    assert(applied == 1 && noah_effective_settings_length() == length);
    noah_effective_profile_snapshot_t compiled = view;
    compiled.identity.kind = NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS;
    noah_effective_profile_snapshot_t partial = {0};
    partial.identity.kind = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE;
    noah_effective_settings_invalidate(&compiled, 1, partial.identity, partial.identity, &partial);
    assert(applied == 1 && noah_effective_settings_length() == length && noah_setting(NOAH_SETTING_TAPPING_TERM, 0) == 200);
    noah_effective_settings_invalidate(&compiled, 1, compiled.identity, compiled.identity, &compiled);
    assert(applied == 1 && noah_effective_settings_length() == length);
    for (uint16_t i = 0; i < length; i++)
        assert(noah_effective_settings_byte(i) == bytes[i]);
    assert(noah_effective_settings_byte(length) == 0);
    // Missing domain returns ownership to defaults.
    view.profile.domain_mask = 0;
    noah_effective_settings_invalidate(NULL, 2, view.identity, view.identity, &view);
    assert(noah_setting(NOAH_SETTING_TAPPING_TERM, 99) == 99);
    assert(!noah_effective_settings_length() && !noah_effective_settings_byte(0));
    assert(applied == 1);
    test_rejects_retired_versions();
    test_macro_name_limits();
    test_custom_key_names_v5();
    // Current names read back as stored.
    defaults_as(5);
    bytes[NOAH_SETTINGS_FIXED_SIZE + 5]  = 2;
    bytes[NOAH_SETTINGS_FIXED_SIZE + 6]  = 'O';
    bytes[NOAH_SETTINGS_FIXED_SIZE + 7]  = 'K';
    length                              += 2;
    view = settings_view();
    noah_effective_settings_invalidate(NULL, 3, view.identity, view.identity, &view);
    assert(noah_setting(NOAH_SETTING_TAPPING_TERM, 99) == 200);
    assert(noah_effective_settings_length() == length);
    assert(noah_effective_settings_byte(NOAH_SETTINGS_FIXED_SIZE + 5) == 2 && noah_effective_settings_byte(NOAH_SETTINGS_FIXED_SIZE + 7) == 'K');
    puts("portable settings validation and publication tests passed");
}
