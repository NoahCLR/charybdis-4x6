// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────
//
// Key-runtime-owned storage layout for active slots, deferred multi-tap state,
// and per-key feedback pulse state. The aggregate runtime layer composes this
// into the broader userspace state, but key runtime owns the slot model.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../state/runtime/keyboard_mod_state.h"
#include "../interaction/multi_tap_engine.h"
#include "key_runtime_interaction.h"
#include "key_runtime_types.h"

typedef enum {
    KEY_RUNTIME_SLOT_PHASE_IDLE = 0,
    KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
    KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
    KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING,
    KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE,
    KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
} key_runtime_slot_phase_t;

typedef struct {
    uint16_t keycode;
    keypos_t key_pos;
} key_runtime_slot_owner_state_t;

typedef struct {
    key_runtime_slot_phase_t phase;
    uint16_t                 held_action_keycode;
    bool                     repeat_binding_active;
    bool                     pd_mode_was_locked_on_press;
    bool                     layer_interrupted;
} key_runtime_slot_lifecycle_state_t;

// One handled-key runtime slot: active press/hold state plus any deferred
// multi-tap chain that still owns this physical key position after release.
typedef struct {
    uint16_t                           timer;
    key_runtime_slot_owner_state_t     owner;
    key_runtime_slot_lifecycle_state_t lifecycle;
    key_runtime_slot_interaction_t     interaction;
    multi_tap_t                        pending_multi_tap;
} active_key_state_t;

#define KEY_RUNTIME_SLOT_TABLE_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))

typedef active_key_state_t key_runtime_slot_state_t;

typedef struct {
    uint8_t active_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t active_slot_count;
    uint8_t pending_multi_tap_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t pending_multi_tap_count;
    uint8_t preview_owner_slot;
    uint8_t pending_fallback_slot;
} key_runtime_index_state_t;

typedef struct {
    uint16_t timer;
    bool     active;
    bool     long_hold_level;
} key_runtime_feedback_state_t;

typedef struct {
    bool    active;
    uint8_t masked_real_mods;
} key_runtime_keyboard_event_mask_state_t;

typedef struct {
    uint16_t             action;
    keyboard_mod_state_t mods;
} key_runtime_deferred_release_dispatch_t;

typedef struct {
    key_runtime_deferred_release_dispatch_t items[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t                                 count;
} key_runtime_deferred_release_dispatch_queue_t;

#define ACTIVE_KEY_STATE_INIT                                                    \
    {                                                                            \
        .owner.keycode                 = KC_NO,                                  \
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_IDLE,            \
        .lifecycle.held_action_keycode = KC_NO,                                  \
        .interaction                   = key_runtime_slot_interaction_default(), \
        .pending_multi_tap             = {0},                                    \
    }

typedef struct {
    key_runtime_slot_state_t     slots_by_position[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    key_runtime_index_state_t    index;
    key_runtime_feedback_state_t feedback;
    key_runtime_keyboard_event_mask_state_t keyboard_event_mask;
    key_runtime_deferred_release_dispatch_queue_t deferred_release_dispatch;
} key_runtime_shared_state_t;

key_runtime_shared_state_t *key_runtime_shared_state(void);

static inline void key_runtime_shared_state_reset(key_runtime_shared_state_t *state) {
    if (!state) {
        return;
    }

    *state                             = (key_runtime_shared_state_t){0};
    state->index.preview_owner_slot    = UINT8_MAX;
    state->index.pending_fallback_slot = UINT8_MAX;

    for (uint16_t index = 0; index < KEY_RUNTIME_SLOT_TABLE_CAPACITY; index++) {
        state->slots_by_position[index] = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
    }
}
