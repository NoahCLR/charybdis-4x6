// ────────────────────────────────────────────────────────────────────────────
// Runtime Debug Snapshot
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_debug.h"

void noah_runtime_debug_snapshot(noah_runtime_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    out->core = noah_runtime_shared_state;
    layer_ownership_debug_snapshot(&out->layer_ownership);
    held_action_debug_snapshot(&out->held_actions);
    keyboard_mod_ownership_debug_snapshot(&out->keyboard_mod_ownership);
}

void noah_runtime_reset_for_test(void) {
    runtime_shared_state_reset(&noah_runtime_shared_state);
    layer_ownership_reset_for_test();
    held_action_reset_for_test();
    keyboard_mod_ownership_reset_for_test();

    layer_state = 0;
    clear_mods();
    clear_weak_mods();
    clear_oneshot_mods();
    clear_oneshot_locked_mods();
    send_keyboard_report();
}
