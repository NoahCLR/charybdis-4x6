// Shared calendar traces through the admitted production owner. State batch
// markers are exact held-layer owner bitmaps from the real registered walker.
#include "users/noah/lib/rgb/lighting/lighting_working.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
layer_state_t layer_state;
uint8_t get_highest_layer(layer_state_t state){(void)state;assert(!"fixture supplies coherent facts");return 0;}
void layer_ownership_lighting_projection(uint8_t *out){(void)out;assert(!"fixture supplies coherent facts");}
bool noah_lighting_fact_next(uint8_t kind,const noah_lighting_facts_t *facts,const uint8_t *p,uint16_t *cursor,noah_lighting_subject_t *s) {
    assert(kind==9);return noah_lighting_fact_layer_status_next(facts,p,cursor,s);
}
static noah_profile_lighting_v1_program_t program;
static noah_lighting_working_t work;
static uint8_t storage[NOAH_LIGHTING_WORKING_CAPACITY];
static uint32_t now;
static void begin(unsigned show,unsigned linger,unsigned flash) {
    uint8_t bytes[48]={5,0,58,0,0,0,0,1,0,0,255};unsigned used=11;
    bytes[used++]=flash?1:2;bytes[used++]=flash?28:9;bytes[used++]=flash?127:1;if(!flash)bytes[used++]=1;
    bytes[used++]=0x40;bytes[used++]=4;bytes[used++]=0x60;bytes[used++]=0;
    unsigned values[]={show,linger,flash};
    for(unsigned i=0;i<3;i++)if(values[i]){bytes[used++]=0xa2;bytes[used++]=i+1;bytes[used++]=values[i];bytes[used++]=values[i]>>8;}
    bytes[8]=used-9;bytes[used++]=0;
    noah_profile_lighting_v1_view_t view;noah_profile_lighting_v1_limits_t limits=noah_profile_lighting_v1_default_limits();
    assert(noah_profile_lighting_v1_decode(bytes,used,&limits,&view,NULL)==NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view,&program));
    assert(noah_lighting_working_begin(&work,&program,storage,sizeof storage,1));
    now=0;noah_lighting_working_advance(&work,now);
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *input=fopen(argv[1],"r");assert(input);
    unsigned show=0,linger=0,flash=0,cases=0,steps=0;char word[96],name[96];
    while(fscanf(input,"%95s",word)==1) {
        if(!strcmp(word,"CASE")){assert(fscanf(input,"%95s %u %u %u",name,&show,&linger,&flash)==4);begin(show,linger,flash);cases++;continue;}
        unsigned long elapsed;unsigned operation,marker,visible,wake,batch;
        assert(!strcmp(word,"STEP") && fscanf(input,"%lu %u %u %u %u %u",&elapsed,&operation,&marker,&visible,&wake,&batch)==6);
        now+=(uint32_t)elapsed;noah_lighting_working_advance(&work,now);
        if(operation==3) {
            noah_lighting_event_group_t event={.thing_kind=5,.thing=127,.has_keys=1,.sides=1,.owners={(uint8_t)marker},.qualified={1}};
            assert(noah_lighting_working_event(&work,&event));
        } else if(operation==1 || operation==2) {
            noah_lighting_facts_t facts={0};
            if(operation==1)for(unsigned k=0;k<7;k++)if(marker & (1u<<k))facts.layer_status[k]=2;
            assert(noah_lighting_working_observe(&work,&facts));
        }
        noah_lighting_working_rule_t r;assert(noah_lighting_working_rule(&work,0,&r));
        const uint8_t *cell=noah_lighting_working_slot(&work,false,&r,flash?127:1);
        noah_lighting_lifetime_parameters_t timing={show,linger,flash};
        uint32_t due=noah_lighting_working_wake(&work);if(due==UINT32_MAX)due=0;
        if(noah_lighting_working_visible(cell,&timing)!=!!visible || due!=wake){fprintf(stderr,"production calendar %s step%u visible%u/%u wake%u/%u\n",name,steps,noah_lighting_working_visible(cell,&timing),visible,due,wake);return 1;}
        if(batch){noah_lighting_subject_t subject;assert(cell[5]==1);noah_lighting_working_member(cell+12,flash?5:1,flash?127:1,&subject,NULL);assert(subject.has_keys && subject.keys[0]==batch);}
        else {
            // Reference `batch` denotes eligible rendering ownership. The
            // direct reservation may cache an ineligible matching batch, but
            // it must neither paint nor create an eligible departed tail.
            assert(!cell[5] || (cell[4]==NOAH_LIGHTING_WORKING_MATCH && !visible && !(cell[7]&NOAH_LIGHTING_WORKING_ELIGIBLE)));
        }
        steps++;
    }
    fclose(input);printf("PRODUCTION admitted working owner: all %u shared lifetime cases/%u steps visibility/whole owners/repeat/reentry/expiry/wake/max delay PASS\n",cases,steps);
}
