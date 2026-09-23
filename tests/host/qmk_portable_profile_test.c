#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "portable_profile_keyboard.h"
#include "users/noah/lib/compat/qmk_portable_profile.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

// The QMK and Charybdis state the readback overlays, as live values that
// start where the stored settings below put them.
static bool     am_enabled = true, combos = true;
static uint8_t  am_layer = 4, am_debounce = 25;
static uint16_t am_timeout = 1200, default_dpi = 1200, sniping_dpi = 200;
uint32_t        default_layer_state = 1;
keymap_config_t keymap_config;
bool            get_auto_mouse_enable(void) {
    return am_enabled;
}
uint8_t get_auto_mouse_layer(void) {
    return am_layer;
}
uint16_t get_auto_mouse_timeout(void) {
    return am_timeout;
}
uint8_t get_auto_mouse_debounce(void) {
    return am_debounce;
}
void set_auto_mouse_enable(bool value) {
    am_enabled = value;
}
void set_auto_mouse_layer(uint8_t value) {
    am_layer = value;
}
void set_auto_mouse_timeout(uint16_t value) {
    am_timeout = value;
}
void set_auto_mouse_debounce(uint8_t value) {
    am_debounce = value;
}
bool is_combo_enabled(void) {
    return combos;
}
void combo_enable(void) {
    combos = true;
}
void combo_disable(void) {
    combos = false;
}
bool rgb_matrix_is_enabled(void) {
    return true;
}
uint8_t rgb_matrix_get_mode(void) {
    return 1;
}
uint8_t rgb_matrix_get_speed(void) {
    return 0;
}
uint8_t rgb_matrix_get_flags(void) {
    return 0;
}
uint16_t rgb_matrix_get_hue(void) {
    return 0;
}
uint8_t rgb_matrix_get_sat(void) {
    return 0xff;
}
uint8_t rgb_matrix_get_val(void) {
    return 0xc8;
}
uint16_t charybdis_get_pointer_default_dpi(void) {
    return default_dpi;
}
uint16_t charybdis_get_pointer_sniping_dpi(void) {
    return sniping_dpi;
}
void charybdis_cycle_pointer_default_dpi(bool forward) {
    (void)forward;
}
void charybdis_cycle_pointer_sniping_dpi(bool forward) {
    (void)forward;
}
void default_layer_set(uint32_t state) {
    default_layer_state = state;
}
void eeconfig_update_default_layer(uint8_t layers) {
    (void)layers;
}
void eeconfig_update_keymap(const keymap_config_t *config) {
    (void)config;
}
uint32_t eeconfig_read_user(void) {
    return 0;
}
void via_eeprom_set_valid(bool valid) {
    (void)valid;
}
uint8_t noah_qmk_portable_editor_page(uint8_t page, uint8_t *payload) {
    (void)page;
    (void)payload;
    return 0;
}
void noah_qmk_portable_apply_lighting(uint32_t mode, uint32_t color) {
    (void)mode;
    (void)color;
}

static uint8_t frame[32];
static void    get(uint8_t page) {
    memset(frame, 0, sizeof(frame));
    frame[0] = 8;
    frame[2] = 7;
    frame[3] = 0x21;
    frame[4] = page;
    assert(noah_qmk_portable_profile_get(frame, sizeof(frame)));
}
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
// Reads every page as the app does, checks the page-0 digests against the
// bytes, and optionally keeps each response for the app-side decode.
static uint16_t read_all(uint8_t *out, FILE *responses) {
    get(0);
    assert(frame[5] == 0 && frame[6] == 12 && frame[7] == 1 && frame[8] == 25);
    if (responses) assert(fwrite(frame, 1, sizeof(frame), responses) == sizeof(frame));
    uint16_t length = frame[9] | frame[10] << 8;
    uint32_t crc = u32(frame + 11), fnv = u32(frame + 15);
    uint16_t offset = 0;
    for (uint8_t page = 1; offset < length; page++) {
        get(page);
        assert(frame[5] == 0 && frame[6] == (length - offset < 25 ? length - offset : 25));
        if (responses) assert(fwrite(frame, 1, sizeof(frame), responses) == sizeof(frame));
        memcpy(out + offset, frame + 7, frame[6]);
        offset += frame[6];
    }
    get((uint8_t)(length / 25 + 2));
    assert(frame[5] == 2 && frame[6] == 0);
    assert(noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, out, length)) == crc);
    assert(noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, out, length) == fnv);
    return length;
}

