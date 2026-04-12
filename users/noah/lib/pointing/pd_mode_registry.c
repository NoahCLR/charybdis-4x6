// ────────────────────────────────────────────────────────────────────────────
// PD Mode Registry
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "noah_keymap_ids.h"
#include "pd_mode_manifest.h"
#include "pd_mode_internal.h"
#include "../compat/qmk_contract.h"
#include "../state/keyboard_mod_ownership.h"
#include "../state/runtime_trace.h"

#include "pd_mode_handlers.h"

struct pd_mode_lifecycle_hooks {
    void (*on_activate)(pd_mode_mask_t mode);
    void (*on_deactivate)(pd_mode_mask_t mode);
    void (*on_lock)(pd_mode_mask_t mode);
    void (*on_unlock)(pd_mode_mask_t mode);
};

static inline bool pd_mode_registry_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait) {
    const pd_mode_def_t *def = pd_mode_lookup(mode);
    return def && (def->traits & trait) == trait;
}

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static bool scroll_mode_auto_mouse_owned = false;

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
    if (pd_mode_registry_has_trait(mode, PD_MODE_TRAIT_PREFER_TYPING_LAYER)) {
        // Arrow mode should fall back to the typing/nav surface immediately
        // instead of waiting for the auto-mouse timeout to drop the pointer
        // layer.
        noah_qmk_contract_auto_mouse_layer_off();
        return;
    }

    if (!was_any_mode_active && pd_mode_registry_has_trait(mode, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED)) {
        pd_mode_auto_mouse_sync_anchor(true);
    }
}

static void pd_mode_auto_mouse_deactivate(pd_mode_mask_t mode, bool was_any_mode_active) {
    if (!pd_mode_registry_has_trait(mode, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED)) {
        return;
    }

    if (was_any_mode_active && !pd_any_local_mode_active()) {
        pd_mode_auto_mouse_sync_anchor(false);
    }
}

static void scroll_mode_lock_attach_auto_mouse(pd_mode_mask_t mode) {
    (void)mode;

    if (noah_qmk_contract_auto_mouse_toggle_enabled()) {
        scroll_mode_auto_mouse_owned = false;
        return;
    }

    noah_qmk_contract_auto_mouse_toggle();
    scroll_mode_auto_mouse_owned = true;
}

static void scroll_mode_lock_detach_auto_mouse(pd_mode_mask_t mode) {
    (void)mode;

    if (scroll_mode_auto_mouse_owned && noah_qmk_contract_auto_mouse_toggle_enabled()) {
        noah_qmk_contract_auto_mouse_toggle();
    }

    scroll_mode_auto_mouse_owned = false;
}

static const pd_mode_lifecycle_hooks_t pd_mode_auto_mouse_lock_toggle_hooks = {
    .on_lock   = scroll_mode_lock_attach_auto_mouse,
    .on_unlock = scroll_mode_lock_detach_auto_mouse,
};

#    define PD_MODE_LIFECYCLE_AUTO_MOUSE_LOCK (&pd_mode_auto_mouse_lock_toggle_hooks)
#else
#    define PD_MODE_LIFECYCLE_AUTO_MOUSE_LOCK NULL
#endif

static bool pinch_command_registered = false;

static void pinch_mode_register_command(pd_mode_mask_t mode) {
    (void)mode;

    if (pinch_command_registered) {
        return;
    }

    // Pinch mode owns a logical GUI hold for trackpad gestures. Keep that on the
    // same real-mod ownership path as the rest of the custom runtime instead of
    // advertising it as a transient weak modifier.
    keyboard_mod_ownership_register(KC_LEFT_GUI);
    pinch_command_registered = true;
}

static void pinch_mode_unregister_command(pd_mode_mask_t mode) {
    (void)mode;

    if (!pinch_command_registered) {
        return;
    }

    keyboard_mod_ownership_unregister(KC_LEFT_GUI);
    pinch_command_registered = false;
}

static const pd_mode_lifecycle_hooks_t pd_mode_pinch_lifecycle_hooks = {
    .on_activate   = pinch_mode_register_command,
    .on_deactivate = pinch_mode_unregister_command,
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    .on_lock       = scroll_mode_lock_attach_auto_mouse,
    .on_unlock     = scroll_mode_lock_detach_auto_mouse,
#endif
};

#define PD_MODE_LIFECYCLE_PINCH (&pd_mode_pinch_lifecycle_hooks)

