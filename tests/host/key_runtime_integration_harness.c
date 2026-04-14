#include "key_runtime_integration_harness.h"

#include "users/noah/lib/key/runtime/key_runtime_api.h"
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
