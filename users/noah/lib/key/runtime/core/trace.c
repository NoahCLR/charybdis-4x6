// ────────────────────────────────────────────────────────────────────────────
// Runtime V2 Trace
// ────────────────────────────────────────────────────────────────────────────

#include "trace.h"

#if defined(NOAH_RUNTIME_TRACE_ENABLE)

enum {
    RUNTIME_V2_POINTER_FLAG_ANCHORED       = 1u << 0,
    RUNTIME_V2_POINTER_FLAG_PD_MODE_ANCHOR = 1u << 1,
    RUNTIME_V2_POINTER_FLAG_PREFERS_TYPING = 1u << 2,
    RUNTIME_V2_POINTER_FLAG_TOGGLE         = 1u << 3,
    RUNTIME_V2_POINTER_FLAG_SNIPING        = 1u << 4,
};

static uint16_t runtime_v2_trace_pack_keypos(keypos_t key_pos) {
    return (uint16_t)(((uint16_t)key_pos.row << 8) | key_pos.col);
}

static keypos_t runtime_v2_trace_unpack_keypos(uint16_t packed) {
    return (keypos_t){
        .row = (uint8_t)(packed >> 8),
        .col = (uint8_t)(packed & 0x00FFu),
    };
}

static uint16_t runtime_v2_trace_pack_signed_bytes(int8_t first, int8_t second) {
    return (uint16_t)(((uint16_t)(uint8_t)second << 8) | (uint8_t)first);
}

static int8_t runtime_v2_trace_unpack_low_signed(uint16_t packed) {
    return (int8_t)(packed & 0x00FFu);
}

static int8_t runtime_v2_trace_unpack_high_signed(uint16_t packed) {
    return (int8_t)(packed >> 8);
}

void runtime_v2_trace_record_input_event(const runtime_event_t *event) {
    if (!event) {
        return;
    }

    switch (event->kind) {
        case RUNTIME_EVENT_KIND_KEY_DOWN:
            noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_KEY_DOWN, runtime_v2_trace_pack_keypos(event->data.key_event.key_pos), event->data.key_event.keycode);
            break;
        case RUNTIME_EVENT_KIND_KEY_UP:
            noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_KEY_UP, runtime_v2_trace_pack_keypos(event->data.key_event.key_pos), event->data.key_event.keycode);
            break;
        case RUNTIME_EVENT_KIND_TIMER_ADVANCE:
            noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_TIMER_ADVANCE, event->data.timer_advance.advance_ms, 0u);
            break;
        case RUNTIME_EVENT_KIND_SCAN:
            noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_SCAN, 0u, 0u);
            break;
        case RUNTIME_EVENT_KIND_POINTER_REPORT:
            noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_POINTER_REPORT_AXES, runtime_v2_trace_pack_signed_bytes(event->data.pointer_report.report.x, event->data.pointer_report.report.y), runtime_v2_trace_pack_signed_bytes(event->data.pointer_report.report.h, event->data.pointer_report.report.v));
            noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_POINTER_REPORT_BUTTONS, event->data.pointer_report.report.buttons, 0u);
            break;
        case RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT:
            noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_REMOTE_SNAPSHOT, event->data.remote_snapshot.active_mode, event->data.remote_snapshot.locked_mode);
            break;
    }
}

