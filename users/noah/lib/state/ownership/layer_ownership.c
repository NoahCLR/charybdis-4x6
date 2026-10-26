// ────────────────────────────────────────────────────────────────────────────
// Layer Ownership
// ────────────────────────────────────────────────────────────────────────────

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "layer_ownership.h"

#include "noah_keymap_ids.h"
#include "../../key/runtime/reducer/ownership_state.h"
#include "../../key/runtime/slot/origin_registry.h"
#include "../diagnostics/runtime_trace.h"
#include "../shared/runtime_context_internal.h"

static inline layer_state_t layer_ownership_mask_for_layer(uint8_t layer) {
    return (layer_state_t)1u << layer;
}

static noah_layer_ownership_state_t *layer_ownership_state(void) {
    return &noah_runtime_context()->layer_ownership;
}

static inline bool layer_ownership_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static uint16_t layer_ownership_trace_pack_keypos(keypos_t key_pos) {
    return (uint16_t)(((uint16_t)key_pos.row << 8) | key_pos.col);
}

static void layer_ownership_log_binding_overflow(keypos_t key_pos, uint8_t layer) {
    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_OVERFLOW, layer, layer_ownership_trace_pack_keypos(key_pos));
#ifdef CONSOLE_ENABLE
    uprintf("Layer ownership table overflow at key (%u,%u) for layer %u; bounded capacity was exhausted unexpectedly\n", (unsigned int)key_pos.row, (unsigned int)key_pos.col, (unsigned int)layer);
#else
    (void)key_pos;
    (void)layer;
#endif
}

static int16_t layer_ownership_find_slot_for_key(keypos_t key_pos) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    for (uint16_t i = 0; i < ARRAY_SIZE(state->bindings); i++) {
        if (state->bindings[i].active && layer_ownership_keypos_equal(state->bindings[i].key_pos, key_pos)) {
            return (int16_t)i;
        }
    }

    return -1;
}

static int16_t layer_ownership_find_free_slot(void) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    for (uint16_t i = 0; i < ARRAY_SIZE(state->bindings); i++) {
        if (!state->bindings[i].active) {
            return (int16_t)i;
        }
    }

    return -1;
}

static bool layer_ownership_should_be_active(uint8_t layer) {
    noah_layer_ownership_state_t *state = layer_ownership_state();
    return layer < LAYER_COUNT && (state->momentary_refcounts[layer] > 0 || ((state->locked_mask | state->oneshot_mask | state->pointing_mode_layer_mask) & layer_ownership_mask_for_layer(layer)) != 0);
}

static bool layer_ownership_apply_layer(uint8_t layer) {
    if (layer >= LAYER_COUNT) {
        return false;
    }

    bool should_be_active = layer_ownership_should_be_active(layer);
    bool is_active        = layer_state_cmp(layer_state, layer);

    if (should_be_active && !is_active) {
        layer_on(layer);
        return true;
    }

    if (!should_be_active && is_active) {
        layer_off(layer);
        return true;
    }

    return false;
}

static bool layer_ownership_remove_slot(uint16_t slot) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    if (slot >= ARRAY_SIZE(state->bindings) || !state->bindings[slot].active) {
        return false;
    }

    uint8_t layer                = state->bindings[slot].layer;
    state->bindings[slot].active = false;
    state->bindings[slot].layer  = 0;

    if (layer >= LAYER_COUNT || state->momentary_refcounts[layer] == 0) {
        return false;
    }

    state->momentary_refcounts[layer]--;
    return layer_ownership_apply_layer(layer);
}

bool layer_ownership_is_locked(uint8_t layer) {
    noah_layer_ownership_state_t *state = layer_ownership_state();
    return layer < LAYER_COUNT && (state->locked_mask & layer_ownership_mask_for_layer(layer)) != 0;
}

// Whether a key holds this layer (MO(), TT(), OSL(), LM(), a behaviour's
// layer hold or a shortcut's layers), as opposed to a lock.
bool layer_ownership_is_held(uint8_t layer) {
    return layer < LAYER_COUNT && layer_ownership_state()->momentary_refcounts[layer] > 0u;
}

