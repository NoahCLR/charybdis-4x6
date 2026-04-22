// ────────────────────────────────────────────────────────────────────────────
// Action Lifecycle
// ────────────────────────────────────────────────────────────────────────────

#include "action_lifecycle.h"

#include "action_kind_dispatch_internal.h"
#include "action_kind_internal.h"
#include "../compat/qmk_via_playback_contract.h"
#include "../macro/macro_dispatch.h"

static bool noah_action_handle_macro_preflight(noah_action_desc_t desc) {
    if (noah_qmk_contract_try_play_via_macro(desc.action)) {
        return true;
    }

    return macro_dispatch(desc.action);
}

void noah_action_tap(uint16_t action) {
    noah_action_desc_t desc = noah_action_describe(action);

    if (noah_action_desc_dispatches_macro_preflight(desc) && noah_action_handle_macro_preflight(desc)) {
        return;
    }

    noah_action_desc_tap_dispatch(desc);
}

void noah_action_press(keypos_t key_pos, uint16_t action) {
    noah_action_desc_t desc = noah_action_describe(action);

    if (noah_action_desc_dispatches_macro_preflight(desc) && noah_action_handle_macro_preflight(desc)) {
        return;
    }

    if (noah_action_desc_intercepts_pd_mode_press(desc) && pd_mode_handle_keycode_press_at(action, key_pos)) {
        return;
    }

    noah_action_desc_press_dispatch(desc, key_pos);
}

void noah_action_release(keypos_t key_pos, uint16_t action) {
    noah_action_desc_t desc = noah_action_describe(action);

    if (noah_action_desc_intercepts_pd_mode_release(desc) && pd_mode_handle_keycode_release_at(action, key_pos)) {
        return;
    }

    noah_action_desc_release_dispatch(desc, key_pos);
}
