// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Process Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "process_internal.h"
#include "trace.h"
#include "../../macro/macro_dispatch.h"
#include "../../pointing/defs/pd_modes.h"
#include "../../pointing/runtime/pd_mode_keyboard_event_internal.h"
#include "core/runtime.h"
#include "../../action/synthetic_record.h"
#include "../../compat/qmk_combo_origin.h"
#include "../../state/ownership/keyboard_mod_ownership.h"
#include "../../state/runtime/runtime_diag.h"
#include "../../state/runtime/keyboard_mod_state.h"
#include "origin_registry.h"

#ifdef NOAH_HOST_TEST_ENV
bool key_runtime_integration_userspace_feeds_core_key_events(void) {
    return true;
}
#endif

typedef enum {
    KEY_RUNTIME_PROCESS_NEXT = 0,
    KEY_RUNTIME_PROCESS_RETURN_TRUE,
    KEY_RUNTIME_PROCESS_RETURN_FALSE,
} key_runtime_process_stage_outcome_t;

typedef struct key_runtime_process_ctx_t key_runtime_process_ctx_t;
typedef key_runtime_process_stage_outcome_t (*key_runtime_process_stage_fn_t)(key_runtime_process_ctx_t *ctx);
typedef struct {
    const char                    *name;
    key_runtime_process_stage_fn_t handler;
} key_runtime_process_stage_entry_t;

struct key_runtime_process_ctx_t {
    uint16_t                 keycode;
    uint16_t                 runtime_keycode;
    keyrecord_t             *record;
    handled_key_resolution_t resolution;
    bool                     resolution_loaded;
};

static keyboard_mod_state_t key_runtime_keyboard_mod_state_current(void) {
    return (keyboard_mod_state_t){
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
}

static void key_runtime_process_end_keyboard_event_mod_mask(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    keyboard_mod_state_t      restored;

    if (!(state && state->keyboard_event_mask_active)) {
        return;
    }

    restored      = key_runtime_keyboard_mod_state_current();
    restored.real = (uint8_t)(restored.real | keyboard_mod_ownership_managed_only_mask(state->keyboard_event_masked_real_mods));
    keyboard_mod_state_apply(restored);
    state->keyboard_event_mask_active      = false;
    state->keyboard_event_masked_real_mods = 0u;
}

static void key_runtime_process_begin_keyboard_event_mod_mask(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    keyboard_mod_state_t      filtered;
    uint8_t                   masked_real_mods;

    if (!(state && !state->keyboard_event_mask_active)) {
        return;
    }

    masked_real_mods = pd_mode_active_keyboard_event_masked_real_mods();
    if (masked_real_mods == 0u) {
        return;
    }

    filtered = key_runtime_keyboard_mod_state_current();
    if ((filtered.real & masked_real_mods) == 0u) {
        return;
    }

    filtered.real &= (uint8_t)~masked_real_mods;
    keyboard_mod_state_apply(filtered);
    state->keyboard_event_masked_real_mods = masked_real_mods;
    state->keyboard_event_mask_active      = true;
}

static handled_key_resolution_t key_runtime_process_resolution(key_runtime_process_ctx_t *ctx) {
    if (!ctx->resolution_loaded) {
        ctx->resolution        = handled_key_lookup(ctx->runtime_keycode);
        ctx->resolution_loaded = true;
    }

    return ctx->resolution;
}

static key_runtime_process_stage_outcome_t key_runtime_process_stage_synthetic_passthrough(key_runtime_process_ctx_t *ctx) {
    if (!noah_synthetic_record_active()) {
        return KEY_RUNTIME_PROCESS_NEXT;
    }

    key_runtime_trace_message("process:synthetic_passthrough", "synthetic record bypassed physical runtime");
    (void)ctx;
    return KEY_RUNTIME_PROCESS_RETURN_TRUE;
}

static key_runtime_process_stage_outcome_t key_runtime_process_stage_preflight(key_runtime_process_ctx_t *ctx) {
    bool keep_processing = key_runtime_preflight_record(ctx->keycode, ctx->record);

    key_runtime_trace_bool_result("process:preflight", ctx->keycode, ctx->record, keep_processing);
    return keep_processing ? KEY_RUNTIME_PROCESS_NEXT : KEY_RUNTIME_PROCESS_RETURN_FALSE;
}

static key_runtime_process_stage_outcome_t key_runtime_process_stage_release_slot_keycode(key_runtime_process_ctx_t *ctx) {
    const press_token_t *token;

    if (ctx->record->event.pressed) {
        return KEY_RUNTIME_PROCESS_NEXT;
    }

    token = key_runtime_core_press_token_at(ctx->record->event.key);
    if (token && token->resolved_keycode != KC_NO) {
        ctx->runtime_keycode = token->resolved_keycode;
    }

    return KEY_RUNTIME_PROCESS_NEXT;
}

static key_runtime_process_stage_outcome_t key_runtime_process_stage_pd_mode(key_runtime_process_ctx_t *ctx) {
    if (!pd_mode_handle_key_event(ctx->runtime_keycode, ctx->record)) {
        return KEY_RUNTIME_PROCESS_NEXT;
    }

    key_runtime_trace_message("process:pd_mode_key_handler", "key event consumed by active pd mode");
    return KEY_RUNTIME_PROCESS_RETURN_FALSE;
}

static key_runtime_process_stage_outcome_t key_runtime_process_stage_handled_key(key_runtime_process_ctx_t *ctx) {
    handled_key_resolution_t resolution = key_runtime_process_resolution(ctx);
    bool                     handled;

    if (!handled_key_resolution_is_handled(resolution)) {
        return KEY_RUNTIME_PROCESS_NEXT;
    }

    handled = ctx->record->event.pressed ? key_runtime_process_handled_key_press(ctx->runtime_keycode, ctx->record, resolution) : key_runtime_process_handled_key_release(ctx->runtime_keycode, ctx->record, resolution);
    key_runtime_trace_bool_result("process:handled_key", ctx->runtime_keycode, ctx->record, handled);
    return handled ? KEY_RUNTIME_PROCESS_RETURN_FALSE : KEY_RUNTIME_PROCESS_NEXT;
}

static key_runtime_process_stage_outcome_t key_runtime_process_stage_non_handled_release_cleanup(key_runtime_process_ctx_t *ctx) {
    if (ctx->record->event.pressed) {
        return KEY_RUNTIME_PROCESS_NEXT;
    }

    if (handled_key_resolution_is_handled(key_runtime_process_resolution(ctx))) {
        return KEY_RUNTIME_PROCESS_NEXT;
    }

    if (key_runtime_core_finalize_non_handled_release(ctx->record->event.key)) {
        key_runtime_trace_message("process:non_handled_release_cleanup", "finalized core-owned release state for non-handled key");
    }

    return KEY_RUNTIME_PROCESS_NEXT;
}

static key_runtime_process_stage_outcome_t key_runtime_process_stage_direct_action(key_runtime_process_ctx_t *ctx) {
    if (!key_runtime_process_direct_action_key(ctx->keycode, ctx->record)) {
        return KEY_RUNTIME_PROCESS_NEXT;
    }

    key_runtime_trace_message("process:direct_action", "direct action key consumed");
    return KEY_RUNTIME_PROCESS_RETURN_FALSE;
}

static key_runtime_process_stage_outcome_t key_runtime_process_stage_macro_dispatch(key_runtime_process_ctx_t *ctx) {
    if (ctx->record->event.pressed && macro_dispatch(ctx->keycode)) {
        key_runtime_trace_message("process:macro_dispatch", "macro keycode consumed");
        return KEY_RUNTIME_PROCESS_RETURN_FALSE;
    }

    return ctx->record->event.pressed ? KEY_RUNTIME_PROCESS_NEXT : KEY_RUNTIME_PROCESS_RETURN_TRUE;
}

static bool key_runtime_process_finish(key_runtime_process_ctx_t *ctx, bool keep_processing) {
    (void)ctx;
    noah_runtime_diag_scope_leave();
    return keep_processing;
}

bool noah_get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

bool noah_pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (noah_synthetic_record_active()) {
        return true;
    }

    if (key_origin_keypos_valid(record->event.key)) {
        key_origin_registry_set_single(record->event.key);
        noah_qmk_combo_origin_observe_physical_key_event(keycode, record);
    }

    keyboard_mod_ownership_track_physical_keycode_event(keycode, record);
    return true;
}

