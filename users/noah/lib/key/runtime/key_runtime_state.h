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

#include "../../state/runtime/runtime_shared_state.h"
#include "delayed_action.h"
#include "../interaction/handled_key.h"
#include "../interaction/key_behavior_lookup.h"

void noah_key_runtime_scan(void);

typedef struct {
    bool                  handled;
    uint16_t              action;
    delayed_action_mods_t mods;
    uint8_t               repeat_count;
} key_runtime_slot_pending_multi_tap_flush_t;

uint8_t                                    behavior_get_layer(uint16_t keycode);
bool                                       is_layer_key(uint16_t keycode);
bool                                       key_runtime_keypos_equal(keypos_t lhs, keypos_t rhs);
active_key_state_t                        *key_runtime_slot_for_position(keypos_t key_pos);
active_key_state_t                        *key_runtime_slot_at(uint8_t index);
bool                                       key_runtime_slot_idle(const active_key_state_t *slot);
bool                                       key_runtime_slot_active(const active_key_state_t *slot);
key_runtime_slot_phase_t                   key_runtime_slot_phase(const active_key_state_t *slot);
bool                                       key_runtime_slot_uses_implicit_hold(const active_key_state_t *slot);
bool                                       key_runtime_slot_uses_fallback_hold(const active_key_state_t *slot);
bool                                       key_runtime_slot_allows_tap_release(const active_key_state_t *slot);
bool                                       key_runtime_slot_has_pending_release_hold(const active_key_state_t *slot);
bool                                       key_runtime_slot_has_active_hold_tier(const active_key_state_t *slot);
bool                                       key_runtime_slot_hold_is_complete(const active_key_state_t *slot);
bool                                       key_runtime_slot_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos);
bool                                       key_runtime_slot_owns_key_position(const active_key_state_t *slot, keypos_t key_pos);
uint8_t                                    key_runtime_slot_preview_layer_hint(const active_key_state_t *slot);
bool                                       key_runtime_slot_has_pending_multi_tap(const active_key_state_t *slot);
bool                                       key_runtime_slot_pending_multi_tap_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos);
bool                                       key_runtime_slot_pending_multi_tap_pending_hold(const active_key_state_t *slot);
bool                                       key_runtime_slot_pending_multi_tap_expired(const active_key_state_t *slot);
multi_tap_t                               *key_runtime_multi_tap_slot_at(uint8_t index);
multi_tap_t                               *key_runtime_multi_tap_for_slot(const active_key_state_t *slot);
active_key_state_t                        *key_runtime_slot_for_multi_tap(const multi_tap_t *mt);
bool                                       key_runtime_multi_tap_slot_active(const multi_tap_t *mt);
multi_tap_t                               *key_runtime_first_active_multi_tap(void);
multi_tap_t                               *key_runtime_find_multi_tap_by_position(keypos_t key_pos);
void                                       key_runtime_slot_begin_pending_multi_tap(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, uint8_t tap_repeat_count, uint16_t tap_hold_term, uint16_t multi_tap_term, bool has_more_taps);
uint16_t                                   key_runtime_slot_advance_pending_multi_tap(active_key_state_t *slot, uint16_t keycode);
uint16_t                                   key_runtime_slot_resolve_pending_multi_tap_hold(active_key_state_t *slot, uint8_t *repeat_count);
key_runtime_slot_pending_multi_tap_flush_t key_runtime_slot_take_pending_multi_tap_flush(active_key_state_t *slot);
void                                       key_runtime_slot_reset_pending_multi_tap(active_key_state_t *slot);
bool                                       key_runtime_slot_activate_pending_fallback_hold(active_key_state_t *slot);
bool                                       key_runtime_activate_pending_fallback_hold(void);
void                                       key_runtime_slot_set_release_hold_pending(active_key_state_t *slot);
void                                       key_runtime_slot_commit_hold_phase(active_key_state_t *slot, bool completes_hold);
void                                       key_runtime_slot_apply_handled_metadata(active_key_state_t *slot, handled_key_view_t key);

void key_runtime_slot_reset(active_key_state_t *slot);
void key_runtime_slot_track(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_phase_t phase, key_runtime_slot_hold_strategy_t hold_strategy);
