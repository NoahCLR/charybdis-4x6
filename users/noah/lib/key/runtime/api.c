// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "api.h"
#include "transition.h"
#include "reducer/runtime.h"
#include "planning/effect_plan.h"
#include "projection/projection.h"
#include "trace.h"
#include "../behavior/key_behavior_lookup.h"
#include "../behavior/participation.h"
#include "../../compat/qmk_source_layer_contract.h"
#include "../../split/runtime_sync.h"

// Which are due is decided for all keys first, as one plan would decide them
// (projecting one may preempt another key's pointing mode); each is then
// announced, traced and projected in key order. No plan is held: this runs
// beneath the projection of an output, on the reviewed stack paths.
bool noah_key_runtime_settle_pending_fallback_hold(void) {
    uint32_t due[KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT];
    uint8_t  projected = 0u;

    if (!key_runtime_core_pending_fallback_holds(due)) {
        return false;
    }
    split_runtime_sync_notify_key_feedback_dirty();
    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        key_runtime_effect_t effect;

        if (!(due[index / 32u] & (UINT32_C(1) << (index % 32u))) || !key_runtime_core_fallback_hold_effect(index, &effect)) {
            continue;
        }
        key_runtime_trace_effect_execute(projected++, &effect);
        key_runtime_core_project_effect(&effect);
    }
    return true;
}

// Not inlined, so a press with no layer tap to settle carries no plan through
// its behaviour lookup.
static __attribute__((noinline)) bool noah_key_runtime_settle_foreign_layer_taps(keypos_t key_pos) {
    key_runtime_core_effect_plan_t plan;
    bool                           settled_any;

    key_runtime_core_effect_plan_init_streaming(&plan);
    settled_any = key_runtime_core_flush_foreign_layer_multi_tap(key_pos, &plan);
    key_runtime_transition_execute_plan(NULL, &plan);
    return settled_any;
}

bool noah_key_runtime_settle_layer_taps_before_press(keyrecord_t *record) {
    uint16_t keycode;

    // Without another key's pending layer tap an ordinary press costs no lookup.
    if (!(record && record->event.type == KEY_EVENT && record->event.pressed && key_runtime_core_flush_foreign_layer_multi_tap(record->event.key, NULL))) {
        return false;
    }
    // Handled keys keep independent series alive (INTERACTION_MODEL.md): only a
    // press that settles pending taps anyway settles these before it resolves.
    keycode = get_record_keycode(record, false);
    if ((noah_participation_behavior(keycode, noah_qmk_contract_resolve_source_layer(record->event.key), record->event.key) ? key_behavior_lookup(keycode) : key_behavior_lookup_without_row(keycode)).handled) {
        return false;
    }

    return noah_key_runtime_settle_foreign_layer_taps(record->event.key);
}

void noah_key_runtime_activity_snapshot(noah_key_runtime_activity_snapshot_t *out) {
    key_runtime_core_state_t *state;

    if (!out) {
        return;
    }

    *out  = (noah_key_runtime_activity_snapshot_t){0};
    state = key_runtime_core_state();
    if (!state) {
        return;
    }

    *out = (noah_key_runtime_activity_snapshot_t){
        .press_token_count       = state->press_token_count,
        .tap_series_count        = state->tap_series_count,
        .lease_count             = state->lease_count,
        .pending_release_count   = state->pending_release_count,
        .persistent_intent_count = state->persistent_intent_count,
    };
}
