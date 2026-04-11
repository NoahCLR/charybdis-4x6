// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Scan Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal scan-event protocol used behind the shared slot-result surface.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_slot_effect.h"

typedef enum {
    KEY_RUNTIME_SLOT_SCAN_EVENT_NONE = 0,
    KEY_RUNTIME_SLOT_SCAN_EVENT_ACTIVE_EFFECTS,
    KEY_RUNTIME_SLOT_SCAN_EVENT_PENDING_MULTI_TAP_EFFECTS,
    KEY_RUNTIME_SLOT_SCAN_EVENT_PENDING_MULTI_TAP_FLUSH,
} key_runtime_slot_scan_event_kind_t;

typedef struct {
    key_runtime_slot_effect_request_t immediate_hold_request;
    key_runtime_slot_effect_request_t effect_request;
} key_runtime_slot_active_scan_effects_t;

typedef struct {
    bool                              release_layer_before_action;
    key_runtime_slot_effect_request_t effect_request;
} key_runtime_slot_pending_multi_tap_scan_effects_t;

typedef struct {
    bool                               handled;
    key_runtime_slot_scan_event_kind_t kind;
    keypos_t                           key_pos;
    union {
        key_runtime_slot_active_scan_effects_t            active_effects;
        key_runtime_slot_pending_multi_tap_scan_effects_t pending_multi_tap_effects;
        key_runtime_slot_pending_multi_tap_flush_t        pending_multi_tap_flush;
    } data;
} key_runtime_slot_scan_event_t;

key_runtime_slot_scan_event_t key_runtime_slot_take_active_scan_event(active_key_state_t *slot);
key_runtime_slot_scan_event_t key_runtime_slot_take_pending_multi_tap_scan_event(active_key_state_t *slot);