// Per-mode pointer DPI overrides. Override any of these in config.h.
// 0 = no override: normal pointer DPI is used while that mode is active.
// Dragscroll and pinch are excluded — Charybdis manages their CPI internally.
#ifndef PD_MODE_VOLUME_DPI
#    define PD_MODE_VOLUME_DPI 0
#endif
#ifndef PD_MODE_BRIGHTNESS_DPI
#    define PD_MODE_BRIGHTNESS_DPI 0
#endif
#ifndef PD_MODE_ZOOM_DPI
#    define PD_MODE_ZOOM_DPI 0
#endif
#ifndef PD_MODE_ARROW_DPI
#    define PD_MODE_ARROW_DPI 0
#endif

#define NOAH_PD_MODE_REGISTRY_ROW(name, keycode, handler, key_handler, reset, dpi, traits, lifecycle) {PD_MODE_##name, keycode, keycode##_LOCK, handler, key_handler, reset, dpi, traits, lifecycle},
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {NOAH_PD_MODE_LIST(NOAH_PD_MODE_REGISTRY_ROW)};
#undef NOAH_PD_MODE_REGISTRY_ROW

static const pd_mode_lifecycle_hooks_t *pd_mode_lifecycle_hooks_for_mode(pd_mode_mask_t mode) {
    const pd_mode_def_t *def = pd_mode_lookup(mode);
    return def ? def->lifecycle : NULL;
}

static void pd_mode_run_lifecycle_callback(void (*callback)(pd_mode_mask_t mode), pd_mode_mask_t mode) {
    if (callback) {
        callback(mode);
    }
}

static void pd_mode_run_activate_hooks(pd_mode_mask_t mode) {
    const pd_mode_lifecycle_hooks_t *hooks = pd_mode_lifecycle_hooks_for_mode(mode);
    pd_mode_run_lifecycle_callback(hooks ? hooks->on_activate : NULL, mode);
}

static void pd_mode_run_deactivate_hooks(pd_mode_mask_t mode) {
    const pd_mode_lifecycle_hooks_t *hooks = pd_mode_lifecycle_hooks_for_mode(mode);
    pd_mode_run_lifecycle_callback(hooks ? hooks->on_deactivate : NULL, mode);
}

static void pd_mode_run_lock_hooks(pd_mode_mask_t mode) {
    const pd_mode_lifecycle_hooks_t *hooks = pd_mode_lifecycle_hooks_for_mode(mode);
    pd_mode_run_lifecycle_callback(hooks ? hooks->on_lock : NULL, mode);
}

static void pd_mode_run_unlock_hooks(pd_mode_mask_t mode) {
    const pd_mode_lifecycle_hooks_t *hooks = pd_mode_lifecycle_hooks_for_mode(mode);
    pd_mode_run_lifecycle_callback(hooks ? hooks->on_unlock : NULL, mode);
}

const pd_mode_def_t *pd_mode_lookup(pd_mode_mask_t mode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].mode_flag == mode) return &pd_modes[i];
    }
    return NULL;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].lock_action == action) return &pd_modes[i];
    }
    return NULL;
}

bool is_pd_mode_lock_action(uint16_t action) {
    return pd_mode_lock_action_lookup(action) != NULL;
}

bool pd_mode_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait) {
    return pd_mode_registry_has_trait(mode, trait);
}

bool pd_any_active_mode_has_trait(pd_mode_traits_t trait) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_local_active(pd_modes[i].mode_flag) && (pd_modes[i].traits & trait) == trait) {
            return true;
        }
    }

    return false;
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

static void pd_mode_enforce_exclusive_active_mode(pd_mode_mask_t keep_mode) {
    pd_mode_unlock_other_locks(keep_mode);
    pd_mode_deactivate_other_unlocked(keep_mode);
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
        pd_mode_run_activate_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_ACTIVATE, mode, pd_mode_local_active_snapshot());
    }
}

void pd_mode_deactivate(pd_mode_mask_t mode) {
    bool was_active = pd_mode_local_active(mode);
    const pd_mode_def_t *def = pd_mode_lookup(mode);
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
        pd_mode_run_deactivate_hooks(mode);
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
        pd_mode_run_lock_hooks(mode);
        noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_LOCK, mode, pd_mode_local_locked_snapshot());
    }
}

void pd_mode_unlock(pd_mode_mask_t mode) {
    if (!pd_mode_local_locked(mode)) {
        return;
    }

    pd_mode_run_unlock_hooks(mode);
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

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) return pd_modes[i].mode_flag;
    }
    return 0;
}

static uint8_t pd_mode_first_snapshot_index(pd_mode_mask_t active_flags) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if ((active_flags & pd_modes[i].mode_flag) != 0) return i;
    }

    return PD_MODE_COUNT;
}

uint8_t pd_mode_first_local_active_index(void) {
    return pd_mode_first_snapshot_index(pd_mode_local_active_snapshot());
}

uint8_t pd_mode_first_display_active_index(void) {
    return pd_mode_first_snapshot_index(pd_mode_display_active_snapshot());
}
