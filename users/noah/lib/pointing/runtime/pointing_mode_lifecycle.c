#include "../modes/pointing_mode_configured.h"
#include "../../profile/runtime/active_profile_pd_runtime.h"
// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Lifecycle
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "pointing_mode_internal.h"
#include "pointing_mode_runtime_shared_state_internal.h"
#include "pointing_mode_registry_internal.h"
#include "../policy/pointing_mode_policy.h"
#include "../../compat/qmk_auto_mouse_contract.h"
#include "../../compat/qmk_pointing_contract.h"
#include "../../state/diagnostics/runtime_trace.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static bool pointing_mode_auto_mouse_uses_synthetic_anchor(pointing_mode_mask_t mode, bool locked) {
    return locked && pointing_mode_policy_mode_keeps_auto_mouse_anchored(mode) && !pointing_mode_has_trait(mode, POINTING_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE);
}

static void pointing_mode_auto_mouse_sync_anchor(bool should_anchor) {
    pointing_mode_runtime_shared_state_t *state = pointing_mode_runtime_shared_state();

    if (state->synthetic_auto_mouse_anchor_active == should_anchor) {
        return;
    }

    // Locked modes without a lock-owned auto-mouse toggle need an explicit
    // tracker anchor to behave like their physically-held form. Ordinary held
    // pointing-mode keys already participate in QMK's mouse-record path, and
    // scroll-like lock paths that own auto-mouse_toggle() should not also push
    // a tracker-style anchor here. QMK zeroes the tracker whenever it resets
    // auto-mouse (a live settings edit can), so the count is only taken back
    // while one is there to take.
    if (should_anchor || noah_qmk_contract_auto_mouse_key_tracker() > 0) {
        noah_qmk_contract_auto_mouse_keyevent(should_anchor);
    }
    state->synthetic_auto_mouse_anchor_active = should_anchor;
}

static void pointing_mode_auto_mouse_activate(pointing_mode_mask_t mode, bool was_any_mode_active, bool locked) {
    if (pointing_mode_policy_mode_prefers_typing_layer(mode)) {
        // Arrow mode should fall back to the typing/nav surface immediately
        // instead of waiting for the auto-mouse timeout to drop the pointer
        // layer.
        noah_qmk_contract_auto_mouse_layer_off();
        return;
    }

    if (!was_any_mode_active && pointing_mode_auto_mouse_uses_synthetic_anchor(mode, locked)) {
        pointing_mode_auto_mouse_sync_anchor(true);
    }
}

static void pointing_mode_auto_mouse_deactivate(pointing_mode_mask_t mode, bool was_any_mode_active, bool was_locked) {
    if (!pointing_mode_auto_mouse_uses_synthetic_anchor(mode, was_locked)) {
        return;
    }

    if (was_any_mode_active && !pd_any_local_mode_active()) {
        pointing_mode_auto_mouse_sync_anchor(false);
    }
}
#endif

void pointing_mode_request_active_dpi_sync(void) {
    pointing_mode_runtime_shared_state()->active_dpi_sync_pending = true;
}

void pointing_mode_set_auto_sniping_layer_active(bool active) {
    pointing_mode_runtime_shared_state()->auto_sniping_layer_active = active;
}

bool pointing_mode_auto_sniping_layer_active(void) {
    return pointing_mode_runtime_shared_state()->auto_sniping_layer_active;
}

void pointing_mode_service_active_dpi_sync(void) {
    if (!pointing_mode_runtime_shared_state()->active_dpi_sync_pending) {
        return;
    }

    pointing_mode_apply_active_dpi();
}

void pointing_mode_apply_active_dpi(void) {
    pointing_mode_runtime_shared_state()->active_dpi_sync_pending = false;

    pointing_mode_snapshot_t snapshot   = pointing_mode_snapshot();
    bool                     sniping    = pointing_mode_auto_sniping_layer_active() || noah_qmk_contract_pointer_sniping_enabled();
    const noah_pd_config_t  *configured = noah_active_profile_pd_for_mask(snapshot.local.active_mode);
    uint16_t                 mode_dpi   = configured ? configured->dpi : 0;

    if (configured && configured->kind == NOAH_PD_KIND_SCROLLING) {
        pointing_device_set_cpi(mode_dpi ? mode_dpi : noah_qmk_contract_pointer_default_dpi());
        return;
    }

    if (sniping) {
        pointing_device_set_cpi(noah_qmk_contract_pointer_sniping_dpi());
        return;
    }

    if (mode_dpi != 0) {
        pointing_device_set_cpi(mode_dpi);
        return;
    }

    // No active mode with a custom DPI — restore Charybdis's configured default.
    pointing_device_set_cpi(noah_qmk_contract_pointer_default_dpi());
}

