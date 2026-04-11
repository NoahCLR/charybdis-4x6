// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Step
// ────────────────────────────────────────────────────────────────────────────
//
// Reducer-style slot event contract for the handled-key runtime. Higher-level
// orchestration feeds slot events through this one seam instead of calling
// separate press/release/scan result producers directly.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "handled_key.h"
#include "key_runtime_slot_result.h"

typedef enum {
    KEY_RUNTIME_SLOT_EVENT_NONE = 0,
    KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS,
    KEY_RUNTIME_SLOT_EVENT_HANDLED_RELEASE,
    KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN,
    KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN,
    KEY_RUNTIME_SLOT_EVENT_INTERRUPT,
    KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_FLUSH,
} key_runtime_slot_event_kind_t;

typedef struct {
    key_runtime_slot_event_kind_t kind;
    union {
        struct {
            uint16_t           keycode;
            keypos_t           key_pos;
            handled_key_view_t key;
            bool               active_held_action_survives_flush;
        } handled_press;
        struct {
            uint16_t            keycode;
            keypos_t            key_pos;
            key_behavior_view_t behavior;
        } handled_release;
        struct {
            keypos_t other_key_pos;
        } interrupt;
    } data;
} key_runtime_slot_event_t;

key_runtime_slot_result_t key_runtime_slot_step(active_key_state_t *slot, key_runtime_slot_event_t event);
