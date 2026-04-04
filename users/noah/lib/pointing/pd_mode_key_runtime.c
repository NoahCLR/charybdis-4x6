// ────────────────────────────────────────────────────────────────────────────
// Pointing-Device Mode Key Runtime
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../state/runtime_shared_state.h"
#include "pd_mode_key_runtime.h"
#include "pointing_device_modes.h"

#if defined(POINTING_DEVICE_ENABLE)

typedef struct {
    uint16_t timer;
    uint16_t keycode;
    uint16_t tap_action;
    uint16_t tap_hold_term;
    uint8_t  mode;
    uint8_t  active_mode;
    uint8_t  alternate_mode;
    bool     was_locked;
} pd_mode_press_state_t;

#    define PD_MODE_PRESS_STATE_INIT {.keycode = KC_NO}

static pd_mode_press_state_t pd_mode_press = PD_MODE_PRESS_STATE_INIT;

static inline void pd_mode_press_reset(void) {
    pd_mode_press = (pd_mode_press_state_t)PD_MODE_PRESS_STATE_INIT;
}

bool pd_mode_key_runtime_process(uint16_t keycode, keyrecord_t *record, uint8_t mode, uint16_t tap_hold_term, bool multi_tap_repress, const pd_mode_key_runtime_hooks_t *hooks) {
    if (!mode) return false;

    bool state_changed = false;

    if (record->event.pressed) {
        if (pd_mode_unlock_other_locks(mode)) {
            state_changed = true;
        }

        if (pd_mode_press.keycode != KC_NO && pd_mode_press.keycode != keycode) {
            pd_mode_press_reset();
        }

        // Pointing-device mode keys are exclusive while held: the latest press
        // wins, and any earlier unlocked mode is cancelled immediately.
        if (pd_mode_deactivate_other_unlocked(mode)) {
            state_changed = true;
        }

        if (multi_tap_repress) {
            uint16_t action = hooks->advance_multi_tap(hooks->context, keycode);
            if (action != KC_NO) {
                hooks->dispatch_action(hooks->context, action);
                pd_mode_press.keycode = KC_NO;
                runtime_shared_state_sync();
                return true;
            }

            if (hooks->capture_pending_multi_tap_hold) {
                uint16_t tap_action  = KC_NO;
                uint16_t hold_action = KC_NO;

                if (hooks->capture_pending_multi_tap_hold(hooks->context, &tap_action, &hold_action)) {
                    uint8_t alternate_mode = pd_mode_for_keycode(hold_action);

                    if (alternate_mode) {
                        pd_mode_press.timer          = timer_read();
                        pd_mode_press.keycode        = keycode;
                        pd_mode_press.tap_action     = tap_action;
                        pd_mode_press.tap_hold_term  = tap_hold_term;
                        pd_mode_press.mode           = mode;
                        pd_mode_press.active_mode    = 0;
                        pd_mode_press.alternate_mode = alternate_mode;
                        pd_mode_press.was_locked     = false;
                        return true;
                    }
                }
            }

            pd_mode_press.keycode = KC_NO;
            return true;
        }

        pd_mode_press.timer          = timer_read();
        pd_mode_press.keycode        = keycode;
        pd_mode_press.tap_action     = KC_NO;
        pd_mode_press.tap_hold_term  = tap_hold_term;
        pd_mode_press.mode           = mode;
        pd_mode_press.active_mode    = mode;
        pd_mode_press.alternate_mode = 0;
        pd_mode_press.was_locked     = pd_mode_locked(mode);
        pd_mode_update(mode, true);
        state_changed = true;
    } else {
        bool     pressed_this_key = pd_mode_press.keycode == keycode;
        if (!pressed_this_key) {
            // Ignore stale releases from an older pd-mode key press. The
            // tracked active mode belongs only to pd_mode_press.keycode.
            return true;
        }

        uint16_t elapsed          = timer_elapsed(pd_mode_press.timer);
        bool     alternate_press  = pd_mode_press.alternate_mode != 0;
        bool     locked_press     = !alternate_press && pd_mode_press.mode == mode && pd_mode_press.was_locked && pd_mode_is_lockable(mode) && pressed_this_key;

        if (pd_mode_press.active_mode && pd_mode_active(pd_mode_press.active_mode) && !pd_mode_locked(pd_mode_press.active_mode)) {
            pd_mode_update(pd_mode_press.active_mode, false);
            state_changed = true;
        }

        if (alternate_press) {
            if (pressed_this_key && elapsed < pd_mode_press.tap_hold_term && pd_mode_press.tap_action != KC_NO) {
                hooks->dispatch_action(hooks->context, pd_mode_press.tap_action);
            }
        } else if (locked_press) {
            if (pd_mode_toggle_lock_state(mode)) {
                state_changed = true;
            }
        } else if (pressed_this_key && elapsed < tap_hold_term) {
            hooks->dispatch_tap_or_begin_multi_tap(hooks->context, keycode, record);
        }

        pd_mode_press_reset();
    }

    if (state_changed) {
        runtime_shared_state_sync();
    }
    return true;
}

void pd_mode_key_runtime_scan(void) {
    if (pd_mode_press.keycode == KC_NO || pd_mode_press.alternate_mode == 0 || pd_mode_press.active_mode != 0) {
        return;
    }

    if (timer_elapsed(pd_mode_press.timer) < pd_mode_press.tap_hold_term) {
        return;
    }

    bool state_changed = false;

    state_changed |= pd_mode_unlock_other_locks(pd_mode_press.alternate_mode);
    state_changed |= pd_mode_deactivate_other_unlocked(pd_mode_press.alternate_mode);
    pd_mode_update(pd_mode_press.alternate_mode, true);
    pd_mode_press.active_mode = pd_mode_press.alternate_mode;
    state_changed             = true;

    if (state_changed) {
        runtime_shared_state_sync();
    }
}

#else

bool pd_mode_key_runtime_process(uint16_t keycode, keyrecord_t *record, uint8_t mode, uint16_t tap_hold_term, bool multi_tap_repress, const pd_mode_key_runtime_hooks_t *hooks) {
    (void)keycode;
    (void)record;
    (void)mode;
    (void)tap_hold_term;
    (void)multi_tap_repress;
    (void)hooks;
    return false;
}

void pd_mode_key_runtime_scan(void) {}

#endif // POINTING_DEVICE_ENABLE
