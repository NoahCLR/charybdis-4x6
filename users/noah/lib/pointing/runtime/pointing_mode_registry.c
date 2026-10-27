// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Registry
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "noah_keymap_ids.h"
#include "../defs/pointing_mode_manifest.h"
#include "pointing_mode_internal.h"
#include "pointing_mode_buffered_tap_internal.h"
#include "pointing_mode_keyboard_event_internal.h"
#include "pointing_mode_registry_internal.h"
#include "../../compat/qmk_auto_mouse_contract.h"

#include "../modes/pointing_mode_handlers.h"
#include "../modes/pointing_mode_configured.h"
#include "../../profile/runtime/active_profile_pd_runtime.h"
#include "../../profile/runtime/active_profile_settings_runtime.h"

pointing_mode_traits_t pointing_mode_effective_traits(pointing_mode_mask_t mode) {
    const noah_pd_config_t *slot = noah_active_profile_pd_for_mask(mode);
    if (!slot) return POINTING_MODE_TRAIT_NONE;
    bool                   prefer_typing = slot->pointer_layer == NOAH_PD_POINTER_LAYER_PREFER_TYPING;
    pointing_mode_traits_t traits        = prefer_typing ? POINTING_MODE_TRAIT_PREFER_TYPING_LAYER : POINTING_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED;
    if (slot->kind == NOAH_PD_KIND_SCROLLING && !prefer_typing) traits |= POINTING_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE;
    return traits;
}

// The layers a link never names to any effect, and that never end a mode: a
// default layer or the pointer layer, either of which would run a mode all the
// time. The slot's validation cannot see the settings.
static layer_state_t pointing_mode_inert_layers(void) {
    layer_state_t inert = (layer_state_t)noah_setting(NOAH_SETTING_DEFAULT_LAYERS, 1u);

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    uint8_t pointer = noah_qmk_contract_auto_mouse_layer();
    if (pointer < sizeof(layer_state_t) * 8u) inert |= (layer_state_t)1u << pointer;
#endif
    return inert;
}

static int8_t pointing_mode_highest_layer(layer_state_t layers) {
    return layers ? (int8_t)(sizeof(unsigned int) * 8u - 1u - (unsigned)__builtin_clz((unsigned int)layers)) : -1;
}

// Every layer a slot's link names: the layers that start its mode and those
// its key turns on.
static uint16_t pointing_mode_slot_link_layers(const noah_pd_config_t *slot) {
    return (uint16_t)(slot->linked_layers | slot->link_turns_on);
}

// Whether a layer that is on in `state` ends the mode `slot`'s layers started,
// the highest of them that is on being `top`. The pointer layer and a default
// layer (`inert`) never end it: a mode its layer started strips the one, and
// the other is on everywhere.
static bool pointing_mode_link_ended(const noah_pd_config_t *slot, layer_state_t state, int8_t top, layer_state_t inert) {
    layer_state_t others = state & ~inert;

    if (slot->link_ends == NOAH_PD_LINK_ENDS_HIGHER_LAYER) return (others & ~(layer_state_t)(((layer_state_t)2u << top) - 1u)) != 0u;
    if (slot->link_ends == NOAH_PD_LINK_ENDS_OTHER_LAYER) return (others & ~(layer_state_t)pointing_mode_slot_link_layers(slot)) != 0u;
    return false;
}

pointing_mode_mask_t pointing_mode_layer_started_mode(layer_state_t state) {
    pointing_mode_mask_t mode          = 0;
    int8_t               highest_layer = -1;
    layer_state_t        inert         = pointing_mode_inert_layers();

    // No layer starts two modes (the domain refuses it), so the highest
    // linked layer that is on names one mode.
    for (uint8_t slot = 0; slot < POINTING_MODE_COUNT; slot++) {
        const noah_pd_config_t *record = noah_active_profile_pd_for_mask(pointing_mode_mask_from_id(slot));
        int8_t                  top;

        if (!record) continue;
        top = pointing_mode_highest_layer(state & (layer_state_t)(record->linked_layers & ~inert));
        if (top <= highest_layer || pointing_mode_link_ended(record, state, top, inert)) continue;
        highest_layer = top;
        mode          = pointing_mode_mask_from_id(slot);
    }
    return mode;
}

uint16_t pointing_mode_key_turns_on(pointing_mode_mask_t mode) {
    const noah_pd_config_t *slot = noah_active_profile_pd_for_mask(mode);

    return slot ? (uint16_t)(slot->link_turns_on & ~pointing_mode_inert_layers()) : 0u;
}

uint16_t pointing_mode_link_layers(pointing_mode_mask_t mode) {
    const noah_pd_config_t *slot = noah_active_profile_pd_for_mask(mode);

    return slot ? (uint16_t)(pointing_mode_slot_link_layers(slot) & ~pointing_mode_inert_layers()) : 0u;
}

static inline bool pointing_mode_registry_has_trait(pointing_mode_mask_t mode, pointing_mode_traits_t trait) {
    return (pointing_mode_effective_traits(mode) & trait) == trait;
}

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static bool scroll_mode_auto_mouse_owned = false;

void pointing_mode_scroll_lock_attach_auto_mouse(pointing_mode_mask_t mode) {
    (void)mode;

    if (noah_qmk_contract_auto_mouse_toggle_enabled()) {
        scroll_mode_auto_mouse_owned = false;
        return;
    }

    noah_qmk_contract_auto_mouse_toggle();
    scroll_mode_auto_mouse_owned = true;
}

