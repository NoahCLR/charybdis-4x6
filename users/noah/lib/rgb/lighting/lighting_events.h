#pragma once

#include QMK_KEYBOARD_H
#include <stdint.h>

// Internal producer occurrences, not wire kind IDs or recent-event booleans.
// Producers call synchronously after their actual transition. The observer
// must consume/copy borrowed owners before returning and may not reenter an
// input producer. No observer means no work, queue or advertised capability.
typedef enum {
    NOAH_LIGHTING_EVENT_LAYER_LOCKED,
    NOAH_LIGHTING_EVENT_LAYER_UNLOCKED,
    NOAH_LIGHTING_EVENT_ONESHOT_USED,
    NOAH_LIGHTING_EVENT_MODE_STARTED,
    NOAH_LIGHTING_EVENT_MODE_ENDED,
    NOAH_LIGHTING_EVENT_COMBO_FIRED,
    NOAH_LIGHTING_EVENT_TAP_FIRED,
    NOAH_LIGHTING_EVENT_HOLD_FIRED,
    NOAH_LIGHTING_EVENT_LONG_HOLD_FIRED,
    NOAH_LIGHTING_EVENT_MACRO_PLAYED,
} noah_lighting_event_kind_t;

typedef struct {
    const uint8_t *owners; // borrowed bitmap; NULL => use valid key, or no keys
    keypos_t key;
    uint8_t kind;
    uint8_t thing; // actual stable layer/mode/combo/behaviour/macro ID
    uint8_t branch;
    uint8_t slot;
    uint8_t outcome;
} noah_lighting_event_t;

#ifndef NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
#    define NOAH_LIGHTING_EVENT_CAPTURE_ENABLE 0
#endif
#if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
// Enabled builds must link the real synchronous consumer. No weak fallback.
void noah_lighting_event_observe(const noah_lighting_event_t *event);
static inline bool noah_lighting_events_observed(void) { return true; }
static inline void noah_lighting_event_emit(const noah_lighting_event_t *event) { noah_lighting_event_observe(event); }
#else
static inline bool noah_lighting_events_observed(void) { return false; }
static inline void noah_lighting_event_emit(const noah_lighting_event_t *event) { (void)event; }
#endif