bool layer_ownership_set_lock_state(uint8_t layer, bool locked) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    if (layer >= LAYER_COUNT) {
        return false;
    }

    layer_state_t layer_mask = layer_ownership_mask_for_layer(layer);
    bool          is_locked  = (state->locked_mask & layer_mask) != 0;

    if (locked == is_locked) {
        return false;
    }

    if (locked) {
        state->locked_mask |= layer_mask;
    } else {
        state->locked_mask &= ~layer_mask;
    }

    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_LOCK, layer, locked ? 1u : 0u);

    bool changed = true;
    changed |= layer_ownership_apply_layer(layer);
    key_runtime_core_layer_lock_set(layer, locked);
    return changed;
}

bool layer_ownership_toggle_lock_state(uint8_t layer) {
    return layer_ownership_set_lock_state(layer, !layer_ownership_is_locked(layer));
}

// Layers past the keymap are left out.
static layer_state_t layer_ownership_keymap_layers(layer_state_t layers) {
    return LAYER_COUNT >= sizeof(layer_state_t) * 8u ? layers : (layer_state_t)(layers & (((layer_state_t)1u << LAYER_COUNT) - 1u));
}

// The layers of a set a lock can hold: the base layer is never locked.
static layer_state_t layer_ownership_lockable_layers(layer_state_t layers) {
    return (layer_state_t)(layer_ownership_keymap_layers(layers) & ~layer_ownership_mask_for_layer(0u));
}

bool layer_ownership_toggle_lock_set(layer_state_t layers) {
    noah_layer_ownership_state_t *state   = layer_ownership_state();
    bool                          changed = false;

    layers = layer_ownership_lockable_layers(layers);
    if (!layers) {
        return false;
    }

    bool lock = (state->locked_mask & layers) != layers;
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if (layers & layer_ownership_mask_for_layer(layer)) {
            changed |= layer_ownership_set_lock_state(layer, lock);
        }
    }
    return changed;
}

bool layer_ownership_goto_set(layer_state_t layers) {
    bool changed = false;

    // The targets lock before the others release, so the keyboard never
    // passes through the base layer on its way to them.
    layers = layer_ownership_lockable_layers(layers);
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if (layers & layer_ownership_mask_for_layer(layer)) {
            changed |= layer_ownership_set_lock_state(layer, true);
        }
    }
    for (uint8_t other = 0u; other < LAYER_COUNT; other++) {
        if (!(layers & layer_ownership_mask_for_layer(other))) {
            changed |= layer_ownership_set_lock_state(other, false);
        }
    }

    return layer_ownership_oneshot_consume() || changed;
}

bool layer_ownership_goto(uint8_t layer) {
    return layer < LAYER_COUNT && layer_ownership_goto_set(layer_ownership_mask_for_layer(layer));
}

uint8_t layer_ownership_oneshot_layer(void) {
    layer_state_t mask = layer_ownership_state()->oneshot_mask;

    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if (mask & layer_ownership_mask_for_layer(layer)) {
            return layer;
        }
    }

    return UINT8_MAX;
}

layer_state_t layer_ownership_oneshot_layers(void) {
    return layer_ownership_state()->oneshot_mask;
}

// Arms `armed` in place of what was armed: the layers that join turn on
// before the ones that leave turn off.
static bool layer_ownership_oneshot_arm(layer_state_t armed) {
    noah_layer_ownership_state_t *state    = layer_ownership_state();
    layer_state_t                 previous = state->oneshot_mask;

    if (armed == previous) {
        return false;
    }

    state->oneshot_mask = armed;
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if ((armed & ~previous) & layer_ownership_mask_for_layer(layer)) {
            noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_ONESHOT, layer, 1u);
            layer_ownership_apply_layer(layer);
            key_runtime_core_layer_oneshot_set(layer, true);
        }
    }
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if ((previous & ~armed) & layer_ownership_mask_for_layer(layer)) {
            noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_ONESHOT, layer, 0u);
            layer_ownership_apply_layer(layer);
            key_runtime_core_layer_oneshot_set(layer, false);
        }
    }
    return true;
}

bool layer_ownership_oneshot_tap_set(layer_state_t layers, uint16_t now, uint16_t double_tap_ms) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    layers = layer_ownership_keymap_layers(layers);
    if (!layers) {
        return false;
    }
    if (state->oneshot_mask == layers) {
        if ((uint16_t)(now - state->oneshot_armed_at) < double_tap_ms) {
            return layer_ownership_oneshot_arm(0u);
        }
        state->oneshot_armed_at = now;
        return false;
    }

    state->oneshot_armed_at = now;
    return layer_ownership_oneshot_arm(layers);
}

