#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "users/noah/lib/profile/schema/profile_settings_v1.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"
static unsigned applied, invalidated;
void            noah_qmk_portable_apply(void) {
    applied++;
}
void macro_dispatch_invalidate(void) {
    invalidated++;
}
static uint8_t  bytes[NOAH_SETTINGS_FIXED_SIZE + 64 + 32];
static uint16_t length;
// A legacy domain carries 16 empty user-macro records; v3 carries 64 empty
// VIA macro names in their place.
static void defaults_as(uint8_t version) {
    const uint32_t values[28] = {200, 150, 400, 150, 1, 4, 1200, 25, 1, 3, 100, 0, 0, 400, 400, 200, 400, 900000, 1200, 200, 1, 257, 0xc8ff00, 1, 0, 200, 10, 0x76543210};
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = version;
    bytes[1] = 8;
    bytes[2] = 28;
    bytes[3] = version >= 3 ? 64 : 16;
    length   = version >= 3 ? NOAH_SETTINGS_FIXED_SIZE + 64 : NOAH_SETTINGS_FIXED_SIZE + 32;
    for (uint8_t i = 0; i < 28; i++)
        for (uint8_t j = 0; j < 4; j++)
            bytes[8 + i * 4 + j] = values[i] >> (j * 8);
#ifdef NOAH_PD_PROFILE_ENABLE
    memset(bytes + 8 + NOAH_SETTING_DRAGSCROLL_DPI * 4, 0, 5 * 4);
#endif
}
static void defaults(void) {
#ifdef NOAH_PD_PROFILE_ENABLE
    defaults_as(2);
#else
    defaults_as(1);
#endif
}
static bool valid_as(uint8_t envelope) {
    noah_profile_settings_v1_validation_t state = {0};
    state.expected_version                      = envelope;
    for (size_t i = 0; i < length; i++)
        if (!noah_profile_settings_v1_consume(&state, bytes[i], length, 8)) return false;
    return noah_profile_settings_v1_complete(&state, length);
}
static bool valid(void) {
    return valid_as(0);
}
#ifdef NOAH_PD_PROFILE_ENABLE
// v3: 64 length-prefixed names after the fixed part, each 0..23 bytes of
// printable UTF-8, in the same ceiling the user macros had.
static void test_macro_names(void) {
    static const char name[] = "Zoom mute";
    defaults_as(3);
    assert(valid() && valid_as(3));
    assert(!valid_as(2)); // the payload must repeat the envelope's version
    bytes[3] = 16;
    assert(!valid());
    defaults_as(3);
    bytes[NOAH_SETTINGS_FIXED_SIZE] = sizeof(name) - 1;
    memmove(bytes + NOAH_SETTINGS_FIXED_SIZE + 1 + sizeof(name) - 1, bytes + NOAH_SETTINGS_FIXED_SIZE + 1, 63);
    memcpy(bytes + NOAH_SETTINGS_FIXED_SIZE + 1, name, sizeof(name) - 1);
    length = NOAH_SETTINGS_FIXED_SIZE + 64 + sizeof(name) - 1;
    assert(valid());
    bytes[NOAH_SETTINGS_FIXED_SIZE + 2] = 0x07; // a control character
    assert(!valid());
    bytes[NOAH_SETTINGS_FIXED_SIZE + 2] = 0xc3; // truncated two-byte sequence ...
    bytes[NOAH_SETTINGS_FIXED_SIZE + 3] = 0x41; // ... followed by ASCII
    assert(!valid());
    defaults_as(3);
    bytes[NOAH_SETTINGS_FIXED_SIZE] = 24; // longer than 23 bytes
    assert(!valid());
    defaults_as(3);
    length--; // only 63 names
    assert(!valid());
}
#endif
int main(void) {
    defaults();
    assert(valid());
    bytes[0] = NOAH_SETTINGS_VERSION == 1 ? 2 : 1;
    assert(!valid());
    defaults();
#ifdef NOAH_PD_PROFILE_ENABLE
    for (uint8_t id = NOAH_SETTING_DRAGSCROLL_DPI; id <= NOAH_SETTING_ARROW_DPI; id++) {
        bytes[8 + id * 4] = 1;
        assert(!valid());
        bytes[8 + id * 4] = 0;
    }
#endif
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
    noah_effective_profile_snapshot_t view = {0};
    view.reader                            = noah_profile_reader_from_memory(bytes, length);
    view.profile.domain_mask               = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS;
    view.profile.settings                  = (noah_profile_settings_v1_view_t){0, length};
    noah_effective_settings_invalidate(NULL, 1, view.identity, view.identity, &view);
    assert(noah_setting(NOAH_SETTING_TAPPING_TERM, 99) == 200);
    macro_payload_ir_t ir;
    assert(noah_effective_settings_macro(15, &ir) && !ir.length);
    assert(applied == 1 && invalidated == 1);
    // Missing domain returns ownership to defaults; an explicitly empty bank
    // above overrides all sixteen compiled slots.
    view.profile.domain_mask = 0;
    noah_effective_settings_invalidate(NULL, 2, view.identity, view.identity, &view);
    assert(noah_setting(NOAH_SETTING_TAPPING_TERM, 99) == 99);
    assert(!noah_effective_settings_macro(0, &ir));
#ifdef NOAH_PD_PROFILE_ENABLE
    test_macro_names();
    // A v3 domain has no user macros: every slot reads as explicitly empty.
    defaults_as(3);
    view.reader              = noah_profile_reader_from_memory(bytes, length);
    view.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS;
    view.profile.settings    = (noah_profile_settings_v1_view_t){0, length};
    noah_effective_settings_invalidate(NULL, 3, view.identity, view.identity, &view);
    assert(noah_setting(NOAH_SETTING_TAPPING_TERM, 99) == 200);
    ir.length = 7;
    assert(noah_effective_settings_macro(3, &ir) && !ir.length);
#endif
    puts("portable settings validation and publication tests passed");
}
