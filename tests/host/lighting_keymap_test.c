#include <assert.h>
#include <stdio.h>
#include "users/noah/lib/rgb/lighting/lighting_keymap.h"
#include "users/noah/lib/key/behavior/key_behavior_lookup.h"
#include "users/noah/lib/profile/runtime/effective_combo_runtime.h"
#include "users/noah/lib/profile/runtime/effective_key_behavior_runtime.h"
#include "noah_keymap_ids.h"

static uint16_t codes[LAYER_COUNT][NOAH_LIGHTING_MATRIX_KEYS];
static uint8_t rows[LAYER_COUNT][NOAH_LIGHTING_MATRIX_KEYS];
static uint32_t generation, reads, interrupt_at;
static combo_t combos[128];
static uint16_t inputs[128][17];
static uint32_t layers[128];
static bool enabled[128], read_error;
noah_effective_key_behavior_result_t noah_effective_key_behavior_lookup(uint16_t code, noah_effective_key_behavior_row_t *row) {
    (void)code; (void)row;
    return read_error ? NOAH_EFFECTIVE_KEY_BEHAVIOR_READ_ERROR : NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK;
}
layer_state_t layer_state, default_layer_state;

uint32_t noah_lighting_keymap_generation(void) { return generation; }
uint16_t keycode_at_keymap_location(uint8_t layer, uint8_t row, uint8_t col) {
    reads++;
    if (interrupt_at && reads == interrupt_at) generation++;
    return codes[layer][row*MATRIX_COLS+col];
}
key_behavior_view_t key_behavior_lookup(uint16_t code) {
    reads++;
    for (unsigned layer=0;layer<LAYER_COUNT;layer++) for(unsigned key=0;key<NOAH_LIGHTING_MATRIX_KEYS;key++)
        if(codes[layer][key]==code && rows[layer][key]) return (key_behavior_view_t){.lighting_identity=rows[layer][key]};
    return (key_behavior_view_t){0};
}
uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t col, uint8_t *out) {
    unsigned key=row*MATRIX_COLS+col;
    if(key>=NOAH_LIGHTING_LED_COUNT) return 0;
    out[0]=key;return 1;
}
uint16_t noah_effective_combo_count(void) { return 128; }
combo_t *noah_effective_combo_get(uint16_t row) { return row<128 ? &combos[row] : NULL; }
bool noah_effective_combo_enabled(uint16_t row) { return row<128 && enabled[row]; }
uint32_t noah_effective_combo_allowed_layers(uint16_t row) { return row<128 ? layers[row] : 0; }

int main(void) {
    for(unsigned row=0;row<128;row++) {
        combos[row].keys=inputs[row];enabled[row]=true;layers[row]=0xffff;
        for(unsigned i=0;i<16;i++) inputs[row][i]=KC_A;
        inputs[row][16]=COMBO_END;
    }
    for(unsigned layer=0;layer<LAYER_COUNT;layer++) for(unsigned key=0;key<NOAH_LIGHTING_MATRIX_KEYS;key++) codes[layer][key]=KC_Z;
    codes[15][0]=MO(15);codes[15][1]=PD_SLOT_31_LOCK;codes[15][2]=QK_MACRO_0+127;
    codes[15][3]=CUSTOM_KEY_127;rows[15][3]=128;
    generation=1;
    assert(noah_lighting_keymap_refresh(generation));
    assert(reads==2u*LAYER_COUNT*NOAH_LIGHTING_MATRIX_KEYS);
    uint32_t settled=reads;
    assert(noah_lighting_keymap_refresh(generation));assert(reads==settled);
    uint8_t kind,id;
    assert(noah_lighting_keymap_identity(15,0,&kind,&id) && kind==1 && id==15);
    assert(noah_lighting_keymap_identity(15,1,&kind,&id) && kind==2 && id==31);
    assert(noah_lighting_keymap_identity(15,2,&kind,&id) && kind==5 && id==127);
    assert(noah_lighting_keymap_matches(15,3,NOAH_LIGHTING_QUERY_BEHAVIOUR,127));
    assert(!noah_lighting_keymap_matches(15,3,NOAH_LIGHTING_QUERY_BEHAVIOUR,0));
    // 255 rules have a bounded query cost, independent of profile readers.
    noah_lighting_keymap_test_reset();
    for(unsigned rule=0;rule<255;rule++) for(unsigned key=0;key<NOAH_LIGHTING_MATRIX_KEYS;key++) assert(!noah_lighting_keymap_matches(0,key,NOAH_LIGHTING_QUERY_COMBO,127));
    assert(noah_lighting_keymap_test_comparisons()==255u*NOAH_LIGHTING_MATRIX_KEYS*16u);
    noah_lighting_keymap_test_reset();
    for(unsigned key=0;key<NOAH_LIGHTING_MATRIX_KEYS;key++) assert(!noah_lighting_keymap_matches(0,key,NOAH_LIGHTING_QUERY_COMBO,255));
    assert(noah_lighting_keymap_test_comparisons()==0u); // canonical any-combo bitmap
    assert(reads==settled);
    // Same coarse category/behaviour metadata, distinct literal input usages.
    inputs[127][0]=KC_Z;inputs[127][1]=COMBO_END;
    assert(noah_lighting_keymap_matches(0,0,NOAH_LIGHTING_QUERY_COMBO,127));
    enabled[127]=false;assert(!noah_lighting_keymap_matches(0,0,NOAH_LIGHTING_QUERY_COMBO,127));enabled[127]=true;
    layers[127]=2;assert(!noah_lighting_keymap_matches(0,0,NOAH_LIGHTING_QUERY_COMBO,127));
    assert(noah_lighting_keymap_matches(1,0,NOAH_LIGHTING_QUERY_COMBO,127));
    codes[15][3]=KC_TRNS;rows[15][3]=0;
    // Old coherent cache is stable until publication invalidates it.
    assert(noah_lighting_keymap_matches(15,3,NOAH_LIGHTING_QUERY_CUSTOM,255));
    generation++;interrupt_at=reads+3;
    assert(!noah_lighting_keymap_refresh(generation));
    assert(!noah_lighting_keymap_matches(15,3,NOAH_LIGHTING_QUERY_CUSTOM,255));
    interrupt_at=0;assert(noah_lighting_keymap_refresh(generation));
    assert(noah_lighting_keymap_matches(15,3,NOAH_LIGHTING_QUERY_TRANSPARENT,255));
    assert(!noah_lighting_keymap_matches(15,3,NOAH_LIGHTING_QUERY_BEHAVIOUR,255));
    generation++; read_error=true;
    assert(!noah_lighting_keymap_refresh(generation)); // failed reads cannot become absent rows
    read_error=false;assert(noah_lighting_keymap_refresh(generation));
    noah_lighting_keymap_invalidate();
    assert(!noah_lighting_keymap_identity(15,0,&kind,&id));
    assert(noah_lighting_keymap_refresh(generation));
    assert(noah_lighting_keymap_led_key(NOAH_LIGHTING_LED_COUNT)==255);
    printf("lighting keymap: %u keys, exact selectors/publication retry/read-free queries; specific255-rule comparisons=%u; any128-row query comparisons=0; rebuild comparison upper bound=%u\n",NOAH_LIGHTING_MATRIX_KEYS,255u*NOAH_LIGHTING_MATRIX_KEYS*16u,LAYER_COUNT*128u*NOAH_LIGHTING_MATRIX_KEYS*16u);
}
