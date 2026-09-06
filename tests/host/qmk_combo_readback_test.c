#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "noah_keymap_ids.h"
#include "users/noah/lib/compat/qmk_combo_readback.h"

static const uint16_t first[] = {0x0007u, 0x4109u, 0u};
static const uint16_t second[] = {0x0806u, 0x0819u, 0u};
combo_t key_combos[] = {COMBO(first, 0x002bu), COMBO(second, 0x0804u)};
const uint8_t noah_combo_count = 2u;
static bool enabled = true;
bool is_combo_enabled(void) {return enabled;}
uint8_t combo_ref_from_layer(uint8_t layer) {return layer;}
uint16_t get_combo_term(uint16_t index, combo_t *combo) {(void)combo; return index ? 0u : 75u;}
bool get_combo_must_hold(uint16_t index, combo_t *combo) {(void)combo; return index == 0u;}
bool get_combo_must_tap(uint16_t index, combo_t *combo) {(void)combo; return index == 1u;}
bool get_combo_must_press_in_order(uint16_t index, combo_t *combo) {(void)combo; return index == 1u;}

static void read_page(uint8_t page, uint8_t report[32]) {
    memset(report, 0, 32);
    report[0] = 8u; report[2] = 6u; report[3] = 7u; report[4] = page;
    assert(noah_qmk_combo_readback_get(report, 32u));
}

int main(int argc, char **argv) {
    uint8_t report[32], original[32];
    read_page(0, report);
    assert(report[5] == 0 && report[6] == 25);
    assert(report[7] == 1 && report[8] == 2 && report[9] == 4 && report[10] == LAYER_COUNT && report[11] == 1);
#ifdef COMBO_ONLY_FROM_LAYER
    assert(report[12] & 32u);
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) assert(report[13u + layer] == COMBO_ONLY_FROM_LAYER);
#endif
    memcpy(original, report, 32);
    read_page(1, report);
    assert(report[5] == 0 && report[7] == 0 && report[8] == 2);
    assert(report[9] == 0x2b && report[10] == 0 && report[16] == 7 && report[18] == 9 && report[19] == 0x41);
#ifdef COMBO_TERM_PER_COMBO
    assert(report[11] == 75);
#    ifdef COMBO_NO_TIMER
    assert(report[15] == 0);
#    else
    assert(report[15] == 1);
#    endif
    read_page(2, report);
    assert(report[11] == 0 && report[15] == 6);
#else
    assert(report[11] == COMBO_TERM && report[15] == 0);
#endif
    // This independent byte fixture is consumed by both firmware and host.
    if (argc > 1) {
        FILE *fixture = fopen(argv[1], "r"); assert(fixture);
        char name[32], hex[51]; unsigned int page = 0;
        while (fscanf(fixture, "%31s %50s", name, hex) == 2) {
            read_page((uint8_t)page++, report);
            assert(strlen(hex) == 50);
            for (unsigned int index = 0; index < 25; index++) {
                unsigned int expected; assert(sscanf(&hex[index * 2], "%2x", &expected) == 1);
                assert(report[7 + index] == expected);
            }
        }
        assert(page == 3); fclose(fixture);
    }
    enabled = false; read_page(0, report);
    assert(report[11] == 0 && memcmp(&original[21], &report[21], 4) != 0);
    read_page(3, report); assert(report[5] == 2 && report[6] == 0);
    report[3] = 0; assert(noah_qmk_combo_readback_get(report, 32)); assert(report[5] == 1);
    read_page(0, report); assert(noah_qmk_combo_readback_get(report, 32)); assert(report[5] == 1);
    assert(!noah_qmk_combo_readback_get(NULL, 32));
    assert(!noah_qmk_combo_readback_get(report, 31));
    const uint16_t duplicate[] = {4, 4, 0};
    key_combos[0].keys = duplicate;
    read_page(0, report); assert(report[5] == 3 && report[6] == 0);
    const uint16_t overlong[] = {4, 5, 6, 7, 8, 0};
    key_combos[0].keys = overlong;
    read_page(0, report); assert(report[5] == 3 && report[6] == 0);
    puts("combo readback host tests passed");
    return 0;
}
