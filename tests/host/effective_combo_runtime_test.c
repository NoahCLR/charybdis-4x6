#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "users/noah/lib/profile/runtime/effective_combo_runtime.h"
#include "users/noah/lib/profile/runtime/profile_action_runtime_v1.h"
#include "users/noah/lib/compat/qmk_combo_readback.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

static const uint16_t compiled_keys[]  = {7, 9, COMBO_END};
combo_t               key_combos[]     = {{.keys = compiled_keys, .keycode = 41}};
const uint8_t         noah_combo_count = 1;
static unsigned       reads;
static bool           fail_read;
static uint8_t        bytes[912];
bool                  is_combo_enabled(void) {
    return true;
}
uint8_t combo_ref_from_layer(uint8_t layer) {
    return layer;
}
uint16_t                                combo_count(void);
combo_t                                *combo_get(uint16_t index);
uint16_t                                get_combo_term(uint16_t index, combo_t *combo);
bool                                    get_combo_must_tap(uint16_t index, combo_t *combo);
bool                                    get_combo_must_press_in_order(uint16_t index, combo_t *combo);
noah_profile_action_runtime_v1_result_t noah_profile_action_runtime_v1_to_native(const noah_profile_action_v1_t *action, uint16_t *native) {
    if (action->kind == 1u)
        *native = action->operand;
    else if (action->kind == 2u)
        *native = (uint16_t)(0x5220u + action->operand);
    else
        return NOAH_PROFILE_ACTION_RUNTIME_V1_UNSUPPORTED;
    return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
}
static bool read_bytes(void *context, size_t offset, uint8_t *out, size_t length) {
    (void)context;
    reads++;
    assert(length <= 28u && offset + length <= sizeof(bytes));
    if (fail_read) return false;
    memcpy(out, &bytes[offset], length);
    return true;
}
static uint32_t boundary(void *context) {
    return *(uint32_t *)context;
}
static unsigned nibble(char value) {
    return value <= '9' ? (unsigned)(value - '0') : (unsigned)(value - 'a' + 10);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *fixture = fopen(argv[1], "r");
    assert(fixture);
    char encoded[121];
    assert(fscanf(fixture, "%120s", encoded) == 1);
    fclose(fixture);
    for (size_t index = 0; index < 60; index++)
        bytes[index] = (uint8_t)((nibble(encoded[2 * index]) << 4) | nibble(encoded[2 * index + 1]));
    noah_effective_combo_runtime_t runtime;
    noah_effective_combo_runtime_init(&runtime);
    assert(noah_effective_combo_runtime_install(&runtime));
    assert(combo_count() == 1 && combo_get(0) == &key_combos[0]);
    assert(get_combo_term(0, combo_get(0)) == COMBO_TERM);
    // Compiled combos all follow QMK's COMBO_TERM.
    assert(noah_effective_combo_default_term() == COMBO_TERM && noah_effective_combo_follows_default(0) && !noah_effective_combo_follows_default(1));
    noah_effective_profile_snapshot_t view = {.reader = {.read = read_bytes, .length = sizeof(bytes)}, .profile = {.domain_mask = 4, .combos = {.row_count = 2, .version = 1}}};
    noah_effective_combo_runtime_invalidate(&runtime, 1, view.identity, view.identity, &view);
    assert(reads == 3 && combo_count() == 2 && noah_effective_combo_valid());
    // Version 1 stores every window explicitly and has no default of its own.
    assert(noah_effective_combo_default_term() == COMBO_TERM && !noah_effective_combo_follows_default(0) && !noah_effective_combo_follows_default(1));
    combo_t *second = combo_get(1);
    assert(second->keys[0] == 0x5221u && second->keys[1] == 6 && second->keys[2] == COMBO_END && second->keycode == 41);
    assert(get_combo_term(1, second) == 45 && get_combo_must_tap(1, second) && get_combo_must_press_in_order(1, second));
    assert(COMBO_HOLD_TERM == 200);
    second->state = 1;
    for (unsigned scan = 0; scan < 100; scan++) {
        assert(combo_get(1) == second && combo_get(1)->state == 1);
        assert(get_combo_term(1, second) == 45 && combo_count() == 2);
    }
    assert(reads == 3); // Typing never goes back to profile storage.
    uint8_t report[32] = {8, 0, 6, 1, 2};
    assert(noah_qmk_combo_readback_get(report, 32));
    assert(report[5] == 0 && report[7] == 1 && report[11] == 45 && report[15] == 6 && report[16] == 0x21 && report[17] == 0x52);
    assert(reads == 3); // Readback observes the identical effective table.
    // The full cache is bounded and swaps only after the provider's idle gate.
    for (unsigned index = 1; index < 32; index++)
        memcpy(&bytes[4 + 28 * index], &bytes[4], 28);
    bytes[0]                      = 32;
    view.profile.combos.row_count = 32;
    reads                         = 0;
    noah_effective_combo_runtime_invalidate(&runtime, 2, view.identity, view.identity, &view);
    assert(reads == 33 && combo_count() == 32);
    fail_read = true;
    noah_effective_combo_runtime_invalidate(&runtime, 3, view.identity, view.identity, &view);
    assert(!noah_effective_combo_valid() && combo_count() == 0 && combo_get(0) == NULL);
    uint8_t failed[32] = {8, 0, 6, 2};
    assert(noah_qmk_combo_readback_get(failed, 32) && failed[5] != 0);
    view.profile.domain_mask = 0;
    noah_effective_combo_runtime_invalidate(&runtime, 4, view.identity, view.identity, &view);
    assert(noah_effective_combo_valid() && combo_count() == 1 && combo_get(0) == &key_combos[0]);
    fail_read                                    = false;
    bytes[0]                                     = 2;
    uint32_t                            blockers = 1;
    noah_effective_profile_provider_t   provider;
    noah_effective_profile_snapshot_t   compiled, candidate;
    noah_profile_reader_t               compiled_reader  = {.read = read_bytes, .context = &runtime, .length = sizeof(bytes)};
    noah_profile_validator_v1_profile_t compiled_profile = {.byte_length = 8};
    assert(noah_effective_profile_snapshot_make_compiled(&compiled_profile, &compiled_reader, 0, &compiled) == NOAH_EFFECTIVE_PROFILE_OK);
    noah_effective_profile_invalidator_t invalidator = {.callback = noah_effective_combo_runtime_invalidate, .context = &runtime};
    assert(noah_effective_profile_provider_init(&provider, &compiled, boundary, &blockers, &invalidator, 1) == NOAH_EFFECTIVE_PROFILE_OK);
    view.profile.domain_mask      = 4;
    view.profile.domain_count     = 1;
    view.profile.byte_length      = 60;
    view.profile.combos.row_count = 2;
    assert(noah_effective_profile_snapshot_make_validated(&view.profile, &view.reader, 0, 1, 0, 0, &candidate) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_request_validated(&provider, &candidate) == NOAH_EFFECTIVE_PROFILE_OK);
    unsigned before = reads;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_WAITING);
    assert(combo_count() == 1 && combo_get(0) == &key_combos[0] && reads == before);
    blockers = 0;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(combo_count() == 2 && reads == before + 3);
    assert(noah_effective_profile_provider_request_compiled_fallback(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    blockers = 1;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_WAITING && combo_count() == 2);
    blockers = 0;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED && combo_count() == 1);
    // Version 2: the default window and hold threshold live in the header, and
    // a row with window zero follows the default.
    static const uint8_t v2[8 + 2 * 28] = {
        2, 0, 0, 0, 60, 0, 150, 0,
        2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 41, 0, 1, 0, 4, 0, 1, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        2, 6, 45, 0, 0, 0, 0, 0, 1, 0, 41, 0, 2, 0, 1, 0, 1, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    memcpy(bytes, v2, sizeof(v2));
    noah_effective_profile_snapshot_t second_view = {.reader = {.read = read_bytes, .length = sizeof(bytes)}, .profile = {.domain_mask = 4, .combos = {.row_count = 2, .version = 2}}};
    noah_effective_combo_runtime_invalidate(&runtime, 5, second_view.identity, second_view.identity, &second_view);
    assert(noah_effective_combo_valid() && combo_count() == 2);
    assert(get_combo_term(0, combo_get(0)) == 60 && get_combo_term(1, combo_get(1)) == 45);
    assert(noah_effective_combo_default_term() == 60 && noah_effective_combo_follows_default(0) && !noah_effective_combo_follows_default(1));
    assert(COMBO_HOLD_TERM == 150);
    uint8_t v2_report[32] = {8, 0, 6, 1, 0};
    assert(noah_qmk_combo_readback_get(v2_report, 32) && v2_report[5] == 0);
    assert(v2_report[7] == 2 && v2_report[25] == 60 && v2_report[27] == 150);
    v2_report[4] = 1;
    memset(&v2_report[5], 0, 27);
    assert(noah_qmk_combo_readback_get(v2_report, 32) && v2_report[11] == 60 && v2_report[13] == 0 && v2_report[15] == 8);
    v2_report[4] = 2;
    memset(&v2_report[5], 0, 27);
    assert(noah_qmk_combo_readback_get(v2_report, 32) && v2_report[11] == 45 && v2_report[15] == 6);
    // An empty version 2 table still carries both combo-wide values.
    bytes[0]                          = 0;
    second_view.profile.combos.row_count = 0;
    noah_effective_combo_runtime_invalidate(&runtime, 6, second_view.identity, second_view.identity, &second_view);
    assert(noah_effective_combo_valid() && combo_count() == 0 && noah_effective_combo_default_term() == 60 && COMBO_HOLD_TERM == 150);
    // A zero default would silently stop every combo that follows it.
    bytes[4] = 0;
    noah_effective_combo_runtime_invalidate(&runtime, 7, second_view.identity, second_view.identity, &second_view);
    assert(!noah_effective_combo_valid() && combo_count() == 0);
    noah_effective_combo_runtime_uninstall(&runtime);
    puts("effective combo runtime, QMK hooks and readback tests passed");
}
