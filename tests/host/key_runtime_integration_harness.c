#include "key_runtime_integration_harness.h"

#include <string.h>

#include "users/noah/lib/pointing/policy/pointer_layer_policy.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/key/runtime/trace/core_trace.h"
#include "users/noah/lib/key/runtime/api.h"
#include "users/noah/noah_runtime.h"

__attribute__((weak)) layer_state_t layer_state;
__attribute__((weak)) layer_state_t default_layer_state;

__attribute__((weak)) uint16_t timer_read(void) {
    return 0u;
}

__attribute__((weak)) bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

__attribute__((weak)) uint8_t get_highest_layer(layer_state_t state) {
    for (int8_t layer = (int8_t)(sizeof(layer_state_t) * 8 - 1); layer >= 0; layer--) {
        if ((state & ((layer_state_t)1u << layer)) != 0) {
            return (uint8_t)layer;
        }
    }

    return 0u;
}

__attribute__((weak)) uint8_t combo_ref_from_layer(uint8_t layer) {
    return layer;
}

__attribute__((weak)) uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    (void)layer_num;
    (void)row;
    (void)column;
    return KC_TRNS;
}

__attribute__((weak)) uint16_t keymap_key_to_keycode(uint8_t layer, keypos_t key) {
    return keycode_at_keymap_location(layer, key.row, key.col);
}

__attribute__((weak)) uint16_t get_record_keycode(keyrecord_t *record, bool update_layer_cache) {
    uint8_t layer;

    (void)update_layer_cache;

    if (!record) {
        return KC_NO;
    }

    layer = get_highest_layer(layer_state | default_layer_state);
    return keymap_key_to_keycode(layer, record->event.key);
}

__attribute__((weak)) bool noah_process_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return true;
}

__attribute__((weak)) bool noah_pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return true;
}

__attribute__((weak)) void noah_process_record_user_finalize(uint16_t keycode, keyrecord_t *record, bool keep_processing) {
    (void)keycode;
    (void)record;
    (void)keep_processing;
}

__attribute__((weak)) void noah_post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    noah_process_record_user_finalize(keycode, record, true);
}

__attribute__((weak)) report_mouse_t noah_pointing_device_task_user(report_mouse_t mouse_report) {
    return mouse_report;
}

__attribute__((weak)) void pd_mode_apply_remote_snapshot(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags) {
    (void)active_flags;
    (void)locked_flags;
}

__attribute__((weak)) void pd_mode_service_active_dpi_sync(void) {}

__attribute__((weak)) uint8_t pd_mode_active_keyboard_event_masked_real_mods(void) {
    return 0u;
}

__attribute__((weak)) pd_mode_mask_t pd_mode_local_active_snapshot(void) {
    return 0u;
}

__attribute__((weak)) pd_mode_mask_t pd_mode_local_locked_snapshot(void) {
    return 0u;
}

__attribute__((weak)) pd_mode_mask_t pd_mode_display_active_snapshot(void) {
    return 0u;
}

__attribute__((weak)) pd_mode_mask_t pd_mode_display_locked_snapshot(void) {
    return 0u;
}

__attribute__((weak)) bool pd_mode_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait) {
    (void)mode;
    (void)trait;
    return false;
}

__attribute__((weak)) void keyboard_mod_ownership_register(uint16_t keycode) {
    (void)keycode;
}

__attribute__((weak)) void keyboard_mod_ownership_unregister(uint16_t keycode) {
    (void)keycode;
}

__attribute__((weak)) void keyboard_mod_ownership_debug_snapshot(keyboard_mod_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
}

__attribute__((weak)) void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->applied_layer_state = layer_state;
}

__attribute__((weak)) void pointer_layer_policy_debug_snapshot(layer_state_t state, pointer_layer_policy_debug_snapshot_t *out) {
    (void)state;

    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
}

__attribute__((weak)) bool key_runtime_integration_pre_userspace_record(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return true;
}

__attribute__((weak)) void key_runtime_integration_shadow_core_apply_event(const runtime_event_t *event, uint16_t event_time) {
    (void)event;
    (void)event_time;
}

