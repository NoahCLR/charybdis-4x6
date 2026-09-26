// ────────────────────────────────────────────────────────────────────────────
// Pointer Layer Policy
// ────────────────────────────────────────────────────────────────────────────
//
// Owns the auto-mouse / pointer-layer arbitration rules that decide when
// QMK should keep the configured auto-mouse target layer alive and which
// keycodes count as mouse records for anchoring purposes.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

// Physical key records and synthetic dispatched actions use separate QMK
// entry points for auto-mouse. Keep both classifications in one policy module
// so pointer anchoring rules stay coherent.
bool          pointer_layer_policy_is_mouse_record(uint16_t keycode);
bool          pointer_layer_policy_is_mouse_action(uint16_t action);
void          pointer_layer_policy_note_action(uint16_t action, bool pressed);
layer_state_t pointer_layer_policy_apply(layer_state_t state);

// Layer locks belong to the runtime (layer_ownership), so a lock on the
// auto-mouse layer is the only thing that keeps it locked. The sync holds
// QMK's auto-mouse on while that lock lasts, as a held mouse key would; it is
// idempotent, and runs at every key record, layer change and pointing task.
// The take-back undoes the toggle QMK's auto-mouse flips for itself on TG(),
// TO() and TT() of its layer, a second lock that TO(0) never released.
void pointer_layer_policy_sync_layer_lock_anchor(void);
void pointer_layer_policy_take_back_qmk_toggle(uint16_t keycode, const keyrecord_t *record);

typedef struct {
    bool    auto_mouse_anchored;
    bool    pd_mode_anchor_active;
    bool    prefers_typing_layer;
    bool    auto_mouse_toggle_enabled;
    bool    sniping_layer_active;
    int8_t  auto_mouse_key_tracker;
    uint8_t auto_mouse_layer;
} pointer_layer_policy_debug_snapshot_t;

void pointer_layer_policy_debug_snapshot(layer_state_t state, pointer_layer_policy_debug_snapshot_t *out);
