// ────────────────────────────────────────────────────────────────────────────
// Handled Key View
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime helpers that adapt authored key_behavior rows into a fully resolved
// handled-key contract for the press/release engine.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../pointing/pd_mode_flags.h"
#include "../state/runtime_shared_state.h"
#include "key_behavior.h"

typedef struct {
    uint16_t                         tap_action;
    hold_behavior_t                  hold;
    hold_behavior_t                  long_hold;
    key_runtime_slot_hold_strategy_t hold_strategy;
    uint16_t                         tap_hold_term;
    uint16_t                         longer_hold_term;
    uint16_t                         multi_tap_term;
    uint8_t                          layer;
    pd_mode_mask_t                   pd_mode;
    uint16_t                         flags;
} handled_key_view_t;

typedef enum {
    HANDLED_KEY_FLAG_HANDLED         = (1u << 0),
    HANDLED_KEY_FLAG_MULTI_TAP       = (1u << 1),
    HANDLED_KEY_FLAG_IMPLICIT_HOLD   = (1u << 2),
    HANDLED_KEY_FLAG_FALLBACK_HOLD   = (1u << 3),
    HANDLED_KEY_FLAG_MOMENTARY_LAYER = (1u << 4),
    HANDLED_KEY_FLAG_LAYER_TAP       = (1u << 5),
} handled_key_flag_t;

handled_key_view_t               handled_key_lookup(uint16_t keycode);
bool                             handled_key_is_handled(handled_key_view_t key);
bool                             handled_key_uses_implicit_hold(handled_key_view_t key);
bool                             handled_key_uses_fallback_hold(handled_key_view_t key);
bool                             handled_key_has_multi_tap(handled_key_view_t key);
bool                             handled_key_is_momentary_layer(handled_key_view_t key);
bool                             handled_key_is_layer_tap(handled_key_view_t key);
hold_behavior_t                  handled_key_single_hold(handled_key_view_t key);
hold_behavior_t                  handled_key_long_hold(handled_key_view_t key);
key_runtime_slot_hold_strategy_t handled_key_hold_strategy(handled_key_view_t key);
uint16_t                         handled_key_tap_action(handled_key_view_t key);
uint16_t                         handled_key_tap_hold_term(handled_key_view_t key);
uint16_t                         handled_key_longer_hold_term(handled_key_view_t key);
uint16_t                         handled_key_multi_tap_term(handled_key_view_t key);
uint8_t                          handled_key_layer(handled_key_view_t key);
pd_mode_mask_t                   handled_key_pd_mode(handled_key_view_t key);
