// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Effects
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_effects.h"

#include "../action/action_dispatch.h"
#include "../state/keyboard_mod_ownership.h"
#include "../state/layer_ownership.h"
#include "../state/split_runtime_sync.h"
#include "held_action.h"
#include "key_runtime_feedback.h"
#include "key_runtime_state.h"

bool key_runtime_effects_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    return keyboard_mod_ownership_should_suppress_default(keycode, record);
}

void key_runtime_effects_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    keyboard_mod_ownership_track_physical_keycode_event(keycode, record);
}

bool key_runtime_effects_activate_pending_fallback_hold(void) {
    if (!active_key.fallback_hold_pending || active_key.held_action_keycode != KC_NO || active_key.keycode == KC_NO) {
        return false;
    }

    held_action_register(active_key.key_pos, active_key.keycode);
    active_key.held_action_keycode = active_key.keycode;
    active_key.hold_fired          = true;
    return true;
}

void key_runtime_effects_dispatch_action(uint16_t action) {
    action_dispatch(action);
}

void key_runtime_effects_held_action_register(keypos_t key_pos, uint16_t action) {
    held_action_register(key_pos, action);
}

void key_runtime_effects_held_action_unregister(keypos_t key_pos, uint16_t action) {
    held_action_unregister(key_pos, action);
}

bool key_runtime_effects_held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    return held_action_survives_flush(key_pos, action);
}

bool key_runtime_effects_release_held_action_owned_by_key(keypos_t key_pos) {
    return held_action_release_owned_by_key(key_pos);
}

void key_runtime_effects_layer_press(keypos_t key_pos, uint8_t layer) {
    layer_ownership_momentary_press(key_pos, layer);
}

void key_runtime_effects_layer_release(keypos_t key_pos) {
    layer_ownership_momentary_release(key_pos);
}

void key_runtime_effects_feedback_pulse_arm(bool long_hold_level) {
    key_feedback_pulse_arm(long_hold_level);
}

void key_runtime_effects_sync_split_runtime(void) {
    split_runtime_sync();
}
