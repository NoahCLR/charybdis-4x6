// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Process Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Press/release handling and process_record_user integration.
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "key_runtime_process_internal.h"
#include "key_runtime_internal.h"
#include "key_runtime_trace.h"
#include "noah_runtime.h"
#include "../../macro/macro_dispatch.h"
#include "../../pointing/defs/pd_modes.h"
#include "../../action/synthetic_record.h"

typedef enum {
    KEY_RUNTIME_PROCESS_NEXT = 0,
    KEY_RUNTIME_PROCESS_RETURN_TRUE,
    KEY_RUNTIME_PROCESS_RETURN_FALSE,
} key_runtime_process_stage_outcome_t;

typedef struct key_runtime_process_ctx_t key_runtime_process_ctx_t;
typedef key_runtime_process_stage_outcome_t (*key_runtime_process_stage_fn_t)(key_runtime_process_ctx_t *ctx);
typedef struct {
    const char *name;
    key_runtime_process_stage_fn_t handler;
} key_runtime_process_stage_entry_t;

struct key_runtime_process_ctx_t {
    uint16_t                 keycode;
    uint16_t                 runtime_keycode;
    keyrecord_t             *record;
    active_key_state_t      *release_slot;
    handled_key_resolution_t resolution;
    bool                     resolution_loaded;
};

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
    if (ctx->record->event.pressed) {
        return KEY_RUNTIME_PROCESS_NEXT;
    }

    ctx->release_slot = key_runtime_slot_for_position(ctx->record->event.key);
    if (key_runtime_slot_active(ctx->release_slot)) {
        ctx->runtime_keycode = ctx->release_slot->owner.keycode;
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
    key_runtime_trace_bool_result("process:return", ctx->keycode, ctx->record, keep_processing);
    return keep_processing;
}

bool noah_get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

bool noah_process_record_user(uint16_t keycode, keyrecord_t *record) {
    static const key_runtime_process_stage_entry_t stages[] = {
        {.name = "synthetic_passthrough", .handler = key_runtime_process_stage_synthetic_passthrough},
        {.name = "preflight", .handler = key_runtime_process_stage_preflight},
        {.name = "release_slot_keycode", .handler = key_runtime_process_stage_release_slot_keycode},
        {.name = "pd_mode", .handler = key_runtime_process_stage_pd_mode},
        {.name = "handled_key", .handler = key_runtime_process_stage_handled_key},
        {.name = "direct_action", .handler = key_runtime_process_stage_direct_action},
        {.name = "macro_dispatch", .handler = key_runtime_process_stage_macro_dispatch},
    };
    key_runtime_process_ctx_t ctx = {
        .keycode         = keycode,
        .runtime_keycode = keycode,
        .record          = record,
    };

    key_runtime_trace_record("process:entry", keycode, record);

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
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
