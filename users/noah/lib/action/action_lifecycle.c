// ────────────────────────────────────────────────────────────────────────────
// Action Lifecycle
// ────────────────────────────────────────────────────────────────────────────

#include "action_lifecycle.h"

#include "noah_keymap_ids.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "action_dispatch.h"
#include "macro_dispatch.h"
#include "owned_keycode.h"
#include "synthetic_record.h"
#include "../compat/qmk_contract.h"
#include "../pointing/pd_modes.h"
#include "../pointing/pointer_layer_policy.h"
#include "../state/layer_ownership.h"
#include "../state/split_runtime_sync.h"

static bool noah_action_is_owned_momentary_layer(uint16_t action) {
    return IS_QK_MOMENTARY(action);
}

static void noah_action_log_unsupported_layer_action(uint16_t action) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported raw QMK layer action 0x%04X; use LOCK_LAYER(...) for persistent changes or PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for owned momentary holds\n", (unsigned int)action);
#else
    (void)action;
#endif
}

static bool noah_action_handle_one_shot_press(uint16_t action) {
    if (action_dispatch_is_layer_lock(action)) {
        layer_ownership_toggle_lock_state((uint8_t)(action - LAYER_LOCK_BASE));
        return true;
    }

    if (is_pd_mode_lock_action(action)) {
        const pd_mode_def_t *def = pd_mode_lock_action_lookup(action);
        if (def && pd_mode_toggle_lock_state(def->mode_flag)) {
            split_runtime_sync();
        }
        return true;
    }

    if (noah_qmk_contract_try_play_via_macro(action)) {
        return true;
    }

    return macro_dispatch(action);
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    if (action_dispatch_is_layer_lock(action) || is_pd_mode_lock_action(action) || action_dispatch_is_macro(action)) {
        return NOAH_ACTION_HOLD_KIND_PRESS_ONLY;
    }

    if (noah_action_is_owned_momentary_layer(action)) {
        return NOAH_ACTION_HOLD_KIND_PER_KEY;
    }

    return NOAH_ACTION_HOLD_KIND_SHARED;
}

void noah_action_tap(uint16_t action) {
    if (noah_action_handle_one_shot_press(action)) {
        return;
    }

    if (action_dispatch_is_raw_qmk_layer_action(action)) {
        noah_action_log_unsupported_layer_action(action);
        return;
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
        noah_dispatch_synthetic_tap(action);
        return;
    }

    if (action_dispatch_is_qmk_behavior_keycode(action)) {
        noah_dispatch_synthetic_qmk_tap(action);
        return;
    }

    pointer_layer_policy_note_action(action, true);
    tap_code16(action);
    pointer_layer_policy_note_action(action, false);
}

void noah_action_press(keypos_t key_pos, uint16_t action) {
    if (noah_action_handle_one_shot_press(action)) {
        return;
    }

    if (pd_mode_handle_keycode_press(action)) {
        return;
    }

    if (noah_action_is_owned_momentary_layer(action)) {
        layer_ownership_momentary_press(key_pos, QK_MOMENTARY_GET_LAYER(action));
        return;
    }

    if (action_dispatch_is_raw_qmk_layer_action(action)) {
        noah_action_log_unsupported_layer_action(action);
        return;
    }

    if (action_dispatch_is_qmk_behavior_keycode(action)) {
        noah_dispatch_synthetic_qmk_record(action, true, 0);
        return;
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
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
    if (noah_action_hold_kind(action) == NOAH_ACTION_HOLD_KIND_PRESS_ONLY) {
        return;
    }

    if (pd_mode_handle_keycode_release(action)) {
        return;
    }

    if (noah_action_is_owned_momentary_layer(action)) {
        layer_ownership_momentary_release(key_pos);
        return;
    }

    if (action_dispatch_is_raw_qmk_layer_action(action)) {
        noah_action_log_unsupported_layer_action(action);
        return;
    }

    if (action_dispatch_is_qmk_behavior_keycode(action)) {
        noah_dispatch_synthetic_qmk_record(action, false, 0);
        return;
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
        noah_dispatch_synthetic_record(action, false);
        return;
    }

    if (owned_keycode_unregister(action)) {
        return;
    }

    unregister_code16(action);
}
