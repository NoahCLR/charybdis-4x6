// ────────────────────────────────────────────────────────────────────────────
// Runtime V2 Foundation
// ────────────────────────────────────────────────────────────────────────────
//
// Single-authority runtime scaffolding for the authored interaction redesign.
// This first landing pass adds the canonical v2 state model, immutable input
// event shape, and projection snapshot contract used by the shadow-parity
// harness. The legacy runtime still owns behavior today; v2 currently stores
// structure and captures projections without driving production semantics.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../pointing/defs/pd_mode_flags.h"
#include "../state/runtime/keyboard_mod_state.h"

#define RUNTIME_V2_PRESS_TOKEN_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))
#define RUNTIME_V2_TAP_SERIES_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))
#define RUNTIME_V2_LEASE_CAPACITY ((uint16_t)(RUNTIME_V2_PRESS_TOKEN_CAPACITY * 4u))
#define RUNTIME_V2_PERSISTENT_INTENT_CAPACITY 16u

typedef enum {
    RUNTIME_EVENT_KIND_KEY_DOWN = 0,
    RUNTIME_EVENT_KIND_KEY_UP,
    RUNTIME_EVENT_KIND_TIMER_ADVANCE,
    RUNTIME_EVENT_KIND_SCAN,
    RUNTIME_EVENT_KIND_POINTER_REPORT,
    RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT,
} runtime_event_kind_t;

typedef struct {
    uint16_t keycode;
    keypos_t key_pos;
} runtime_key_event_t;

typedef struct {
    uint16_t advance_ms;
} runtime_timer_advance_t;

typedef struct {
    report_mouse_t report;
} runtime_pointer_report_t;

typedef struct {
    pd_mode_mask_t active_mode;
    pd_mode_mask_t locked_mode;
} runtime_remote_snapshot_t;

typedef struct {
    runtime_event_kind_t kind;
    union {
        runtime_key_event_t       key_event;
        runtime_timer_advance_t   timer_advance;
        runtime_pointer_report_t  pointer_report;
        runtime_remote_snapshot_t remote_snapshot;
    } data;
} runtime_event_t;

typedef enum {
    PRESS_TOKEN_PHASE_IDLE = 0,
    PRESS_TOKEN_PHASE_PRESSED,
    PRESS_TOKEN_PHASE_HOLD_PENDING,
    PRESS_TOKEN_PHASE_HELD,
    PRESS_TOKEN_PHASE_RELEASE_PENDING,
    PRESS_TOKEN_PHASE_RELEASED,
    PRESS_TOKEN_PHASE_CANCELLED,
} press_token_phase_t;

typedef struct {
    bool                active;
    uint16_t            token_id;
    keypos_t            key_pos;
    uint16_t            physical_keycode;
    uint16_t            resolved_keycode;
    uint16_t            pressed_at;
    uint16_t            hold_term_ms;
    press_token_phase_t phase;
    bool                resolved_from_transparent;
    bool                pending_release_emission;
} press_token_t;

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint8_t  tap_count;
    bool     pending_hold;
    uint16_t last_action;
} tap_series_t;

typedef enum {
    LEASE_KIND_NONE = 0,
    LEASE_KIND_LAYER,
    LEASE_KIND_MODIFIER,
    LEASE_KIND_HELD_ACTION,
    LEASE_KIND_REPEAT,
    LEASE_KIND_PD_MODE,
    LEASE_KIND_POINTER_ANCHOR,
} lease_kind_t;

typedef struct {
    bool        active;
    lease_kind_t kind;
    uint16_t    owner_token_id;
    union {
        uint8_t       layer;
        uint8_t       modifiers;
        uint16_t      action;
        pd_mode_mask_t pd_mode;
        struct {
            uint16_t action;
            uint8_t  repeat_hz;
        } repeat;
        struct {
            uint8_t layer;
            bool    keep_typing_surface;
        } pointer_anchor;
    } data;
} lease_t;

typedef enum {
    PERSISTENT_INTENT_KIND_NONE = 0,
    PERSISTENT_INTENT_KIND_LAYER_LOCK,
    PERSISTENT_INTENT_KIND_PD_MODE_LOCK,
    PERSISTENT_INTENT_KIND_POINTER_TOGGLE,
} persistent_intent_kind_t;

typedef struct {
    bool                    active;
    persistent_intent_kind_t kind;
    union {
        uint8_t       layer;
        pd_mode_mask_t pd_mode;
        uint8_t       pointer_layer;
    } data;
} persistent_intent_t;

typedef struct {
    layer_state_t        layer_state;
    layer_state_t        locked_layer_mask;
    keyboard_mod_state_t keyboard_mod_state;
    uint8_t              keyboard_managed_mod_mask;
    uint8_t              keyboard_physical_mod_mask;
    pd_mode_mask_t       pd_mode_local_active;
    pd_mode_mask_t       pd_mode_local_locked;
    pd_mode_mask_t       pd_mode_display_active;
    pd_mode_mask_t       pd_mode_display_locked;
    bool                 pointer_anchor_active;
    bool                 pointer_pd_mode_anchor_active;
    bool                 pointer_prefers_typing_layer;
    bool                 pointer_toggle_enabled;
    bool                 pointer_sniping_layer_active;
    int8_t               pointer_key_tracker;
    uint8_t              pointer_layer;
    uint8_t              active_slot_count;
    uint8_t              pending_multi_tap_slot_count;
    uint8_t              deferred_release_count;
    uint8_t              v2_press_token_count;
    uint8_t              v2_tap_series_count;
    uint8_t              v2_lease_count;
    uint8_t              v2_persistent_intent_count;
} projection_snapshot_t;

typedef struct {
    press_token_t       press_tokens[RUNTIME_V2_PRESS_TOKEN_CAPACITY];
    tap_series_t        tap_series[RUNTIME_V2_TAP_SERIES_CAPACITY];
    lease_t             leases[RUNTIME_V2_LEASE_CAPACITY];
    persistent_intent_t persistent_intents[RUNTIME_V2_PERSISTENT_INTENT_CAPACITY];
    projection_snapshot_t last_projection;
    uint16_t            next_token_id;
    uint8_t             press_token_count;
    uint8_t             tap_series_count;
    uint8_t             lease_count;
    uint8_t             persistent_intent_count;
} runtime_v2_state_t;

runtime_v2_state_t  *runtime_v2_state(void);
projection_snapshot_t runtime_v2_projection_snapshot_capture(void);
bool                 runtime_v2_projection_snapshot_equal(const projection_snapshot_t *lhs, const projection_snapshot_t *rhs);

static inline void runtime_v2_state_reset(runtime_v2_state_t *state) {
    if (!state) {
        return;
    }

    *state              = (runtime_v2_state_t){0};
    state->next_token_id = 1u;
}
