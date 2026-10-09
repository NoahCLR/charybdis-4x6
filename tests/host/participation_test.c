// The participation policy (participation-policy.md, D-F14) against fake
// effective settings, behaviour rows and combo rows.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/key/behavior/participation.h"
#include "users/noah/lib/profile/runtime/effective_key_behavior_runtime.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"

static uint32_t scalars[NOAH_SETTINGS_COUNT];
static uint8_t  records[NOAH_SETTINGS_LAYERS][NOAH_SETTINGS_LAYER_RECORD_SIZE];
uint32_t        noah_setting(uint8_t id, uint32_t fallback) {
    (void)fallback;
    return scalars[id];
}
uint8_t noah_setting_layer_record(uint8_t layer, uint8_t field) {
    return layer < NOAH_SETTINGS_LAYERS && field < NOAH_SETTINGS_LAYER_RECORD_SIZE ? records[layer][field] : 0u;
}

// Rows: KC_W live (flags/allowed below), KC_X compiled, everything else none.
static noah_effective_key_behavior_row_t w_row;
noah_effective_key_behavior_result_t     noah_effective_key_behavior_lookup(uint16_t keycode, noah_effective_key_behavior_row_t *row) {
    if (keycode == KC_W) {
        *row = w_row;
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_OK;
    }
    return keycode == KC_X ? NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK : NOAH_EFFECTIVE_KEY_BEHAVIOR_NOT_FOUND;
}

static bool     combo_enabled[2]       = {true, true};
static uint32_t combo_layers[2]        = {UINT32_MAX, UINT32_MAX};
bool            noah_effective_combo_enabled(uint16_t index) {
    return index < 2u && combo_enabled[index];
}
uint32_t noah_effective_combo_allowed_layers(uint16_t index) {
    return index < 2u ? combo_layers[index] : 0u;
}

static void defaults(void) {
    memset(scalars, 0, sizeof(scalars));
    memset(records, 0, sizeof(records));
    scalars[NOAH_SETTING_BEHAVIORS_ENABLED] = 1u;
    scalars[NOAH_SETTING_LAYER_BEHAVIORS]   = 0xffffu;
    scalars[NOAH_SETTING_LAYER_COMBOS]      = 0xffffu;
    w_row                                   = (noah_effective_key_behavior_row_t){.allowed_layers = 0xffffu};
    combo_enabled[0] = combo_enabled[1] = true;
    combo_layers[0] = combo_layers[1] = 0xffffu;
}

static void set_placement(uint8_t layer, uint8_t field, keypos_t key_pos) {
    uint16_t position = (uint16_t)(key_pos.row * MATRIX_COLS + key_pos.col);
    records[layer][field + position / 8u] |= (uint8_t)(1u << (position % 8u));
}

static const keypos_t A = {.row = 1, .col = 2}, B = {.row = 7, .col = 7};

static void test_defaults_allow_everything(void) {
    defaults();
    for (uint8_t layer = 0; layer < 16; layer++) {
        assert(noah_participation_behavior(KC_W, layer, A) && noah_participation_behavior(KC_X, layer, B));
        assert(noah_participation_combo_press(layer, A) && noah_participation_combo_row(0, layer));
    }
}

// Behaviours and combos off on layer 2, with layers 1, 2 and 3 active: each
// press uses the layer that supplied its keycode, so a key sourced from 2 is
// bypassed and one that falls through 3 and 2 to 1 is not.
static void test_source_layer_decides(void) {
    defaults();
    scalars[NOAH_SETTING_LAYER_BEHAVIORS] &= ~(1u << 2);
    scalars[NOAH_SETTING_LAYER_COMBOS] &= ~(1u << 2);
    assert(noah_participation_behavior(KC_W, 3, A));
    assert(!noah_participation_behavior(KC_W, 2, A));
    assert(noah_participation_behavior(KC_W, 1, A));
    assert(noah_participation_combo_press(3, A) && !noah_participation_combo_press(2, A) && noah_participation_combo_press(1, A));
}

