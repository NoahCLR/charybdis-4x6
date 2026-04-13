#include "key_runtime_integration_harness.h"

#include <string.h>

#include "users/noah/lib/key/runtime/key_runtime_state.h"
#include "users/noah/noah_runtime.h"

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

static keyrecord_t key_runtime_integration_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .key     = key_pos,
                .pressed = pressed,
            },
    };
}

static bool key_runtime_integration_slot_position_is_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static uint16_t key_runtime_integration_slot_table_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * (uint16_t)MATRIX_COLS + (uint16_t)key_pos.col);
}

void key_runtime_integration_run(uint16_t *time, const key_runtime_integration_step_t *steps, uint8_t step_count) {
    if (!time || !steps) {
        return;
    }

    for (uint8_t index = 0; index < step_count; index++) {
        switch (steps[index].kind) {
            case KEY_RUNTIME_INTEGRATION_STEP_PRESS: {
                keyrecord_t record = key_runtime_integration_record(steps[index].data.key_event.key_pos, true);
                (void)noah_process_record_user(steps[index].data.key_event.keycode, &record);
                break;
            }
            case KEY_RUNTIME_INTEGRATION_STEP_RELEASE: {
                keyrecord_t record = key_runtime_integration_record(steps[index].data.key_event.key_pos, false);
                (void)noah_process_record_user(steps[index].data.key_event.keycode, &record);
                break;
            }
            case KEY_RUNTIME_INTEGRATION_STEP_ADVANCE_MS:
                *time = (uint16_t)(*time + steps[index].data.advance_ms);
                break;
            case KEY_RUNTIME_INTEGRATION_STEP_SCAN:
                noah_key_runtime_scan();
                break;
        }
    }
}

void key_runtime_integration_debug_snapshot(noah_runtime_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->core = noah_runtime_shared_state;

    layer_ownership_debug_snapshot(&out->layer_ownership);
    held_action_debug_snapshot(&out->held_actions);
    held_repeat_debug_snapshot(&out->held_repeats);
    keyboard_mod_ownership_debug_snapshot(&out->keyboard_mod_ownership);
    noah_runtime_trace_snapshot(&out->trace);
}

const active_key_state_t *key_runtime_integration_snapshot_slot(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    if (!snapshot || !key_runtime_integration_slot_position_is_valid(key_pos)) {
        return NULL;
    }

    return &snapshot->core.key.slots_by_position[key_runtime_integration_slot_table_index(key_pos)];
}
