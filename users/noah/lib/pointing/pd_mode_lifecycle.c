// ────────────────────────────────────────────────────────────────────────────
// PD Mode Lifecycle
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "pd_mode_internal.h"
#include "pd_mode_registry_internal.h"
#include "../compat/qmk_contract.h"
#include "../state/runtime_trace.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static void pd_mode_auto_mouse_sync_anchor(bool should_anchor) {
    static bool pd_mode_auto_mouse_anchor_active = false;

    if (pd_mode_auto_mouse_anchor_active == should_anchor) {
        return;
    }

    // Pd-mode handlers freeze the outgoing mouse report, so locked modes need
    // an explicit auto-mouse anchor to behave like their physically-held form.
    noah_qmk_contract_auto_mouse_keyevent(should_anchor);
    pd_mode_auto_mouse_anchor_active = should_anchor;
}

static void pd_mode_auto_mouse_activate(pd_mode_mask_t mode, bool was_any_mode_active) {
    if (pd_mode_has_trait(mode, PD_MODE_TRAIT_PREFER_TYPING_LAYER)) {
        // Arrow mode should fall back to the typing/nav surface immediately
        // instead of waiting for the auto-mouse timeout to drop the pointer
        // layer.
        noah_qmk_contract_auto_mouse_layer_off();
        return;
    }

    if (!was_any_mode_active && pd_mode_has_trait(mode, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED)) {
        pd_mode_auto_mouse_sync_anchor(true);
    }
}

static void pd_mode_auto_mouse_deactivate(pd_mode_mask_t mode, bool was_any_mode_active) {
    if (!pd_mode_has_trait(mode, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED)) {
        return;
    }

    if (was_any_mode_active && !pd_any_local_mode_active()) {
        pd_mode_auto_mouse_sync_anchor(false);
    }
}
#endif

static void pd_mode_enforce_exclusive_active_mode(pd_mode_mask_t keep_mode) {
    pd_mode_unlock_other_locks(keep_mode);
    pd_mode_deactivate_other_unlocked(keep_mode);
}

void pd_mode_apply_active_dpi(void) {
    // Sniping and dragscroll manage their own CPI through Charybdis internals.
    if (charybdis_get_pointer_dragscroll_enabled()) {
        return;
    }

    if (charybdis_get_pointer_sniping_enabled()) {
        return;
    }

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_local_active(pd_modes[i].mode_flag) && pd_modes[i].dpi != 0) {
            pointing_device_set_cpi(pd_modes[i].dpi);
            return;
        }
    }

    // No active mode with a custom DPI — restore Charybdis's configured default.
    pointing_device_set_cpi(charybdis_get_pointer_default_dpi());
}

void pd_mode_activate(pd_mode_mask_t mode) {
    bool was_active = pd_mode_local_active(mode);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    bool was_any_mode_active = pd_any_local_mode_active();
#endif
    pd_mode_enforce_exclusive_active_mode(mode);

    if (!was_active) {
        pd_mode_set(mode);
    }

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    pd_mode_auto_mouse_activate(mode, was_any_mode_active);
#endif

    if (pd_mode_has_trait(mode, PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND)) {
        charybdis_set_pointer_dragscroll_enabled(true);
        // Charybdis sets CHARYBDIS_DRAGSCROLL_DPI via maybe_update_pointing_device_cpi().
    } else {
        pd_mode_apply_active_dpi();
    }

    if (!was_active) {
        pd_mode_registry_run_activate_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_ACTIVATE, mode, pd_mode_local_active_snapshot());
    }
}

void pd_mode_deactivate(pd_mode_mask_t mode) {
    bool                was_active = pd_mode_local_active(mode);
    const pd_mode_def_t *def       = pd_mode_lookup(mode);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    bool was_any_mode_active = pd_any_local_mode_active();
#endif
    pd_mode_clear(mode);

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    pd_mode_auto_mouse_deactivate(mode, was_any_mode_active);
#endif

    if (def && def->reset) {
        def->reset();
    }

    if (pd_mode_has_trait(mode, PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND)) {
        charybdis_set_pointer_dragscroll_enabled(false);
        // Charybdis restores normal pointer DPI via maybe_update_pointing_device_cpi().
    } else {
        pd_mode_apply_active_dpi();
    }

    if (was_active) {
        pd_mode_registry_run_deactivate_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_DEACTIVATE, mode, pd_mode_local_active_snapshot());
    }
}

void pd_mode_lock(pd_mode_mask_t mode) {
    bool was_locked = pd_mode_local_locked(mode);

    pd_mode_enforce_exclusive_active_mode(mode);

    if (!was_locked) {
        pd_mode_set_locked(mode);
    }
    pd_mode_activate(mode);

    if (!was_locked) {
        pd_mode_registry_run_lock_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_LOCK, mode, pd_mode_local_locked_snapshot());
    }
}

void pd_mode_unlock(pd_mode_mask_t mode) {
    if (!pd_mode_local_locked(mode)) {
        return;
    }

    pd_mode_registry_run_unlock_hooks(mode);
    pd_mode_clear_locked(mode);
    pd_mode_deactivate(mode);
    noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_UNLOCK, mode, pd_mode_local_locked_snapshot());
}

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_local_active(pd_modes[i].mode_flag) && pd_modes[i].key_handler && pd_modes[i].key_handler(keycode, record)) {
            return true;
        }
    }
    return false;
}
