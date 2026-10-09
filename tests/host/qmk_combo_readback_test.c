#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "noah_keymap_ids.h"
#include "users/noah/lib/compat/qmk_combo_readback.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

static const uint16_t first[]          = {0x0007u, 0x4109u, 0u};
static const uint16_t second[]         = {0x0806u, 0x0819u, 0u};
combo_t               key_combos[]     = {COMBO(first, 0x002bu), COMBO(second, 0x0804u)};
const uint8_t         noah_combo_count = 2u;
static bool           enabled          = true;
bool                  is_combo_enabled(void) {
    return enabled;
}
uint8_t combo_ref_from_layer(uint8_t layer) {
    return layer;
}
uint16_t get_combo_term(uint16_t index, combo_t *combo) {
    (void)combo;
    return index ? 0u : 75u;
}
bool get_combo_must_hold(uint16_t index, combo_t *combo) {
    (void)combo;
    return index == 0u;
}
bool get_combo_must_tap(uint16_t index, combo_t *combo) {
    (void)combo;
    return index == 1u;
}
bool get_combo_must_press_in_order(uint16_t index, combo_t *combo) {
    (void)combo;
    return index == 1u;
}

static void read_page(uint16_t page, uint8_t report[32]) {
    memset(report, 0, 32);
    report[0] = 8u;
    report[2] = 6u;
    report[3] = 7u;
    report[4] = (uint8_t)page;
    report[5] = (uint8_t)(page >> 8u);
    assert(noah_qmk_combo_readback_get(report, 32u));
}
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static bool zero(const uint8_t *p, size_t length) {
    for (size_t i = 0; i < length; i++)
        if (p[i]) return false;
    return true;
}
enum { PAGES = 6 }; // metadata, references, then two pages for each of two rows
// The metadata digest covers its own bytes 0..5 and 10..24, the reference
// page, then each row's two pages in order.
static uint32_t digest_of_pages(void) {
    uint8_t  report[32];
    read_page(0, report);
    uint32_t digest = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, &report[7], 6u);
    digest          = noah_profile_fnv1a_update(digest, &report[17], 15u);
    for (uint16_t page = 1; page < PAGES; page++) {
        read_page(page, report);
        assert(report[5] == 0 && report[6] == 25);
        digest = noah_profile_fnv1a_update(digest, &report[7], 25u);
    }
    return digest;
}

int main(int argc, char **argv) {
    uint8_t report[32], original[32];
    read_page(0, report);
    assert(report[5] == 0 && report[6] == 25);
    // Version 3: two rows of at most sixteen inputs over the sixteen-layer bank.
    assert(report[7] == 3 && report[8] == 2 && report[9] == 16 && report[10] == LAYER_COUNT && LAYER_COUNT == 16 && report[11] == 1);
    assert(u32(&report[13]) == digest_of_pages());
    // The combo-wide default window and hold threshold, then pages per row.
    assert(report[17] == COMBO_TERM && report[18] == 0 && report[19] == (uint8_t)TAPPING_TERM && report[20] == (uint8_t)(TAPPING_TERM >> 8u));
    assert(report[21] == 2 && zero(&report[22], 10));
#ifdef COMBO_ONLY_FROM_LAYER
    assert(report[12] & 32u);
#endif
    memcpy(original, report, 32);
    // Page 1: every layer's reference layer, counted by the layer count.
    read_page(1, report);
    assert(report[5] == 0 && report[6] == 25);
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
#ifdef COMBO_ONLY_FROM_LAYER
        assert(report[7u + layer] == COMBO_ONLY_FROM_LAYER);
#else
        assert(report[7u + layer] == layer);
#endif
    }
    assert(zero(&report[7u + LAYER_COUNT], 25u - LAYER_COUNT));
    // Row 0, page A: index, input count, output, window, flags, allowed
    // layers, then inputs 0..6.
    read_page(2, report);
    assert(report[5] == 0 && report[7] == 0 && report[8] == 2);
    assert(report[9] == 0x2b && report[10] == 0);
    assert(u32(&report[14]) == 0xffffu); // every layer of the bank
    assert(report[18] == 7 && report[19] == 0 && report[20] == 9 && report[21] == 0x41 && zero(&report[22], 10));
