// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime-state transitions for the handled-key engine. These helpers mutate
// the shared key runtime state and emit an ordered plan of side effects, but do
// not execute those effects themselves.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../interaction/handled_key.h"
#include "effects/key_runtime_effect_queue.h"

#define KEY_RUNTIME_TRANSITION_PLAN_CAPACITY 16

typedef struct {
    KEY_RUNTIME_EFFECT_QUEUE_FIELDS(KEY_RUNTIME_TRANSITION_PLAN_CAPACITY);
} key_runtime_transition_plan_t;

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan);
void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan);

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan);
void key_runtime_transition_flush_foreign_multi_tap(uint16_t keycode, keypos_t key_pos, key_runtime_transition_plan_t *plan);
void key_runtime_transition_flush_active_keys_except(keypos_t key_pos, key_runtime_transition_plan_t *plan);
bool key_runtime_transition_has_foreign_tap_release_slot_except(keypos_t key_pos);
bool key_runtime_transition_has_any_tap_release_slot(void);
void key_runtime_transition_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_transition_plan_t *plan);
void key_runtime_transition_interrupt_active_key_on_other_press(key_runtime_transition_plan_t *plan);
bool key_runtime_transition_handled_key_press(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan);
bool key_runtime_transition_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan);
void key_runtime_transition_scan(key_runtime_transition_plan_t *plan);
