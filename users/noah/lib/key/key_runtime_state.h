// ────────────────────────────────────────────────────────────────────────────
// Key Runtime State
// ────────────────────────────────────────────────────────────────────────────
//
// Shared per-slot handled-key state for the split key runtime modules.
// Each slot owns both the currently pressed key state and any deferred
// multi-tap chain that still belongs to that physical key. The helper surface
// keeps higher-level code off the raw storage layout so the runtime can keep
// evolving toward clearer per-key FSMs without another global-state sweep.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../state/runtime_shared_state.h"
#include "delayed_action.h"
#include "key_behavior_lookup.h"

// Transitional aliases while key-runtime modules move from file-local globals
// to the shared runtime-owned state object.
#define active_key (noah_runtime_shared_state.key.active_slots[0])
#define multi_tap (noah_runtime_shared_state.key.active_slots[0].pending_multi_tap)

void noah_key_runtime_scan(void);

typedef enum {
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP,
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION,
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP,
} key_runtime_slot_release_outcome_t;

typedef struct {
    bool                               release_owned_state;
    key_runtime_slot_release_outcome_t outcome;
    uint16_t                           action;
    pd_mode_mask_t                     pd_mode_lock_tap;
} key_runtime_slot_release_resolution_t;

typedef enum {
    KEY_RUNTIME_SLOT_SCAN_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD,
    KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD,
} key_runtime_slot_scan_outcome_t;

typedef struct {
    bool                            commit_immediate_hold;
    bool                            immediate_hold_needs_feedback;
    bool                            immediate_hold_completes_hold;
    bool                            activate_fallback_hold;
    key_runtime_slot_scan_outcome_t outcome;
    hold_behavior_t                 hold;
    hold_behavior_t                 long_hold;
} key_runtime_slot_scan_resolution_t;

typedef enum {
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD,
} key_runtime_slot_pending_multi_tap_scan_outcome_t;

typedef struct {
    key_runtime_slot_pending_multi_tap_scan_outcome_t outcome;
    bool                                              release_layer_before_action;
    hold_behavior_t                                   hold;
    hold_behavior_t                                   long_hold;
} key_runtime_slot_pending_multi_tap_scan_resolution_t;

typedef enum {
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE = 0,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_REPEAT_START,
} key_runtime_slot_effect_request_kind_t;

typedef struct {
    bool                                 release_owned_state;
    bool                                 feedback_pulse;
    bool                                 feedback_long_hold_level;
    key_runtime_slot_effect_request_kind_t kind;
    uint16_t                             action;
    uint16_t                             repeat_hz;
} key_runtime_slot_effect_request_t;

typedef struct {
    bool                           release_layer_before_action;
    key_runtime_slot_effect_request_t effect_request;
} key_runtime_slot_pending_multi_tap_scan_apply_t;

typedef struct {
    bool                handled;
    uint16_t            action;
    delayed_action_mods_t mods;
    uint8_t             repeat_count;
} key_runtime_slot_pending_multi_tap_flush_t;

typedef enum {
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_NONE = 0,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_DELAYED_ACTION,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_HELD_LIFECYCLE,
} key_runtime_slot_pending_multi_tap_hold_release_outcome_t;

typedef struct {
    bool                                                    handled;
    bool                                                    release_layer_after_action;
    keypos_t                                                 key_pos;
    key_runtime_slot_pending_multi_tap_hold_release_outcome_t outcome;
    uint16_t                                                action;
    delayed_action_mods_t                                   mods;
    uint8_t                                                 repeat_count;
} key_runtime_slot_pending_multi_tap_hold_release_t;

