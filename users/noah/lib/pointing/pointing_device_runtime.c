// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Runtime
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "noah_keymap.h"
#include "../state/pd_shared_state.h"
#include "pointing_device_modes.h"
#include "pointer_layer_policy.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#endif

#if defined(POINTING_DEVICE_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
void noah_pointing_device_init_user(void) {
    set_auto_mouse_layer(LAYER_POINTER);
    set_auto_mouse_enable(true);
    pd_shared_state_init();
}

bool noah_is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)record;
    return pointer_layer_policy_is_mouse_record(keycode);
}
#else
void noah_pointing_device_init_user(void) {
}

bool noah_is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}
#endif // defined(POINTING_DEVICE_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

report_mouse_t noah_pointing_device_task_user(report_mouse_t mouse_report) {
#ifdef POINTING_DEVICE_ENABLE
#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    pd_shared_state_sync_tick();
#    endif

    uint8_t active_mode = pd_mode_first_active_index();
    if (active_mode < PD_MODE_COUNT && pd_modes[active_mode].handler) {
        return pd_modes[active_mode].handler(mouse_report);
    }

    return mouse_report;
#else
    return mouse_report;
#endif
}

layer_state_t noah_layer_state_set_user(layer_state_t state) {
#ifdef POINTING_DEVICE_ENABLE
    charybdis_set_pointer_sniping_enabled(layer_state_cmp(state, LAYER_NAV));
    return pointer_layer_policy_apply(state);
#else
    return state;
#endif
}
