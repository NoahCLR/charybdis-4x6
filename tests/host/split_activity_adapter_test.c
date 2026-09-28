#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "transactions.h"
static uint32_t now, timeout=900000;
uint32_t timer_read32(void) {return now;}
uint32_t timer_elapsed32(uint32_t t) {return now-t;}
uint32_t sync_timer_elapsed32(uint32_t t) {return now+1000-t;}
uint32_t noah_setting(uint8_t id,uint32_t fallback) {(void)id;(void)fallback;return timeout;}
int main(void) {
    split_slave_activity_sync_t sent={0,0,1000}, current=sent;
    assert(split_activity_sync_should_send(&current,&sent,0,false,true));
    split_activity_sync_sent(true);
    now=1;current.pointing_device_timestamp=1001;
    assert(!split_activity_sync_should_send(&current,&sent,0,true,false));
    now=32;current.pointing_device_timestamp=1032;
    assert(split_activity_sync_should_send(&current,&sent,0,true,false));
    split_activity_sync_sent(false);
    now=33;assert(split_activity_sync_should_send(&current,&sent,0,true,false));
    split_activity_sync_sent(true);sent=current;
    now=34;current.pointing_device_timestamp=1034;
    assert(!split_activity_sync_should_send(&current,&sent,33,true,false));
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    timeout=1;
    assert(split_activity_sync_should_send(&current,&sent,33,true,false));
    split_activity_sync_sent(true);sent=current;
    timeout=0; now=35;current.pointing_device_timestamp=1035;
    assert(split_activity_sync_should_send(&current,&sent,34,true,false));
#endif
    puts("actual activity adapter tests passed");
}
