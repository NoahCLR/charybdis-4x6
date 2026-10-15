#include "users/noah/lib/rgb/lighting/lighting_lifetime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Deliberately all255 rules *128 things. These host cells test independent
// transition semantics; they are not a production reservation or arena bound.
static noah_lighting_lifetime_t independent[255u][128u];
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *input = fopen(argv[1], "r");
    assert(input);
    unsigned cases=0, steps=0, show, linger, flash;
    char word[96], name[96];
    noah_lighting_lifetime_t cell = {{0}};
    noah_lighting_lifetime_parameters_t parameters = {0};
    unsigned retained_batch = 0;
    while (fscanf(input,"%95s",word)==1) {
        if (!strcmp(word,"CASE")) {
            assert(fscanf(input,"%95s %u %u %u",name,&show,&linger,&flash)==4);
            parameters=(noah_lighting_lifetime_parameters_t){show,linger,flash};
            cell=(noah_lighting_lifetime_t){{0}};retained_batch=0;cases++;continue;
        }
        assert(!strcmp(word,"STEP"));
        unsigned long delta;
        unsigned observation,batch,visible,wake,expected_batch;
        assert(fscanf(input,"%lu %u %u %u %u %u",&delta,&observation,&batch,&visible,&wake,&expected_batch)==6);
        uint8_t action=noah_lighting_lifetime_observe(&cell,&parameters,(uint32_t)delta,observation);
        if (action==NOAH_LIGHTING_BATCH_REPLACE) retained_batch=batch;
        if (action==NOAH_LIGHTING_BATCH_RELEASE_RENDER) retained_batch=0;
        if (retained_batch!=expected_batch || noah_lighting_lifetime_visible(&cell,&parameters)!=!!visible || noah_lighting_lifetime_wake(&cell,&parameters)!=wake) {
            fprintf(stderr,"lifetime %s step%u: expected batch%u visible%u wake%u, got batch%u visible%u wake%lu\n",name,steps,expected_batch,visible,wake,retained_batch,noah_lighting_lifetime_visible(&cell,&parameters),(unsigned long)noah_lighting_lifetime_wake(&cell,&parameters));return 1;
        }
        steps++;
    }
    fclose(input);
    assert(cases>=13 && steps>=60);
    parameters=(noah_lighting_lifetime_parameters_t){.flash=65535};
    for (unsigned r=0;r<255;r++) for (unsigned t=0;t<128;t++) {
        noah_lighting_lifetime_observe(&independent[r][t],&parameters,0,NOAH_LIGHTING_LIFETIME_EVENT);
        noah_lighting_lifetime_observe(&independent[r][t],&parameters,r*128u+t,NOAH_LIGHTING_LIFETIME_NONQUALIFYING);
    }
    for (unsigned r=0;r<255;r++) for (unsigned t=0;t<128;t++) {
        assert(noah_lighting_lifetime_visible(&independent[r][t],&parameters));
        assert(noah_lighting_lifetime_wake(&independent[r][t],&parameters)==65535u-r*128u-t);
    }
    printf("lifetime controls: %u shared cases/%u steps;255rules*128 independent things; packed cell3B (not a history-bank bound)\n",cases,steps);
}
