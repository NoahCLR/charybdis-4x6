// ────────────────────────────────────────────────────────────────────────────
// PD Mode Lifecycle
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "pd_mode_internal.h"
#include "pd_mode_runtime_shared_state_internal.h"
#include "pd_mode_registry_internal.h"
#include "../policy/pd_mode_policy.h"
#include "../../compat/qmk_auto_mouse_contract.h"
#include "../../compat/qmk_pointing_contract.h"
#include "../../state/runtime/runtime_trace.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static bool pd_mode_auto_mouse_uses_synthetic_anchor(pd_mode_mask_t mode, bool locked) {
    return locked && pd_mode_policy_mode_keeps_auto_mouse_anchored(mode) && !pd_mode_has_trait(mode, PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE);
}

static void pd_mode_auto_mouse_sync_anchor(bool should_anchor) {
    pd_mode_runtime_shared_state_t *state = pd_mode_runtime_shared_state();

    if (state->synthetic_auto_mouse_anchor_active == should_anchor) {
        return;
    }

    // Locked modes without a lock-owned auto-mouse toggle need an explicit
    // tracker anchor to behave like their physically-held form. Ordinary held
    // pd-mode keys already participate in QMK's mouse-record path, and scroll-
    // like lock paths that own auto-mouse_toggle() should not also push a
    // tracker-style anchor here.
    noah_qmk_contract_auto_mouse_keyevent(should_anchor);
    state->synthetic_auto_mouse_anchor_active = should_anchor;
}

static void pd_mode_auto_mouse_activate(pd_mode_mask_t mode, bool was_any_mode_active, bool locked) {
    if (pd_mode_policy_mode_prefers_typing_layer(mode)) {
        // Arrow mode should fall back to the typing/nav surface immediately
        // instead of waiting for the auto-mouse timeout to drop the pointer
        // layer.
        noah_qmk_contract_auto_mouse_layer_off();
        return;
    }

    if (!was_any_mode_active && pd_mode_auto_mouse_uses_synthetic_anchor(mode, locked)) {
        pd_mode_auto_mouse_sync_anchor(true);
    }
}

static void pd_mode_auto_mouse_deactivate(pd_mode_mask_t mode, bool was_any_mode_active, bool was_locked) {
    if (!pd_mode_auto_mouse_uses_synthetic_anchor(mode, was_locked)) {
        return;
    }

    if (was_any_mode_active && !pd_any_local_mode_active()) {
        pd_mode_auto_mouse_sync_anchor(false);
    }
}
#endif

void pd_mode_apply_active_dpi(void) {
    pd_mode_snapshot_t   snapshot    = pd_mode_snapshot();
    const pd_mode_def_t *active_mode = pd_mode_lookup(snapshot.local.active_mode);

    if (pd_mode_policy_mode_uses_dragscroll_backend(snapshot.local.active_mode)) {
        pointing_device_set_cpi(noah_qmk_contract_pointer_dragscroll_dpi());
        return;
    }

    if (noah_qmk_contract_pointer_sniping_enabled()) {
        noah_qmk_contract_pointer_set_sniping_enabled(true);
        return;
    }

    if (active_mode && active_mode->dpi != 0) {
        pointing_device_set_cpi(active_mode->dpi);
        return;
    }

    // No active mode with a custom DPI — restore Charybdis's configured default.
    pointing_device_set_cpi(noah_qmk_contract_pointer_default_dpi());
}

void pd_mode_transition_activate(pd_mode_mask_t mode) {
    bool was_active = pd_mode_local_active(mode);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    bool was_any_mode_active = pd_any_local_mode_active();
    bool locked              = pd_mode_local_locked(mode);
#endif

    if (!was_active) {
        pd_mode_set(mode);
    }

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    pd_mode_auto_mouse_activate(mode, was_any_mode_active, locked);
#endif

    pd_mode_apply_active_dpi();

    if (!was_active) {
        pd_mode_registry_run_activate_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_ACTIVATE, mode, pd_mode_local_active_snapshot());
    }
}

static void pd_mode_transition_deactivate_with_lock_state(pd_mode_mask_t mode, bool was_locked) {
    bool                 was_active = pd_mode_local_active(mode);
    const pd_mode_def_t *def        = pd_mode_lookup(mode);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    bool was_any_mode_active = pd_any_local_mode_active();
#endif
    pd_mode_clear(mode);

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    pd_mode_auto_mouse_deactivate(mode, was_any_mode_active, was_locked);
#endif

    if (def && def->reset) {
        def->reset();
    }

    pd_mode_apply_active_dpi();

    if (was_active) {
        pd_mode_registry_run_deactivate_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_DEACTIVATE, mode, pd_mode_local_active_snapshot());
    }
}

void pd_mode_transition_deactivate(pd_mode_mask_t mode) {
    pd_mode_transition_deactivate_with_lock_state(mode, pd_mode_local_locked(mode));
}

void pd_mode_transition_lock(pd_mode_mask_t mode) {
    bool was_locked = pd_mode_local_locked(mode);

    if (!was_locked) {
        pd_mode_set_locked(mode);
    }
    pd_mode_transition_activate(mode);

    if (!was_locked) {
        pd_mode_registry_run_lock_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_LOCK, mode, pd_mode_local_locked_snapshot());
    }
}

void pd_mode_transition_unlock(pd_mode_mask_t mode) {
    bool was_locked = pd_mode_local_locked(mode);

    if (!pd_mode_local_locked(mode)) {
        return;
    }

    pd_mode_registry_run_unlock_hooks(mode);
    pd_mode_clear_locked(mode);
    pd_mode_transition_deactivate_with_lock_state(mode, was_locked);
    noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_UNLOCK, mode, pd_mode_local_locked_snapshot());
}

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    pd_mode_snapshot_t   snapshot = pd_mode_snapshot();
    const pd_mode_def_t *def      = pd_mode_lookup(snapshot.local.active_mode);

    if (def && def->key_handler && def->key_handler(keycode, record)) {
        return true;
    }

    return false;
}
