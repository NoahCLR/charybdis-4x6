// ────────────────────────────────────────────────────────────────────────────
// Runtime Trace
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_trace.h"
#include "../shared/runtime_context_internal.h"

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

    state->next_index = (uint16_t)((state->next_index + 1u) % NOAH_RUNTIME_TRACE_CAPACITY);

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

    uint16_t start = (uint16_t)((state->next_index + NOAH_RUNTIME_TRACE_CAPACITY - state->count) % NOAH_RUNTIME_TRACE_CAPACITY);

    for (uint16_t i = 0; i < state->count; i++) {
        out->entries[i] = state->entries[(uint16_t)((start + i) % NOAH_RUNTIME_TRACE_CAPACITY)];
    }
}

void noah_runtime_trace_reset(void) {
    memset(runtime_trace_state(), 0, sizeof(*runtime_trace_state()));
}

#    if defined(CONSOLE_ENABLE)

#        include "print.h"

void noah_runtime_trace_dump_snapshot_to_console(const noah_runtime_trace_snapshot_t *snapshot) {
    if (!snapshot) {
        return;
    }

    uprintf("Runtime trace snapshot count=%u overflowed=%u\n", (unsigned int)snapshot->count, snapshot->overflowed ? 1u : 0u);
    for (uint16_t index = 0; index < snapshot->count; index++) {
        const noah_runtime_trace_entry_t *entry = &snapshot->entries[index];

        uprintf("  [%u] kind=%u event=%u a=0x%04X b=0x%04X\n", (unsigned int)index, (unsigned int)entry->kind, (unsigned int)entry->event, (unsigned int)entry->a, (unsigned int)entry->b);
    }
}

#    endif // defined(CONSOLE_ENABLE)

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
