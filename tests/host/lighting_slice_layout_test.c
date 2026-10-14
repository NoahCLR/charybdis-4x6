// Producer-sized writes and last-record reads under nondefault geometry/buffers.
#include "users/noah/lib/rgb/lighting/lighting.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#include <assert.h>
#include <string.h>
layer_state_t layer_state;
uint8_t get_highest_layer(layer_state_t state) {
    uint8_t top=0;
    for(uint8_t i=0;i<LAYER_COUNT;i++) if(state & ((layer_state_t)1u<<i)) top=i;
    return top;
}
void layer_ownership_lighting_projection(uint8_t *out) {
    memset(out,0,LAYER_OWNERSHIP_LIGHTING_BYTES);
    out[MATRIX_ROWS*MATRIX_COLS-1u]=16u;
    out[LAYER_OWNERSHIP_LIGHTING_LOCK_OFFSET+1u]=0x80u;
    out[LAYER_OWNERSHIP_LIGHTING_ONESHOT_OFFSET]=15u;
}
void key_feedback_lighting_phases(uint8_t *out) {
    memset(out,0,KEY_FEEDBACK_LIGHTING_PHASE_BYTES);
    uint8_t *last=out+KEY_FEEDBACK_LIGHTING_PHASE_BYTES-KEY_FEEDBACK_LIGHTING_PHASE_RECORD_BYTES;
    last[0]=128u;last[1]=8u;last[2]=1u;
}
void noah_qmk_combo_origin_lighting_layers(uint8_t *out) {
    memset(out,255u,COMBO_BUFFER_LENGTH*NOAH_LIGHTING_COMBO_RECORD_BYTES);
    uint8_t *last=out+(COMBO_BUFFER_LENGTH-1u)*NOAH_LIGHTING_COMBO_RECORD_BYTES;
    last[0]=127u;last[1]=15u;
    key_origin_bitmap_fill_single(last+2u,(keypos_t){.row=MATRIX_ROWS-1u,.col=MATRIX_COLS-1u});
}
int main(void) {
    struct { uint8_t pre;noah_lighting_facts_t facts;uint8_t post; } frame;
    memset(&frame,0xa5,sizeof(frame));layer_state=(layer_state_t)1u<<15u;
    noah_lighting_slice_layer_status_idle(frame.facts.layer_status);
    noah_lighting_slice_layer_status_collect(frame.facts.layer_status);
    noah_lighting_slice_behaviour_phase_idle(frame.facts.behaviour_phase);
    noah_lighting_slice_behaviour_phase_collect(frame.facts.behaviour_phase);
    noah_lighting_slice_combo_layer_idle(frame.facts.combo_layer);
    noah_lighting_slice_combo_layer_collect(frame.facts.combo_layer);
    uint8_t layer[]={15u,1u},phase[]={127u,3u,255u},combo[]={127u,15u};
    noah_lighting_subject_t subject;uint16_t cursor=0;
    assert(noah_lighting_fact_layer_status_next(&frame.facts,layer,&cursor,&subject));
    assert(subject.thing==15u && subject.has_keys);
    cursor=0;assert(noah_lighting_fact_behaviour_phase_next(&frame.facts,phase,&cursor,&subject));
    assert(subject.thing==127u && subject.slot==0u);
    cursor=0;assert(noah_lighting_fact_combo_layer_next(&frame.facts,combo,&cursor,&subject));
    assert(subject.thing==127u && subject.has_keys);
    assert(key_origin_bitmap_has_keypos(subject.keys,(keypos_t){.row=MATRIX_ROWS-1u,.col=MATRIX_COLS-1u}));
    assert(frame.pre==0xa5 && frame.post==0xa5);
    return 0;
}
