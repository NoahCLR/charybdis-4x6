// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Foundation
// ────────────────────────────────────────────────────────────────────────────
//
// Single-authority handled-key runtime state and reducer surface.
// key_runtime_core owns the canonical press/tap/lease/pending-release state plus
// the effect-plan APIs consumed by the QMK-facing orchestration layer under
// users/noah/lib/key/runtime/. That orchestration layer feeds physical events
// into key_runtime_core, then projects the returned effects into the existing
// ownership registries and QMK hook seams.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../../interaction/handled_key.h"
#include "../effects/effect_queue.h"
#include "../interaction.h"
#include "../../../pointing/defs/pd_mode_flags.h"
#include "../../../state/runtime/keyboard_mod_state.h"

#define KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))
#define KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))
#define KEY_RUNTIME_CORE_LEASE_CAPACITY ((uint16_t)(KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY * 4u))
#define KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY ((uint16_t)(KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY * 2u))
#define KEY_RUNTIME_CORE_DEFERRED_RELEASE_BLOCKER_CAPACITY KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY
#define KEY_RUNTIME_CORE_PERSISTENT_INTENT_CAPACITY 16u
#define KEY_RUNTIME_CORE_EFFECT_PLAN_CAPACITY 16u

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
    bool                           active;
    uint16_t                       token_id;
    keypos_t                       key_pos;
    uint16_t                       physical_keycode;
    uint16_t                       resolved_keycode;
    uint16_t                       observed_release_keycode;
    uint16_t                       pressed_at;
    uint16_t                       released_at;
    uint16_t                       hold_term_ms;
    uint16_t                       longer_hold_term_ms;
    press_token_phase_t            phase;
    bool                           handled_key;
    bool                           tap_outcome_available;
    bool                           pd_mode_was_locked_on_press;
    bool                           other_press_interrupted;
    bool                           momentary_layer_tap_interrupted;
    bool                           resolved_from_transparent;
    bool                           pending_release_emission;
    bool                           release_keycode_mismatched;
    key_runtime_slot_interaction_t interaction;
    key_runtime_slot_phase_t       slot_phase;
} press_token_t;

typedef struct {
    bool                 active;
    keypos_t             key_pos;
    uint16_t             keycode;
    uint8_t              tap_count;
    bool                 pending_hold;
    uint16_t             single_action;
    uint16_t             tap_action;
    uint8_t              tap_repeat_count;
    bool                 has_more_taps;
    hold_behavior_t      hold;
    hold_behavior_t      long_hold;
    uint16_t             tap_hold_term_ms;
    uint16_t             last_action;
    uint16_t             last_tap_at;
    uint16_t             tap_term_ms;
    keyboard_mod_state_t saved_mod_state;
} tap_series_t;

typedef struct {
    KEY_RUNTIME_EFFECT_QUEUE_FIELDS(KEY_RUNTIME_CORE_EFFECT_PLAN_CAPACITY);
} key_runtime_core_effect_plan_t;

