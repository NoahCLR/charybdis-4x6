#include "users/noah/lib/rgb/lighting/lighting_event_protocol.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static uint8_t digit(char c) {
    return c <= '9' ? (uint8_t)(c-'0') : (uint8_t)(c-'a'+10);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *vectors = fopen(argv[1], "r");
    assert(vectors);
    char verdict[8], hex[130];
    while (fscanf(vectors, "%7s %129s", verdict, hex) == 2) {
        uint8_t wire[64];
        size_t length = strlen(hex) / 2u;
        assert(length <= sizeof wire);
        for (size_t i = 0; i < length; i++) wire[i] = (uint8_t)(digit(hex[2*i])*16u + digit(hex[2*i+1]));
        noah_lighting_event_group_t decoded;
        bool valid = noah_lighting_event_group_decode(&decoded,wire,length);
        assert(valid == (verdict[0] == 'V'));
        if (valid) {
            uint8_t roundtrip[54];
            assert(noah_lighting_event_group_encode(roundtrip,sizeof roundtrip,&decoded));
            assert(!memcmp(roundtrip,wire,length));
        }
    }
    fclose(vectors);
    noah_lighting_event_group_t g = {.order = UINT64_C(0xfedcba9876543210), .thing_kind = 5, .thing = 127, .slot = 2, .outcome = 128, .sides = 3, .has_keys = 1, .owners = {0x81,0,0,0,0,0,0,8}, .qualified = {1}};
    g.qualified[31] = 0x40; // highest accepted rule254
    uint8_t bytes[54];
    assert(noah_lighting_event_group_encode(bytes, sizeof bytes, &g));
    assert(bytes[0] == 0x10 && bytes[7] == 0xfe && bytes[53] == 0x40);
    noah_lighting_event_group_t read = {0};
    assert(noah_lighting_event_group_decode(&read,bytes,sizeof bytes));
    uint8_t again[54];
    assert(noah_lighting_event_group_encode(again,sizeof again,&read));
    assert(memcmp(bytes,again,sizeof bytes) == 0);
    bytes[53] |= 0x80;
    assert(!noah_lighting_event_group_decode(&read,bytes,sizeof bytes));
    assert(read.order == g.order && read.qualified[31] == 0x40);
    for(unsigned n = 0; n < 54; n++) assert(!noah_lighting_event_group_decode(&read,bytes,n));
}
