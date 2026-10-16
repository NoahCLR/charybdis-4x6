#include "users/noah/lib/compat/qmk_lighting_live.h"
#include <stdio.h>
int main(void) {
    uint8_t p[25];uint8_t length=noah_lighting_live_capability(p);
    for(uint8_t i=0;i<length;i++)printf("%02x",p[i]);
    puts("");return 0;
}
