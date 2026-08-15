// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Process API
// ────────────────────────────────────────────────────────────────────────────
//
// Private orchestration surface shared by the split key-runtime process
// modules.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../behavior/handled_key.h"

bool key_runtime_preflight_record(uint16_t keycode, keyrecord_t *record, bool press_is_handled);
__attribute__((noinline)) bool key_runtime_process_handled_key_press(uint16_t keycode, keyrecord_t *record);
__attribute__((noinline)) bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution);
bool key_runtime_process_direct_action_key(uint16_t keycode, keyrecord_t *record);