bool noah_process_record_user(uint16_t keycode, keyrecord_t *record) {
    static const key_runtime_process_stage_entry_t stages[] = {
        {.name = "synthetic_passthrough", .handler = key_runtime_process_stage_synthetic_passthrough}, {.name = "preflight", .handler = key_runtime_process_stage_preflight}, {.name = "release_slot_keycode", .handler = key_runtime_process_stage_release_slot_keycode}, {.name = "pd_mode", .handler = key_runtime_process_stage_pd_mode}, {.name = "handled_key", .handler = key_runtime_process_stage_handled_key}, {.name = "non_handled_release_cleanup", .handler = key_runtime_process_stage_non_handled_release_cleanup}, {.name = "direct_action", .handler = key_runtime_process_stage_direct_action}, {.name = "macro_dispatch", .handler = key_runtime_process_stage_macro_dispatch},
    };
    key_runtime_process_ctx_t ctx = {
        .keycode         = keycode,
        .runtime_keycode = keycode,
        .record          = record,
    };

    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD);

    noah_qmk_combo_origin_normalize_record(keycode, record);

    if (!noah_synthetic_record_active()) {
        key_runtime_core_observe_process_record_event(keycode, record);
    }

    key_runtime_trace_record("process:entry", keycode, record);

    switch (key_runtime_process_stage_synthetic_passthrough(&ctx)) {
        case KEY_RUNTIME_PROCESS_RETURN_TRUE:
            return key_runtime_process_finish(&ctx, true);
        case KEY_RUNTIME_PROCESS_RETURN_FALSE:
            return key_runtime_process_finish(&ctx, false);
        case KEY_RUNTIME_PROCESS_NEXT:
        default:
            break;
    }

    key_runtime_process_begin_keyboard_event_mod_mask();

    for (uint8_t index = 1; index < ARRAY_SIZE(stages); index++) {
        const key_runtime_process_stage_entry_t *stage = &stages[index];

        switch (stage->handler(&ctx)) {
            case KEY_RUNTIME_PROCESS_RETURN_TRUE:
                return key_runtime_process_finish(&ctx, true);
            case KEY_RUNTIME_PROCESS_RETURN_FALSE:
                return key_runtime_process_finish(&ctx, false);
            case KEY_RUNTIME_PROCESS_NEXT:
            default:
                (void)stage->name;
                break;
        }
    }

    return key_runtime_process_finish(&ctx, true);
}

void noah_process_record_user_finalize(uint16_t keycode, keyrecord_t *record, bool keep_processing) {
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD_FINALIZE);
    key_runtime_process_end_keyboard_event_mod_mask();
    key_runtime_trace_bool_result("process:return", keycode, record, keep_processing);
    noah_runtime_diag_scope_leave();
}

void noah_post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    noah_process_record_user_finalize(keycode, record, true);
}
