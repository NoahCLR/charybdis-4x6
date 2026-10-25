// Pointing Mode Policy
//
// Shared interpretation helpers for pointing-mode behavior policy. This keeps
// cross-cutting decisions such as auto-mouse anchoring, typing-layer
// preference, and dragscroll backend ownership in one place instead of
// open-coding the same trait/flag logic across
// multiple subsystems.
#pragma once

#include "../defs/pointing_modes.h"

static inline bool pointing_mode_policy_mode_prefers_typing_layer(pointing_mode_mask_t mode) {
    return pointing_mode_has_trait(mode, POINTING_MODE_TRAIT_PREFER_TYPING_LAYER);
}

static inline bool pointing_mode_policy_mode_keeps_auto_mouse_anchored(pointing_mode_mask_t mode) {
    return pointing_mode_has_trait(mode, POINTING_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED);
}

// A mode its layer started neither keeps nor prefers to drop the pointer
// layer: the layer-state hook strips a separate one while it runs (D-F62).
static inline bool pointing_mode_policy_snapshot_prefers_typing_layer(pointing_mode_snapshot_t snapshot) {
    return snapshot.local.active_mode != 0 && snapshot.local.active_source == POINTING_MODE_SOURCE_KEY && pointing_mode_policy_mode_prefers_typing_layer(snapshot.local.active_mode);
}

static inline bool pointing_mode_policy_snapshot_keeps_auto_mouse_anchored(pointing_mode_snapshot_t snapshot) {
    return snapshot.local.active_mode != 0 && snapshot.local.active_source == POINTING_MODE_SOURCE_KEY && pointing_mode_policy_mode_keeps_auto_mouse_anchored(snapshot.local.active_mode);
}

static inline bool pointing_mode_policy_snapshot_layer_started(pointing_mode_snapshot_t snapshot) {
    return snapshot.local.active_mode != 0 && snapshot.local.active_source == POINTING_MODE_SOURCE_LAYER;
}

static inline bool pointing_mode_policy_key_keeps_auto_mouse_anchored(uint16_t keycode) {
    return pointing_mode_policy_mode_keeps_auto_mouse_anchored(pointing_mode_for_keycode(keycode));
}
