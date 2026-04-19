#include "key_runtime_integration_harness.h"

#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/runtime_v2/runtime_v2_trace.h"
#include "users/noah/lib/key/runtime/key_runtime_api.h"
#include "users/noah/noah_runtime.h"

__attribute__((weak)) layer_state_t layer_state;

__attribute__((weak)) uint16_t timer_read(void) {
    return 0u;
}

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

__attribute__((weak)) bool key_runtime_integration_pre_userspace_record(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return true;
}

__attribute__((weak)) void key_runtime_integration_shadow_runtime_v2_apply_event(const runtime_event_t *event, uint16_t event_time) {
    (void)event;
    (void)event_time;
}

__attribute__((weak)) bool key_runtime_integration_userspace_feeds_runtime_v2_key_events(void) {
    return false;
}

__attribute__((weak)) bool key_runtime_integration_userspace_feeds_runtime_v2_scan_events(void) {
    return false;
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
    runtime_event_t event = {
        .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
        .data.timer_advance =
            {
                .advance_ms = advance_ms,
            },
    };

    runtime_v2_trace_record_input_event(&event);
    key_runtime_integration_shadow_runtime_v2_apply_event(&event, time ? *time : timer_read());

    if (time) {
        *time = (uint16_t)(*time + advance_ms);
    }
}

void key_runtime_integration_scan(void) {
    runtime_event_t event = {
        .kind = RUNTIME_EVENT_KIND_SCAN,
    };

    runtime_v2_trace_record_input_event(&event);
    if (!key_runtime_integration_userspace_feeds_runtime_v2_scan_events()) {
        key_runtime_integration_shadow_runtime_v2_apply_event(&event, timer_read());
    }
    noah_key_runtime_scan();
    runtime_v2_trace_capture_projection();
}

bool key_runtime_integration_process_record(uint16_t keycode, keypos_t key_pos, bool pressed) {
    keyrecord_t record = key_runtime_integration_record(key_pos, pressed);
    bool        keep_processing;
    runtime_event_t event = {
        .kind = pressed ? RUNTIME_EVENT_KIND_KEY_DOWN : RUNTIME_EVENT_KIND_KEY_UP,
        .data.key_event =
            {
                .keycode = keycode,
                .key_pos = key_pos,
            },
    };

    runtime_v2_trace_record_input_event(&event);
    if (!key_runtime_integration_userspace_feeds_runtime_v2_key_events()) {
        key_runtime_integration_shadow_runtime_v2_apply_event(&event, timer_read());
    }

    if (!key_runtime_integration_pre_userspace_record(keycode, &record)) {
        runtime_v2_trace_capture_projection();
        return false;
    }

    if (!noah_pre_process_record_user(keycode, &record)) {
        runtime_v2_trace_capture_projection();
        return false;
    }

    keep_processing = noah_process_record_user(keycode, &record);
    if (!keep_processing) {
        noah_process_record_user_finalize(keycode, &record, false);
        runtime_v2_trace_capture_projection();
        return false;
    }

    noah_post_process_record_user(keycode, &record);
    runtime_v2_trace_capture_projection();
    return true;
}

bool key_runtime_integration_apply_runtime_v2_event(uint16_t *time, const runtime_event_t *event) {
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
            runtime_v2_trace_record_input_event(event);
            key_runtime_integration_shadow_runtime_v2_apply_event(event, time ? *time : timer_read());
            (void)noah_pointing_device_task_user(event->data.pointer_report.report);
            runtime_v2_trace_capture_projection();
            return true;
        case RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT:
            runtime_v2_trace_record_input_event(event);
            key_runtime_integration_shadow_runtime_v2_apply_event(event, time ? *time : timer_read());
            pd_mode_apply_remote_snapshot(event->data.remote_snapshot.active_mode, event->data.remote_snapshot.locked_mode);
            runtime_v2_trace_capture_projection();
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