static void test_master_and_definition(void) {
    defaults();
    scalars[NOAH_SETTING_BEHAVIORS_ENABLED] = 0u;
    assert(!noah_participation_behavior(KC_W, 0, A) && !noah_participation_behavior(KC_X, 0, A));
    assert(!noah_participation_behavior_generated(KC_W, 0));
    assert(noah_participation_combo_press(0, A)); // behaviours and combos are independent
    defaults();
    w_row.flags = NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_DISABLED;
    assert(!noah_participation_behavior(KC_W, 0, A) && noah_participation_behavior(KC_X, 0, A));
    defaults();
    w_row.allowed_layers = 1u << 4;
    assert(noah_participation_behavior(KC_W, 4, A) && !noah_participation_behavior(KC_W, 5, A));
    // A lower scope's on cannot override an off master or layer.
    scalars[NOAH_SETTING_LAYER_BEHAVIORS] = 0xffffu & ~(1u << 4);
    assert(!noah_participation_behavior(KC_W, 4, A));
}

// One KC_W row serves every W placement: bypassing W at A on layer 3 leaves W
// at B on layer 3 and at A on layer 0 eligible. Behaviour bypass and combo
// exclusion are separate bits.
static void test_placement(void) {
    defaults();
    set_placement(3, NOAH_SETTINGS_LAYER_BYPASS, A);
    assert(!noah_participation_behavior(KC_W, 3, A));
    assert(noah_participation_behavior(KC_W, 3, B) && noah_participation_behavior(KC_W, 0, A));
    assert(noah_participation_combo_press(3, A));
    set_placement(3, NOAH_SETTINGS_LAYER_EXCLUDE, B);
    assert(!noah_participation_combo_press(3, B) && noah_participation_combo_press(3, A) && noah_participation_combo_press(0, B));
    // The last position of an 8 x 8 matrix is bit 63.
    set_placement(15, NOAH_SETTINGS_LAYER_BYPASS, B);
    assert(records[15][NOAH_SETTINGS_LAYER_BYPASS + 7] == 0x80u && !noah_participation_behavior(KC_W, 15, B));
    // Off the matrix there is no placement bit.
    assert(noah_participation_behavior(KC_W, 15, (keypos_t){.row = MATRIX_ROWS, .col = 0}));
}

// A generated custom key has no placement and no layer switch of its own:
// the master and its row's enable decide, on the combo's origin layer.
static void test_generated_outputs(void) {
    defaults();
    set_placement(2, NOAH_SETTINGS_LAYER_BYPASS, A);
    scalars[NOAH_SETTING_LAYER_BEHAVIORS] = 0u;
    assert(noah_participation_behavior_generated(KC_W, 2));
    w_row.allowed_layers = 1u << 1;
    assert(noah_participation_behavior_generated(KC_W, 1) && !noah_participation_behavior_generated(KC_W, 2));
    w_row.flags = NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_DISABLED;
    assert(!noah_participation_behavior_generated(KC_W, 1));
}

static void test_combo_rows(void) {
    defaults();
    combo_enabled[1] = false;
    assert(noah_participation_combo_row(0, 0) && !noah_participation_combo_row(1, 0));
    combo_layers[0] = (1u << 0) | (1u << 15);
    assert(noah_participation_combo_row(0, 15) && !noah_participation_combo_row(0, 7));
    assert(!noah_participation_combo_row(2, 0)); // no such combo
}

// A later press at the same position cannot rewrite buffered records or the
// matching release; both permission directions and source-bank edges persist.
static void test_record_context_is_immutable(void) {
    for (unsigned allowed = 0; allowed < 2; allowed++) {
        keyrecord_t press = {.event = {.key = A, .pressed = true, .type = KEY_EVENT}};
        noah_participation_press_store(A, allowed, !allowed);
        noah_participation_record_capture(&press, 15);
        keyrecord_t release = {.event = {.key = A, .type = KEY_EVENT}};
        noah_participation_record_capture(&release, 0);
        keyrecord_t next = {.event = {.key = A, .pressed = true, .type = KEY_EVENT}};
        noah_participation_press_store(A, !allowed, allowed);
        noah_participation_record_capture(&next, 3);
        assert(noah_participation_record_behavior(&press) == (bool)allowed);
        assert(noah_participation_record_combo(&press) == !allowed);
        assert(noah_participation_record_behavior(&release) == (bool)allowed);
        assert(noah_participation_record_source(&press) == 15 && noah_participation_record_source(&release) == 15);
        assert(noah_participation_record_behavior(&next) == !allowed && noah_participation_record_source(&next) == 3);
    }
}

int main(void) {
    test_defaults_allow_everything();
    test_source_layer_decides();
    test_master_and_definition();
    test_placement();
    test_generated_outputs();
    test_combo_rows();
    test_record_context_is_immutable();
    puts("participation policy host tests passed");
    return 0;
}
