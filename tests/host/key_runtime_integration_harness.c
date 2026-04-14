#include "key_runtime_integration_harness.h"

#include "users/noah/lib/key/runtime/key_runtime_state.h"
#include "users/noah/noah_runtime.h"

__attribute__((weak)) layer_state_t layer_state;

__attribute__((weak)) bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

__attribute__((weak)) uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    (void)layer_num;
    (void)row;
    (void)column;
    return KC_TRNS;
}

__attribute__((weak)) bool noah_process_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return true;
}

__attribute__((weak)) void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (layer_ownership_debug_snapshot_t){0};
}

__attribute__((weak)) void held_action_debug_snapshot(held_action_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (held_action_debug_snapshot_t){0};
}

__attribute__((weak)) void held_repeat_debug_snapshot(held_repeat_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (held_repeat_debug_snapshot_t){0};
}

__attribute__((weak)) void keyboard_mod_ownership_debug_snapshot(keyboard_mod_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (keyboard_mod_ownership_debug_snapshot_t){0};
}

__attribute__((weak)) void layer_ownership_reset_for_test(void) {
}

__attribute__((weak)) void held_action_reset_for_test(void) {
}

__attribute__((weak)) void held_repeat_reset_for_test(void) {
}

__attribute__((weak)) void keyboard_mod_ownership_reset_for_test(void) {
}

static keyrecord_t key_runtime_integration_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .key     = key_pos,
                .pressed = pressed,
            },
    };
}

void key_runtime_integration_advance(uint16_t *time, uint16_t advance_ms) {
    if (!time) {
        return;
    }

    *time = (uint16_t)(*time + advance_ms);
}

void key_runtime_integration_scan(void) {
    noah_key_runtime_scan();
}

bool key_runtime_integration_process_record(uint16_t keycode, keypos_t key_pos, bool pressed) {
    keyrecord_t record = key_runtime_integration_record(key_pos, pressed);
    return noah_process_record_user(keycode, &record);
}

void key_runtime_integration_run(uint16_t *time, const key_runtime_integration_step_t *steps, uint8_t step_count) {
    if (!time || !steps) {
        return;
    }

    for (uint8_t index = 0; index < step_count; index++) {
        switch (steps[index].kind) {
            case KEY_RUNTIME_INTEGRATION_STEP_PRESS: {
                (void)key_runtime_integration_process_record(steps[index].data.key_event.keycode, steps[index].data.key_event.key_pos, true);
                break;
            }
            case KEY_RUNTIME_INTEGRATION_STEP_RELEASE: {
                (void)key_runtime_integration_process_record(steps[index].data.key_event.keycode, steps[index].data.key_event.key_pos, false);
                break;
            }
            case KEY_RUNTIME_INTEGRATION_STEP_ADVANCE_MS:
                key_runtime_integration_advance(time, steps[index].data.advance_ms);
                break;
            case KEY_RUNTIME_INTEGRATION_STEP_SCAN:
                key_runtime_integration_scan();
                break;
        }
    }
}

handled_key_resolution_t key_runtime_integration_multi_tap_handled_key(uint16_t keycode, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term) {
    handled_key_resolution_t resolution = handled_key_lookup(keycode);

    resolution.flags |= HANDLED_KEY_FLAG_MULTI_TAP;
    resolution.tap_hold_term    = tap_hold_term;
    resolution.longer_hold_term = longer_hold_term;
    resolution.multi_tap_term   = multi_tap_term;
    return resolution;
}

bool key_runtime_integration_process_handled_press(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution) {
    keyrecord_t record = key_runtime_integration_record(key_pos, true);
    return key_runtime_process_handled_key_press(keycode, &record, resolution);
}

bool key_runtime_integration_process_handled_release(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution) {
    keyrecord_t record = key_runtime_integration_record(key_pos, false);
    return key_runtime_process_handled_key_release(keycode, &record, resolution);
}

void key_runtime_integration_debug_snapshot(noah_runtime_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    noah_runtime_debug_snapshot(out);
}

uint16_t key_runtime_integration_snapshot_slot_owner_keycode(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    return noah_runtime_debug_slot_owner_keycode(snapshot, key_pos);
}

uint16_t key_runtime_integration_snapshot_slot_held_action_keycode(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    return noah_runtime_debug_slot_held_action_keycode(snapshot, key_pos);
}

uint8_t key_runtime_integration_snapshot_slot_pending_multi_tap_count(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    return noah_runtime_debug_slot_pending_multi_tap_count(snapshot, key_pos);
}

bool key_runtime_integration_snapshot_slot_pending_multi_tap_holding(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    return noah_runtime_debug_slot_pending_multi_tap_holding(snapshot, key_pos);
}

bool key_runtime_integration_snapshot_slot_has_pending_multi_tap(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    return noah_runtime_debug_slot_has_pending_multi_tap(snapshot, key_pos);
}

bool key_runtime_integration_snapshot_slot_hold_is_complete(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    return noah_runtime_debug_slot_hold_is_complete(snapshot, key_pos);
}