bool layer_ownership_oneshot_tap(uint8_t layer, uint16_t now, uint16_t double_tap_ms) {
    return layer < LAYER_COUNT && layer_ownership_oneshot_tap_set(layer_ownership_mask_for_layer(layer), now, double_tap_ms);
}

bool layer_ownership_oneshot_consume(void) {
    return layer_ownership_oneshot_arm(0u);
}

layer_state_t layer_ownership_pointing_mode_layers(void) {
    return layer_ownership_state()->pointing_mode_layer_mask;
}

bool layer_ownership_set_pointing_mode_layers(layer_state_t layers) {
    noah_layer_ownership_state_t *state    = layer_ownership_state();
    layer_state_t                 previous = state->pointing_mode_layer_mask;

    layers = layer_ownership_keymap_layers(layers);
    if (layers == previous) {
        return false;
    }

    state->pointing_mode_layer_mask = layers;
    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_POINTING_MODE, layers, previous);
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if ((layers & ~previous) & layer_ownership_mask_for_layer(layer)) {
            layer_ownership_apply_layer(layer);
        }
    }
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if ((previous & ~layers) & layer_ownership_mask_for_layer(layer)) {
            layer_ownership_apply_layer(layer);
        }
    }
    return true;
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    if (layer >= LAYER_COUNT) {
        return;
    }

    int16_t slot = layer_ownership_find_slot_for_key(key_pos);
    if (slot >= 0) {
        if (state->bindings[slot].layer == layer) {
            layer_ownership_apply_layer(layer);
            return;
        }

        layer_ownership_remove_slot((uint16_t)slot);
    } else {
        slot = layer_ownership_find_free_slot();
        if (slot < 0) {
            layer_ownership_log_binding_overflow(key_pos, layer);
            return;
        }
    }

    state->bindings[slot] = (layer_ownership_binding_snapshot_t){
        .active  = true,
        .key_pos = key_pos,
        .layer   = layer,
    };

    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_PRESS, layer, layer_ownership_trace_pack_keypos(key_pos));

    if (state->momentary_refcounts[layer]++ == 0) {
        layer_ownership_apply_layer(layer);
    }
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    noah_layer_ownership_state_t *state = layer_ownership_state();
    int16_t                       slot  = layer_ownership_find_slot_for_key(key_pos);

    if (slot < 0) {
        return false;
    }

    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_RELEASE, state->bindings[slot].layer, layer_ownership_trace_pack_keypos(key_pos));

    return layer_ownership_remove_slot((uint16_t)slot);
}

// Each layer of `layers` gains or loses one key owner; a layer whose first
// owner arrives turns on, one whose last leaves turns off.
static void layer_ownership_shortcut_acquire(layer_state_t layers) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if ((layers & layer_ownership_mask_for_layer(layer)) && state->momentary_refcounts[layer]++ == 0u) {
            layer_ownership_apply_layer(layer);
        }
    }
}

static bool layer_ownership_shortcut_drop(layer_state_t layers) {
    noah_layer_ownership_state_t *state   = layer_ownership_state();
    bool                          changed = false;

    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if ((layers & layer_ownership_mask_for_layer(layer)) && state->momentary_refcounts[layer] > 0u && --state->momentary_refcounts[layer] == 0u) {
            changed |= layer_ownership_apply_layer(layer);
        }
    }
    return changed;
}

static int16_t layer_ownership_find_shortcut_slot(keypos_t key_pos) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    for (uint16_t i = 0; i < ARRAY_SIZE(state->shortcut_bindings); i++) {
        if ((state->shortcut_bindings[i].held | state->shortcut_bindings[i].momentary) && layer_ownership_keypos_equal(state->shortcut_bindings[i].key_pos, key_pos)) {
            return (int16_t)i;
        }
    }
    return -1;
}

// The binding's layers for one owner.
static layer_state_t *layer_ownership_shortcut_owned(layer_ownership_shortcut_binding_t *binding, bool momentary) {
    return momentary ? &binding->momentary : &binding->held;
}

