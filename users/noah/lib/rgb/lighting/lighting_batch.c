#include "lighting_batch.h"
#include <string.h>

enum { OWNER_NONE, OWNER_KEY, OWNER_BITMAP, SAME_PAINT = 32u };
void noah_lighting_batch_write_begin(noah_lighting_batch_writer_t *writer, uint8_t *bytes, size_t size) {
    *writer = (noah_lighting_batch_writer_t){.bytes=bytes,.size=size,.used=size ? 1u : 0u};
    if (bytes && size) bytes[0]=0;
}
bool noah_lighting_batch_write(noah_lighting_batch_writer_t *writer, const noah_lighting_subject_t *subject) {
    if (!writer || !writer->bytes || !writer->size || !subject || subject->sides > 3u || writer->count >= NOAH_LIGHTING_BATCH_SUBJECTS) return false;
    unsigned key_count=0u;
    uint8_t key=0u;
    for (unsigned i=0u;i<64u;i++) if (subject->keys[i/8u] & (1u << (i%8u))) {key_count++;key=(uint8_t)i;}
    uint8_t owner=key_count==0u ? OWNER_NONE : key_count==1u ? OWNER_KEY : OWNER_BITMAP;
    bool same=subject->slot==writer->slot && subject->outcome==writer->outcome;
    size_t length=1u+(same?0u:2u)+(owner==OWNER_NONE?0u:owner==OWNER_KEY?1u:KEY_ORIGIN_BITMAP_SIZE);
    if (writer->used>writer->size || length>writer->size-writer->used) return false;
    uint8_t *at=writer->bytes+writer->used;
    *at++=(subject->sides&3u)|(subject->has_keys?4u:0u)|(owner<<3u)|(same?SAME_PAINT:0u);
    if (!same) {*at++=subject->slot;*at++=subject->outcome;}
    if (owner==OWNER_KEY) *at++=key;
    else if (owner==OWNER_BITMAP) memcpy(at,subject->keys,KEY_ORIGIN_BITMAP_SIZE);
    writer->used+=length;writer->count++;
    writer->bytes[0]=writer->count;
    writer->slot=subject->slot;writer->outcome=subject->outcome;
    return true;
}
bool noah_lighting_batch_read_begin(noah_lighting_batch_reader_t *reader, const uint8_t *bytes, size_t size, uint8_t thing_kind, uint8_t thing) {
    if (!reader || !bytes || !size || bytes[0]>NOAH_LIGHTING_BATCH_SUBJECTS) return false;
    *reader=(noah_lighting_batch_reader_t){.bytes=bytes,.size=size,.offset=1u,.remaining=bytes[0],.thing_kind=thing_kind,.thing=thing};
    return true;
}
bool noah_lighting_batch_read(noah_lighting_batch_reader_t *reader, noah_lighting_subject_t *subject) {
    if (!reader || !subject || !reader->remaining || reader->offset>=reader->size) return false;
    size_t offset=reader->offset;
    uint8_t flags=reader->bytes[offset++];
    uint8_t owner=(flags>>3u)&3u;
    if ((flags&0xc0u) || owner>OWNER_BITMAP) return false;
    size_t length=(flags&SAME_PAINT?0u:2u)+(owner==OWNER_NONE?0u:owner==OWNER_KEY?1u:KEY_ORIGIN_BITMAP_SIZE);
    if (length>reader->size-offset) return false;
    uint8_t slot=reader->slot,outcome=reader->outcome;
    if (!(flags&SAME_PAINT)) {slot=reader->bytes[offset++];outcome=reader->bytes[offset++];}
    noah_lighting_subject_init(subject,reader->thing_kind,reader->thing,slot);
    subject->outcome=outcome;subject->has_keys=!!(flags&4u);subject->sides=flags&3u;
    if (owner==OWNER_KEY) {
        uint8_t key=reader->bytes[offset++];if(key>=64u)return false;
        subject->keys[key/8u]|=1u<<(key%8u);
    } else if (owner==OWNER_BITMAP) {memcpy(subject->keys,reader->bytes+offset,KEY_ORIGIN_BITMAP_SIZE);offset+=KEY_ORIGIN_BITMAP_SIZE;}
    reader->offset=offset;reader->remaining--;reader->slot=slot;reader->outcome=outcome;
    return true;
}
