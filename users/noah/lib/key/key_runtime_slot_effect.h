// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Effects
// ────────────────────────────────────────────────────────────────────────────
//
// Shared effect-request contract for slot result surfaces.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_state.h"

typedef enum {
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE = 0,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_REPEAT_START,
} key_runtime_slot_effect_request_kind_t;

typedef struct {
    bool                                   release_owned_state;
    bool                                   feedback_pulse;
    bool                                   feedback_long_hold_level;
    key_runtime_slot_effect_request_kind_t kind;
    uint16_t                               action;
    uint16_t                               repeat_hz;
} key_runtime_slot_effect_request_t;
