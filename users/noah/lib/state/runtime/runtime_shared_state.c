// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_shared_state.h"

runtime_shared_state_t noah_runtime_shared_state = {0};

key_runtime_shared_state_t *key_runtime_shared_state(void) {
    return &noah_runtime_shared_state.key;
}

void runtime_shared_state_reset(runtime_shared_state_t *state) {
    if (!state) {
        return;
    }

    *state = (runtime_shared_state_t){0};
    key_runtime_shared_state_reset(&state->key);
}
