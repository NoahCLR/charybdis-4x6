"""Compile the actual QMK activity sender with a fault-injectable transport."""
from pathlib import Path
import subprocess
import sys
qmk, build, root = map(Path, sys.argv[1:])
source = (qmk / 'quantum/split_common/transactions.c').read_text()
start = source.index('static bool activity_handlers_master(')
end = source.index('\nstatic void activity_handlers_slave', start)
handler = source[start:end]
fixture = r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "users/noah/lib/compat/qmk_split_activity_policy.h"
typedef uint8_t matrix_row_t;
typedef struct { uint32_t matrix_timestamp, encoder_timestamp, pointing_device_timestamp; } split_slave_activity_sync_t;
#define FORCED_SYNC_THROTTLE_MS 100
#define PUT_ACTIVITY 1
static uint32_t now;
static bool connected=true, fail;
static unsigned calls;
static split_slave_activity_sync_t current, staging, received;
static noah_split_activity_policy_t policy;
static bool resend;
static bool split_resend_due(int id) {assert(id==PUT_ACTIVITY); return resend;}
static void split_resend_done(int id) {assert(id==PUT_ACTIVITY); resend=false;}
static uint32_t timer_read32(void) {return now;}
static uint32_t timer_elapsed32(uint32_t then) {return now-then;}
static bool is_transport_connected(void) {return connected;}
static uint32_t last_matrix_activity_time(void) {return current.matrix_timestamp;}
static uint32_t last_encoder_activity_time(void) {return current.encoder_timestamp;}
static uint32_t last_pointing_device_activity_time(void) {return current.pointing_device_timestamp;}
static bool transport_write(int id, const void *data, unsigned length) {
    assert(id==PUT_ACTIVITY && length==sizeof(staging));
    ++calls; memcpy(&staging,data,length); // QMK stages even failed writes
    if(fail) return false;
    received=staging; return true;
}
static bool split_activity_sync_should_send(const split_slave_activity_sync_t *value,const split_slave_activity_sync_t *sent,uint32_t last,bool once,bool force) {
    uint32_t times[3]={value->matrix_timestamp,value->encoder_timestamp,value->pointing_device_timestamp};
    return noah_split_activity_due(&policy,times,now,now-last,now-sent->pointing_device_timestamp,900000,force||!once);
}
static void split_activity_sync_sent(bool success) {noah_split_activity_complete(&policy,success);}
'''
fixture += handler
fixture += r'''
int main(void) {
    assert(activity_handlers_master(NULL,NULL)); assert(calls==1);
    now=100; current.pointing_device_timestamp=100;
    assert(activity_handlers_master(NULL,NULL)); assert(received.pointing_device_timestamp==100);
    unsigned before=calls;
    for(now=101;now<132;++now) {current.pointing_device_timestamp=now; assert(activity_handlers_master(NULL,NULL));}
    assert(calls==before);
    fail=true; current.pointing_device_timestamp=132;
    assert(!activity_handlers_master(NULL,NULL)); assert(staging.pointing_device_timestamp==132);
    assert(received.pointing_device_timestamp==100);
    fail=false; ++now;
    assert(activity_handlers_master(NULL,NULL)); assert(received.pointing_device_timestamp==132);
    now=134; current.pointing_device_timestamp=134;
    assert(activity_handlers_master(NULL,NULL)); assert(received.pointing_device_timestamp==132);
    now=165; assert(activity_handlers_master(NULL,NULL)); assert(received.pointing_device_timestamp==134);
    before=calls; now=166; connected=false;
    assert(activity_handlers_master(NULL,NULL)); assert(calls==before+1);
    connected=true; now=266; before=calls;
    assert(activity_handlers_master(NULL,NULL)); assert(calls==before+1);
    before=calls;
    for(now=1000;now<2000;++now) {
        current.pointing_device_timestamp=now;
        assert(activity_handlers_master(NULL,NULL));
    }
    assert(calls-before==32); // first immediate plus one latest snapshot per 32 ms
    // A write the slave reported dropped is resent on the next scan, past
    // coalescing, and the flag clears once the resend succeeds.
    now=2000; current.pointing_device_timestamp=2000; before=calls; resend=true;
    assert(activity_handlers_master(NULL,NULL)); assert(calls==before+1); assert(!resend);
    assert(received.pointing_device_timestamp==2000);
    fail=true; resend=true; ++now;
    assert(!activity_handlers_master(NULL,NULL)); assert(resend);
    fail=false; ++now;
    assert(activity_handlers_master(NULL,NULL)); assert(!resend);
    puts("actual QMK activity sender tests passed: 1000 motion scans -> 32 writes");
}
'''
p = build / 'qmk_activity.c'
p.write_text(fixture)
binary=build/'qmk_activity'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-I',str(root),str(p),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
