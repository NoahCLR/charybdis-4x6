// ───────────────────────────────────────────────────────────────────────────
// A Clock That Stands Still While Storage Waits (D-F32)
// ───────────────────────────────────────────────────────────────────────────
//
// The profile owner runs inside the durable-I/O grant, which skips it while a
// save waits for a quiet keyboard before it may erase (D-F29). Its timers
// (the host's 15 s, the copy's no-progress window, the receive lease, the
// peer poll) must not count that wait: the owner's clock is the scan clock
// less every interval it was not allowed to run.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t paused_total;
    uint32_t paused_since;
    bool     paused;
} noah_profile_paused_clock_t;

// Once per scan, before the owner may run: whether it may not.
static inline void noah_profile_paused_clock_note(noah_profile_paused_clock_t *clock, bool paused, uint32_t now) {
    if (paused && !clock->paused) {
        clock->paused       = true;
        clock->paused_since = now;
    } else if (!paused && clock->paused) {
        clock->paused = false;
        clock->paused_total += now - clock->paused_since;
    }
}

static inline uint32_t noah_profile_paused_clock_now(const noah_profile_paused_clock_t *clock, uint32_t now) {
    return now - clock->paused_total - (clock->paused ? now - clock->paused_since : 0u);
}
