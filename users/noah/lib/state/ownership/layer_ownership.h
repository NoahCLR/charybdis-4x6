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
    layer_state_t                      oneshot_mask;
    uint8_t                            momentary_refcounts[LAYER_COUNT];
    layer_ownership_binding_snapshot_t bindings[LAYER_OWNERSHIP_BINDING_CAPACITY];
} layer_ownership_debug_snapshot_t;

bool layer_ownership_is_locked(uint8_t layer);
bool layer_ownership_is_held(uint8_t layer);
bool layer_ownership_set_lock_state(uint8_t layer, bool locked);
bool layer_ownership_toggle_lock_state(uint8_t layer);
// TO(layer): lock only this layer. Other locks are released; held momentary
// layers stay on until their keys are released. Layer 0 is the base and is
// never locked, so layer_ownership_goto(0) releases every lock.
bool layer_ownership_goto(uint8_t layer);

// OSL(layer): a tap turns the layer on for the next key press. At most one
// layer is one-shot at a time; arming another replaces it. Tapping the armed
// layer again within double_tap_ms of arming it cancels it, as QMK's quick
// double tap does; a later tap keeps it on. The next key that uses it calls
// layer_ownership_oneshot_consume() once its own press has been processed.
bool    layer_ownership_oneshot_tap(uint8_t layer, uint16_t now, uint16_t double_tap_ms);
bool    layer_ownership_oneshot_consume(void);
uint8_t layer_ownership_oneshot_layer(void);

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer);
bool layer_ownership_momentary_release(keypos_t key_pos);
void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out);
void layer_ownership_reset_for_test(void);
