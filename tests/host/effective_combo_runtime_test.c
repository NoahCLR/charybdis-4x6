#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "users/noah/lib/profile/runtime/effective_combo_runtime.h"
#include "users/noah/lib/profile/runtime/profile_action_runtime_v1.h"
#include "users/noah/lib/compat/qmk_combo_readback.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "profile_test_blob.h"

static const uint16_t compiled_keys[]  = {7, 9, COMBO_END};
combo_t               key_combos[]     = {{.keys = compiled_keys, .keycode = 41}};
const uint8_t         noah_combo_count = 1;
#ifndef COMPILED_WINDOW
#    define COMPILED_WINDOW 0
#endif
// COMBO leaves a compiled combo's window zero; COMBO_WINDOW gives it one.
const uint16_t  noah_combo_terms[] = {COMPILED_WINDOW};
static unsigned       reads;
static bool           fail_read;
// Room for the whole table: 128 rows of sixteen inputs.
static uint8_t        bytes[8 + 128 * 76];
static uint16_t       payload_length; // the combo domain's, inside a one-domain blob
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
    assert(length <= 20u && offset + length <= NOAH_PROFILE_TEST_BLOB_PAYLOAD_OFFSET + sizeof(bytes));
    if (fail_read) return false;
    for (size_t index = 0; index < length; index++)
        out[index] = noah_profile_test_blob_byte(NOAH_PROFILE_DOMAIN_V1_COMBOS, bytes, payload_length, offset + index);
    return true;
}
// A view of the combo domain as the validator leaves it: the payload inside
// its blob, found by walking the envelope (two reads per publication).
static noah_effective_profile_snapshot_t combo_view(uint16_t length) {
    payload_length = length;
    return (noah_effective_profile_snapshot_t){.reader = {.read = read_bytes, .length = NOAH_PROFILE_TEST_BLOB_PAYLOAD_OFFSET + sizeof(bytes)}, .profile = {.domain_mask = 4, .domain_count = 1, .byte_length = (uint16_t)(NOAH_PROFILE_TEST_BLOB_PAYLOAD_OFFSET + length)}};
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
    char encoded[129];
    assert(fscanf(fixture, "%128s", encoded) == 1);
    fclose(fixture);
    // Two version-3 rows of two inputs each: 8 + 2 * 20 bytes.
    assert(strlen(encoded) == 2u * (8u + 2u * 20u));
    for (size_t index = 0; index < 8u + 2u * 20u; index++)
        bytes[index] = (uint8_t)((nibble(encoded[2 * index]) << 4) | nibble(encoded[2 * index + 1]));
    uint8_t fixture_bytes[8 + 2 * 20];
    memcpy(fixture_bytes, bytes, sizeof(fixture_bytes));
    noah_effective_combo_runtime_t runtime;
    noah_effective_combo_runtime_init(&runtime);
    assert(noah_effective_combo_runtime_install(&runtime));
    assert(combo_count() == 1 && combo_get(0) == &key_combos[0]);
    assert(get_combo_term(0, combo_get(0)) == (COMPILED_WINDOW ? COMPILED_WINDOW : COMBO_TERM));
    // A compiled combo follows QMK's COMBO_TERM unless the keymap gave it a window.
    assert(noah_effective_combo_default_term() == COMBO_TERM && noah_effective_combo_follows_default(0) == !COMPILED_WINDOW && !noah_effective_combo_follows_default(1));
    noah_effective_profile_snapshot_t view = combo_view(8 + 2 * 20);
    noah_effective_combo_runtime_invalidate(&runtime, 1, view.identity, view.identity, &view);
    assert(reads == 7 && combo_count() == 2 && noah_effective_combo_valid());
    // These fixture rows store explicit windows beside the current default.
    assert(noah_effective_combo_default_term() == 60 && !noah_effective_combo_follows_default(0) && !noah_effective_combo_follows_default(1));
    combo_t *second = combo_get(1);
    assert(second->keys[0] == 0x5221u && second->keys[1] == 6 && second->keys[2] == COMBO_END && second->keycode == 41);
    assert(get_combo_term(1, second) == 45 && get_combo_must_tap(1, second) && get_combo_must_press_in_order(1, second));
    assert(COMBO_HOLD_TERM == 200);
    second->state = 1;
    for (unsigned scan = 0; scan < 100; scan++) {
        assert(combo_get(1) == second && combo_get(1)->state == 1);
        assert(get_combo_term(1, second) == 45 && combo_count() == 2);
    }
    assert(reads == 7); // Typing never goes back to profile storage.
    // Readout v3: row 1's first page is page 4.
    uint8_t report[32] = {8, 0, 6, 1, 4};
    assert(noah_qmk_combo_readback_get(report, 32));
    assert(report[5] == 0 && report[7] == 1 && report[11] == 45 && report[13] == 6 && report[18] == 0x21 && report[19] == 0x52);
    assert(reads == 7); // Readback observes the identical effective table.
    // The full table is bounded and swaps only after the provider's idle gate:
    // two envelope reads, the header, then two reads a two-input row.
    for (unsigned index = 1; index < 128; index++) {
        memcpy(&bytes[8 + 20 * index], &bytes[8], 20);
        bytes[8 + 20 * index + 14] = (uint8_t)(10 + index); // distinct first inputs
        bytes[8 + 20 * index + 15] = 0;
    }
    bytes[0] = 128;
    view     = combo_view(8 + 128 * 20);
    reads    = 0;
    noah_effective_combo_runtime_invalidate(&runtime, 2, view.identity, view.identity, &view);
    assert(reads == 3 + 2 * 128 && combo_count() == 128 && noah_effective_combo_valid());
    assert(combo_get(127)->keys[0] == 10 + 127 && combo_get(127)->keys[2] == COMBO_END);
    // Sixteen inputs: the fixed part, then four reads of four inputs.
    uint8_t *wide = &bytes[8];
    wide[0]       = 16;
    for (unsigned input = 0; input < 16; input++) {
        uint8_t *action = &wide[12 + 4 * input];
        action[0]       = 1, action[1] = 0, action[2] = (uint8_t)(0x40 + input), action[3] = 0;
    }
    bytes[0] = 1;
    view     = combo_view(8 + 12 + 64);
    reads    = 0;
    noah_effective_combo_runtime_invalidate(&runtime, 2, view.identity, view.identity, &view);
    assert(reads == 3 + 5 && combo_count() == 1 && noah_effective_combo_valid());
    for (unsigned input = 0; input < 16; input++)
        assert(combo_get(0)->keys[input] == 0x40 + input);
    assert(combo_get(0)->keys[16] == COMBO_END && noah_effective_combo_allowed_layers(0) == 0xffffu && noah_effective_combo_enabled(0));
    wide[1] = 8; // disabled: kept, and reported
    noah_effective_combo_runtime_invalidate(&runtime, 2, view.identity, view.identity, &view);
    assert(combo_count() == 1 && !noah_effective_combo_enabled(0) && combo_get(0)->keys[15] == 0x4f);
    view = combo_view(8 + 128 * 20);
    fail_read = true;
    noah_effective_combo_runtime_invalidate(&runtime, 3, view.identity, view.identity, &view);
    assert(!noah_effective_combo_valid() && combo_count() == 0 && combo_get(0) == NULL);
    uint8_t failed[32] = {8, 0, 6, 2};
    assert(noah_qmk_combo_readback_get(failed, 32) && failed[5] != 0);
    view.profile.domain_mask = 0;
    noah_effective_combo_runtime_invalidate(&runtime, 4, view.identity, view.identity, &view);
    assert(noah_effective_combo_valid() && combo_count() == 1 && combo_get(0) == &key_combos[0]);
    fail_read = false;
    memcpy(bytes, fixture_bytes, sizeof(fixture_bytes));
    uint32_t                            blockers = 1;
    noah_effective_profile_provider_t   provider;
    noah_effective_profile_snapshot_t   compiled, candidate;
    noah_profile_reader_t               compiled_reader  = {.read = read_bytes, .context = &runtime, .length = sizeof(bytes)};
    noah_profile_validator_v1_profile_t compiled_profile = {.byte_length = 8};
    assert(noah_effective_profile_snapshot_make_compiled(&compiled_profile, &compiled_reader, 0, &compiled) == NOAH_EFFECTIVE_PROFILE_OK);
    noah_effective_profile_invalidator_t invalidator = {.callback = noah_effective_combo_runtime_invalidate, .context = &runtime};
    assert(noah_effective_profile_provider_init(&provider, &compiled, boundary, &blockers, &invalidator, 1) == NOAH_EFFECTIVE_PROFILE_OK);
    view = combo_view(8 + 2 * 20);
    assert(noah_effective_profile_snapshot_make_validated(&view.profile, &view.reader, 0, 1, 0, 0, &candidate) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_request_validated(&provider, &candidate) == NOAH_EFFECTIVE_PROFILE_OK);
    unsigned before = reads;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_WAITING);
    assert(combo_count() == 1 && combo_get(0) == &key_combos[0] && reads == before);
    blockers = 0;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(combo_count() == 2 && reads == before + 7);
    assert(noah_effective_profile_provider_request_compiled_fallback(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    blockers = 1;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_WAITING && combo_count() == 2);
    blockers = 0;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED && combo_count() == 1);
    // The default window and hold threshold live in the header, and a row with
    // window zero follows the default.
    static const uint8_t v3[8 + 2 * 20] = {
        2, 0, 0, 0, 60, 0, 150, 0,
        2, 0, 0, 0, 0xff, 0xff, 0, 0, 1, 0, 41, 0, 1, 0, 4, 0, 1, 0, 5, 0,
        2, 6, 45, 0, 0xff, 0xff, 0, 0, 1, 0, 41, 0, 2, 0, 1, 0, 1, 0, 6, 0,
    };
    memcpy(bytes, v3, sizeof(v3));
    noah_effective_profile_snapshot_t second_view = combo_view(8 + 2 * 20);
    noah_effective_combo_runtime_invalidate(&runtime, 5, second_view.identity, second_view.identity, &second_view);
    assert(noah_effective_combo_valid() && combo_count() == 2);
    assert(get_combo_term(0, combo_get(0)) == 60 && get_combo_term(1, combo_get(1)) == 45);
    assert(noah_effective_combo_default_term() == 60 && noah_effective_combo_follows_default(0) && !noah_effective_combo_follows_default(1));
    assert(COMBO_HOLD_TERM == 150);
    uint8_t v2_report[32] = {8, 0, 6, 1, 0};
    assert(noah_qmk_combo_readback_get(v2_report, 32) && v2_report[5] == 0);
    assert(v2_report[7] == 3 && v2_report[17] == 60 && v2_report[19] == 150);
    v2_report[4] = 2;
    memset(&v2_report[5], 0, 27);
    assert(noah_qmk_combo_readback_get(v2_report, 32) && v2_report[11] == 60 && v2_report[12] == 0 && v2_report[13] == 8);
    v2_report[4] = 4;
    memset(&v2_report[5], 0, 27);
    assert(noah_qmk_combo_readback_get(v2_report, 32) && v2_report[11] == 45 && v2_report[13] == 6);
    // An empty version 2 table still carries both combo-wide values.
    bytes[0]                          = 0;
    second_view = combo_view(8);
    noah_effective_combo_runtime_invalidate(&runtime, 6, second_view.identity, second_view.identity, &second_view);
    assert(noah_effective_combo_valid() && combo_count() == 0 && noah_effective_combo_default_term() == 60 && COMBO_HOLD_TERM == 150);
    // A zero default would silently stop every combo that follows it.
    bytes[4] = 0;
    noah_effective_combo_runtime_invalidate(&runtime, 7, second_view.identity, second_view.identity, &second_view);
    assert(!noah_effective_combo_valid() && combo_count() == 0);
    noah_effective_combo_runtime_uninstall(&runtime);
    puts("effective combo runtime, QMK hooks and readback tests passed");
}
