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
// A shortcut's layers as one layer key (D-F26). Lock: every layer of the set
// locks, or, when all are locked already, every one releases. Move to: TO()
// of the set, which locks it alone. The base layer is never locked, and
// layers past the keymap are left out.
bool layer_ownership_toggle_lock_set(layer_state_t layers);
bool layer_ownership_goto_set(layer_state_t layers);

// OSL(layer): a tap turns the layer on for the next key press; a shortcut's
// One-shot does the same for its set of layers together (D-F26). One set is
// one-shot at a time; arming another replaces it, the new layers on before the
// old go. Tapping the armed set again within double_tap_ms of arming it cancels
// it, as QMK's quick double tap does; a later tap keeps it on. The next key
// that uses it calls layer_ownership_oneshot_consume() once its own press has
// been processed. layer_ownership_oneshot_layer() is the lowest armed layer,
// UINT8_MAX for none.
bool          layer_ownership_oneshot_tap(uint8_t layer, uint16_t now, uint16_t double_tap_ms);
bool          layer_ownership_oneshot_tap_set(layer_state_t layers, uint16_t now, uint16_t double_tap_ms);
bool          layer_ownership_oneshot_consume(void);
uint8_t       layer_ownership_oneshot_layer(void);
layer_state_t layer_ownership_oneshot_layers(void);

// A pointing mode its key started owns the layers its key turns on while it
// runs, beside the key holds and locks (D-F62): each stays on while any owner
// keeps it. Zero releases them; layers past the keymap are left out. The
// layers that join turn on before the ones that leave turn off.
bool          layer_ownership_set_pointing_mode_layers(layer_state_t layers);
layer_state_t layer_ownership_pointing_mode_layers(void);

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer);
bool layer_ownership_momentary_release(keypos_t key_pos);
// A custom key's shortcut owns its layers while it is held (D-F26), one owner
// per holding key beside that key's own layer hold, the locks and the
// pointing mode: each layer stays on while any owner keeps it. A press records
// the layers it took, so its release lets go of exactly those; pressing again
// moves the key to the new layers, the new ones on first. Layers past the
// keymap are left out, and no layers binds nothing. The release reports
// whether the key held any.
void layer_ownership_shortcut_press(keypos_t key_pos, layer_state_t layers);
bool layer_ownership_shortcut_release(keypos_t key_pos);
// A Tap-toggle or One-shot custom key's own momentary hold of its shortcut's
// layers, from its press to its release, as TT() and OSL() hold theirs: a
// second owner at the key, beside a custom key its behaviour holds, so neither
// lets go of the other's layers.
void layer_ownership_shortcut_momentary_press(keypos_t key_pos, layer_state_t layers);
bool layer_ownership_shortcut_momentary_release(keypos_t key_pos);
// A tapped shortcut's layers, on from begin to end around the tap it sends.
void layer_ownership_shortcut_tap_begin(layer_state_t layers);
void layer_ownership_shortcut_tap_end(layer_state_t layers);
// Marks, in a key-origin bitmap, every key whose press holds a momentary
// layer now: MO(), LM(), a held TT()/OSL(), a behavior hold that entered one,
// or a held shortcut with layers.
// A combo's output is held by its owner key, so this names those combos too.
void layer_ownership_momentary_owner_bitmap(uint8_t *out_bitmap);
void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out);
void layer_ownership_reset_for_test(void);
