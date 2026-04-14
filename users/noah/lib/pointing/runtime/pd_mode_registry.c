// ────────────────────────────────────────────────────────────────────────────
// PD Mode Registry
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "noah_keymap_ids.h"
#include "../defs/pd_mode_manifest.h"
#include "pd_mode_internal.h"
#include "pd_mode_buffered_tap_internal.h"
#include "pd_mode_registry_internal.h"
#include "../../compat/qmk_auto_mouse_contract.h"

#include "../modes/pd_mode_handlers.h"

static inline bool pd_mode_registry_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait) {
    const pd_mode_def_t *def = pd_mode_lookup(mode);
    return def && (def->traits & trait) == trait;
}

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static bool scroll_mode_auto_mouse_owned = false;

void pd_mode_scroll_lock_attach_auto_mouse(pd_mode_mask_t mode) {
    (void)mode;

    if (noah_qmk_contract_auto_mouse_toggle_enabled()) {
        scroll_mode_auto_mouse_owned = false;
        return;
    }

    noah_qmk_contract_auto_mouse_toggle();
    scroll_mode_auto_mouse_owned = true;
}

void pd_mode_scroll_lock_detach_auto_mouse(pd_mode_mask_t mode) {
    (void)mode;

    if (scroll_mode_auto_mouse_owned && noah_qmk_contract_auto_mouse_toggle_enabled()) {
        noah_qmk_contract_auto_mouse_toggle();
    }

    scroll_mode_auto_mouse_owned = false;
}

static const pd_mode_lifecycle_hooks_t pd_mode_auto_mouse_lock_toggle_hooks = {
    .on_lock   = pd_mode_scroll_lock_attach_auto_mouse,
    .on_unlock = pd_mode_scroll_lock_detach_auto_mouse,
};

#    define PD_MODE_LIFECYCLE_AUTO_MOUSE_LOCK (&pd_mode_auto_mouse_lock_toggle_hooks)
#else
#    define PD_MODE_LIFECYCLE_AUTO_MOUSE_LOCK NULL
#endif

#define PD_MODE_LIFECYCLE_PINCH (&pd_mode_pinch_lifecycle_hooks)

// Per-mode pointer DPI overrides. Override any of these in config.h.
// 0 = no override: normal pointer DPI is used while that mode is active.
// Dragscroll and pinch are excluded — the shared dragscroll handler manages
// their CPI through pd_mode_apply_active_dpi().
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

static uint8_t pd_mode_lifecycle_buffered_tap_masked_real_mods(pd_mode_mask_t mode) {
    const pd_mode_lifecycle_hooks_t *hooks = pd_mode_lifecycle_hooks_for_mode(mode);
    return hooks && hooks->buffered_tap_masked_real_mods ? hooks->buffered_tap_masked_real_mods(mode) : 0;
}

static void pd_mode_run_lifecycle_callback(void (*callback)(pd_mode_mask_t mode), pd_mode_mask_t mode) {
    if (callback) {
        callback(mode);
    }
}

void pd_mode_registry_run_activate_hooks(pd_mode_mask_t mode) {
    const pd_mode_lifecycle_hooks_t *hooks = pd_mode_lifecycle_hooks_for_mode(mode);
    pd_mode_run_lifecycle_callback(hooks ? hooks->on_activate : NULL, mode);
}

void pd_mode_registry_run_deactivate_hooks(pd_mode_mask_t mode) {
    const pd_mode_lifecycle_hooks_t *hooks = pd_mode_lifecycle_hooks_for_mode(mode);
    pd_mode_run_lifecycle_callback(hooks ? hooks->on_deactivate : NULL, mode);
}

void pd_mode_registry_run_lock_hooks(pd_mode_mask_t mode) {
    const pd_mode_lifecycle_hooks_t *hooks = pd_mode_lifecycle_hooks_for_mode(mode);
    pd_mode_run_lifecycle_callback(hooks ? hooks->on_lock : NULL, mode);
}

void pd_mode_registry_run_unlock_hooks(pd_mode_mask_t mode) {
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
    return (pd_mode_snapshot().local.active_traits & trait) == trait;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) return pd_modes[i].mode_flag;
    }
    return 0;
}

uint8_t pd_mode_buffered_tap_masked_real_mods(uint16_t keycode) {
    pd_mode_mask_t mode = pd_mode_for_keycode(keycode);
    return mode != 0 ? pd_mode_lifecycle_buffered_tap_masked_real_mods(mode) : 0;
}
