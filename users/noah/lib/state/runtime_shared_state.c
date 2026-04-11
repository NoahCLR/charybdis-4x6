// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_shared_state.h"

runtime_shared_state_t noah_runtime_shared_state = {
    .key =
        {
            .active_slots =
                {
                    [0] = (key_runtime_slot_state_t)ACTIVE_KEY_STATE_INIT,
                    [1] = (key_runtime_slot_state_t)ACTIVE_KEY_STATE_INIT,
                },
        },
    .pd = {0},
};
