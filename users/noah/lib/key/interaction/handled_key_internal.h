// ────────────────────────────────────────────────────────────────────────────
// Handled Key Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Shared helpers for authored handled-key defaults, transparent-source
// resolution, and contextual materialization.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../action/action_dispatch.h"
#include "handled_key.h"
#include "handled_key_policy.h"
#include "key_behavior_lookup.h"

typedef enum {
    HANDLED_KEY_TRANSPARENT_FIELD_TAP = 0,
    HANDLED_KEY_TRANSPARENT_FIELD_HOLD,
    HANDLED_KEY_TRANSPARENT_FIELD_LONG_HOLD,
} handled_key_transparent_field_t;

typedef struct {
    handled_key_resolution_t resolution;
    bool                     found;
} handled_key_transparent_source_t;

uint8_t                          behavior_get_layer(uint16_t keycode);
bool                             is_layer_key(uint16_t keycode);
pd_mode_mask_t                   handled_key_pd_mode_for_behavior(key_behavior_view_t behavior);
uint16_t                         handled_key_flags_from_behavior(key_behavior_view_t behavior);
bool                             handled_key_resolution_step_present(handled_key_resolution_t resolution);
bool                             handled_key_resolution_uses_fallback_hold_behavior(handled_key_resolution_t resolution);
bool                             handled_key_resolution_uses_deferred_stacked_pd_hold(handled_key_resolution_t resolution);
uint16_t                         handled_key_tap_action_behavior(handled_key_resolution_t resolution);
uint8_t                          handled_key_tap_repeat_count_behavior(handled_key_resolution_t resolution, uint16_t tap_action);
hold_behavior_t                  handled_key_hold_behavior(handled_key_resolution_t resolution);
key_runtime_slot_hold_strategy_t handled_key_hold_strategy_behavior(handled_key_resolution_t resolution);
bool                             handled_key_tap_resolves_on_press_behavior(handled_key_resolution_t resolution);
bool                             handled_key_resolution_uses_transparent_source(handled_key_resolution_t resolution, handled_key_transparent_field_t field);
bool                             handled_key_resolution_source_is_layer_tap(handled_key_resolution_t resolution);
bool                             handled_key_resolution_source_is_momentary_layer(handled_key_resolution_t resolution);
uint8_t                          handled_key_resolution_source_layer(handled_key_resolution_t resolution);
handled_key_transparent_source_t handled_key_transparent_source_at_position(handled_key_resolution_t resolution, handled_key_resolution_ctx_t ctx, handled_key_transparent_field_t field);
handled_key_materialized_t       handled_key_materialized_refresh_contract(handled_key_materialized_t materialized);
