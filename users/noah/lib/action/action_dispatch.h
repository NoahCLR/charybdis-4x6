// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Dispatches authored key actions, including custom layer-lock and
// pointing-device lock actions.
//
// Actions are keycode-like values. Keymap-local custom keycodes starting at
// NOAH_KEYMAP_SAFE_RANGE are routed back through process_record_user().
// Non-layer QMK behavior keycodes such as OSM()/MT() need explicit handling
// too, because tap_code16/register_code16 only model plain key press/release.
// Raw QMK layer actions are intentionally handled separately so they cannot
// bypass the userspace layer ownership model.
//
// Scope note: this module intentionally keeps authored tap actions on normal
// QMK tap semantics. Ownership-aware literal keycode dispatch, including held
// QK_MODS actions such as S(KC_1), lives in owned_keycode.c and is reused by
// the overlap-sensitive synthetic paths this userspace runtime owns.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

typedef struct {
    bool settle_pending_fallback_holds;
    bool preserve_keyboard_mod_state;
} noah_emit_policy_t;

#define NOAH_EMIT_POLICY_NONE ((noah_emit_policy_t){0})
#define NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS ((noah_emit_policy_t){.settle_pending_fallback_holds = true})
#define NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS ((noah_emit_policy_t){.settle_pending_fallback_holds = true, .preserve_keyboard_mod_state = true})

bool action_dispatch_is_layer_lock(uint16_t action);
bool action_dispatch_is_raw_qmk_layer_action(uint16_t action);
bool action_dispatch_is_layer_action(uint16_t action);
bool action_dispatch_is_macro(uint16_t action);
bool action_dispatch_is_qmk_behavior_keycode(uint16_t action);
bool action_dispatch_layer_is_locked(uint8_t layer);
void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy);
void noah_emit_synthetic_qmk_tap(uint16_t keycode, noah_emit_policy_t policy);
void noah_emit_literal_tap(uint16_t keycode, noah_emit_policy_t policy);
void action_dispatch(uint16_t action);
