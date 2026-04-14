// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────

#include <string.h>

#include "runtime_context.h"
#include "runtime_shared_state.h"

static noah_runtime_context_t noah_runtime_singleton = {
    .shared.key.index.preview_owner_slot    = UINT8_MAX,
    .shared.key.index.pending_fallback_slot = UINT8_MAX,
};

noah_runtime_context_t *noah_runtime_context(void) {
    return &noah_runtime_singleton;
}

runtime_shared_state_t *noah_runtime_shared_state_ptr(void) {
    return &noah_runtime_context()->shared;
}

key_runtime_shared_state_t *key_runtime_shared_state(void) {
    return &noah_runtime_context()->shared.key;
}

void runtime_shared_state_reset(runtime_shared_state_t *state) {
    if (!state) {
        return;
    }

    *state = (runtime_shared_state_t){0};
    key_runtime_shared_state_reset(&state->key);
}

void noah_runtime_context_reset_for_test(noah_runtime_context_t *ctx) {
    if (!ctx) {
        return;
    }

    runtime_shared_state_reset(&ctx->shared);
    memset(&ctx->layer_ownership, 0, sizeof(ctx->layer_ownership));
    memset(&ctx->held_actions, 0, sizeof(ctx->held_actions));
    memset(&ctx->held_repeats, 0, sizeof(ctx->held_repeats));
    memset(&ctx->keyboard_mod_ownership, 0, sizeof(ctx->keyboard_mod_ownership));
    memset(&ctx->trace, 0, sizeof(ctx->trace));
}
