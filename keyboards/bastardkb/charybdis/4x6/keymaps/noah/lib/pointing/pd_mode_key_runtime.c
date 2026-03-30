// ────────────────────────────────────────────────────────────────────────────
// Pointing-Device Mode Key Runtime
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "pd_mode_key_runtime.h"
#include "pointing_device_modes.h"
#include "split_sync.h"

#if defined(POINTING_DEVICE_ENABLE)

typedef struct {
    uint16_t timer;
    uint16_t keycode;
    uint8_t  mode;
    bool     was_locked;
} pd_mode_press_state_t;

#    define PD_MODE_PRESS_STATE_INIT {.keycode = KC_NO}

static pd_mode_press_state_t pd_mode_press = PD_MODE_PRESS_STATE_INIT;

static inline void pd_mode_press_reset(void) {
    pd_mode_press = (pd_mode_press_state_t)PD_MODE_PRESS_STATE_INIT;
}

bool pd_mode_key_runtime_process(uint16_t keycode, keyrecord_t *record, uint8_t mode, bool multi_tap_repress, const pd_mode_key_runtime_hooks_t *hooks) {
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
            pd_mode_press.mode       = mode;
            pd_mode_press.was_locked = pd_mode_locked(mode);

            uint16_t action = hooks->advance_multi_tap(hooks->context, keycode);
            if (action != KC_NO) {
                hooks->dispatch_action(hooks->context, action);
            }

            pd_mode_press.keycode = KC_NO;
            pd_state_sync();
            return true;
        }

        pd_mode_press.timer      = timer_read();
        pd_mode_press.keycode    = keycode;
        pd_mode_press.mode       = mode;
        pd_mode_press.was_locked = pd_mode_locked(mode);
        pd_mode_update(mode, true);
        state_changed = true;
    } else {
        bool     pressed_this_key = pd_mode_press.keycode == keycode;
        uint16_t elapsed          = pressed_this_key ? timer_elapsed(pd_mode_press.timer) : 0;
        bool     locked_press     = pd_mode_press.mode == mode && pd_mode_press.was_locked && pd_mode_is_lockable(mode) && pressed_this_key;

        if (pd_mode_active(mode) && !pd_mode_locked(mode)) {
            pd_mode_update(mode, false);
            state_changed = true;
        }

        if (locked_press) {
            if (pd_mode_toggle_lock_state(mode)) {
                state_changed = true;
            }
        } else if (pressed_this_key && elapsed < CUSTOM_TAP_HOLD_TERM) {
            hooks->dispatch_tap_or_begin_multi_tap(hooks->context, keycode, record);
        }

        pd_mode_press_reset();
    }

    if (state_changed) {
        pd_state_sync();
    }
    return true;
}

#else

bool pd_mode_key_runtime_process(uint16_t keycode, keyrecord_t *record, uint8_t mode, bool multi_tap_repress, const pd_mode_key_runtime_hooks_t *hooks) {
    (void)keycode;
    (void)record;
    (void)mode;
    (void)multi_tap_repress;
    (void)hooks;
    return false;
}

#endif // POINTING_DEVICE_ENABLE
