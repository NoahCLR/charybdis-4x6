// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_shared_state.h"

runtime_shared_state_t noah_runtime_shared_state = {0};

void runtime_shared_state_reset(runtime_shared_state_t *state) {
    if (!state) {
        return;
    }

    *state = (runtime_shared_state_t){0};

    for (uint16_t i = 0; i < KEY_RUNTIME_SLOT_TABLE_CAPACITY; i++) {
        state->key.slots_by_position[i] = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
    }
}
