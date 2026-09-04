// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────

#include <string.h>

#include "runtime_context_internal.h"
#include "../diagnostics/runtime_diag.h"

__attribute__((weak)) void noah_owned_keycode_reset_for_test(void) {}

// Zero-initialised on purpose. A partial static initialiser here set four of
// the seven non-zero core defaults and, because any non-zero member forces the
// whole 18 KB context into .data, also spent 18 KB of flash on an image that
// was 99.98% zeros plus a boot-time copy of it. Production now runs the same
// reset the tests do, so both paths start from one definition of the defaults.
static noah_runtime_context_t noah_runtime_singleton;

void noah_runtime_shared_state_post_init(void) {
    key_runtime_core_state_reset(&noah_runtime_singleton.shared.core);
}

noah_runtime_context_t *noah_runtime_context(void) {
    return &noah_runtime_singleton;
}

pd_mode_runtime_shared_state_t *pd_mode_runtime_shared_state(void) {
    return &noah_runtime_context()->shared.pd;
}

key_runtime_core_state_t *key_runtime_core_state(void) {
    return &noah_runtime_context()->shared.core;
}

static void runtime_shared_state_reset(runtime_shared_state_t *state) {
    if (!state) {
        return;
    }

    *state = (runtime_shared_state_t){0};
    key_runtime_core_state_reset(&state->core);
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

void noah_runtime_reset_for_test(void) {
    noah_runtime_context_reset_for_test(noah_runtime_context());
    noah_runtime_diag_reset_for_test();
    noah_owned_keycode_reset_for_test();

    layer_state = 0;
    clear_mods();
    clear_weak_mods();
    clear_oneshot_mods();
    clear_oneshot_locked_mods();
    send_keyboard_report();
}