static void layer_ownership_shortcut_bind(keypos_t key_pos, bool momentary, layer_state_t layers) {
    noah_layer_ownership_state_t *state = layer_ownership_state();
    int16_t                       slot  = layer_ownership_find_shortcut_slot(key_pos);
    layer_state_t                 previous;

    layers = layer_ownership_keymap_layers(layers);
    if (slot < 0) {
        if (!layers) {
            return;
        }
        for (uint16_t i = 0; i < ARRAY_SIZE(state->shortcut_bindings) && slot < 0; i++) {
            if (!(state->shortcut_bindings[i].held | state->shortcut_bindings[i].momentary)) {
                slot = (int16_t)i;
            }
        }
        if (slot < 0) {
            layer_ownership_log_binding_overflow(key_pos, (uint8_t)__builtin_ctz(layers));
            return;
        }
        state->shortcut_bindings[slot] = (layer_ownership_shortcut_binding_t){.key_pos = key_pos};
    }

    layer_state_t *owned = layer_ownership_shortcut_owned(&state->shortcut_bindings[slot], momentary);
    previous             = *owned;
    *owned               = layers;
    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_SHORTCUT, layers, layer_ownership_trace_pack_keypos(key_pos));
    layer_ownership_shortcut_acquire(layers);
    (void)layer_ownership_shortcut_drop(previous);
}

static bool layer_ownership_shortcut_unbind(keypos_t key_pos, bool momentary) {
    noah_layer_ownership_state_t *state = layer_ownership_state();
    int16_t                       slot  = layer_ownership_find_shortcut_slot(key_pos);

    if (slot < 0) {
        return false;
    }

    layer_state_t *owned  = layer_ownership_shortcut_owned(&state->shortcut_bindings[slot], momentary);
    layer_state_t  layers = *owned;
    if (!layers) {
        return false;
    }
    *owned = 0u;
    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_SHORTCUT, 0u, layer_ownership_trace_pack_keypos(key_pos));
    (void)layer_ownership_shortcut_drop(layers);
    return true;
}

void layer_ownership_shortcut_press(keypos_t key_pos, layer_state_t layers) {
    layer_ownership_shortcut_bind(key_pos, false, layers);
}

bool layer_ownership_shortcut_release(keypos_t key_pos) {
    return layer_ownership_shortcut_unbind(key_pos, false);
}

void layer_ownership_shortcut_momentary_press(keypos_t key_pos, layer_state_t layers) {
    layer_ownership_shortcut_bind(key_pos, true, layers);
}

bool layer_ownership_shortcut_momentary_release(keypos_t key_pos) {
    return layer_ownership_shortcut_unbind(key_pos, true);
}

void layer_ownership_shortcut_tap_begin(layer_state_t layers) {
    layer_ownership_shortcut_acquire(layer_ownership_keymap_layers(layers));
}

void layer_ownership_shortcut_tap_end(layer_state_t layers) {
    (void)layer_ownership_shortcut_drop(layer_ownership_keymap_layers(layers));
}

void layer_ownership_momentary_owner_bitmap(uint8_t *out_bitmap) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    if (!out_bitmap) {
        return;
    }

    key_origin_bitmap_clear(out_bitmap);
    for (uint16_t i = 0; i < ARRAY_SIZE(state->bindings); i++) {
        if (state->bindings[i].active) {
            key_origin_bitmap_add_keypos(out_bitmap, state->bindings[i].key_pos);
        }
    }
    for (uint16_t i = 0; i < ARRAY_SIZE(state->shortcut_bindings); i++) {
        if (state->shortcut_bindings[i].held | state->shortcut_bindings[i].momentary) {
            key_origin_bitmap_add_keypos(out_bitmap, state->shortcut_bindings[i].key_pos);
        }
    }
}

void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out) {
    noah_layer_ownership_state_t *state = layer_ownership_state();

    if (!out) {
        return;
    }

    *out = (layer_ownership_debug_snapshot_t){
        .applied_layer_state = layer_state,
        .locked_mask         = state->locked_mask,
        .oneshot_mask        = state->oneshot_mask,
        .pointing_mode_layer_mask  = state->pointing_mode_layer_mask,
    };

    memcpy(out->momentary_refcounts, state->momentary_refcounts, sizeof(state->momentary_refcounts));

    for (uint16_t i = 0; i < ARRAY_SIZE(state->bindings); i++) {
        out->bindings[i] = (layer_ownership_binding_snapshot_t){
            .active  = state->bindings[i].active,
            .key_pos = state->bindings[i].key_pos,
            .layer   = state->bindings[i].layer,
        };
    }
}

void layer_ownership_reset_for_test(void) {
    memset(layer_ownership_state(), 0, sizeof(*layer_ownership_state()));
}