#ifdef COMBO_TERM_PER_COMBO
    // Without the live owner a per-combo hook is the user's own: its windows
    // are explicit, never following the default.
    assert(report[11] == 75);
#    ifdef COMBO_NO_TIMER
    assert(report[13] == 0);
#    else
    assert(report[13] == 1);
#    endif
    read_page(4, report);
    assert(report[11] == 0 && report[13] == 6);
#else
    assert(report[11] == COMBO_TERM && report[13] == 8);
#endif
    // Page B holds inputs 7..15; these rows have none.
    read_page(3, report);
    assert(report[5] == 0 && report[6] == 25 && zero(&report[7], 25));
    read_page(4, report);
    assert(report[7] == 1 && report[8] == 2 && report[9] == 4 && report[10] == 8 && report[18] == 6 && report[19] == 8 && report[20] == 0x19 && report[21] == 8);
    // This independent byte fixture is consumed by both firmware and host.
    if (argc > 2 && !strcmp(argv[2], "--write-fixture")) {
        static const char *const names[PAGES] = {"metadata", "references", "row0a", "row0b", "row1a", "row1b"};
        FILE                    *fixture       = fopen(argv[1], "w");
        assert(fixture);
        for (uint16_t page = 0; page < PAGES; page++) {
            read_page(page, report);
            fprintf(fixture, "%s ", names[page]);
            for (unsigned int index = 0; index < 25; index++)
                fprintf(fixture, "%02x", report[7 + index]);
            fputc('\n', fixture);
        }
        assert(fclose(fixture) == 0);
    } else if (argc > 1) {
        FILE *fixture = fopen(argv[1], "r");
        assert(fixture);
        char         name[32], hex[51];
        unsigned int page = 0;
        while (fscanf(fixture, "%31s %50s", name, hex) == 2) {
            read_page((uint16_t)page++, report);
            assert(strlen(hex) == 50);
            for (unsigned int index = 0; index < 25; index++) {
                unsigned int expected;
                assert(sscanf(&hex[index * 2], "%2x", &expected) == 1);
                assert(report[7 + index] == expected);
            }
        }
        assert(page == PAGES);
        fclose(fixture);
    }
    enabled = false;
    read_page(0, report);
    assert(report[11] == 0 && u32(&original[13]) != u32(&report[13]) && u32(&report[13]) == digest_of_pages());
    // Past the last row, including a page that needs the wide high byte.
    read_page(PAGES, report);
    assert(report[5] == 2 && report[6] == 0);
    read_page(0x0102u, report);
    assert(report[5] == 2 && report[6] == 0);
    read_page(1, report);
    report[3] = 0;
    assert(noah_qmk_combo_readback_get(report, 32));
    assert(report[5] == 1);
    read_page(0, report);
    report[6] = 1; // the fixed request fields end before byte 6
    assert(noah_qmk_combo_readback_get(report, 32));
    assert(report[5] == 1);
    assert(!noah_qmk_combo_readback_get(NULL, 32));
    assert(!noah_qmk_combo_readback_get(report, 31));
    const uint16_t duplicate[] = {4, 4, 0};
    key_combos[0].keys         = duplicate;
    read_page(0, report);
    assert(report[5] == 3 && report[6] == 0);
    // Sixteen inputs fill page A's seven and page B's nine.
    const uint16_t sixteen[] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0};
    key_combos[0].keys       = sixteen;
    read_page(0, report);
    assert(report[5] == 0);
    read_page(2, report);
    assert(report[5] == 0 && report[8] == 16 && report[18] == 0x40 && report[30] == 0x46);
    read_page(3, report);
    assert(report[5] == 0 && report[7] == 0x47 && report[7 + 16] == 0x4f && report[7 + 17] == 0 && zero(&report[7 + 18], 7));
    const uint16_t overlong[] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0};
    key_combos[0].keys        = overlong;
    read_page(0, report);
    assert(report[5] == 3 && report[6] == 0);
    puts("combo readback host tests passed");
    return 0;
}
