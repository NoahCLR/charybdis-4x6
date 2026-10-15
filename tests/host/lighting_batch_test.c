#include "users/noah/lib/rgb/lighting/lighting_batch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t random_state=0x981274adu;
static uint32_t next_random(void) {random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static uint8_t bytes[NOAH_LIGHTING_BATCH_BYTES],saved[NOAH_LIGHTING_BATCH_BYTES];
static noah_lighting_subject_t original[NOAH_LIGHTING_BATCH_SUBJECTS];
static void roundtrip(unsigned count) {
    noah_lighting_batch_writer_t writer;
    noah_lighting_batch_write_begin(&writer,bytes,sizeof bytes);
    for(unsigned i=0;i<count;i++) assert(noah_lighting_batch_write(&writer,&original[i]));
    assert(writer.count==count && writer.used<=sizeof bytes);
    noah_lighting_batch_reader_t reader;
    assert(noah_lighting_batch_read_begin(&reader,bytes,writer.used,4,127));
    for(unsigned i=0;i<count;i++) {
        noah_lighting_subject_t subject;
        assert(noah_lighting_batch_read(&reader,&subject));
        assert(!memcmp(&subject,&original[i],sizeof subject));
    }
    noah_lighting_subject_t unused;
    assert(!noah_lighting_batch_read(&reader,&unused) && reader.remaining==0 && reader.offset==writer.used);
    // A short target rejects atomically, preserving every prior member.
    for(size_t capacity=0;capacity<writer.used;capacity++) {
        noah_lighting_batch_writer_t short_writer;
        noah_lighting_batch_write_begin(&short_writer,saved,capacity);
        for(unsigned i=0;i<count;i++) {
            size_t before=short_writer.used;
            uint8_t prior_count=short_writer.count;
            uint8_t before_bytes[NOAH_LIGHTING_BATCH_BYTES];memcpy(before_bytes,saved,sizeof saved);
            if(!noah_lighting_batch_write(&short_writer,&original[i])) {
                assert(short_writer.used==before && short_writer.count==prior_count);
                assert(!memcmp(before_bytes,saved,sizeof saved));break;
            }
        }
    }
}
int main(void) {
    for(unsigned i=0;i<64;i++) {
        noah_lighting_subject_init(&original[i],4,127,0);
        original[i].has_keys=true;original[i].sides=i<30?1:2;
        unsigned key=i%(KEY_ORIGIN_BITMAP_SIZE*8u);
        original[i].keys[key/8]|=1u<<(key%8);
    }
    roundtrip(64);
    noah_lighting_batch_writer_t writer;
    noah_lighting_batch_write_begin(&writer,bytes,sizeof bytes);
    for(unsigned i=0;i<64;i++)assert(noah_lighting_batch_write(&writer,&original[i]));
    assert(writer.used==129); // common slot/outcome plus exact per-key order
    memcpy(saved,bytes,writer.used);size_t frozen=writer.used;
    // Later partial departure updates a new whole batch; frozen bytes own no
    // producer pointers and remain valid across remap/owner scratch mutation.
    memset(original,0,sizeof original);
    assert(bytes[0]==64 && !memcmp(saved,bytes,frozen));
    for(unsigned sample=0;sample<256;sample++) {
        unsigned count=sample%65;
        for(unsigned i=0;i<count;i++) {
            noah_lighting_subject_init(&original[i],4,127,(uint8_t)next_random());
            original[i].outcome=(uint8_t)next_random();original[i].has_keys=!!(next_random()&1);original[i].sides=next_random()&3;
            for(unsigned j=0;j<KEY_ORIGIN_BITMAP_SIZE;j++)original[i].keys[j]=(uint8_t)next_random();
        }
        roundtrip(count);
    }
    for(unsigned i=0;i<64;i++) {
        noah_lighting_subject_init(&original[i],4,127,(uint8_t)(i+1));original[i].outcome=(uint8_t)(i+2);original[i].has_keys=true;original[i].sides=3;memset(original[i].keys,0xff,sizeof original[i].keys);
    }
    roundtrip(64);noah_lighting_batch_write_begin(&writer,bytes,sizeof bytes);
    for(unsigned i=0;i<64;i++)assert(noah_lighting_batch_write(&writer,&original[i]));
    assert(writer.used==NOAH_LIGHTING_BATCH_BYTES); // proved maximum achieved
    memcpy(saved,bytes,sizeof saved);assert(!noah_lighting_batch_write(&writer,&original[0]));assert(!memcmp(saved,bytes,sizeof bytes));
    noah_lighting_batch_reader_t reader;
    uint8_t malformed[]={1,0xc0};assert(noah_lighting_batch_read_begin(&reader,malformed,sizeof malformed,4,127));assert(!noah_lighting_batch_read(&reader,&original[0]));
    uint8_t malformed_key[]={1,0x28,64};assert(noah_lighting_batch_read_begin(&reader,malformed_key,sizeof malformed_key,4,127));assert(!noah_lighting_batch_read(&reader,&original[0]));
    printf("ordered retained batches: exact owners/slot/outcome/order, partial batches, atomic insufficient workspace;64-member same-paint129B, exact max%uB (per batch, not bank capacity)\n",(unsigned)NOAH_LIGHTING_BATCH_BYTES);
}
