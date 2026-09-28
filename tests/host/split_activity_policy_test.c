#include <assert.h>
#include <stdio.h>
#include "users/noah/lib/compat/qmk_split_activity_policy.h"

static void motion_and_render(uint32_t timeout, uint32_t start) {
    noah_split_activity_policy_t state      = {0};
    uint32_t                     current[3] = {0}, sent[3] = {0};
    uint32_t                     last_success = start;
    unsigned                     sends        = 0;
    for (uint32_t tick = 0; tick < 3000; ++tick) {
        uint32_t now = start + tick;
        // Two motion bursts and matrix input; final timestamps must survive stop.
        if ((tick >= 100 && tick <= 1100) || (tick >= 1600 && tick <= 1700)) current[2] = now;
        if (tick == 1400) current[0] = now;
        uint32_t latest_sent = sent[0] > sent[2] ? sent[0] : sent[2];
        bool     dirty       = memcmp(current, sent, sizeof(sent)) != 0;
        bool     force       = tick == 0 || (uint32_t)(now - last_success) >= 100;
        bool     due         = noah_split_activity_due(&state, current, now, now - last_success, now - latest_sent, timeout, force);
        if (force || (dirty && due)) {
            memcpy(sent, current, sizeof(sent));
            last_success = now;
            ++sends;
            noah_split_activity_complete(&state, true);
        }
        uint32_t latest = current[0] > current[2] ? current[0] : current[2];
        latest_sent     = sent[0] > sent[2] ? sent[0] : sent[2];
        assert((timeout && (uint32_t)(now - latest) > timeout) == (timeout && (uint32_t)(now - latest_sent) > timeout));
        if (tick == 100 || tick == 1600 || tick == 1400) assert(!memcmp(current, sent, sizeof(sent)));
        if (tick == 1140 || tick == 1740) assert(!memcmp(current, sent, sizeof(sent)));
    }
    if (timeout > 100 || timeout == 0) assert(sends < 100);
}

int main(void) {
    const uint32_t timeouts[] = {0, 1, 31, 32, 64, 65, 80, 100, 900000};
    for (unsigned i = 0; i < sizeof(timeouts) / sizeof(timeouts[0]); ++i) {
        motion_and_render(timeouts[i], 10000);
        motion_and_render(timeouts[i], UINT32_MAX - 1500);
    }
    noah_split_activity_policy_t state      = {0};
    uint32_t                     current[3] = {100, 0, 100};
    assert(noah_split_activity_due(&state, current, 100, 0, 0, 900000, true));
    noah_split_activity_complete(&state, false);
    assert(noah_split_activity_due(&state, current, 101, 1, 1, 900000, false));
    noah_split_activity_complete(&state, true);
    current[2] = 102;
    assert(!noah_split_activity_due(&state, current, 102, 1, 2, 900000, false));
    assert(noah_split_activity_due(&state, current, 103, 2, 3, 10, false));
    noah_split_activity_complete(&state, true);
    assert(noah_split_activity_due(&state, current, 104, 1, 4, 0, false));
    puts("split activity policy tests passed");
}