void runtime_v2_trace_record_projection_snapshot(const projection_snapshot_t *snapshot) {
    uint16_t pointer_flags = 0;

    if (!snapshot) {
        return;
    }

    if (snapshot->pointer_anchor_active) {
        pointer_flags |= RUNTIME_V2_POINTER_FLAG_ANCHORED;
    }
    if (snapshot->pointer_pd_mode_anchor_active) {
        pointer_flags |= RUNTIME_V2_POINTER_FLAG_PD_MODE_ANCHOR;
    }
    if (snapshot->pointer_prefers_typing_layer) {
        pointer_flags |= RUNTIME_V2_POINTER_FLAG_PREFERS_TYPING;
    }
    if (snapshot->pointer_toggle_enabled) {
        pointer_flags |= RUNTIME_V2_POINTER_FLAG_TOGGLE;
    }
    if (snapshot->pointer_sniping_layer_active) {
        pointer_flags |= RUNTIME_V2_POINTER_FLAG_SNIPING;
    }

    noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_OUTPUT_PROJECTION_LAYER_STATE, (uint16_t)(snapshot->layer_state & 0xFFFFu), (uint16_t)((snapshot->layer_state >> 16) & 0xFFFFu));
    noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_OUTPUT_PROJECTION_LAYER_LOCKS, (uint16_t)(snapshot->locked_layer_mask & 0xFFFFu), (uint16_t)((snapshot->locked_layer_mask >> 16) & 0xFFFFu));
    noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_OUTPUT_PROJECTION_MOD_STATE, (uint16_t)(((uint16_t)snapshot->keyboard_mod_state.real) | ((uint16_t)snapshot->keyboard_mod_state.weak << 8)), (uint16_t)(((uint16_t)snapshot->keyboard_mod_state.oneshot) | ((uint16_t)snapshot->keyboard_mod_state.oneshot_locked << 8)));
    noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_OUTPUT_PROJECTION_MOD_OWNERSHIP, snapshot->keyboard_managed_mod_mask, snapshot->keyboard_physical_mod_mask);
    noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_OUTPUT_PROJECTION_PD_MODE_LOCAL, snapshot->pd_mode_local_active, snapshot->pd_mode_local_locked);
    noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_OUTPUT_PROJECTION_PD_MODE_DISPLAY, snapshot->pd_mode_display_active, snapshot->pd_mode_display_locked);
    noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_OUTPUT_PROJECTION_POINTER_LAYER, pointer_flags, (uint16_t)(((uint16_t)(uint8_t)snapshot->pointer_key_tracker << 8) | snapshot->pointer_layer));
    noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_OUTPUT_PROJECTION_RUNTIME_COUNTS, (uint16_t)(((uint16_t)snapshot->active_slot_count) | ((uint16_t)snapshot->pending_multi_tap_slot_count << 8)), (uint16_t)(((uint16_t)snapshot->deferred_release_count) | ((uint16_t)snapshot->v2_press_token_count << 8)));
    noah_runtime_trace_emit(NOAH_TRACE_RUNTIME_V2, NOAH_TRACE_RUNTIME_V2_EVENT_OUTPUT_PROJECTION_V2_COUNTS, (uint16_t)(((uint16_t)snapshot->v2_tap_series_count) | ((uint16_t)snapshot->v2_lease_count << 8)), (uint16_t)(((uint16_t)snapshot->v2_persistent_intent_count) | ((uint16_t)snapshot->v2_pending_release_count << 8)));
}

uint16_t runtime_v2_trace_decode_input_events(const noah_runtime_trace_snapshot_t *snapshot, runtime_event_t *out, uint16_t capacity) {
    uint16_t count = 0;

    if (!(snapshot && out && capacity != 0u)) {
        return 0u;
    }

    for (uint8_t index = 0; index < snapshot->count && count < capacity; index++) {
        const noah_runtime_trace_entry_t *entry = &snapshot->entries[index];

        if (entry->kind != NOAH_TRACE_RUNTIME_V2) {
            continue;
        }

        switch (entry->event) {
            case NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_KEY_DOWN:
            case NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_KEY_UP:
                out[count++] = (runtime_event_t){
                    .kind = entry->event == NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_KEY_DOWN ? RUNTIME_EVENT_KIND_KEY_DOWN : RUNTIME_EVENT_KIND_KEY_UP,
                    .data.key_event =
                        {
                            .keycode = entry->b,
                            .key_pos = runtime_v2_trace_unpack_keypos(entry->a),
                        },
                };
                break;
            case NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_TIMER_ADVANCE:
                out[count++] = (runtime_event_t){
                    .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
                    .data.timer_advance =
                        {
                            .advance_ms = entry->a,
                        },
                };
                break;
            case NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_SCAN:
                out[count++] = (runtime_event_t){
                    .kind = RUNTIME_EVENT_KIND_SCAN,
                };
                break;
            case NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_POINTER_REPORT_AXES: {
                runtime_event_t decoded = {
                    .kind = RUNTIME_EVENT_KIND_POINTER_REPORT,
                    .data.pointer_report =
                        {
                            .report =
                                {
                                    .x = runtime_v2_trace_unpack_low_signed(entry->a),
                                    .y = runtime_v2_trace_unpack_high_signed(entry->a),
                                    .h = runtime_v2_trace_unpack_low_signed(entry->b),
                                    .v = runtime_v2_trace_unpack_high_signed(entry->b),
                                },
                        },
                };

                if (index + 1u < snapshot->count) {
                    const noah_runtime_trace_entry_t *buttons = &snapshot->entries[index + 1u];

                    if (buttons->kind == NOAH_TRACE_RUNTIME_V2 && buttons->event == NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_POINTER_REPORT_BUTTONS) {
                        decoded.data.pointer_report.report.buttons = (uint8_t)buttons->a;
                        index++;
                    }
                }

                out[count++] = decoded;
                break;
            }
            case NOAH_TRACE_RUNTIME_V2_EVENT_INPUT_REMOTE_SNAPSHOT:
                out[count++] = (runtime_event_t){
                    .kind = RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT,
                    .data.remote_snapshot =
                        {
                            .active_mode = entry->a,
                            .locked_mode = entry->b,
                        },
                };
                break;
            default:
                break;
        }
    }

    return count;
}

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