typedef struct {
    bool                 active;
    uint16_t             owner_token_id;
    uint16_t             sequence;
    keypos_t             key_pos;
    uint16_t             action;
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
    bool         active;
    lease_kind_t kind;
    uint16_t     owner_token_id;
    keypos_t     owner_key_pos;
    union {
        uint8_t layer;
        struct {
            uint8_t modifiers;
            bool    physical;
        } modifier;
        uint16_t       action;
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
    bool                     active;
    persistent_intent_kind_t kind;
    union {
        uint8_t        layer;
        pd_mode_mask_t pd_mode;
        uint8_t        pointer_layer;
        struct {
            uint8_t        pointer_layer;
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
} key_runtime_core_shadow_projection_t;

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
    layer_state_t        core_shadow_layer_state;
    layer_state_t        core_shadow_locked_layer_mask;
    keyboard_mod_state_t core_shadow_keyboard_mod_state;
    uint8_t              core_shadow_keyboard_managed_mod_mask;
    uint8_t              core_shadow_keyboard_physical_mod_mask;
    pd_mode_mask_t       core_shadow_pd_mode_local_active;
    pd_mode_mask_t       core_shadow_pd_mode_local_locked;
    bool                 core_shadow_pointer_anchor_active;
    bool                 core_shadow_pointer_pd_mode_anchor_active;
    bool                 core_shadow_pointer_prefers_typing_layer;
    bool                 core_shadow_pointer_toggle_enabled;
    uint8_t              core_press_token_count;
    uint8_t              core_tap_series_count;
    uint8_t              core_lease_count;
    uint8_t              core_pending_release_count;
    uint8_t              core_deferred_release_blocker_count;
    uint8_t              core_deferred_release_timed_blocker_count;
    uint8_t              core_persistent_intent_count;
    uint8_t              core_release_keycode_mismatch_count;
    uint8_t              core_orphan_release_count;
    uint8_t              core_cancelled_press_count;
} projection_snapshot_t;

typedef struct {
    press_token_t                        press_tokens[KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY];
    tap_series_t                         tap_series[KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY];
    lease_t                              leases[KEY_RUNTIME_CORE_LEASE_CAPACITY];
    pending_release_t                    pending_releases[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY];
    deferred_release_blocker_t           deferred_release_blockers[KEY_RUNTIME_CORE_DEFERRED_RELEASE_BLOCKER_CAPACITY];
    persistent_intent_t                  persistent_intents[KEY_RUNTIME_CORE_PERSISTENT_INTENT_CAPACITY];
    key_runtime_core_shadow_projection_t shadow_projection;
    uint16_t                             current_time;
    uint16_t                             next_token_id;
    uint16_t                             next_pending_release_sequence;
    uint8_t                              press_token_count;
    uint8_t                              tap_series_count;
    uint8_t                              lease_count;
    uint8_t                              pending_release_count;
    uint8_t                              deferred_release_blocker_count;
    uint8_t                              deferred_release_timed_blocker_count;
    uint8_t                              persistent_intent_count;
    uint8_t                              release_keycode_mismatch_count;
    uint8_t                              orphan_release_count;
    uint8_t                              cancelled_press_count;
    uint16_t                             feedback_pulse_timer;
    bool                                 feedback_pulse_active;
    bool                                 feedback_pulse_long_hold_level;
    uint8_t                              keyboard_event_masked_real_mods;
    bool                                 keyboard_event_mask_active;
} key_runtime_core_state_t;

key_runtime_core_state_t                   *key_runtime_core_state(void);
void                                        key_runtime_core_effect_plan_init(key_runtime_core_effect_plan_t *plan);
void                                        key_runtime_core_apply_event(const runtime_event_t *event, uint16_t event_time);
void                                        key_runtime_core_observe_process_record_event(uint16_t keycode, keyrecord_t *record);
void                                        key_runtime_core_observe_scan_cycle(uint16_t now);
void                                        key_runtime_core_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
void                                        key_runtime_core_flush_foreign_multi_tap(uint16_t keycode, keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
void                                        key_runtime_core_flush_multi_tap(key_runtime_core_effect_plan_t *plan);
void                                        key_runtime_core_flush_active_keys_except(keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
bool                                        key_runtime_core_handle_handled_key_press(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, key_runtime_core_effect_plan_t *plan);
bool                                        key_runtime_core_handle_handled_key_release(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, keyboard_mod_state_t keyboard_mod_state, key_runtime_core_effect_plan_t *plan);
void                                        key_runtime_core_scan(key_runtime_core_effect_plan_t *plan, uint16_t now);
bool                                        key_runtime_core_settle_pending_fallback_hold(key_runtime_core_effect_plan_t *plan);
bool                                        key_runtime_core_blocker_queries_authoritative(void);
bool                                        key_runtime_core_has_any_deferred_release_blocker(void);
bool                                        key_runtime_core_has_foreign_deferred_release_blocker_except(keypos_t key_pos);
uint8_t                                     key_runtime_core_pending_release_count(void);
bool                                        key_runtime_core_queue_pending_release_dispatch(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods);
bool                                        key_runtime_core_pending_release_at_order(uint8_t order, pending_release_t *out);
uint8_t                                     key_runtime_core_take_pending_release_dispatches(pending_release_t *out, uint8_t capacity);
void                                        key_runtime_core_project_effect(const key_runtime_effect_t *effect);
void                                        key_runtime_core_project_pending_release_dispatch(const pending_release_t *pending);
bool                                        key_runtime_core_take_pending_multi_tap_flush(keypos_t key_pos, uint16_t *action, uint8_t *repeat_count);
bool                                        key_runtime_core_reset_pending_multi_tap(keypos_t key_pos);
bool                                        key_runtime_core_retire_press_token(keypos_t key_pos);
void                                        key_runtime_core_observe_held_action_register(keypos_t key_pos, uint16_t action);
void                                        key_runtime_core_observe_held_action_unregister(keypos_t key_pos, uint16_t action);
void                                        key_runtime_core_observe_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz);
bool                                        key_runtime_core_release_owned_state_by_key(keypos_t key_pos);
bool                                        key_runtime_core_finalize_non_handled_release(keypos_t key_pos);
const press_token_t                        *key_runtime_core_press_token_at(keypos_t key_pos);
const tap_series_t                         *key_runtime_core_tap_series_at(keypos_t key_pos);
uint8_t                                     key_runtime_core_active_press_token_count(void);
bool                                        key_runtime_core_active_press_token_key_pos(uint8_t order, keypos_t *out);
uint8_t                                     key_runtime_core_pending_multi_tap_count(void);
bool                                        key_runtime_core_pending_multi_tap_key_pos(uint8_t order, keypos_t *out);
bool                                        key_runtime_core_has_other_active_press_token(keypos_t key_pos);
bool                                        key_runtime_core_has_foreign_pending_multi_tap(uint16_t keycode, keypos_t key_pos);
uint16_t                                    key_runtime_core_owner_keycode_at(keypos_t key_pos);
uint16_t                                    key_runtime_core_tap_action_at(keypos_t key_pos);
uint16_t                                    key_runtime_core_held_action_keycode_at(keypos_t key_pos);
bool                                        key_runtime_core_repeat_active_at(keypos_t key_pos);
key_runtime_slot_phase_t                    key_runtime_core_slot_phase_at(keypos_t key_pos);
bool                                        key_runtime_core_momentary_layer_tap_interrupted_at(keypos_t key_pos);
uint8_t                                     key_runtime_core_pending_multi_tap_tap_count_at(keypos_t key_pos);
bool                                        key_runtime_core_pending_multi_tap_holding_at(keypos_t key_pos);
bool                                        key_runtime_core_has_pending_multi_tap_at(keypos_t key_pos);
bool                                        key_runtime_core_hold_is_complete_at(keypos_t key_pos);
uint8_t                                     key_runtime_core_deferred_release_blocker_count(void);
uint8_t                                     key_runtime_core_deferred_release_timed_blocker_count(void);
bool                                        key_runtime_core_preview_owner_key_pos(keypos_t *out);
bool                                        key_runtime_core_pending_fallback_key_pos(keypos_t *out);
const key_runtime_core_shadow_projection_t *key_runtime_core_shadow_projection(void);
uint8_t                                     key_runtime_core_pending_release_count_for_keypos(keypos_t key_pos);
uint8_t                                     key_runtime_core_deferred_release_blocker_count_for_keypos(keypos_t key_pos);
void                                        key_runtime_core_layer_lock_set(uint8_t layer, bool active);
void                                        key_runtime_core_pd_mode_lock_set(pd_mode_mask_t mode, bool active);
void                                        key_runtime_core_observe_release_dispatch_deferred(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods);
void                                        key_runtime_core_observe_release_dispatch_drained(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods);
void                                        key_runtime_core_observe_deferred_release_blocker_profile(keypos_t key_pos, bool active, bool blocks_before_tap_term, bool blocks_after_tap_term);
projection_snapshot_t                       key_runtime_core_projection_snapshot_capture(void);
bool                                        key_runtime_core_projection_snapshot_equal(const projection_snapshot_t *lhs, const projection_snapshot_t *rhs);

static inline void key_runtime_core_state_reset(key_runtime_core_state_t *state) {
    if (!state) {
        return;
    }

    *state                               = (key_runtime_core_state_t){0};
    state->next_token_id                 = 1u;
    state->next_pending_release_sequence = 1u;
}
