#include <stdio.h>
#include <stdlib.h>

#include "keycodes.h"
#include "host_layout.h"
#include "users/noah/lib/compat/qmk_host_setting.h"

extern const uint8_t qmk_us_shift_lut[16];
extern const uint8_t qmk_us_altgr_lut[16];
extern const uint8_t qmk_us_dead_lut[16];
extern const uint8_t qmk_us_keycode_lut[128];

#define QMK_LUTS(lang) \
    extern const uint8_t qmk_##lang##_shift_lut[16], qmk_##lang##_altgr_lut[16], qmk_##lang##_dead_lut[16], qmk_##lang##_keycode_lut[128];
QMK_LUTS(german)
QMK_LUTS(french)
QMK_LUTS(uk)
QMK_LUTS(us_international)

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fprintf(stderr, "test failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

enum { LAYOUT_COUNT = 15, MACOS_DUTCH = 2, MACOS_UNICODE_HEX_INPUT = 3, WINDOWS_US_INTERNATIONAL = 7, WINDOWS_UK = 8, WINDOWS_GERMAN = 9, WINDOWS_FRENCH = 10, LINUX_GERMAN = 13 };

static const uint16_t SHIFT = HOST_LAYOUT_STROKE_SHIFT, ALTGR = HOST_LAYOUT_STROKE_ALTGR;

static int lut_bit(const uint8_t *lut, unsigned code) {
    return (lut[code / 8u] >> (code % 8u)) & 1;
}

static void check_types(uint8_t layout_id, uint32_t codepoint, uint16_t first, uint16_t second) {
    uint16_t strokes[2] = {0xFFFFu, 0xFFFFu};
    CHECK(host_layout_lookup(host_layout_get(layout_id), codepoint, strokes));
    CHECK(strokes[0] == first && strokes[1] == second);
}

static void test_catalogue_is_complete_and_well_formed(void) {
    CHECK(host_layout_table_count == LAYOUT_COUNT && NOAH_HOST_LAYOUT_LIMIT == LAYOUT_COUNT);
    CHECK(sizeof(host_layout_char_t) == 8u);
    CHECK(host_layout_get(LAYOUT_COUNT) == NULL && host_layout_get(0xFFu) == NULL);
    unsigned entries = 0u;
    for (uint8_t id = 0u; id < LAYOUT_COUNT; id++) {
        const host_layout_t *layout = host_layout_get(id);
        CHECK(layout && layout->id == id && layout->count > 0u);
        CHECK(!!(layout->flags & HOST_LAYOUT_FLAG_MACOS) == (id >= 1u && id <= 6u));
        entries += layout->count;
        for (uint16_t i = 0u; i < layout->count; i++) {
            const host_layout_char_t *entry = &layout->chars[i];
            CHECK(i == 0u || layout->chars[i - 1u].codepoint < entry->codepoint);
            CHECK(entry->codepoint >= 0x09u && entry->codepoint <= 0x10FFFFu);
            CHECK(entry->strokes[0] != 0u);
            for (unsigned s = 0u; s < 2u; s++) {
                uint16_t stroke = entry->strokes[s];
                uint8_t  key    = HOST_LAYOUT_STROKE_KEYCODE(stroke);
                CHECK((stroke & ~(0xFFu | SHIFT | ALTGR)) == 0u);
                CHECK(stroke == 0u || (key >= KC_A && key <= KC_SLASH) || key == KC_NONUS_BACKSLASH);
                if (layout->flags & HOST_LAYOUT_FLAG_UNICODE_HEX_INPUT) CHECK(!(stroke & ALTGR));
            }
        }
        // Every layout types all printable ASCII, tab and newline.
        uint16_t strokes[2];
        for (uint32_t c = 0x20u; c < 0x7Fu; c++) CHECK(host_layout_lookup(layout, c, strokes));
        CHECK(host_layout_lookup(layout, '\t', strokes) && strokes[0] == KC_TAB && strokes[1] == 0u);
        CHECK(host_layout_lookup(layout, '\n', strokes) && strokes[0] == KC_ENTER && strokes[1] == 0u);
        CHECK(!host_layout_lookup(layout, 0x1F642u, strokes));
        // Unicode entry types its hex digits and U through the layout in one stroke each.
        for (const char *c = "0123456789abcdefu"; *c; c++) CHECK(host_layout_lookup(layout, (uint8_t)*c, strokes) && strokes[1] == 0u);
        CHECK(!host_layout_lookup(layout, 0x7Fu, strokes) && !host_layout_lookup(layout, 0x0Du, strokes));
    }
    // Flash cost of the first set: a regression tripwire, not a budget.
    CHECK(entries * sizeof(host_layout_char_t) <= 40u * 1024u);
    CHECK(host_layout_get(MACOS_UNICODE_HEX_INPUT)->flags == (HOST_LAYOUT_FLAG_UNICODE_HEX_INPUT | HOST_LAYOUT_FLAG_MACOS));
    CHECK(!host_layout_lookup(NULL, 'a', (uint16_t[2]){0}));
}

static void test_us_matches_qmk_send_string(void) {
    const host_layout_t *us = host_layout_get(HOST_LAYOUT_US);
    for (unsigned c = 0u; c < 128u; c++) {
        if (!(c == '\t' || c == '\n' || (c >= 0x20u && c < 0x7Fu))) continue;
        uint16_t strokes[2];
        CHECK(host_layout_lookup(us, c, strokes));
        CHECK(HOST_LAYOUT_STROKE_KEYCODE(strokes[0]) == qmk_us_keycode_lut[c]);
        CHECK(!!(strokes[0] & SHIFT) == lut_bit(qmk_us_shift_lut, c));
        CHECK(!!(strokes[0] & ALTGR) == lut_bit(qmk_us_altgr_lut, c));
        CHECK(!lut_bit(qmk_us_dead_lut, c) && strokes[1] == 0u);
    }
    CHECK(us->count == 97u); // ASCII only.
}

// KC_BSLS and KC_NUHS are one key on every host OS; QMK writes either.
static uint8_t same_key(uint8_t keycode) {
    return keycode == KC_NONUS_HASH ? KC_BACKSLASH : keycode;
}

static void check_ascii_matches_qmk(uint8_t layout_id, const uint8_t *shift, const uint8_t *altgr, const uint8_t *dead, const uint8_t *keycode) {
    const host_layout_t *layout = host_layout_get(layout_id);
    for (unsigned c = 0x20u; c < 0x7Fu; c++) {
        uint16_t strokes[2];
        CHECK(host_layout_lookup(layout, c, strokes));
        if (same_key(HOST_LAYOUT_STROKE_KEYCODE(strokes[0])) != same_key(keycode[c]) || !!(strokes[0] & SHIFT) != lut_bit(shift, c) ||
            !!(strokes[0] & ALTGR) != lut_bit(altgr, c) || (strokes[1] == KC_SPACE) != lut_bit(dead, c)) {
            fprintf(stderr, "layout %u types '%c' as 0x%04X 0x%04X; QMK sends keycode 0x%02X shift %d altgr %d dead %d\n", layout_id, (int)c, strokes[0], strokes[1],
                    keycode[c], lut_bit(shift, c), lut_bit(altgr, c), lut_bit(dead, c));
            CHECK(false);
        }
    }
}

static void test_windows_ascii_matches_qmk_send_string(void) {
    check_ascii_matches_qmk(WINDOWS_US_INTERNATIONAL, qmk_us_international_shift_lut, qmk_us_international_altgr_lut, qmk_us_international_dead_lut, qmk_us_international_keycode_lut);
    check_ascii_matches_qmk(WINDOWS_UK, qmk_uk_shift_lut, qmk_uk_altgr_lut, qmk_uk_dead_lut, qmk_uk_keycode_lut);
    check_ascii_matches_qmk(WINDOWS_GERMAN, qmk_german_shift_lut, qmk_german_altgr_lut, qmk_german_dead_lut, qmk_german_keycode_lut);
    check_ascii_matches_qmk(WINDOWS_FRENCH, qmk_french_shift_lut, qmk_french_altgr_lut, qmk_french_dead_lut, qmk_french_keycode_lut);
}

static void test_layouts_type_their_own_characters(void) {
    check_types(WINDOWS_GERMAN, 'z', KC_Y, 0u);
    check_types(WINDOWS_GERMAN, 0x00F6u /* ö */, KC_SEMICOLON, 0u);
    check_types(WINDOWS_GERMAN, '@', ALTGR | KC_Q, 0u);
    check_types(WINDOWS_GERMAN, '^', KC_GRAVE, KC_SPACE);
    check_types(WINDOWS_FRENCH, 'a', KC_Q, 0u);
    check_types(WINDOWS_FRENCH, '~', ALTGR | KC_2, KC_SPACE);
    check_types(WINDOWS_US_INTERNATIONAL, '\'', KC_QUOTE, KC_SPACE);
    check_types(WINDOWS_US_INTERNATIONAL, '"', SHIFT | KC_QUOTE, KC_SPACE);
    check_types(WINDOWS_US_INTERNATIONAL, 0x00E9u /* é */, ALTGR | KC_E, 0u);
    check_types(MACOS_DUTCH, 0x20ACu /* € */, ALTGR | KC_2, 0u);
    check_types(MACOS_DUTCH, 0x00E9u /* é */, ALTGR | KC_E, KC_E);
    check_types(MACOS_DUTCH, 0x201Cu /* “ */, ALTGR | KC_LEFT_BRACKET, 0u);
    check_types(MACOS_DUTCH, '{', SHIFT | KC_LEFT_BRACKET, 0u);
    // Linux composes dead acute and Space into an apostrophe, so ´ itself is not typeable.
    check_types(LINUX_GERMAN, '^', KC_GRAVE, KC_SPACE);
    uint16_t strokes[2];
    CHECK(!host_layout_lookup(host_layout_get(LINUX_GERMAN), 0x00B4u, strokes));
    CHECK(!host_layout_lookup(host_layout_get(MACOS_UNICODE_HEX_INPUT), 0x00E9u, strokes));
}

int main(void) {
    test_catalogue_is_complete_and_well_formed();
    test_us_matches_qmk_send_string();
    test_windows_ascii_matches_qmk_send_string();
    test_layouts_type_their_own_characters();
    puts("host_layout host tests passed");
    return 0;
}
