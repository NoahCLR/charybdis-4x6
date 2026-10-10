// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "api.h"
#include "transition.h"
#include "reducer/runtime.h"
#include "../behavior/key_behavior_lookup.h"
#include "../behavior/participation.h"
#include "../../compat/qmk_source_layer_contract.h"

bool noah_key_runtime_settle_pending_fallback_hold(void) {
    key_runtime_transition_plan_t plan;
    bool                          settled_any;

    key_runtime_transition_plan_init(&plan);
    settled_any = key_runtime_transition_settle_pending_fallback_hold(&plan);
    key_runtime_transition_execute_plan(&plan);
    return settled_any;
}

bool noah_key_runtime_settle_layer_taps_before_press(keyrecord_t *record) {
    key_runtime_transition_plan_t plan;
    bool                          settled_any;
    uint16_t                      keycode;

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

    key_runtime_transition_plan_init(&plan);
    settled_any = key_runtime_transition_flush_foreign_layer_multi_tap(record->event.key, &plan);
    key_runtime_transition_execute_plan(&plan);
    return settled_any;
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
