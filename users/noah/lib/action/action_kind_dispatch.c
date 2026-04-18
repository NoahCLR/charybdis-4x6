// ────────────────────────────────────────────────────────────────────────────
// Action Kind Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include "action_kind_dispatch_internal.h"

#include "noah_keymap_ids.h"
#include "action_kind_internal.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "owned_keycode.h"
#include "synthetic_record.h"
#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/layer_ownership.h"

typedef void (*noah_action_tap_impl_t)(noah_action_desc_t desc);
typedef void (*noah_action_press_impl_t)(noah_action_desc_t desc, keypos_t key_pos);
typedef void (*noah_action_release_impl_t)(noah_action_desc_t desc, keypos_t key_pos);

typedef struct {
    noah_action_tap_impl_t     tap;
    noah_action_press_impl_t   press;
    noah_action_release_impl_t release;
} noah_action_kind_dispatch_ops_t;

static volatile bool               noah_action_dispatch_fault_seen;
static volatile noah_action_kind_t noah_action_dispatch_fault_kind = NOAH_ACTION_KIND_COUNT;

static void noah_action_tap_noop(noah_action_desc_t desc) {
    (void)desc;
}

static void noah_action_press_noop(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    (void)key_pos;
}

static void noah_action_release_noop(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    (void)key_pos;
}

static const noah_action_kind_dispatch_ops_t noah_action_kind_dispatch_noop_ops = {
    .tap     = noah_action_tap_noop,
    .press   = noah_action_press_noop,
    .release = noah_action_release_noop,
};

static void noah_action_log_unsupported_layer_action(noah_action_desc_t desc) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported raw QMK layer action 0x%04X; use LOCK_LAYER(...) for persistent changes or PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for owned momentary holds\n", (unsigned int)desc.action);
#else
    (void)desc;
#endif
}

static void noah_action_tap_literal(noah_action_desc_t desc) {
    pointer_layer_policy_note_action(desc.action, true);
    tap_code16(desc.action);
    pointer_layer_policy_note_action(desc.action, false);
}

static void noah_action_press_literal(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;

    if (owned_keycode_register(desc.action)) {
        return;
    }

    register_code16(desc.action);
}

static void noah_action_release_literal(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;

    if (owned_keycode_unregister(desc.action)) {
        return;
    }

    unregister_code16(desc.action);
}

static void noah_action_tap_unsupported_layer(noah_action_desc_t desc) {
    noah_action_log_unsupported_layer_action(desc);
}

static void noah_action_press_unsupported_layer(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_log_unsupported_layer_action(desc);
}

static void noah_action_release_unsupported_layer(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_log_unsupported_layer_action(desc);
}

static void noah_action_toggle_layer_lock(noah_action_desc_t desc) {
    (void)layer_ownership_toggle_lock_state(desc.layer);
}

static void noah_action_toggle_pd_mode_lock(noah_action_desc_t desc) {
    const pd_mode_def_t *def = pd_mode_lock_action_lookup(desc.action);
    if (def) {
        (void)pd_mode_toggle_lock_state(def->mode_flag);
    }
}

static void noah_action_tap_layer_lock(noah_action_desc_t desc) {
    noah_action_toggle_layer_lock(desc);
}

static void noah_action_press_layer_lock(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_toggle_layer_lock(desc);
}

static void noah_action_tap_pd_mode_lock(noah_action_desc_t desc) {
    noah_action_toggle_pd_mode_lock(desc);
}

static void noah_action_press_pd_mode_lock(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_toggle_pd_mode_lock(desc);
}

static void noah_action_press_owned_momentary_layer(noah_action_desc_t desc, keypos_t key_pos) {
    layer_ownership_momentary_press(key_pos, desc.layer);
}

static void noah_action_release_owned_momentary_layer(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    (void)layer_ownership_momentary_release(key_pos);
}

static void noah_action_tap_keymap_custom(noah_action_desc_t desc) {
    noah_dispatch_synthetic_tap(desc.action);
}

static void noah_action_press_keymap_custom(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_dispatch_synthetic_record(desc.action, true);
}

static void noah_action_release_keymap_custom(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_dispatch_synthetic_record(desc.action, false);
}

