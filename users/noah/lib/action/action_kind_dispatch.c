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
#include "../compat/qmk_oneshot_contract.h"
#include "../profile/runtime/active_profile_settings_runtime.h"
#include "../state/ownership/layer_ownership.h"

typedef void (*noah_action_tap_impl_t)(noah_action_desc_t desc, keypos_t key_pos);
typedef void (*noah_action_press_impl_t)(noah_action_desc_t desc, keypos_t key_pos);
typedef void (*noah_action_release_impl_t)(noah_action_desc_t desc, keypos_t key_pos);

typedef struct {
    noah_action_tap_impl_t     tap;
    noah_action_press_impl_t   press;
    noah_action_release_impl_t release;
} noah_action_kind_dispatch_ops_t;

static void noah_action_tap_noop(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    (void)key_pos;
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
    uprintf("Unsupported raw QMK layer action 0x%04X; use LOCK_LAYER(...)/TG(...)/TO(...) for persistent changes, OSL(...) for one-shots, or PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for owned momentary holds\n", (unsigned int)desc.action);
#else
    (void)desc;
#endif
}

static void noah_action_tap_literal(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    owned_keycode_tap_literal(desc.action);
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

static void noah_action_tap_unsupported_layer(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
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

static void noah_action_goto_layer(noah_action_desc_t desc) {
    (void)layer_ownership_goto(desc.layer);
}

static void noah_action_toggle_pointing_mode_lock(noah_action_desc_t desc, keypos_t key_pos) {
    const pointing_mode_def_t *def = pointing_mode_lock_action_lookup(desc.action);
    if (def) {
        (void)pointing_mode_toggle_lock_state_at(def->mode_flag, key_pos);
    }
}

static void noah_action_tap_layer_lock(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_toggle_layer_lock(desc);
}

static void noah_action_press_layer_lock(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_toggle_layer_lock(desc);
}

static void noah_action_tap_layer_goto(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_goto_layer(desc);
}

static void noah_action_press_layer_goto(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    noah_action_goto_layer(desc);
}

// With one-shots off, OSL() is only a hold, as QMK's is. A second tap counts
// as QMK's double tap when it comes within TAPPING_TERM of the first.
static void noah_action_tap_layer_oneshot(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
    if (noah_qmk_contract_oneshot_enabled()) {
        (void)layer_ownership_oneshot_tap(desc.layer, timer_read(), (uint16_t)noah_setting(NOAH_SETTING_TAPPING_TERM, TAPPING_TERM));
    }
}

static void noah_action_tap_pointing_mode_lock(noah_action_desc_t desc, keypos_t key_pos) {
    noah_action_toggle_pointing_mode_lock(desc, key_pos);
}

static void noah_action_press_pointing_mode_lock(noah_action_desc_t desc, keypos_t key_pos) {
    noah_action_toggle_pointing_mode_lock(desc, key_pos);
}

static void noah_action_press_owned_momentary_layer(noah_action_desc_t desc, keypos_t key_pos) {
    layer_ownership_momentary_press(key_pos, desc.layer);
}

static void noah_action_release_owned_momentary_layer(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    (void)layer_ownership_momentary_release(key_pos);
}

// A custom key sent as an action sends only its shortcut (D-F26), never its
// own behaviour, so a step can never recurse into another key's row. One
// without a shortcut sends nothing. A tap sends its modifiers and keys through one lease for
// the tap's length, and its layers do what a tap of their layer key does: Hold
// and Tap-toggle turn them on around the keys, Lock and Move to lock them
// first, as a tap of TG() or TO() would, and One-shot arms them after the keys,
// so the keys cannot use them up. A held shortcut is owned by the key holding
// it: held_action.c keeps its lease and layers, recording exactly what it
// took, so a shortcut changed or removed while held still releases what it
// took. These generic press and release paths have no owner to keep them, so
// they send nothing; every held custom key goes through held_action.c.
static void noah_action_tap_custom_key(noah_action_desc_t desc, keypos_t key_pos) {
    noah_custom_key_shortcut_t shortcut = noah_custom_key_shortcut_for_keycode(desc.action);
    bool                       around   = shortcut.layers && (shortcut.layer_kind == NOAH_SHORTCUT_LAYERS_HOLD || shortcut.layer_kind == NOAH_SHORTCUT_LAYERS_TAP_TOGGLE);

    (void)key_pos;
    if (shortcut.layers && shortcut.layer_kind == NOAH_SHORTCUT_LAYERS_LOCK) {
        (void)layer_ownership_toggle_lock_set(shortcut.layers);
    } else if (shortcut.layers && shortcut.layer_kind == NOAH_SHORTCUT_LAYERS_MOVE) {
        (void)layer_ownership_goto_set(shortcut.layers);
    } else if (around) {
        layer_ownership_shortcut_tap_begin(shortcut.layers);
    }
    if (noah_custom_key_shortcut_has_report_parts(shortcut)) {
        (void)owned_keycode_tap_chord(shortcut.mods, shortcut.keys);
    }
    if (around) {
        layer_ownership_shortcut_tap_end(shortcut.layers);
    } else if (shortcut.layers && shortcut.layer_kind == NOAH_SHORTCUT_LAYERS_ONESHOT && noah_qmk_contract_oneshot_enabled()) {
        (void)layer_ownership_oneshot_tap_set(shortcut.layers, timer_read(), (uint16_t)noah_setting(NOAH_SETTING_TAPPING_TERM, TAPPING_TERM));
    }
}

// What a dual-role custom key's own tap does to its layers
// (CUSTOM_KEY_LAYER_TAP(n), which only the runtime sends): its keys and layers
// were held from the press, so only this is left. One-shot arms the layers,
// as OSL()'s tap does; Tap-toggle's last tap locks them as one, as TT()'s
// locks its layer.
static void noah_action_tap_custom_key_layer_tap(noah_action_desc_t desc, keypos_t key_pos) {
    noah_custom_key_shortcut_t shortcut = noah_custom_key_shortcut((uint8_t)(desc.action - NOAH_KEYCODE_CUSTOM_KEY_LAYER_TAP_BASE));

    (void)key_pos;
    if (shortcut.layers && shortcut.layer_kind == NOAH_SHORTCUT_LAYERS_TAP_TOGGLE) {
        (void)layer_ownership_toggle_lock_set(shortcut.layers);
    } else if (shortcut.layers && shortcut.layer_kind == NOAH_SHORTCUT_LAYERS_ONESHOT && noah_qmk_contract_oneshot_enabled()) {
        (void)layer_ownership_oneshot_tap_set(shortcut.layers, timer_read(), (uint16_t)noah_setting(NOAH_SETTING_TAPPING_TERM, TAPPING_TERM));
    }
}

static void noah_action_press_custom_key(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    (void)key_pos;
}

static void noah_action_release_custom_key(noah_action_desc_t desc, keypos_t key_pos) {
    (void)desc;
    (void)key_pos;
}

static void noah_action_tap_qmk_behavior(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;
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

static void noah_action_log_missing_dispatch_ops(noah_action_kind_t kind) {
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

void noah_action_desc_tap_dispatch(noah_action_desc_t desc, keypos_t key_pos) {
    const noah_action_kind_dispatch_ops_t *ops = noah_action_desc_dispatch_ops(desc);
    ops->tap(desc, key_pos);
}

void noah_action_desc_press_dispatch(noah_action_desc_t desc, keypos_t key_pos) {
    const noah_action_kind_dispatch_ops_t *ops = noah_action_desc_dispatch_ops(desc);
    ops->press(desc, key_pos);
}

void noah_action_desc_release_dispatch(noah_action_desc_t desc, keypos_t key_pos) {
    const noah_action_kind_dispatch_ops_t *ops = noah_action_desc_dispatch_ops(desc);
    ops->release(desc, key_pos);
}
