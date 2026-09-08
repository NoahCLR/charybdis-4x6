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
static uint8_t bytes[344];
static void    defaults(void) {
    const uint32_t values[28] = {200, 150, 400, 150, 1, 4, 1200, 25, 1, 3, 100, 0, 0, 400, 400, 200, 400, 900000, 1200, 200, 1, 257, 0xc8ff00, 1, 0, 200, 10, 0x76543210};
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = 1;
    bytes[1] = 8;
    bytes[2] = 28;
    bytes[3] = 16;
    for (uint8_t i = 0; i < 28; i++)
        for (uint8_t j = 0; j < 4; j++)
            bytes[8 + i * 4 + j] = values[i] >> (j * 8);
}
static bool valid(void) {
    noah_profile_settings_v1_validation_t state = {0};
    for (size_t i = 0; i < sizeof(bytes); i++)
        if (!noah_profile_settings_v1_consume(&state, bytes[i], sizeof(bytes), 8)) return false;
    return noah_profile_settings_v1_complete(&state, sizeof(bytes));
}
int main(void) {
    defaults();
    assert(valid());
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
    view.reader                            = noah_profile_reader_from_memory(bytes, sizeof(bytes));
    view.profile.domain_mask               = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS;
    view.profile.settings                  = (noah_profile_settings_v1_view_t){0, sizeof(bytes)};
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
    puts("portable settings validation and publication tests passed");
}
