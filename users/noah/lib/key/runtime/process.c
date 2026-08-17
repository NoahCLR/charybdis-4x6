// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Process Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../behavior/handled_key.h"
#include "deferred_release.h"
#include "process_internal.h"
#include "trace.h"
#include "../../macro/macro_dispatch.h"
#include "../../pointing/defs/pd_modes.h"
#include "../../pointing/runtime/pd_mode_keyboard_event_internal.h"
#include "reducer/ownership_state.h"
#include "reducer/runtime.h"
#include "reducer/state_query.h"
#include "../../action/synthetic_record.h"
#include "../../action/owned_keycode.h"
#include "../../compat/qmk_combo_origin.h"
#include "../../state/ownership/keyboard_mod_ownership.h"
#include "../../state/modifiers/keyboard_mod_policy.h"
#include "../../state/diagnostics/runtime_diag.h"
#include "slot/origin_registry.h"

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

static void key_runtime_process_end_keyboard_event_mod_mask(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && state->keyboard_event_mask_active)) {
        return;
    }

    keyboard_mod_policy_end_real_mod_mask(state->keyboard_event_masked_real_mods);
    state->keyboard_event_mask_active      = false;
    state->keyboard_event_masked_real_mods = 0u;
}

static void key_runtime_process_begin_keyboard_event_mod_mask(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    uint8_t                   masked_real_mods;

    if (!(state && !state->keyboard_event_mask_active)) {
        return;
    }

    masked_real_mods = pd_mode_active_keyboard_event_masked_real_mods();
    if (!keyboard_mod_policy_begin_real_mod_mask(masked_real_mods)) {
        return;
    }

    state->keyboard_event_masked_real_mods = masked_real_mods;
    state->keyboard_event_mask_active      = true;
}

static const handled_key_resolution_t *key_runtime_process_resolution(key_runtime_process_ctx_t *ctx) {
    if (!ctx->resolution_loaded) {
        handled_key_lookup_into(ctx->runtime_keycode, &ctx->resolution);
        ctx->resolution_loaded = true;
    }

    return &ctx->resolution;
}

static bool key_runtime_process_observed_press_is_handled(key_runtime_process_ctx_t *ctx) {
    const press_token_t *token;

    if (!(ctx && ctx->record && ctx->record->event.pressed)) {
        return false;
    }

    token = key_runtime_core_press_token_at(ctx->record->event.key);
    if (token && token->active) {
        return token->handled_key;
    }

    return handled_key_resolution_is_handled(*key_runtime_process_resolution(ctx));
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
    bool keep_processing = key_runtime_preflight_record(ctx->keycode, ctx->record, key_runtime_process_observed_press_is_handled(ctx));

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
    const handled_key_resolution_t *resolution;
    const press_token_t            *token;
    bool                            handled;

    if (ctx->record->event.pressed) {
        token = key_runtime_core_press_token_at(ctx->record->event.key);
        if (token && token->active) {
            if (!token->handled_key) {
                return KEY_RUNTIME_PROCESS_NEXT;
            }
        } else {
            resolution = key_runtime_process_resolution(ctx);
            if (!handled_key_resolution_is_handled(*resolution)) {
                return KEY_RUNTIME_PROCESS_NEXT;
            }
        }
        handled = key_runtime_process_handled_key_press(ctx->runtime_keycode, ctx->record);
    } else {
        handled = key_runtime_process_handled_key_release(ctx->runtime_keycode, ctx->record, NULL);
        if (!handled) {
            resolution = key_runtime_process_resolution(ctx);
            if (!handled_key_resolution_is_handled(*resolution)) {
                return KEY_RUNTIME_PROCESS_NEXT;
            }
            handled = key_runtime_process_handled_key_release(ctx->runtime_keycode, ctx->record, resolution);
        }
        if (handled) {
            key_runtime_deferred_release_drain_dispatches();
        }
    }
    key_runtime_trace_bool_result("process:handled_key", ctx->runtime_keycode, ctx->record, handled);
    return handled ? KEY_RUNTIME_PROCESS_RETURN_FALSE : KEY_RUNTIME_PROCESS_NEXT;
}

static key_runtime_process_stage_outcome_t key_runtime_process_stage_non_handled_release_cleanup(key_runtime_process_ctx_t *ctx) {
    if (ctx->record->event.pressed) {
        return KEY_RUNTIME_PROCESS_NEXT;
    }

    if (handled_key_resolution_is_handled(*key_runtime_process_resolution(ctx))) {
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

// Report ownership means "QMK's default handler holds this usage in the report".
// That is only knowable once the event result is final, so it is settled from
// the finalize hook rather than pre-process. Crediting a press that userspace
// later consumes would make a managed owner of the same usage skip its own key
// registration, and would keep the last managed modifier release from clearing
// the report bit. Pre-process still tracks which physical keys are down, which
// is a different question and stays where it is.
static void key_runtime_process_settle_report_ownership(uint16_t keycode, keyrecord_t *record, bool keep_processing) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && record) || noah_synthetic_record_active()) {
        return;
    }

    if (record->event.type != KEY_EVENT || !key_origin_keypos_valid(record->event.key)) {
        return;
    }

    if (record->event.pressed) {
        if (!keep_processing || key_origin_bitmap_has_keypos(state->default_report_owner_bitmap, record->event.key)) {
            return;
        }

        key_origin_bitmap_add_keypos(state->default_report_owner_bitmap, record->event.key);
    } else {
        // A credited press is always retired on its release, including when
        // preflight consumes that release to keep a managed owner's usage
        // down. QMK skips its default teardown there, so the managed owner
        // becomes the sole owner and must see a zero report count.
        if (!key_origin_bitmap_has_keypos(state->default_report_owner_bitmap, record->event.key)) {
            return;
        }

        key_origin_bitmap_remove_keypos(state->default_report_owner_bitmap, record->event.key);
    }

    owned_keycode_track_physical_event(keycode, record);
    keyboard_mod_ownership_track_report_keycode_event(keycode, record);
}

bool noah_pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (noah_synthetic_record_active()) {
        return true;
    }

    if (record->event.type == KEY_EVENT && key_origin_keypos_valid(record->event.key)) {
        key_origin_registry_set_single(record->event.key);
        noah_qmk_combo_origin_observe_physical_key_event(keycode, record);
        keyboard_mod_ownership_track_physical_keycode_event(keycode, record);
    }

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
    // Settle ownership before the mask restore below, which selects managed-only
    // modifiers from the same ledgers.
    key_runtime_process_settle_report_ownership(keycode, record, keep_processing);
    key_runtime_process_end_keyboard_event_mod_mask();
    key_runtime_trace_bool_result("process:return", keycode, record, keep_processing);
    noah_runtime_diag_scope_leave();
}

void noah_post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    noah_process_record_user_finalize(keycode, record, true);
}
