// Reuse the existing QMK/runtime stand-ins, not its reference renderer. Every
// assertion below calls the shipping working owner and scene.working resolver.
#define main lighting_reference_vectors_main
#include "lighting_render_vectors_test.c"
#undef main

static noah_lighting_subject_t ordered[8];
static unsigned ordered_count;
static void record_order(void *context, const noah_lighting_subject_t *subject) {
    (void)context;
    assert(ordered_count < sizeof ordered / sizeof *ordered);
    ordered[ordered_count++] = *subject;
}
static void require(bool ok, const char *name, unsigned step, const char *what) {
    if (!ok) { fprintf(stderr,"PRODUCTION ordering %s step%u: %s\n",name,step,what);exit(1); }
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file=fopen(argv[1],"r");assert(file);
    noah_profile_lighting_v1_limits_t limits=noah_profile_lighting_v1_default_limits();
    static noah_profile_lighting_v1_program_t program;
    static uint8_t bytes[8192],pool[8220];
    noah_lighting_working_t working={0};
    char tag[16],name[128],hex[16385];
    unsigned cases=0,steps=0,rejected=0;
    while (fscanf(file,"%15s",tag)==1) {
        assert(!strcmp(tag,"CASE"));
        unsigned cells,expected_cost,admitted,start,expected_rule;
        assert(fscanf(file,"%127s %16384s %u %u %u %u %u",name,hex,&cells,&expected_cost,&admitted,&start,&expected_rule)==7 && cells==2);
        size_t size=strlen(hex)/2u;
        for(size_t i=0;i<size;i++){unsigned v;assert(sscanf(hex+2*i,"%2x",&v)==1);bytes[i]=v;}
        noah_profile_lighting_v1_view_t view;
        assert(noah_profile_lighting_v1_decode(bytes,size,&limits,&view,NULL)==NOAH_PROFILE_LIGHTING_V1_OK);
        noah_lighting_working_cost_t cost;
        noah_profile_lighting_v1_result_t result=noah_profile_lighting_v1_working_admit(&view,sizeof pool,&cost);
        bool accepted=result==NOAH_PROFILE_LIGHTING_V1_OK;
        require(cost.required==expected_cost && accepted==!!admitted,name,0,"actual registry/geometry admission");
        if(!accepted)assert(result==NOAH_PROFILE_LIGHTING_V1_WORKING_MEMORY_EXCEEDED);
        if(accepted) {
            assert(noah_profile_lighting_v1_program_load(&view,&program));
            assert(noah_lighting_working_begin(&working,&program,pool,sizeof pool,++cases));
        }
        else rejected++;
        unsigned kind[2],thing[2];
        for(unsigned c=0;c<cells;c++)assert(fscanf(file,"%u %u",&kind[c],&thing[c])==2);
        unsigned last[2][2]={{0}},last_count[2]={0};
        uint32_t now=start;
        if(accepted){noah_lighting_engine_program_changed();noah_lighting_working_advance(&working,now);}
        unsigned step=0;
        while(fscanf(file,"%15s",tag)==1 && strcmp(tag,"END")) {
            assert(!strcmp(tag,"STEP"));
            unsigned elapsed,matching[2],keys[2][2]={{0}};
            assert(fscanf(file,"%u",&elapsed)==1);now+=elapsed;
            noah_lighting_facts_t facts={0};
            noah_lighting_facts_init_synced(&facts);
            facts.layer_active[0]=1u;
            for(unsigned c=0;c<cells;c++) {
                assert(fscanf(file,"%u",&matching[c])==1 && matching[c]<=2);
                for(unsigned m=0;m<matching[c];m++) {
                    unsigned key;assert(fscanf(file,"%u",&key)==1 && key<NOAH_LIGHTING_MATRIX_KEYS);keys[c][m]=key;
                    if(kind[c]==NOAH_LIGHTING_THING_BEHAVIOUR) {
                        uint8_t *record=facts.behaviour+key*KEY_FEEDBACK_LIGHTING_RECORD_BYTES;
                        assert(!record[0]); // one real behaviour identity per physical key
                        record[0]=thing[c]+1u;record[1]=1u;record[2]=1u+c+key;
                        facts.layer_status[key]=record[2]; // this held action's real layer outcome
                        unsigned layer=record[2]-1u;facts.layer_active[layer/8u]|=1u<<(layer%8u);
                    } else {
                        assert(kind[c]==NOAH_LIGHTING_THING_LAYER && !facts.layer_status[key]);
                        facts.layer_status[key]=thing[c]+1u; // real held-layer ownership
                        facts.layer_active[thing[c]/8u]|=1u<<(thing[c]%8u);
                    }
                }
                if(matching[c]){last_count[c]=matching[c];memcpy(last[c],keys[c],sizeof keys[c]);}
            }
            layer_state=(layer_state_t)facts.layer_active[0]|(layer_state_t)facts.layer_active[1]<<8u;
            facts.layer_status[NOAH_LIGHTING_LAYER_TOP_OFFSET]=get_highest_layer(layer_state);
            char pixel[8];unsigned members,expected[8][2],active[2];int relation;
            assert(fscanf(file,"%7s %u",pixel,&members)==2 && members<=8);
            for(unsigned m=0;m<members;m++)assert(fscanf(file,"%u %u",&expected[m][0],&expected[m][1])==2);
            assert(fscanf(file,"%u %u %d",&active[0],&active[1],&relation)==3);
            if(!accepted)continue; // explicitly non-admitted, never paint a fake bank
            noah_lighting_working_advance(&working,now);
            require(noah_lighting_working_observe(&working,&facts),name,step,"actual coherent observation");
            noah_lighting_working_rule_t descriptor;
            assert(noah_lighting_working_rule(&working,0,&descriptor));
            assert(descriptor.rule==expected_rule);
            const uint8_t *slots[2];
            for(unsigned c=0;c<cells;c++) {
                slots[c]=noah_lighting_working_slot(&working,false,&descriptor,thing[c]);
                // These are the documented slot codec fields, not a new debug
                // API. Compare only relations: ranks may be freely renumbered.
                require(slots[c] && !!slots[c][4]==!!active[c],name,step,"entry exists before maturity / disappears on expiry");
            }
            if(active[0] && active[1]) {
                int actual=(slots[0][6]>slots[1][6])-(slots[0][6]<slots[1][6]);
                require(actual==-relation,name,step,"relative entry recency / simultaneous equality");
            }
            ordered_count=0;
            assert(noah_lighting_working_paint(&working,descriptor.rule,record_order,NULL));
            require(ordered_count==members,name,step,"complete visible member count");
            for(unsigned m=0;m<members;m++) {
                unsigned c=expected[members-1u-m][0],ordinal=expected[members-1u-m][1];
                assert(c<cells && ordinal<last_count[c]);
                unsigned key=last[c][ordinal];
                const noah_lighting_subject_t *s=&ordered[m];
                uint8_t owners[KEY_ORIGIN_BITMAP_SIZE]={0};owners[key/8u]|=1u<<(key%8u);
                require(s->thing_kind==kind[c] && s->thing==thing[c] && s->slot==0u &&
                    s->outcome==(kind[c]==NOAH_LIGHTING_THING_BEHAVIOUR?1u+c+key:0u) &&
                    s->has_keys && !memcmp(s->keys,owners,sizeof owners) &&
                    s->sides==(key<NOAH_LIGHTING_MATRIX_KEYS/2u?1u:2u),name,step,"ordered full member/owner/slot/outcome");
            }
            noah_lighting_scene_t scene={.program=&program,.facts=&facts,.working=&working};
            noah_lighting_frame_t full={0},chunks={0};noah_lighting_resolve_info_t info={0};
            noah_lighting_resolve(&scene,NOAH_LIGHTING_ALL_LEDS,now,true,&full,&info);
            noah_lighting_resolve(&scene,NOAH_LIGHTING_LEFT_LEDS,now,false,&chunks,&info);
            noah_lighting_resolve(&scene,NOAH_LIGHTING_RIGHT_LEDS,now,false,&chunks,&info);
            bool painted=strcmp(pixel,"-")!=0;
            require(full.painted==(painted?NOAH_LIGHTING_ALL_LEDS:0u) && chunks.painted==full.painted,name,step,"whole/chunk scene paint coverage");
            unsigned r=0,g=0,b=0;
            if(painted)assert(sscanf(pixel,"%2x%2x%2x",&r,&g,&b)==3);
            for(unsigned led=0;painted && led<NOAH_LIGHTING_LED_COUNT;led++)
                require(full.colour[led].r==r && full.colour[led].g==g && full.colour[led].b==b &&
                    !memcmp(&full.colour[led],&chunks.colour[led],sizeof(rgb_t)),name,step,"shipping whole+half scene pixels");
            step++;steps++;
        }
        printf("production ordering %s geometry%ux%u Q%u/%zu %s\n",name,MATRIX_ROWS,MATRIX_COLS,cost.required,sizeof pool,accepted?"PASS":"NONADMITTED reference mapping only");
    }
    fclose(file);
    assert(cases==(NOAH_LIGHTING_MATRIX_KEYS==2u?8u:1u));
    assert(steps==(NOAH_LIGHTING_MATRIX_KEYS==2u?35u:3u));
    assert(rejected==(NOAH_LIGHTING_MATRIX_KEYS==2u?0u:7u));
    printf("production ordering: %u admitted traces/%u steps, %u explicitly nonadmitted; shipping entry relations/member order/whole+half scene pixels PASS\n",cases,steps,rejected);
    return 0;
}
