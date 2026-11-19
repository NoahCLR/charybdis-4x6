// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse Switch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "auto_mouse_switch.h"
#include "../../compat/qmk_auto_mouse_contract.h"
#include "../../profile/runtime/active_profile_settings_runtime.h"
#include "../../state/ownership/layer_ownership.h"

static bool auto_mouse_override = true;

void auto_mouse_switch_apply(bool stored) {
    bool    enable = stored && auto_mouse_override;
    bool    was_on = noah_qmk_contract_auto_mouse_enabled();
    uint8_t layer  = noah_qmk_contract_auto_mouse_layer();

    noah_qmk_contract_auto_mouse_set_enable(enable);
    // QMK's set_auto_mouse_enable(false) clears its tracker and timers but
    // leaves the pointer layer on, and its task stops before the timeout that
    // would turn it off. A layer auto-mouse turned on goes off here; one a key
    // holds or locks, or a pointing mode owns, stays with its owner, and the
    // pointer layer policy keeps one an anchoring pointing mode needs. Layer
    // state is the master half's; the slave half follows it.
    if (was_on && !enable && is_keyboard_master() && layer_state_cmp(layer_state, layer) && !layer_ownership_owns(layer)) {
        layer_off(layer);
    }
}

void auto_mouse_switch_set_override(bool allowed) {
    auto_mouse_override = allowed;
    auto_mouse_switch_apply(noah_setting(NOAH_SETTING_AUTO_MOUSE_ENABLED, 1u) != 0u);
}

bool auto_mouse_switch_override(void) {
    return auto_mouse_override;
}
