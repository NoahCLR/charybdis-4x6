// ────────────────────────────────────────────────────────────────────────────
// Runtime Context Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Canonical userspace-owned runtime storage. The concrete layout is internal
// to the runtime owner layer so callers cannot reach aggregate state through a
// public context type.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../action/owned_keycode.h"
#include "../../key/ownership/held_action.h"
#include "../../key/ownership/held_repeat.h"
#include "../ownership/keyboard_mod_ownership.h"
#include "../ownership/layer_ownership.h"
#include "../diagnostics/runtime_trace.h"
#include "runtime_shared_state_internal.h"

// The layers a key's shortcuts own (D-F26), by owner: a custom key it holds
// through its held action, and a Tap-toggle or One-shot custom key's own
// momentary hold. One key can have both at once (the key's own layers from the
// press, and the custom key its behaviour holds), so each keeps its own. Both
// none is a free entry.
typedef struct {
    keypos_t      key_pos;
    layer_state_t held;
    layer_state_t momentary;
} layer_ownership_shortcut_binding_t;

typedef struct {
    layer_ownership_binding_snapshot_t bindings[LAYER_OWNERSHIP_BINDING_CAPACITY];
    layer_ownership_shortcut_binding_t shortcut_bindings[LAYER_OWNERSHIP_BINDING_CAPACITY];
    // Each layer's key owners: its momentary bindings, the held shortcuts
    // naming it and the shortcut taps in flight.
    uint8_t                            momentary_refcounts[LAYER_COUNT];
    layer_state_t                      locked_mask;
    layer_state_t                      oneshot_mask;
    // The layers the running pointing mode's key turns on (D-F62).
    layer_state_t                      pointing_mode_layer_mask;
    uint16_t                           oneshot_armed_at;
} noah_layer_ownership_state_t;

typedef struct {
    held_action_binding_snapshot_t modifiers[HELD_ACTION_BINDING_CAPACITY];
    uint8_t                        modifier_refcounts[8];
    held_action_binding_snapshot_t actions[HELD_ACTION_BINDING_CAPACITY];
    owned_keycode_lease_t          action_leases[HELD_ACTION_BINDING_CAPACITY];
    uint8_t                        action_lease_managed[(HELD_ACTION_BINDING_CAPACITY + 7u) / 8u];
} noah_held_action_state_t;

typedef struct {
    held_repeat_binding_snapshot_t bindings[HELD_REPEAT_BINDING_CAPACITY];
    uint8_t                        active_count;
} noah_held_repeat_state_t;

typedef struct {
    uint8_t physical_refcounts[KEYBOARD_MOD_OWNERSHIP_MOD_COUNT];
    uint8_t managed_refcounts[KEYBOARD_MOD_OWNERSHIP_MOD_COUNT];
    uint8_t report_refcounts[KEYBOARD_MOD_OWNERSHIP_MOD_COUNT];
    uint8_t warned_state[KEYBOARD_MOD_OWNERSHIP_MOD_COUNT];
} noah_keyboard_mod_ownership_state_t;

typedef struct {
    noah_runtime_trace_entry_t entries[NOAH_RUNTIME_TRACE_CAPACITY];
    uint16_t                   next_index;
    uint16_t                   count;
    bool                       overflowed;
} noah_runtime_trace_state_t;

typedef struct noah_runtime_context_t {
    runtime_shared_state_t              shared;
    noah_layer_ownership_state_t        layer_ownership;
    noah_held_action_state_t            held_actions;
    noah_held_repeat_state_t            held_repeats;
    noah_keyboard_mod_ownership_state_t keyboard_mod_ownership;
    noah_runtime_trace_state_t          trace;
} noah_runtime_context_t;

noah_runtime_context_t *noah_runtime_context(void);
void                    noah_runtime_context_reset_for_test(noah_runtime_context_t *ctx);
