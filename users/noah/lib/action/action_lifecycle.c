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
#include "../compat/qmk_contract.h"
#include "../macro/macro_dispatch.h"
#include "../pointing/defs/pd_modes.h"
#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/layer_ownership.h"
#include "../state/runtime/split_runtime_sync.h"

static void noah_action_log_unsupported_layer_action(noah_action_desc_t desc) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported raw QMK layer action 0x%04X; use LOCK_LAYER(...) for persistent changes or PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for owned momentary holds\n", (unsigned int)desc.action);
#else
    (void)desc;
#endif
}

static bool noah_action_handle_one_shot_press(noah_action_desc_t desc) {
    if (desc.is_layer_lock) {
        layer_ownership_toggle_lock_state(desc.layer);
        return true;
    }

    if (desc.is_pd_mode_lock) {
        const pd_mode_def_t *def = pd_mode_lock_action_lookup(desc.action);
        if (def && pd_mode_toggle_lock_state(def->mode_flag)) {
            split_runtime_sync();
        }
        return true;
    }

    if (noah_qmk_contract_try_play_via_macro(desc.action)) {
        return true;
    }

    return macro_dispatch(desc.action);
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    noah_action_desc_t desc = noah_action_describe(action);

    if (noah_action_desc_is_press_only(desc)) {
        return NOAH_ACTION_HOLD_KIND_PRESS_ONLY;
    }

    if (noah_action_desc_requires_per_key_hold(desc)) {
        return NOAH_ACTION_HOLD_KIND_PER_KEY;
    }

    return NOAH_ACTION_HOLD_KIND_SHARED;
}

void noah_action_tap(uint16_t action) {
    noah_action_desc_t desc = noah_action_describe(action);

    if (noah_action_handle_one_shot_press(desc)) {
        return;
    }

    if (desc.is_raw_qmk_layer_action) {
        noah_action_log_unsupported_layer_action(desc);
        return;
    }

    if (desc.is_keymap_custom) {
        noah_dispatch_synthetic_tap(action);
        return;
    }

    if (desc.is_qmk_behavior_keycode) {
        noah_dispatch_synthetic_qmk_tap(action);
        return;
    }

    pointer_layer_policy_note_action(action, true);
    tap_code16(action);
    pointer_layer_policy_note_action(action, false);
}

void noah_action_press(keypos_t key_pos, uint16_t action) {
    noah_action_desc_t desc = noah_action_describe(action);

    if (noah_action_handle_one_shot_press(desc)) {
        return;
    }

    if (pd_mode_handle_keycode_press(action)) {
        return;
    }

    if (desc.is_owned_momentary_layer) {
        layer_ownership_momentary_press(key_pos, desc.layer);
        return;
    }

    if (desc.is_raw_qmk_layer_action) {
        noah_action_log_unsupported_layer_action(desc);
        return;
    }

    if (desc.is_qmk_behavior_keycode) {
        noah_dispatch_synthetic_qmk_record(action, true, 0);
        return;
    }

    if (desc.is_keymap_custom) {
        noah_dispatch_synthetic_record(action, true);
        return;
    }

    // Route literal keycodes, including QK_MODS such as S(KC_1), through the
    // shared owned-keycode contract so held actions and macro dispatch cannot
    // drift apart.
    if (owned_keycode_register(action)) {
        return;
    }

    register_code16(action);
}

void noah_action_release(keypos_t key_pos, uint16_t action) {
    noah_action_desc_t desc = noah_action_describe(action);

    if (noah_action_desc_is_press_only(desc)) {
        return;
    }

    if (pd_mode_handle_keycode_release(action)) {
        return;
    }

    if (desc.is_owned_momentary_layer) {
        layer_ownership_momentary_release(key_pos);
        return;
    }

    if (desc.is_raw_qmk_layer_action) {
        noah_action_log_unsupported_layer_action(desc);
        return;
    }

    if (desc.is_qmk_behavior_keycode) {
        noah_dispatch_synthetic_qmk_record(action, false, 0);
        return;
    }

    if (desc.is_keymap_custom) {
        noah_dispatch_synthetic_record(action, false);
        return;
    }

    if (owned_keycode_unregister(action)) {
        return;
    }

    unregister_code16(action);
}
