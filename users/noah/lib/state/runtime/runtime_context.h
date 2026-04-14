// ────────────────────────────────────────────────────────────────────────────
// Runtime Context
// ────────────────────────────────────────────────────────────────────────────
//
// Canonical userspace-owned runtime storage. This composes the split key/pd
// aggregate with the state slices that were previously held in private module
// statics so reset/debug helpers can operate over one owner.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../key/ownership/held_action.h"
#include "../../key/ownership/held_repeat.h"
#include "../ownership/keyboard_mod_ownership.h"
#include "../ownership/layer_ownership.h"
#include "runtime_shared_state.h"
#include "runtime_trace.h"

typedef struct {
    layer_ownership_binding_snapshot_t bindings[LAYER_OWNERSHIP_BINDING_CAPACITY];
    uint8_t                            momentary_refcounts[LAYER_COUNT];
    layer_state_t                      locked_mask;
} noah_layer_ownership_state_t;

typedef struct {
    held_action_binding_snapshot_t modifiers[HELD_ACTION_BINDING_CAPACITY];
    uint8_t                        modifier_refcounts[8];
    held_action_binding_snapshot_t actions[HELD_ACTION_BINDING_CAPACITY];
} noah_held_action_state_t;

typedef struct {
    held_repeat_binding_snapshot_t bindings[HELD_REPEAT_BINDING_CAPACITY];
} noah_held_repeat_state_t;

typedef struct {
    uint8_t physical_refcounts[KEYBOARD_MOD_OWNERSHIP_MOD_COUNT];
    uint8_t managed_refcounts[KEYBOARD_MOD_OWNERSHIP_MOD_COUNT];
    uint8_t warned_state[KEYBOARD_MOD_OWNERSHIP_MOD_COUNT];
} noah_keyboard_mod_ownership_state_t;

typedef struct {
    noah_runtime_trace_entry_t entries[NOAH_RUNTIME_TRACE_CAPACITY];
    uint8_t                    next_index;
    uint8_t                    count;
    bool                       overflowed;
} noah_runtime_trace_state_t;

typedef struct {
    runtime_shared_state_t             shared;
    noah_layer_ownership_state_t       layer_ownership;
    noah_held_action_state_t           held_actions;
    noah_held_repeat_state_t           held_repeats;
    noah_keyboard_mod_ownership_state_t keyboard_mod_ownership;
    noah_runtime_trace_state_t         trace;
} noah_runtime_context_t;

noah_runtime_context_t *noah_runtime_context(void);
void                    noah_runtime_context_reset_for_test(noah_runtime_context_t *ctx);
