// ────────────────────────────────────────────────────────────────────────────
// Pointer Layer Policy
// ────────────────────────────────────────────────────────────────────────────
//
// Owns the auto-mouse / pointer-layer arbitration rules that decide when
// QMK should keep LAYER_POINTER alive and which keycodes count as mouse
// records for anchoring purposes.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

bool          pointer_layer_policy_is_mouse_record(uint16_t keycode);
layer_state_t pointer_layer_policy_apply(layer_state_t state);
