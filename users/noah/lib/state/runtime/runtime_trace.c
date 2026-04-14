// ────────────────────────────────────────────────────────────────────────────
// Runtime Trace
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_trace.h"
#include "runtime_context_internal.h"

#if defined(NOAH_RUNTIME_TRACE_ENABLE)

#    include <string.h>

static noah_runtime_trace_state_t *runtime_trace_state(void) {
    return &noah_runtime_context()->trace;
}

void noah_runtime_trace_emit(noah_trace_kind_t kind, uint8_t event, uint16_t a, uint16_t b) {
    noah_runtime_trace_state_t *state = runtime_trace_state();

    state->entries[state->next_index] = (noah_runtime_trace_entry_t){
        .kind  = (uint8_t)kind,
        .event = event,
        .a     = a,
        .b     = b,
    };

    state->next_index = (uint8_t)((state->next_index + 1u) % NOAH_RUNTIME_TRACE_CAPACITY);

    if (state->count < NOAH_RUNTIME_TRACE_CAPACITY) {
        state->count++;
    } else {
        state->overflowed = true;
    }
}

void noah_runtime_trace_snapshot(noah_runtime_trace_snapshot_t *out) {
    const noah_runtime_trace_state_t *state = runtime_trace_state();

    if (!out) {
        return;
    }

    *out = (noah_runtime_trace_snapshot_t){
        .count      = state->count,
        .overflowed = state->overflowed,
    };

    uint8_t start = (uint8_t)((state->next_index + NOAH_RUNTIME_TRACE_CAPACITY - state->count) % NOAH_RUNTIME_TRACE_CAPACITY);

    for (uint8_t i = 0; i < state->count; i++) {
        out->entries[i] = state->entries[(uint8_t)((start + i) % NOAH_RUNTIME_TRACE_CAPACITY)];
    }
}

void noah_runtime_trace_reset(void) {
    memset(runtime_trace_state(), 0, sizeof(*runtime_trace_state()));
}

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
