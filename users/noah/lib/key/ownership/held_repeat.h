// ────────────────────────────────────────────────────────────────────────────
// Held Repeat Scheduling
// ────────────────────────────────────────────────────────────────────────────
//
// Tracks time-based repeat bindings and their pointer-layer anchor state so
// authored repeat holds can survive active-key flushes and stop on release.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#define HELD_REPEAT_BINDING_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
    uint16_t interval_ms;
    uint16_t last_fire_time;
} held_repeat_binding_snapshot_t;

typedef struct {
    held_repeat_binding_snapshot_t bindings[HELD_REPEAT_BINDING_CAPACITY];
} held_repeat_debug_snapshot_t;

void held_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz);
void held_repeat_tick(void);
bool held_repeat_release_owned_by_key(keypos_t key_pos);
void held_repeat_debug_snapshot(held_repeat_debug_snapshot_t *out);
void held_repeat_reset_for_test(void);
