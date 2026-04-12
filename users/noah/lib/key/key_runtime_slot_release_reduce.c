// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Release Reducer
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_release_reduce.h"

#include "key_runtime_slot_release_active.h"
#include "key_runtime_slot_pending_multi_tap.h"
#include "key_runtime_slot_result_internal.h"

key_runtime_slot_result_t key_runtime_slot_reduce_handled_release(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key) {
    key_runtime_slot_result_t result = {0};

    if (slot) {
        uint16_t elapsed = timer_elapsed(slot->timer);
        result           = key_runtime_slot_pending_multi_tap_handle_release(slot, keycode, key, elapsed);
        if (result.handled) {
            return result;
        }
    }

    if (key_runtime_slot_matches(slot, keycode, key_pos)) {
        return key_runtime_slot_reduce_active_release(slot, keycode, key);
    }

    result.handled = true;
    if (handled_key_is_momentary_layer(key)) {
        key_runtime_slot_result_push_layer_release(&result, key_pos);
    }
    key_runtime_slot_result_push_builder_if_present(&result, key_pos,
                                                    (key_runtime_effect_builder_t){
                                                        .release_owned_state = true,
                                                    });
    return result;
}
