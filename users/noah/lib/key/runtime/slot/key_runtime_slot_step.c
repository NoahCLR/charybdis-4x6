// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Step
// ────────────────────────────────────────────────────────────────────────────
//
// Reducer implementation for handled-key slot events. The transition layer
// feeds one slot event at a time through this module, which owns the remaining
// press/release/scan/interrupt/flush state reducers.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_step.h"

#include "key_runtime_slot_pending_multi_tap.h"
#include "key_runtime_slot_policy.h"
#include "key_runtime_slot_press_reduce.h"
#include "key_runtime_slot_release_reduce.h"
#include "key_runtime_slot_scan_reduce.h"
#include "key_runtime_slot_result_internal.h"

#include "../key_runtime_trace.h"

static key_runtime_slot_result_t key_runtime_slot_step_interrupt(active_key_state_t *slot, keypos_t other_key_pos) {
    key_runtime_slot_result_t    result  = {0};
    key_runtime_effect_builder_t builder = key_runtime_slot_policy_interrupt_on_other_press(slot, other_key_pos);

    if (!key_runtime_slot_result_builder_has_effect(builder)) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_builder_if_present(&result, slot ? slot->owner.key_pos : (keypos_t){0}, builder);
    return result;
}

static key_runtime_slot_result_t key_runtime_slot_step_pending_multi_tap_flush(active_key_state_t *slot) {
    key_runtime_slot_result_t                  result = {0};
    key_runtime_slot_pending_multi_tap_flush_t flush  = key_runtime_slot_take_pending_multi_tap_flush(slot);

    if (!flush.handled) {
        return result;
    }

    result.handled = true;
    key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_FLUSH_PENDING_CHAIN, flush.repeat_count, flush.action);
    key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    return result;
}

typedef key_runtime_slot_result_t (*key_runtime_slot_step_event_handler_t)(active_key_state_t *slot, const key_runtime_slot_event_t *event);

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_handled_press(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    return key_runtime_slot_reduce_handled_press(slot, event->data.handled_press.keycode, event->data.handled_press.key_pos, event->data.handled_press.key, event->data.handled_press.active_held_action_survives_flush);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_handled_release(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    return key_runtime_slot_reduce_handled_release(slot, event->data.handled_release.keycode, event->data.handled_release.key_pos, event->data.handled_release.key);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_active_scan(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    (void)event;
    return key_runtime_slot_reduce_active_scan(slot);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_pending_multi_tap_scan(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    (void)event;
    return key_runtime_slot_pending_multi_tap_handle_scan(slot);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_interrupt(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    return key_runtime_slot_step_interrupt(slot, event->data.interrupt.other_key_pos);
}

static key_runtime_slot_result_t key_runtime_slot_step_handle_event_pending_multi_tap_flush(active_key_state_t *slot, const key_runtime_slot_event_t *event) {
    (void)event;
    return key_runtime_slot_step_pending_multi_tap_flush(slot);
}

static const key_runtime_slot_step_event_handler_t key_runtime_slot_step_event_handlers[] = {
    [KEY_RUNTIME_SLOT_EVENT_NONE] = NULL, [KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS] = key_runtime_slot_step_handle_event_handled_press, [KEY_RUNTIME_SLOT_EVENT_HANDLED_RELEASE] = key_runtime_slot_step_handle_event_handled_release, [KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN] = key_runtime_slot_step_handle_event_active_scan, [KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN] = key_runtime_slot_step_handle_event_pending_multi_tap_scan, [KEY_RUNTIME_SLOT_EVENT_INTERRUPT] = key_runtime_slot_step_handle_event_interrupt, [KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_FLUSH] = key_runtime_slot_step_handle_event_pending_multi_tap_flush,
};

key_runtime_slot_result_t key_runtime_slot_step(active_key_state_t *slot, key_runtime_slot_event_t event) {
    key_runtime_slot_step_event_handler_t handler = event.kind < ARRAY_SIZE(key_runtime_slot_step_event_handlers) ? key_runtime_slot_step_event_handlers[event.kind] : NULL;

    return handler ? handler(slot, &event) : (key_runtime_slot_result_t){0};
}
