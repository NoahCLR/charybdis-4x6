// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Effect Builders
// ────────────────────────────────────────────────────────────────────────────
//
// Reducer-local builder contract for slot-policy helper surfaces.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_state.h"

typedef enum {
    KEY_RUNTIME_EFFECT_BUILDER_NONE = 0,
    KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION,
    KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER,
    KEY_RUNTIME_EFFECT_BUILDER_HELD_UNREGISTER,
    KEY_RUNTIME_EFFECT_BUILDER_REPEAT_START,
} key_runtime_effect_builder_kind_t;

typedef struct {
    bool                              release_owned_state;
    bool                              feedback_pulse;
    bool                              feedback_long_hold_level;
    key_runtime_effect_builder_kind_t kind;
    uint16_t                          action;
    uint16_t                          repeat_hz;
} key_runtime_effect_builder_t;