static void noah_action_tap_qmk_behavior(noah_action_desc_t desc) {
    noah_dispatch_synthetic_qmk_tap(desc.action);
}

static void noah_action_press_qmk_behavior(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_dispatch_synthetic_qmk_record(desc.action, true, 0);
}

static void noah_action_release_qmk_behavior(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_dispatch_synthetic_qmk_record(desc.action, false, 0);
}

static const noah_action_kind_dispatch_ops_t noah_action_kind_dispatch_ops[NOAH_ACTION_KIND_COUNT] = {
#define NOAH_ACTION_KIND_DEF(name, priority, matcher, cap_mask, feedback_kept, uses_desc_layer_preview, dispatch_mask, policy_mask, tap_impl, press_impl, release_impl) \
    [NOAH_ACTION_KIND_##name] = {                                                                                                                                       \
        .tap     = tap_impl,                                                                                                                                            \
        .press   = press_impl,                                                                                                                                          \
        .release = release_impl,                                                                                                                                        \
    },
    NOAH_ACTION_KIND_REGISTRY(NOAH_ACTION_KIND_DEF)
#undef NOAH_ACTION_KIND_DEF
};

static bool noah_action_kind_dispatch_ops_complete(const noah_action_kind_dispatch_ops_t *ops) {
    return ops && ops->tap && ops->press && ops->release;
}

bool noah_action_kind_dispatch_has_complete_ops(noah_action_kind_t kind) {
    return kind < NOAH_ACTION_KIND_COUNT && noah_action_kind_dispatch_ops_complete(&noah_action_kind_dispatch_ops[kind]);
}

bool noah_action_desc_has_dispatch_ops(noah_action_desc_t desc) {
    return noah_action_kind_metadata_defined(desc.kind) && noah_action_kind_dispatch_has_complete_ops(desc.kind);
}

bool noah_action_kind_dispatch_faulted(void) {
    return noah_action_dispatch_fault_seen;
}

noah_action_kind_t noah_action_kind_last_dispatch_fault_kind(void) {
    return (noah_action_kind_t)noah_action_dispatch_fault_kind;
}

void noah_action_kind_dispatch_clear_fault_for_test(void) {
    noah_action_dispatch_fault_seen = false;
    noah_action_dispatch_fault_kind = NOAH_ACTION_KIND_COUNT;
}

static void noah_action_log_missing_dispatch_ops(noah_action_kind_t kind) {
    noah_action_dispatch_fault_seen = true;
    noah_action_dispatch_fault_kind = kind;

#ifdef CONSOLE_ENABLE
    uprintf("Missing action dispatch ops for kind %u\n", (unsigned int)kind);
#else
    (void)kind;
#endif
}

static void noah_action_fail_host_missing_dispatch_ops(noah_action_kind_t kind) {
#ifdef NOAH_HOST_TEST_ENV
    fprintf(stderr, "host test failed: missing action dispatch ops for kind %u\n", (unsigned int)kind);
    exit(1);
#else
    (void)kind;
#endif
}

static const noah_action_kind_dispatch_ops_t *noah_action_desc_dispatch_ops(noah_action_desc_t desc) {
    if (!noah_action_desc_has_dispatch_ops(desc)) {
        noah_action_log_missing_dispatch_ops(desc.kind);
        noah_action_fail_host_missing_dispatch_ops(desc.kind);
        return &noah_action_kind_dispatch_noop_ops;
    }

    return &noah_action_kind_dispatch_ops[desc.kind];
}

void noah_action_desc_tap_dispatch(noah_action_desc_t desc) {
    const noah_action_kind_dispatch_ops_t *ops = noah_action_desc_dispatch_ops(desc);
    ops->tap(desc);
}

void noah_action_desc_press_dispatch(noah_action_desc_t desc, keypos_t key_pos) {
    const noah_action_kind_dispatch_ops_t *ops = noah_action_desc_dispatch_ops(desc);
    ops->press(desc, key_pos);
}

void noah_action_desc_release_dispatch(noah_action_desc_t desc, keypos_t key_pos) {
    const noah_action_kind_dispatch_ops_t *ops = noah_action_desc_dispatch_ops(desc);
    ops->release(desc, key_pos);
}
