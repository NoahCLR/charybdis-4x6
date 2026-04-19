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

#include "../key/interaction/handled_key.h"
#include "../key/runtime/key_runtime_interaction.h"
#include "../pointing/defs/pd_mode_flags.h"
#include "../state/runtime/keyboard_mod_state.h"

#define RUNTIME_V2_PRESS_TOKEN_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))
#define RUNTIME_V2_TAP_SERIES_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))
#define RUNTIME_V2_LEASE_CAPACITY ((uint16_t)(RUNTIME_V2_PRESS_TOKEN_CAPACITY * 4u))
#define RUNTIME_V2_PENDING_RELEASE_CAPACITY ((uint16_t)(RUNTIME_V2_PRESS_TOKEN_CAPACITY * 2u))
#define RUNTIME_V2_DEFERRED_RELEASE_BLOCKER_CAPACITY RUNTIME_V2_PRESS_TOKEN_CAPACITY
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
    uint16_t            observed_release_keycode;
    uint16_t            pressed_at;
    uint16_t            released_at;
    uint16_t            hold_term_ms;
    uint16_t            longer_hold_term_ms;
    press_token_phase_t phase;
    handled_key_behavior_contract_t behavior_contract;
    bool                handled_key;
    bool                tap_outcome_available;
    bool                pd_mode_was_locked_on_press;
    bool                other_press_interrupted;
    bool                momentary_layer_tap_interrupted;
    bool                resolved_from_transparent;
    bool                pending_release_emission;
    bool                release_keycode_mismatched;
    key_runtime_slot_interaction_t interaction;
    key_runtime_slot_phase_t       slot_phase;
} press_token_t;

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t keycode;
    uint8_t  tap_count;
    bool     pending_hold;
    uint16_t single_action;
    uint16_t tap_action;
    uint8_t  tap_repeat_count;
    bool     has_more_taps;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
    uint16_t tap_hold_term_ms;
    uint16_t last_action;
    uint16_t last_tap_at;
    uint16_t tap_term_ms;
} tap_series_t;

typedef struct {
    bool                active;
    uint16_t            owner_token_id;
    uint16_t            sequence;
    keypos_t            key_pos;
    uint16_t            action;
    keyboard_mod_state_t mods;
} pending_release_t;

typedef struct {
    bool     active;
    uint16_t owner_token_id;
    keypos_t key_pos;
    bool     blocks_before_tap_term;
    bool     blocks_after_tap_term;
} deferred_release_blocker_t;

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
    keypos_t    owner_key_pos;
    union {
        uint8_t       layer;
        struct {
            uint8_t modifiers;
            bool    physical;
        } modifier;
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
        struct {
            uint8_t       pointer_layer;
            pd_mode_mask_t mode;
        } pointer_toggle;
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
    bool                 pointer_anchor_active;
    bool                 pointer_pd_mode_anchor_active;
    bool                 pointer_prefers_typing_layer;
    bool                 pointer_toggle_enabled;
} runtime_v2_shadow_projection_t;

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
    uint8_t              deferred_release_blocker_count;
    uint8_t              deferred_release_timed_blocker_count;
    layer_state_t        v2_shadow_layer_state;
    layer_state_t        v2_shadow_locked_layer_mask;
    keyboard_mod_state_t v2_shadow_keyboard_mod_state;
    uint8_t              v2_shadow_keyboard_managed_mod_mask;
    uint8_t              v2_shadow_keyboard_physical_mod_mask;
    pd_mode_mask_t       v2_shadow_pd_mode_local_active;
    pd_mode_mask_t       v2_shadow_pd_mode_local_locked;
    bool                 v2_shadow_pointer_anchor_active;
    bool                 v2_shadow_pointer_pd_mode_anchor_active;
    bool                 v2_shadow_pointer_prefers_typing_layer;
    bool                 v2_shadow_pointer_toggle_enabled;
    uint8_t              v2_press_token_count;
    uint8_t              v2_tap_series_count;
    uint8_t              v2_lease_count;
    uint8_t              v2_pending_release_count;
    uint8_t              v2_deferred_release_blocker_count;
    uint8_t              v2_deferred_release_timed_blocker_count;
    uint8_t              v2_persistent_intent_count;
    uint8_t              v2_release_keycode_mismatch_count;
    uint8_t              v2_orphan_release_count;
    uint8_t              v2_cancelled_press_count;
} projection_snapshot_t;

