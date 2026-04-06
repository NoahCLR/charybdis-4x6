// ────────────────────────────────────────────────────────────────────────────
// Layer Ownership
// ────────────────────────────────────────────────────────────────────────────

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "layer_ownership.h"

#include "noah_keymap.h"

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint8_t  layer;
} layer_momentary_binding_t;

#ifndef LAYER_OWNERSHIP_BINDING_MAX_CAPACITY
#    define LAYER_OWNERSHIP_BINDING_MAX_CAPACITY 16u
#endif

#define LAYER_OWNERSHIP_BINDING_CAPACITY ((uint16_t)(((MATRIX_ROWS * MATRIX_COLS) < LAYER_OWNERSHIP_BINDING_MAX_CAPACITY) ? (MATRIX_ROWS * MATRIX_COLS) : LAYER_OWNERSHIP_BINDING_MAX_CAPACITY))

static layer_momentary_binding_t layer_momentary_bindings[LAYER_OWNERSHIP_BINDING_CAPACITY] = {0};
static uint8_t                   layer_momentary_refcounts[LAYER_COUNT]                      = {0};
static uint8_t                   layer_locked_state                                           = UINT8_MAX;

static inline bool layer_ownership_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static void layer_ownership_log_binding_overflow(keypos_t key_pos, uint8_t layer) {
#ifdef CONSOLE_ENABLE
    uprintf("Layer ownership table overflow at key (%u,%u) for layer %u; bounded capacity was exhausted unexpectedly\n", (unsigned int)key_pos.row, (unsigned int)key_pos.col, (unsigned int)layer);
#else
    (void)key_pos;
    (void)layer;
#endif
}

static int16_t layer_ownership_find_slot_for_key(keypos_t key_pos) {
    for (uint16_t i = 0; i < ARRAY_SIZE(layer_momentary_bindings); i++) {
        if (layer_momentary_bindings[i].active && layer_ownership_keypos_equal(layer_momentary_bindings[i].key_pos, key_pos)) {
            return (int16_t)i;
        }
    }

    return -1;
}

static int16_t layer_ownership_find_free_slot(void) {
    for (uint16_t i = 0; i < ARRAY_SIZE(layer_momentary_bindings); i++) {
        if (!layer_momentary_bindings[i].active) {
            return (int16_t)i;
        }
    }

    return -1;
}

static bool layer_ownership_should_be_active(uint8_t layer) {
    return layer < LAYER_COUNT && (layer_momentary_refcounts[layer] > 0 || layer_locked_state == layer);
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
    if (slot >= ARRAY_SIZE(layer_momentary_bindings) || !layer_momentary_bindings[slot].active) {
        return false;
    }

    uint8_t layer = layer_momentary_bindings[slot].layer;
    layer_momentary_bindings[slot].active = false;
    layer_momentary_bindings[slot].layer  = 0;

    if (layer >= LAYER_COUNT || layer_momentary_refcounts[layer] == 0) {
        return false;
    }

    layer_momentary_refcounts[layer]--;
    return layer_ownership_apply_layer(layer);
}

bool layer_ownership_is_locked(uint8_t layer) {
    return layer_locked_state == layer;
}

bool layer_ownership_set_lock_state(uint8_t layer, bool locked) {
    if (layer >= LAYER_COUNT) {
        return false;
    }

    if (!locked) {
        if (layer_locked_state != layer) {
            return false;
        }

        layer_locked_state = UINT8_MAX;
        layer_ownership_apply_layer(layer);
        return true;
    }

    bool changed = false;

    if (layer_locked_state < LAYER_COUNT && layer_locked_state != layer) {
        uint8_t old_locked_layer = layer_locked_state;
        layer_locked_state = UINT8_MAX;
        changed |= layer_ownership_apply_layer(old_locked_layer);
    }

    if (layer_locked_state != layer) {
        layer_locked_state = layer;
        changed            = true;
    }

    changed |= layer_ownership_apply_layer(layer);
    return changed;
}

bool layer_ownership_toggle_lock_state(uint8_t layer) {
    return layer_ownership_set_lock_state(layer, !layer_ownership_is_locked(layer));
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    if (layer >= LAYER_COUNT) {
        return;
    }

    int16_t slot = layer_ownership_find_slot_for_key(key_pos);
    if (slot >= 0) {
        if (layer_momentary_bindings[slot].layer == layer) {
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

    layer_momentary_bindings[slot] = (layer_momentary_binding_t){
        .active  = true,
        .key_pos = key_pos,
        .layer   = layer,
    };

    if (layer_momentary_refcounts[layer]++ == 0) {
        layer_ownership_apply_layer(layer);
    }
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    int16_t slot = layer_ownership_find_slot_for_key(key_pos);

    if (slot < 0) {
        return false;
    }

    return layer_ownership_remove_slot((uint16_t)slot);
}
