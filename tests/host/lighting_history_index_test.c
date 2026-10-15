#include "users/noah/lib/rgb/lighting/lighting_history_index.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static noah_profile_lighting_v1_program_t program;
static noah_lighting_history_index_t      index_table;
static uint8_t                            bytes[NOAH_PROFILE_LIGHTING_V1_PAYLOAD_MAX];
static void                               load(const char *hex) {
    size_t length = strlen(hex) / 2;
    assert(length <= sizeof bytes);
    for (size_t i = 0; i < length; i++) {
        unsigned value;
        assert(sscanf(hex + 2 * i, "%2x", &value) == 1);
        bytes[i] = value;
    }
    noah_profile_lighting_v1_limits_t limits = noah_profile_lighting_v1_default_limits();
    limits.logical_layer_count               = 16;
    limits.supported_mode_mask               = UINT32_MAX;
    noah_profile_lighting_v1_view_t view;
    assert(noah_profile_lighting_v1_decode(bytes, length, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view, &program));
    assert(noah_lighting_history_index_prepare(&program, &index_table));
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *input = fopen(argv[1], "r");
    assert(input);
    char     name[100], hex[2u * NOAH_PROFILE_LIGHTING_V1_PAYLOAD_MAX + 1u];
    unsigned cases = 0, rules = 0, total;
    while (fscanf(input, "%99s %16384s %u", name, hex, &total) == 3) {
        load(hex);
        assert(index_table.offsets[program.rule_count] == total);
        uint16_t offset = 0;
        for (unsigned r = 0; r < program.rule_count; r++) {
            unsigned kind, thing_kind, domain, selected, show, linger, flash, count;
            assert(fscanf(input, "%u %u %u %u %u %u %u %u", &kind, &thing_kind, &domain, &selected, &show, &linger, &flash, &count) == 8);
            noah_lighting_history_rule_t description;
            assert(noah_lighting_history_rule(&program, r, &description));
            assert(description.kind == kind && description.thing_kind == thing_kind && description.domain == domain && description.selected == selected);
            assert(description.event == (kind >= 19 && kind <= 28));
            assert(description.timing.show_after == show && description.timing.linger == linger && description.timing.flash == flash);
            assert(index_table.offsets[r] == offset);
            offset += count;
            assert(index_table.offsets[r + 1] == offset);
            for (unsigned thing = 0; thing < 256; thing++) {
                uint16_t                            slot       = 0;
                noah_lighting_lifetime_parameters_t parameters = {0};
                bool                                wanted     = count && (domain == 1 || (thing < domain && (selected == 255 || selected == thing)));
                bool                                found      = noah_lighting_history_index_lookup(&program, &index_table, r, thing, &slot, &parameters);
                assert(found == wanted);
                if (found) {
                    assert(slot == index_table.offsets[r] + (domain > 1 && selected == 255 ? thing : 0));
                    assert(parameters.show_after == show && parameters.linger == linger && parameters.flash == flash);
                }
            }
            rules++;
        }
        assert(total == offset);
        cases++;
    }
    fclose(input);
    assert(cases > 10 && rules >= 255);
    printf("history profile index: %u validated profiles/%u rules, all256 lookup IDs, full255*128 logical bound32640, prefix index512B; no retained-bank reservation PASS\n", cases, rules);
}