typedef struct {
    press_token_t       press_tokens[RUNTIME_V2_PRESS_TOKEN_CAPACITY];
    tap_series_t        tap_series[RUNTIME_V2_TAP_SERIES_CAPACITY];
    lease_t             leases[RUNTIME_V2_LEASE_CAPACITY];
    pending_release_t   pending_releases[RUNTIME_V2_PENDING_RELEASE_CAPACITY];
    deferred_release_blocker_t deferred_release_blockers[RUNTIME_V2_DEFERRED_RELEASE_BLOCKER_CAPACITY];
    persistent_intent_t persistent_intents[RUNTIME_V2_PERSISTENT_INTENT_CAPACITY];
    runtime_v2_shadow_projection_t shadow_projection;
    projection_snapshot_t last_projection;
    uint16_t            current_time;
    uint16_t            next_token_id;
    uint16_t            next_pending_release_sequence;
    uint8_t             press_token_count;
    uint8_t             tap_series_count;
    uint8_t             lease_count;
    uint8_t             pending_release_count;
    uint8_t             deferred_release_blocker_count;
    uint8_t             deferred_release_timed_blocker_count;
    uint8_t             persistent_intent_count;
    uint8_t             release_keycode_mismatch_count;
    uint8_t             orphan_release_count;
    uint8_t             cancelled_press_count;
    bool                input_stream_observed;
} runtime_v2_state_t;

runtime_v2_state_t  *runtime_v2_state(void);
void                 runtime_v2_apply_event(const runtime_event_t *event, uint16_t event_time);
void                 runtime_v2_observe_process_record_event(uint16_t keycode, keyrecord_t *record);
void                 runtime_v2_observe_scan_cycle(uint16_t now);
bool                 runtime_v2_blocker_queries_authoritative(void);
bool                 runtime_v2_has_any_deferred_release_blocker(void);
bool                 runtime_v2_has_foreign_deferred_release_blocker_except(keypos_t key_pos);
uint8_t              runtime_v2_pending_release_count(void);
uint8_t              runtime_v2_take_pending_release_dispatches(pending_release_t *out, uint8_t capacity);
void                 runtime_v2_observe_held_action_register(keypos_t key_pos, uint16_t action);
void                 runtime_v2_observe_held_action_unregister(keypos_t key_pos, uint16_t action);
void                 runtime_v2_observe_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz);
bool                 runtime_v2_release_owned_state_by_key(keypos_t key_pos);
const press_token_t *runtime_v2_press_token_at(keypos_t key_pos);
const tap_series_t  *runtime_v2_tap_series_at(keypos_t key_pos);
const runtime_v2_shadow_projection_t *runtime_v2_shadow_projection(void);
uint8_t              runtime_v2_pending_release_count_for_keypos(keypos_t key_pos);
uint8_t              runtime_v2_deferred_release_blocker_count_for_keypos(keypos_t key_pos);
void                 runtime_v2_layer_lock_set(uint8_t layer, bool active);
void                 runtime_v2_pd_mode_lock_set(pd_mode_mask_t mode, bool active);
void                 runtime_v2_observe_release_dispatch_deferred(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods);
void                 runtime_v2_observe_release_dispatch_drained(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods);
void                 runtime_v2_observe_deferred_release_blocker_profile(keypos_t key_pos, bool active, bool blocks_before_tap_term, bool blocks_after_tap_term);
projection_snapshot_t runtime_v2_projection_snapshot_capture(void);
bool                 runtime_v2_projection_snapshot_equal(const projection_snapshot_t *lhs, const projection_snapshot_t *rhs);

static inline void runtime_v2_state_reset(runtime_v2_state_t *state) {
    if (!state) {
        return;
    }

    *state              = (runtime_v2_state_t){0};
    state->next_token_id = 1u;
    state->next_pending_release_sequence = 1u;
}