__attribute__((weak)) bool key_runtime_integration_userspace_feeds_core_key_events(void) {
    return false;
}

__attribute__((weak)) bool key_runtime_integration_userspace_feeds_core_scan_events(void) {
    return false;
}

static keyrecord_t key_runtime_integration_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .type    = KEY_EVENT,
                .key     = key_pos,
                .pressed = pressed,
            },
    };
}

void key_runtime_integration_advance(uint16_t *time, uint16_t advance_ms) {
    runtime_event_t event = {
        .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
        .data.timer_advance =
            {
                .advance_ms = advance_ms,
            },
    };

    key_runtime_core_trace_record_input_event(&event);
    key_runtime_integration_shadow_core_apply_event(&event, time ? *time : timer_read());

    if (time) {
        *time = (uint16_t)(*time + advance_ms);
    }
}

void key_runtime_integration_scan(void) {
    runtime_event_t event = {
        .kind = RUNTIME_EVENT_KIND_SCAN,
    };

    key_runtime_core_trace_record_input_event(&event);
    if (!key_runtime_integration_userspace_feeds_core_scan_events()) {
        key_runtime_integration_shadow_core_apply_event(&event, timer_read());
    }
    noah_key_runtime_scan();
    key_runtime_core_trace_capture_projection();
}

bool key_runtime_integration_process_record(uint16_t keycode, keypos_t key_pos, bool pressed) {
    keyrecord_t     record = key_runtime_integration_record(key_pos, pressed);
    bool            keep_processing;
    runtime_event_t event = {
        .kind = pressed ? RUNTIME_EVENT_KIND_KEY_DOWN : RUNTIME_EVENT_KIND_KEY_UP,
        .data.key_event =
            {
                .keycode = keycode,
                .key_pos = key_pos,
            },
    };

    key_runtime_core_trace_record_input_event(&event);
    if (!key_runtime_integration_userspace_feeds_core_key_events()) {
        key_runtime_integration_shadow_core_apply_event(&event, timer_read());
    }

    if (!key_runtime_integration_pre_userspace_record(keycode, &record)) {
        key_runtime_core_trace_capture_projection();
        return false;
    }

    if (!noah_pre_process_record_user(keycode, &record)) {
        key_runtime_core_trace_capture_projection();
        return false;
    }

    keep_processing = noah_process_record_user(keycode, &record);
    if (!keep_processing) {
        noah_process_record_user_finalize(keycode, &record, false);
        key_runtime_core_trace_capture_projection();
        return false;
    }

    noah_post_process_record_user(keycode, &record);
    key_runtime_core_trace_capture_projection();
    return true;
}

bool key_runtime_integration_apply_core_event(uint16_t *time, const runtime_event_t *event) {
    if (!event) {
        return false;
    }

    switch (event->kind) {
        case RUNTIME_EVENT_KIND_KEY_DOWN:
            return key_runtime_integration_process_record(event->data.key_event.keycode, event->data.key_event.key_pos, true);
        case RUNTIME_EVENT_KIND_KEY_UP:
            return key_runtime_integration_process_record(event->data.key_event.keycode, event->data.key_event.key_pos, false);
        case RUNTIME_EVENT_KIND_TIMER_ADVANCE:
            key_runtime_integration_advance(time, event->data.timer_advance.advance_ms);
            return true;
        case RUNTIME_EVENT_KIND_SCAN:
            key_runtime_integration_scan();
            return true;
        case RUNTIME_EVENT_KIND_POINTER_REPORT:
            key_runtime_core_trace_record_input_event(event);
            key_runtime_integration_shadow_core_apply_event(event, time ? *time : timer_read());
            (void)noah_pointing_device_task_user(event->data.pointer_report.report);
            key_runtime_core_trace_capture_projection();
            return true;
        case RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT:
            key_runtime_core_trace_record_input_event(event);
            key_runtime_integration_shadow_core_apply_event(event, time ? *time : timer_read());
            pd_mode_apply_remote_snapshot(event->data.remote_snapshot.active_mode, event->data.remote_snapshot.locked_mode);
            key_runtime_core_trace_capture_projection();
            return true;
    }

    return false;
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
