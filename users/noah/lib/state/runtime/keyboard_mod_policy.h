// ────────────────────────────────────────────────────────────────────────────
// Keyboard Modifier Policy
// ────────────────────────────────────────────────────────────────────────────
//
// Shared modifier masking and replay helpers over QMK's live modifier report.
// Callers should use this layer for temporary filtering/restoration policy and
// reserve keyboard_mod_state.c for the low-level snapshot/apply mechanics.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "keyboard_mod_state.h"

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods);

keyboard_mod_state_t keyboard_mod_policy_current_state(void);
keyboard_mod_state_t keyboard_mod_policy_without_mods(keyboard_mod_state_t state, uint8_t mods);
keyboard_mod_state_t keyboard_mod_policy_without_real_mods(keyboard_mod_state_t state, uint8_t mods);
keyboard_mod_state_t keyboard_mod_policy_with_real_mods(keyboard_mod_state_t state, uint8_t mods);
keyboard_mod_state_t keyboard_mod_policy_restore_after_action_replay(uint16_t action, keyboard_mod_state_t saved);

static inline uint8_t keyboard_mod_policy_managed_only_mask(uint8_t mods) {
    return keyboard_mod_ownership_managed_only_mask(mods);
}
