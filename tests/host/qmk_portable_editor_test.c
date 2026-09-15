#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "keycode_config.h"
#include "rgb_matrix.h"
#include "users/noah/lib/compat/qmk_portable_editor.h"
led_config_t   g_led_config = {{1, 4, 4}};
static bool    enabled, saved_enabled;
static uint8_t effect, speed, flags, h, s, v;
void           rgb_matrix_enable_noeeprom(void) {
    enabled = true;
}
void rgb_matrix_enable(void) {
    enabled = saved_enabled = true;
}
void rgb_matrix_disable(void) {
    enabled = saved_enabled = false;
}
void rgb_matrix_mode(uint8_t value) {
    if (enabled) effect = value;
}
void rgb_matrix_set_speed(uint8_t value) {
    speed = value;
}
void rgb_matrix_set_flags(uint8_t value) {
    flags = value;
}
void rgb_matrix_sethsv(uint16_t hue, uint8_t saturation, uint8_t brightness) {
    if (enabled) {
        h = hue;
        s = saturation;
        v = brightness;
    }
}

int main(int argc, char **argv) {
    uint8_t payload[25] = {0}, metadata[25] = {0}, bytes[6400] = {0};
    assert(noah_qmk_portable_editor_page(0, payload) == 0);
    assert(noah_qmk_portable_editor_page(1, NULL) == 0);
    assert(noah_qmk_portable_editor_page(1, payload) == 2 && payload[0] == 1 && payload[1] == 180);
    assert(noah_qmk_portable_editor_page(2, metadata) == 9);
    unsigned length = metadata[2] | metadata[3] << 8;
    assert(metadata[0] == 1 && metadata[1] == 25 && metadata[5] == 13 && metadata[8] == 5);
    assert(metadata[4] == RGB_MATRIX_EFFECT_MAX - 1);
    assert(length == 26 + metadata[4] * 64u);
    unsigned offset = 0;
    for (uint8_t page = 3; offset < length; page++) {
        memset(payload, 0xa5, sizeof(payload));
        uint8_t count = noah_qmk_portable_editor_page(page, payload);
        assert(count == (length - offset < 25 ? length - offset : 25));
        memcpy(bytes + offset, payload, count);
        for (unsigned i = count; i < sizeof(payload); i++)
            assert(payload[i] == 0xa5);
        offset += count;
    }
    assert(noah_qmk_portable_editor_page(255, payload) == 0);
    keymap_config_t option = {.raw = bytes[4] | bytes[5] << 8};
    assert(option.swap_lalt_lgui);
    assert(strcmp((const char *)bytes + 26, "SOLID_COLOR") == 0);
#ifdef MAGIC_ENABLE
    assert((metadata[6] | metadata[7] << 8) == 0x1fff);
    assert(strstr((const char *)bytes + 26 + 64, "BREATHING"));
#else
    assert((metadata[6] | metadata[7] << 8) == 0x0400);
#endif
    for (unsigned before = 0; before < 2; before++)
        for (unsigned after = 0; after < 2; after++) {
            enabled = before;
            effect = h = s = v = 0;
            noah_qmk_portable_apply_lighting(after | 1u << 8 | 35u << 16 | 5u << 24, 70u | 200u << 8 | 100u << 16);
            assert(enabled == after && saved_enabled == after);
            assert(effect == 1 && speed == 35 && flags == 5 && h == 70 && s == 200 && v == 100);
        }
    if (argc > 1) {
        FILE *file = fopen(argv[1], "wb");
        assert(file);
        assert(fwrite(metadata, 1, 9, file) == 9);
        assert(fwrite(bytes, 1, length, file) == length);
        assert(fclose(file) == 0);
    }
    puts("portable editor metadata and disabled-lighting restore tests passed");
    return 0;
}