void pointing_mode_transition_activate(pointing_mode_mask_t mode) {
    pointing_mode_transition_activate_from(mode, POINTING_MODE_SOURCE_KEY);
}

void pointing_mode_transition_activate_from(pointing_mode_mask_t mode, pointing_mode_source_t source) {
    bool was_active = pointing_mode_local_active(mode);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    bool was_any_mode_active = pd_any_local_mode_active();
    bool locked              = pointing_mode_local_locked(mode);
#endif

    if (!was_active) {
        pointing_mode_set(mode);
    }
    pointing_mode_runtime_shared_state()->local_active_source = (uint8_t)source;

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    // A mode its layer started never anchors the pointer layer or owns QMK's
    // auto-mouse toggle; the layer-state hook strips a separate pointer layer
    // while it runs (D-F62).
    if (source == POINTING_MODE_SOURCE_KEY) {
        pointing_mode_auto_mouse_activate(mode, was_any_mode_active, locked);
    }
#endif

    pointing_mode_request_active_dpi_sync();

    if (!was_active) {
        pointing_mode_registry_run_activate_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_ACTIVATE, mode, pointing_mode_local_active_snapshot());
    }
}

static void pointing_mode_transition_deactivate_with_lock_state(pointing_mode_mask_t mode, bool was_locked) {
    bool was_active = pointing_mode_local_active(mode);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    bool was_any_mode_active = pd_any_local_mode_active();
#endif
    pointing_mode_clear(mode);
    if (!pd_any_local_mode_active()) {
        pointing_mode_runtime_shared_state()->local_active_source = POINTING_MODE_SOURCE_NONE;
    }

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    pointing_mode_auto_mouse_deactivate(mode, was_any_mode_active, was_locked);
#endif

    pointing_mode_request_active_dpi_sync();

    if (was_active) {
        pointing_mode_registry_run_deactivate_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_DEACTIVATE, mode, pointing_mode_local_active_snapshot());
    }
}

void pointing_mode_transition_deactivate(pointing_mode_mask_t mode) {
    pointing_mode_transition_deactivate_with_lock_state(mode, pointing_mode_local_locked(mode));
}

void pointing_mode_transition_lock(pointing_mode_mask_t mode) {
    bool was_locked = pointing_mode_local_locked(mode);

    if (!was_locked) {
        pointing_mode_set_locked(mode);
    }
    pointing_mode_transition_activate(mode);

    if (!was_locked) {
        pointing_mode_registry_run_lock_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_LOCK, mode, pointing_mode_local_locked_snapshot());
    }
}

void pointing_mode_transition_hand_to_layer(pointing_mode_mask_t mode) {
    bool was_locked = pointing_mode_local_locked(mode);

    if (!pointing_mode_local_active(mode)) {
        return;
    }
    if (was_locked) {
        pointing_mode_registry_run_unlock_hooks(mode);
        pointing_mode_clear_locked(mode);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
        if (pointing_mode_auto_mouse_uses_synthetic_anchor(mode, was_locked)) {
            pointing_mode_auto_mouse_sync_anchor(false);
        }
#endif
        noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_UNLOCK, mode, pointing_mode_local_locked_snapshot());
    }
    pointing_mode_runtime_shared_state()->local_active_source = POINTING_MODE_SOURCE_LAYER;
    pointing_mode_request_active_dpi_sync();
}

void pointing_mode_transition_take_from_layer(pointing_mode_mask_t mode) {
    pointing_mode_runtime_shared_state()->local_active_source = POINTING_MODE_SOURCE_KEY;
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    // Its layer started it with none of a key-started mode's auto-mouse side
    // effects, so they run now as though it started from nothing.
    pointing_mode_auto_mouse_activate(mode, false, pointing_mode_local_locked(mode));
#endif
    pointing_mode_request_active_dpi_sync();
}

void pointing_mode_transition_unlock(pointing_mode_mask_t mode) {
    bool was_locked = pointing_mode_local_locked(mode);

    if (!pointing_mode_local_locked(mode)) {
        return;
    }

    pointing_mode_registry_run_unlock_hooks(mode);
    pointing_mode_clear_locked(mode);
    pointing_mode_transition_deactivate_with_lock_state(mode, was_locked);
    noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_UNLOCK, mode, pointing_mode_local_locked_snapshot());
}