uint8_t             behavior_get_layer(uint16_t keycode);
bool                is_layer_key(uint16_t keycode);
bool                key_runtime_keypos_equal(keypos_t lhs, keypos_t rhs);
active_key_state_t *key_runtime_primary_slot(void);
active_key_state_t *key_runtime_slot_at(uint8_t index);
active_key_state_t *key_runtime_first_active_slot(void);
uint8_t             key_runtime_slot_index(const active_key_state_t *slot);
bool                key_runtime_slot_idle(const active_key_state_t *slot);
bool                key_runtime_slot_active(const active_key_state_t *slot);
bool                key_runtime_slot_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos);
bool                key_runtime_slot_owns_key_position(const active_key_state_t *slot, keypos_t key_pos);
bool                key_runtime_slot_has_pending_multi_tap(const active_key_state_t *slot);
bool                key_runtime_slot_pending_multi_tap_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos);
bool                key_runtime_slot_pending_multi_tap_pending_hold(const active_key_state_t *slot);
bool                key_runtime_slot_pending_multi_tap_expired(const active_key_state_t *slot);
active_key_state_t *key_runtime_find_slot_by_position(keypos_t key_pos);
active_key_state_t *key_runtime_find_slot_with_pending_multi_tap(keypos_t key_pos);
active_key_state_t *key_runtime_find_free_slot(void);
active_key_state_t *key_runtime_find_reclaimable_slot(void);
active_key_state_t *key_runtime_select_slot_for_press(keypos_t key_pos);
multi_tap_t        *key_runtime_primary_multi_tap(void);
multi_tap_t        *key_runtime_multi_tap_slot_at(uint8_t index);
multi_tap_t        *key_runtime_multi_tap_for_slot(const active_key_state_t *slot);
active_key_state_t *key_runtime_slot_for_multi_tap(const multi_tap_t *mt);
bool                key_runtime_multi_tap_slot_active(const multi_tap_t *mt);
multi_tap_t        *key_runtime_first_active_multi_tap(void);
multi_tap_t        *key_runtime_find_multi_tap_by_position(keypos_t key_pos);
void                key_runtime_slot_begin_pending_multi_tap(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t single_action, uint16_t tap_hold_term, uint16_t multi_tap_term);
uint16_t            key_runtime_slot_advance_pending_multi_tap(active_key_state_t *slot, uint16_t keycode);
uint16_t            key_runtime_slot_resolve_pending_multi_tap_hold(active_key_state_t *slot, uint16_t keycode, uint8_t *repeat_count);
key_runtime_slot_pending_multi_tap_flush_t key_runtime_slot_take_pending_multi_tap_flush(active_key_state_t *slot);
key_runtime_slot_release_resolution_t key_runtime_slot_resolve_release(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior, uint16_t elapsed);
key_runtime_slot_scan_resolution_t key_runtime_slot_resolve_scan(active_key_state_t active_key_state, uint16_t elapsed);
key_runtime_slot_pending_multi_tap_scan_resolution_t key_runtime_slot_resolve_pending_multi_tap_scan(active_key_state_t active_key_state, multi_tap_t multi_tap_state, uint16_t elapsed);
key_runtime_slot_effect_request_t key_runtime_slot_begin_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired, bool implicit_hold, bool fallback_hold_pending, bool pd_mode_was_locked_on_press);
key_runtime_slot_effect_request_t key_runtime_slot_activate_pending_fallback_hold_request(active_key_state_t *slot);
key_runtime_slot_effect_request_t key_runtime_slot_interrupt_on_other_press(active_key_state_t *slot, keypos_t other_key_pos);
key_runtime_slot_effect_request_t key_runtime_slot_commit_immediate_hold(active_key_state_t *slot, bool needs_feedback, bool completes_hold);
key_runtime_slot_effect_request_t key_runtime_slot_take_flush(active_key_state_t *slot, bool active_held_action_survives_flush);
key_runtime_slot_effect_request_t key_runtime_slot_fire_hold_at_threshold(active_key_state_t *slot, hold_behavior_t hold, hold_behavior_t long_hold, bool pulse_momentary_layer_action);
key_runtime_slot_effect_request_t key_runtime_slot_promote_to_long_hold(active_key_state_t *slot, hold_behavior_t long_hold, bool pulse_momentary_layer_action);
key_runtime_slot_pending_multi_tap_scan_apply_t key_runtime_slot_apply_pending_multi_tap_scan_resolution(active_key_state_t *slot, key_runtime_slot_pending_multi_tap_scan_resolution_t resolution);
key_runtime_slot_pending_multi_tap_hold_release_t key_runtime_slot_take_pending_multi_tap_hold_release(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior, uint16_t elapsed);
void                key_runtime_slot_reset_pending_multi_tap(active_key_state_t *slot);
bool                active_key_matches(uint16_t keycode, keypos_t key_pos);
bool                key_runtime_slot_activate_pending_fallback_hold(active_key_state_t *slot);
bool                key_runtime_activate_pending_fallback_hold(void);

void key_runtime_slot_reset(active_key_state_t *slot);
void key_runtime_slot_track(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired);
void active_key_reset(void);
void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired);
