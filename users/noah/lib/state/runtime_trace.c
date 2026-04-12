// ────────────────────────────────────────────────────────────────────────────
// Runtime Trace
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_trace.h"

#if defined(NOAH_RUNTIME_TRACE_ENABLE)

#    include <string.h>

typedef struct {
    noah_runtime_trace_entry_t entries[NOAH_RUNTIME_TRACE_CAPACITY];
    uint8_t                    next_index;
    uint8_t                    count;
    bool                       overflowed;
} noah_runtime_trace_state_t;

static noah_runtime_trace_state_t noah_runtime_trace_state = {0};

void noah_runtime_trace_emit(noah_trace_kind_t kind, uint8_t event, uint16_t a, uint16_t b) {
    noah_runtime_trace_state.entries[noah_runtime_trace_state.next_index] = (noah_runtime_trace_entry_t){
        .kind  = (uint8_t)kind,
        .event = event,
        .a     = a,
        .b     = b,
    };

    noah_runtime_trace_state.next_index = (uint8_t)((noah_runtime_trace_state.next_index + 1u) % NOAH_RUNTIME_TRACE_CAPACITY);

    if (noah_runtime_trace_state.count < NOAH_RUNTIME_TRACE_CAPACITY) {
        noah_runtime_trace_state.count++;
    } else {
        noah_runtime_trace_state.overflowed = true;
    }
}

void noah_runtime_trace_snapshot(noah_runtime_trace_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (noah_runtime_trace_snapshot_t){
        .count      = noah_runtime_trace_state.count,
        .overflowed = noah_runtime_trace_state.overflowed,
    };

    uint8_t start = (uint8_t)((noah_runtime_trace_state.next_index + NOAH_RUNTIME_TRACE_CAPACITY - noah_runtime_trace_state.count) % NOAH_RUNTIME_TRACE_CAPACITY);

    for (uint8_t i = 0; i < noah_runtime_trace_state.count; i++) {
        out->entries[i] = noah_runtime_trace_state.entries[(uint8_t)((start + i) % NOAH_RUNTIME_TRACE_CAPACITY)];
    }
}

void noah_runtime_trace_reset(void) {
    memset(&noah_runtime_trace_state, 0, sizeof(noah_runtime_trace_state));
}

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
