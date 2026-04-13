// ────────────────────────────────────────────────────────────────────────────
// Runtime Trace
// ────────────────────────────────────────────────────────────────────────────
//
// Optional structured trace sink for cross-subsystem runtime debugging. Keep
// this intentionally small: one shared ring buffer with subsystem/event ids
// plus two 16-bit payload slots.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NOAH_TRACE_KEY_RUNTIME = 0,
    NOAH_TRACE_PD_MODE,
    NOAH_TRACE_LAYER_OWNERSHIP,
    NOAH_TRACE_SPLIT_SYNC,
} noah_trace_kind_t;

typedef enum {
    NOAH_TRACE_KEY_RUNTIME_EVENT_PLAN = 0,
    NOAH_TRACE_KEY_RUNTIME_EVENT_EFFECT_EXECUTE,
} noah_trace_key_runtime_event_t;

typedef enum {
    NOAH_TRACE_KEY_RUNTIME_STAGE_UNKNOWN = 0,
    NOAH_TRACE_KEY_RUNTIME_STAGE_INTERRUPT_ACTIVE_KEY,
    NOAH_TRACE_KEY_RUNTIME_STAGE_FLUSH_MULTI_TAP,
    NOAH_TRACE_KEY_RUNTIME_STAGE_PRESS,
    NOAH_TRACE_KEY_RUNTIME_STAGE_RELEASE,
    NOAH_TRACE_KEY_RUNTIME_STAGE_SCAN,
} noah_trace_key_runtime_stage_t;

typedef enum {
    NOAH_TRACE_PD_MODE_EVENT_ACTIVATE = 0,
    NOAH_TRACE_PD_MODE_EVENT_DEACTIVATE,
    NOAH_TRACE_PD_MODE_EVENT_LOCK,
    NOAH_TRACE_PD_MODE_EVENT_UNLOCK,
    NOAH_TRACE_PD_MODE_EVENT_REMOTE_SNAPSHOT,
} noah_trace_pd_mode_event_t;

typedef enum {
    NOAH_TRACE_LAYER_OWNERSHIP_EVENT_LOCK = 0,
    NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_PRESS,
    NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_RELEASE,
    NOAH_TRACE_LAYER_OWNERSHIP_EVENT_OVERFLOW,
} noah_trace_layer_ownership_event_t;

typedef enum {
    NOAH_TRACE_SPLIT_SYNC_EVENT_INIT = 0,
    NOAH_TRACE_SPLIT_SYNC_EVENT_SEND,
    NOAH_TRACE_SPLIT_SYNC_EVENT_RECEIVE,
} noah_trace_split_sync_event_t;

typedef struct {
    uint8_t  kind;
    uint8_t  event;
    uint16_t a;
    uint16_t b;
} noah_runtime_trace_entry_t;

#ifndef NOAH_RUNTIME_TRACE_CAPACITY
#    define NOAH_RUNTIME_TRACE_CAPACITY 32u
#endif

typedef struct {
    noah_runtime_trace_entry_t entries[NOAH_RUNTIME_TRACE_CAPACITY];
    uint8_t                    count;
    bool                       overflowed;
} noah_runtime_trace_snapshot_t;

#if defined(NOAH_RUNTIME_TRACE_ENABLE)

void noah_runtime_trace_emit(noah_trace_kind_t kind, uint8_t event, uint16_t a, uint16_t b);
void noah_runtime_trace_snapshot(noah_runtime_trace_snapshot_t *out);
void noah_runtime_trace_reset(void);

#else

static inline void noah_runtime_trace_emit(noah_trace_kind_t kind, uint8_t event, uint16_t a, uint16_t b) {
    (void)kind;
    (void)event;
    (void)a;
    (void)b;
}

static inline void noah_runtime_trace_snapshot(noah_runtime_trace_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (noah_runtime_trace_snapshot_t){0};
}

static inline void noah_runtime_trace_reset(void) {}

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
