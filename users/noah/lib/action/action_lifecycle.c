// ────────────────────────────────────────────────────────────────────────────
// Action Lifecycle
// ────────────────────────────────────────────────────────────────────────────

#include "action_lifecycle.h"

#include "noah_keymap_ids.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "action_dispatch.h"
#include "owned_keycode.h"
#include "synthetic_record.h"
#include "../compat/qmk_via_playback_contract.h"
#include "../macro/macro_dispatch.h"
#include "../pointing/defs/pd_modes.h"
#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/layer_ownership.h"

typedef void (*noah_action_tap_impl_t)(noah_action_desc_t desc);
typedef void (*noah_action_press_impl_t)(noah_action_desc_t desc, keypos_t key_pos);
typedef void (*noah_action_release_impl_t)(noah_action_desc_t desc, keypos_t key_pos);

typedef struct {
    noah_action_tap_impl_t     tap;
    noah_action_press_impl_t   press;
    noah_action_release_impl_t release;
} noah_action_ops_t;

static void noah_action_log_unsupported_layer_action(noah_action_desc_t desc) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported raw QMK layer action 0x%04X; use LOCK_LAYER(...) for persistent changes or PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for owned momentary holds\n", (unsigned int)desc.action);
#else
    (void)desc;
#endif
}

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

static void noah_action_tap_literal(noah_action_desc_t desc) {
    pointer_layer_policy_note_action(desc.action, true);
    tap_code16(desc.action);
    pointer_layer_policy_note_action(desc.action, false);
}

static void noah_action_press_literal(noah_action_desc_t desc, keypos_t key_pos) {
    (void)key_pos;

    // Route literal keycodes, including QK_MODS such as S(KC_1), through the
    // shared owned-keycode contract so held actions and macro dispatch cannot
    // drift apart.
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
    layer_ownership_toggle_lock_state(desc.layer);
}

static void noah_action_toggle_pd_mode_lock(noah_action_desc_t desc) {
    const pd_mode_def_t *def = pd_mode_lock_action_lookup(desc.action);
    if (def) {
        pd_mode_toggle_lock_state(def->mode_flag);
    }
}

static bool noah_action_handle_macro_preflight(noah_action_desc_t desc) {
    if (noah_qmk_contract_try_play_via_macro(desc.action)) {
        return true;
    }

    return macro_dispatch(desc.action);
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
    layer_ownership_momentary_release(key_pos);
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

static const noah_action_ops_t noah_action_ops_by_kind[NOAH_ACTION_KIND_COUNT] = {
    [NOAH_ACTION_KIND_LITERAL] =
        {
            .tap     = noah_action_tap_literal,
            .press   = noah_action_press_literal,
            .release = noah_action_release_literal,
        },
    [NOAH_ACTION_KIND_LAYER_LOCK] =
        {
            .tap     = noah_action_tap_layer_lock,
            .press   = noah_action_press_layer_lock,
            .release = noah_action_release_noop,
        },
    [NOAH_ACTION_KIND_LAYER_HOLD] =
        {
            .tap     = noah_action_tap_unsupported_layer,
            .press   = noah_action_press_owned_momentary_layer,
            .release = noah_action_release_owned_momentary_layer,
        },
    [NOAH_ACTION_KIND_LAYER_TAP] =
        {
            .tap     = noah_action_tap_unsupported_layer,
            .press   = noah_action_press_unsupported_layer,
            .release = noah_action_release_unsupported_layer,
        },
    [NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION] =
        {
            .tap     = noah_action_tap_unsupported_layer,
            .press   = noah_action_press_unsupported_layer,
            .release = noah_action_release_unsupported_layer,
        },
    [NOAH_ACTION_KIND_MACRO] =
        {
            .tap     = noah_action_tap_noop,
            .press   = noah_action_press_noop,
            .release = noah_action_release_noop,
        },
    [NOAH_ACTION_KIND_QMK_BEHAVIOR] =
        {
            .tap     = noah_action_tap_qmk_behavior,
            .press   = noah_action_press_qmk_behavior,
            .release = noah_action_release_qmk_behavior,
        },
    [NOAH_ACTION_KIND_KEYMAP_CUSTOM] =
        {
            .tap     = noah_action_tap_keymap_custom,
            .press   = noah_action_press_keymap_custom,
            .release = noah_action_release_keymap_custom,
        },
    [NOAH_ACTION_KIND_PD_MODE_HOLD] =
        {
            .tap     = noah_action_tap_literal,
            .press   = noah_action_press_literal,
            .release = noah_action_release_literal,
        },
    [NOAH_ACTION_KIND_PD_MODE_LOCK] =
        {
            .tap     = noah_action_tap_pd_mode_lock,
            .press   = noah_action_press_pd_mode_lock,
            .release = noah_action_release_noop,
        },
};

static const noah_action_ops_t *noah_action_ops(noah_action_desc_t desc) {
    if (desc.kind >= NOAH_ACTION_KIND_COUNT) {
        return &noah_action_ops_by_kind[NOAH_ACTION_KIND_LITERAL];
    }

    return &noah_action_ops_by_kind[desc.kind];
}

void noah_action_tap(uint16_t action) {
    noah_action_desc_t       desc = noah_action_describe(action);
    const noah_action_ops_t *ops  = noah_action_ops(desc);

    if (noah_action_desc_is_layer_lock(desc) || noah_action_desc_is_pd_mode_lock(desc)) {
        ops->tap(desc);
        return;
    }

    if (noah_action_handle_macro_preflight(desc)) {
        return;
    }

    ops->tap(desc);
}

void noah_action_press(keypos_t key_pos, uint16_t action) {
    noah_action_desc_t       desc = noah_action_describe(action);
    const noah_action_ops_t *ops  = noah_action_ops(desc);

    if (noah_action_desc_is_layer_lock(desc) || noah_action_desc_is_pd_mode_lock(desc)) {
        ops->press(desc, key_pos);
        return;
    }

    if (noah_action_handle_macro_preflight(desc)) {
        return;
    }

    if (pd_mode_handle_keycode_press(action)) {
        return;
    }

    ops->press(desc, key_pos);
}

void noah_action_release(keypos_t key_pos, uint16_t action) {
    noah_action_desc_t       desc = noah_action_describe(action);
    const noah_action_ops_t *ops  = noah_action_ops(desc);

    if (pd_mode_handle_keycode_release(action)) {
        return;
    }

    ops->release(desc, key_pos);
}
