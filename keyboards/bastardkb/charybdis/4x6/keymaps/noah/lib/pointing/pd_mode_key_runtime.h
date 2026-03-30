// ────────────────────────────────────────────────────────────────────────────
// Pointing-Device Mode Key Runtime
// ────────────────────────────────────────────────────────────────────────────
//
// Owns press/release handling for pointing-device mode keys, including
// tap-vs-hold timing, lock toggles, and overlap rules between mode keys.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

typedef uint16_t (*pd_mode_key_runtime_advance_multi_tap_t)(void *context, uint16_t keycode);
typedef void (*pd_mode_key_runtime_dispatch_tap_or_begin_multi_tap_t)(void *context, uint16_t keycode, keyrecord_t *record);
typedef void (*pd_mode_key_runtime_dispatch_action_t)(void *context, uint16_t action);

typedef struct {
    void                                                  *context;
    pd_mode_key_runtime_advance_multi_tap_t                advance_multi_tap;
    pd_mode_key_runtime_dispatch_tap_or_begin_multi_tap_t  dispatch_tap_or_begin_multi_tap;
    pd_mode_key_runtime_dispatch_action_t                  dispatch_action;
} pd_mode_key_runtime_hooks_t;

bool pd_mode_key_runtime_process(uint16_t keycode, keyrecord_t *record, uint8_t mode, uint16_t tap_hold_term, bool multi_tap_repress, const pd_mode_key_runtime_hooks_t *hooks);
