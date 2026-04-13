// PD Mode Policy
//
// Shared interpretation helpers for pd-mode behavior policy. This keeps
// cross-cutting decisions such as auto-mouse anchoring, typing-layer
// preference, dragscroll backend ownership, and remote display-mode selection
// in one place instead of open-coding the same trait/flag logic across
// multiple subsystems.
#pragma once

#include "../defs/pd_modes.h"

static inline pd_mode_mask_t pd_mode_policy_first_snapshot_mode(pd_mode_mask_t flags) {
    if (flags == 0) {
        return 0;
    }

    for (uint8_t index = 0; index < PD_MODE_COUNT; index++) {
        if ((flags & pd_modes[index].mode_flag) != 0) {
            return pd_modes[index].mode_flag;
        }
    }

    return 0;
}

static inline pd_mode_mask_t pd_mode_policy_remote_display_locked_mode(pd_mode_mask_t locked_flags) {
    return pd_mode_policy_first_snapshot_mode(locked_flags);
}

static inline pd_mode_mask_t pd_mode_policy_remote_display_active_mode(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags) {
    pd_mode_mask_t locked_mode = pd_mode_policy_remote_display_locked_mode(locked_flags);
    return locked_mode ? locked_mode : pd_mode_policy_first_snapshot_mode(active_flags);
}

static inline bool pd_mode_policy_mode_prefers_typing_layer(pd_mode_mask_t mode) {
    return pd_mode_has_trait(mode, PD_MODE_TRAIT_PREFER_TYPING_LAYER);
}

static inline bool pd_mode_policy_mode_keeps_auto_mouse_anchored(pd_mode_mask_t mode) {
    return pd_mode_has_trait(mode, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED);
}

static inline bool pd_mode_policy_mode_uses_dragscroll_backend(pd_mode_mask_t mode) {
    return pd_mode_has_trait(mode, PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND);
}

static inline bool pd_mode_policy_snapshot_prefers_typing_layer(pd_mode_snapshot_t snapshot) {
    return snapshot.local.active_mode != 0 && pd_mode_policy_mode_prefers_typing_layer(snapshot.local.active_mode);
}

static inline bool pd_mode_policy_snapshot_keeps_auto_mouse_anchored(pd_mode_snapshot_t snapshot) {
    return snapshot.local.active_mode != 0 && pd_mode_policy_mode_keeps_auto_mouse_anchored(snapshot.local.active_mode);
}

static inline bool pd_mode_policy_key_keeps_auto_mouse_anchored(uint16_t keycode) {
    return pd_mode_policy_mode_keeps_auto_mouse_anchored(pd_mode_for_keycode(keycode));
}
