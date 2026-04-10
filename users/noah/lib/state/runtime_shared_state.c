// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_shared_state.h"

runtime_shared_state_t noah_runtime_shared_state = {
    .key =
        {
            .active_key = (active_key_state_t)ACTIVE_KEY_STATE_INIT,
            .multi_tap  = {0},
        },
    .pd = {0},
};