void pointing_mode_scroll_lock_detach_auto_mouse(pointing_mode_mask_t mode) {
    (void)mode;

    if (scroll_mode_auto_mouse_owned && noah_qmk_contract_auto_mouse_toggle_enabled()) {
        noah_qmk_contract_auto_mouse_toggle();
    }

    scroll_mode_auto_mouse_owned = false;
}

static const pointing_mode_lifecycle_hooks_t pointing_mode_auto_mouse_lock_toggle_hooks = {
    .on_lock   = pointing_mode_scroll_lock_attach_auto_mouse,
    .on_unlock = pointing_mode_scroll_lock_detach_auto_mouse,
};
#endif

// The manifest fixes deployed action identities. Motion and policy are read
// from the effective slot; the registry has no per-preset callbacks.
#define NOAH_POINTING_MODE_REGISTRY_ROW(name, mode_keycode) {.mode_flag = POINTING_MODE_##name, .keycode = mode_keycode, .lock_action = mode_keycode##_LOCK},
const pointing_mode_def_t pointing_modes[POINTING_MODE_COUNT] = {NOAH_POINTING_MODE_LIST(NOAH_POINTING_MODE_REGISTRY_ROW)};
#undef NOAH_POINTING_MODE_REGISTRY_ROW

static const pointing_mode_lifecycle_hooks_t *pointing_mode_lifecycle_hooks_for_mode(pointing_mode_mask_t mode) {
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    return pointing_mode_has_trait(mode, POINTING_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE) ? &pointing_mode_auto_mouse_lock_toggle_hooks : NULL;
#else
    (void)mode;
    return NULL;
#endif
}

static uint8_t pointing_mode_lifecycle_buffered_tap_masked_real_mods(pointing_mode_mask_t mode) {
    return noah_pd_engine_masked_mods(noah_active_profile_pd_for_mask(mode));
}

static uint8_t pointing_mode_lifecycle_keyboard_event_masked_real_mods(pointing_mode_mask_t mode) {
    return noah_pd_engine_masked_mods(noah_active_profile_pd_for_mask(mode));
}

static void pointing_mode_run_lifecycle_callback(void (*callback)(pointing_mode_mask_t mode), pointing_mode_mask_t mode) {
    if (callback) {
        callback(mode);
    }
}

void pointing_mode_registry_run_activate_hooks(pointing_mode_mask_t mode) {
    noah_pd_engine_enter(noah_active_profile_pd_for_mask(mode));
    const pointing_mode_lifecycle_hooks_t *hooks = pointing_mode_lifecycle_hooks_for_mode(mode);
    pointing_mode_run_lifecycle_callback(hooks ? hooks->on_activate : NULL, mode);
}

void pointing_mode_registry_run_deactivate_hooks(pointing_mode_mask_t mode) {
    noah_pd_engine_exit();
    const pointing_mode_lifecycle_hooks_t *hooks = pointing_mode_lifecycle_hooks_for_mode(mode);
    pointing_mode_run_lifecycle_callback(hooks ? hooks->on_deactivate : NULL, mode);
}

void pointing_mode_registry_run_lock_hooks(pointing_mode_mask_t mode) {
    const pointing_mode_lifecycle_hooks_t *hooks = pointing_mode_lifecycle_hooks_for_mode(mode);
    pointing_mode_run_lifecycle_callback(hooks ? hooks->on_lock : NULL, mode);
}

void pointing_mode_registry_run_unlock_hooks(pointing_mode_mask_t mode) {
    const pointing_mode_lifecycle_hooks_t *hooks = pointing_mode_lifecycle_hooks_for_mode(mode);
    pointing_mode_run_lifecycle_callback(hooks ? hooks->on_unlock : NULL, mode);
}

const pointing_mode_def_t *pointing_mode_lookup(pointing_mode_mask_t mode) {
    for (uint8_t i = 0; i < POINTING_MODE_COUNT; i++) {
        if (pointing_modes[i].mode_flag == mode) return &pointing_modes[i];
    }
    return NULL;
}

const pointing_mode_def_t *pointing_mode_lock_action_lookup(uint16_t action) {
    for (uint8_t i = 0; i < POINTING_MODE_COUNT; i++) {
        if (pointing_modes[i].lock_action == action) return &pointing_modes[i];
    }
    return NULL;
}

bool is_pointing_mode_lock_action(uint16_t action) {
    return pointing_mode_lock_action_lookup(action) != NULL;
}

bool pointing_mode_has_trait(pointing_mode_mask_t mode, pointing_mode_traits_t trait) {
    return pointing_mode_registry_has_trait(mode, trait);
}

bool pd_any_active_mode_has_trait(pointing_mode_traits_t trait) {
    return (pointing_mode_snapshot().local.active_traits & trait) == trait;
}

pointing_mode_mask_t pointing_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < POINTING_MODE_COUNT; i++) {
        if (pointing_modes[i].keycode != KC_NO && pointing_modes[i].keycode == keycode) return pointing_modes[i].mode_flag;
    }
    return 0;
}

uint8_t pointing_mode_buffered_tap_masked_real_mods(uint16_t keycode) {
    pointing_mode_mask_t mode = pointing_mode_for_keycode(keycode);
    return mode != 0 ? pointing_mode_lifecycle_buffered_tap_masked_real_mods(mode) : 0;
}

uint8_t pointing_mode_active_keyboard_event_masked_real_mods(void) {
    pointing_mode_mask_t mode = pointing_mode_local_active_snapshot();
    return mode != 0 ? pointing_mode_lifecycle_keyboard_event_masked_real_mods(mode) : 0;
}
