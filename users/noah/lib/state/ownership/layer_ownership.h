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
    layer_state_t                      pointing_mode_layer_mask;
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

// A pointing mode its key started owns the layers its key turns on while it
// runs, beside the key holds and locks (D-F62): each stays on while any owner
// keeps it. Zero releases them; layers past the keymap are left out. The
// layers that join turn on before the ones that leave turn off.
bool          layer_ownership_set_pointing_mode_layers(layer_state_t layers);
layer_state_t layer_ownership_pointing_mode_layers(void);

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer);
bool layer_ownership_momentary_release(keypos_t key_pos);
// Marks, in a key-origin bitmap, every key whose press holds a momentary
// layer now: MO(), LM(), a held TT()/OSL(), or a behavior hold that entered one.
// A combo's output is held by its owner key, so this names those combos too.
void layer_ownership_momentary_owner_bitmap(uint8_t *out_bitmap);
void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out);
void layer_ownership_reset_for_test(void);
