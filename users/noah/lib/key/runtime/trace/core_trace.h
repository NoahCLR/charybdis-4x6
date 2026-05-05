// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Trace
// ────────────────────────────────────────────────────────────────────────────
//
// Shared normalized input/output trace helpers for the reducer-owned runtime.
// Input events are recorded as the QMK-facing orchestration layer feeds them
// into key_runtime_core, and projection checkpoints are emitted after each event so
// replay can compare both externally visible output traces and quiescent end
// state.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../reducer/runtime.h"

#include "../../../state/diagnostics/runtime_trace.h"

#if defined(NOAH_RUNTIME_TRACE_ENABLE)

void     key_runtime_core_trace_record_input_event(const runtime_event_t *event);
void     key_runtime_core_trace_record_projection_snapshot(const projection_snapshot_t *snapshot);
void     key_runtime_core_trace_capture_projection(void);
uint16_t key_runtime_core_trace_decode_input_events(const noah_runtime_trace_snapshot_t *snapshot, runtime_event_t *out, uint16_t capacity);

#else

static inline void key_runtime_core_trace_record_input_event(const runtime_event_t *event) {
    (void)event;
}

static inline void key_runtime_core_trace_record_projection_snapshot(const projection_snapshot_t *snapshot) {
    (void)snapshot;
}

static inline void key_runtime_core_trace_capture_projection(void) {}

static inline uint16_t key_runtime_core_trace_decode_input_events(const noah_runtime_trace_snapshot_t *snapshot, runtime_event_t *out, uint16_t capacity) {
    (void)snapshot;
    (void)out;
    (void)capacity;
    return 0u;
}

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
