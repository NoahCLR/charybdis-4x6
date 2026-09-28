#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Transport admission owns no input or lighting state. Times are unsigned
// elapsed values; source timestamps are opaque identities, not ordered clocks.
typedef struct {
    uint32_t observed[3];
    uint32_t changed_at;
    uint32_t timeout;
    bool     known;
    bool     pending;
} noah_split_activity_policy_t;

enum { NOAH_SPLIT_ACTIVITY_INTERVAL_MS = 32u };

static inline bool noah_split_activity_due(noah_split_activity_policy_t *state, const uint32_t current[3], uint32_t now, uint32_t since_success, uint32_t sent_activity_age, uint32_t timeout, bool force) {
    bool changed      = !state->known || memcmp(state->observed, current, sizeof(state->observed)) != 0;
    bool resumed      = changed && (!state->known || (uint32_t)(now - state->changed_at) >= NOAH_SPLIT_ACTIVITY_INTERVAL_MS);
    bool reconfigured = state->known && state->timeout != timeout;
    if (changed) {
        memcpy(state->observed, current, sizeof(state->observed));
        state->changed_at = now;
    }
    state->known   = true;
    state->timeout = timeout;
    // Preserve wake and imminent sleep. Very short timeouts retain original
    // per-change delivery. A final pending snapshot flushes even after input stops.
    bool boundary = timeout != 0u && (timeout <= 2u * NOAH_SPLIT_ACTIVITY_INTERVAL_MS || sent_activity_age >= timeout - NOAH_SPLIT_ACTIVITY_INTERVAL_MS);
    state->pending |= force || resumed || reconfigured || boundary || since_success >= NOAH_SPLIT_ACTIVITY_INTERVAL_MS;
    return state->pending;
}

static inline void noah_split_activity_complete(noah_split_activity_policy_t *state, bool success) {
    if (success) state->pending = false;
}
