// ────────────────────────────────────────────────────────────────────────────
// Layer Ownership
// ────────────────────────────────────────────────────────────────────────────
//
// Tracks momentary and locked layer owners so overlapping layer holds and
// layer locks can coexist without raw layer_on()/layer_off() races.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#define LAYER_OWNERSHIP_BINDING_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint8_t  layer;
} layer_ownership_binding_snapshot_t;

typedef struct {
    layer_state_t                      applied_layer_state;
    layer_state_t                      locked_mask;
    uint8_t                            momentary_refcounts[LAYER_COUNT];
    layer_ownership_binding_snapshot_t bindings[LAYER_OWNERSHIP_BINDING_CAPACITY];
} layer_ownership_debug_snapshot_t;

bool layer_ownership_is_locked(uint8_t layer);
bool layer_ownership_set_lock_state(uint8_t layer, bool locked);
bool layer_ownership_toggle_lock_state(uint8_t layer);

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer);
bool layer_ownership_momentary_release(keypos_t key_pos);
void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out);
void layer_ownership_reset_for_test(void);