int main(int argc, char **argv) {
    uint8_t  bytes[NOAH_SETTINGS_MAX_SIZE], stored[NOAH_SETTINGS_MAX_SIZE] = {0};
    uint16_t length;

    // A malformed request is answered, not read.
    memset(frame, 0, sizeof(frame));
    frame[0] = 8;
    frame[2] = 7;
    frame[3] = 1;
    frame[31] = 1;
    assert(noah_qmk_portable_profile_get(frame, sizeof(frame)) && frame[5] == 1);

    // No profile settings are live: the current version, every macro unnamed.
    length = read_all(bytes, NULL);
    assert(length == NOAH_SETTINGS_FIXED_SIZE + NOAH_SETTINGS_MACRO_NAMES);
    const uint8_t header[8] = {NOAH_SETTINGS_VERSION, 8, NOAH_SETTINGS_COUNT, NOAH_SETTINGS_MACRO_NAMES, 0, 0, 0, 0};
    assert(!memcmp(bytes, header, 8));
    assert(u32(bytes + 8 + NOAH_SETTING_TAPPING_TERM * 4) == TAPPING_TERM);
    assert(u32(bytes + 8 + NOAH_SETTING_AUTO_MOUSE_TIMEOUT * 4) == 1200);
    assert(u32(bytes + 8 + NOAH_SETTING_DRAGSCROLL_DPI * 4) == 0);
    for (uint16_t i = 8 + NOAH_SETTINGS_COUNT * 4; i < length; i++)
        assert(bytes[i] == 0);

    // A live v3 domain reads back as stored, with its values overlaid by the
    // live QMK owners, and without a second copy held for the read.
    const uint32_t values[28] = {180, 150, 400, 150, 1, 4, 1200, 25, 1, 3, 0, 0, 0, 0, 0, 200, 400, 900000, 1200, 200, 1, 257, 0xc8ff00, 1, 0, 200, 10, 0x76543210};
    memcpy(stored, header, 8);
    for (uint8_t id = 0; id < 28; id++)
        for (uint8_t j = 0; j < 4; j++)
            stored[8 + id * 4 + j] = values[id] >> (j * 8);
    uint16_t names = NOAH_SETTINGS_FIXED_SIZE;
    for (uint8_t slot = 0; slot < NOAH_SETTINGS_MACRO_NAMES; slot++) {
        const char *name = slot == 5 ? "Screenshot" : "";
        stored[names++]  = (uint8_t)strlen(name);
        memcpy(stored + names, name, strlen(name));
        names += strlen(name);
    }
    noah_effective_profile_snapshot_t view = {0};
    view.reader                            = noah_profile_reader_from_memory(stored, names);
    view.profile.domain_mask               = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS;
    view.profile.settings                  = (noah_profile_settings_v1_view_t){0, names};
    noah_effective_settings_invalidate(NULL, 1, view.identity, view.identity, &view);
    am_timeout = 900; // a live change after publication wins in readback
    FILE *responses = argc > 1 ? fopen(argv[1], "wb") : NULL;
    assert(argc < 2 || responses);
    length = read_all(bytes, responses);
    if (responses) assert(fclose(responses) == 0);
    assert(length == names);
    assert(u32(bytes + 8 + NOAH_SETTING_TAPPING_TERM * 4) == 180);
    assert(u32(bytes + 8 + NOAH_SETTING_AUTO_MOUSE_TIMEOUT * 4) == 900);
    assert(!memcmp(bytes + NOAH_SETTINGS_FIXED_SIZE, stored + NOAH_SETTINGS_FIXED_SIZE, names - NOAH_SETTINGS_FIXED_SIZE));
    assert(!memcmp(bytes + 8 + NOAH_SETTINGS_COUNT * 4, stored + 8 + NOAH_SETTINGS_COUNT * 4, NOAH_SETTINGS_FIXED_SIZE - 8 - NOAH_SETTINGS_COUNT * 4));
    puts("portable settings readback streams the live domain");
    return 0;
}
