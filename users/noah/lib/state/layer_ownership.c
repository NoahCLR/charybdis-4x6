// ────────────────────────────────────────────────────────────────────────────
// Layer Ownership
// ────────────────────────────────────────────────────────────────────────────

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "layer_ownership.h"

#include "noah_keymap_ids.h"
#include "runtime_trace.h"

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint8_t  layer;
} layer_momentary_binding_t;

static layer_momentary_binding_t layer_momentary_bindings[LAYER_OWNERSHIP_BINDING_CAPACITY] = {0};
static uint8_t                   layer_momentary_refcounts[LAYER_COUNT]                     = {0};
static layer_state_t             layer_locked_mask                                          = 0;

static inline layer_state_t layer_ownership_mask_for_layer(uint8_t layer) {
    return (layer_state_t)1u << layer;
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
    return layer < LAYER_COUNT && (layer_momentary_refcounts[layer] > 0 || (layer_locked_mask & layer_ownership_mask_for_layer(layer)) != 0);
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

    uint8_t layer                         = layer_momentary_bindings[slot].layer;
    layer_momentary_bindings[slot].active = false;
    layer_momentary_bindings[slot].layer  = 0;

    if (layer >= LAYER_COUNT || layer_momentary_refcounts[layer] == 0) {
        return false;
    }

    layer_momentary_refcounts[layer]--;
    return layer_ownership_apply_layer(layer);
}

bool layer_ownership_is_locked(uint8_t layer) {
    return layer < LAYER_COUNT && (layer_locked_mask & layer_ownership_mask_for_layer(layer)) != 0;
}

bool layer_ownership_set_lock_state(uint8_t layer, bool locked) {
    if (layer >= LAYER_COUNT) {
        return false;
    }

    layer_state_t layer_mask = layer_ownership_mask_for_layer(layer);
    bool          is_locked  = (layer_locked_mask & layer_mask) != 0;

    if (locked == is_locked) {
        return false;
    }

    if (locked) {
        layer_locked_mask |= layer_mask;
    } else {
        layer_locked_mask &= ~layer_mask;
    }

    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_LOCK, layer, locked ? 1u : 0u);

    bool changed = true;
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

    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_PRESS, layer, layer_ownership_trace_pack_keypos(key_pos));

    if (layer_momentary_refcounts[layer]++ == 0) {
        layer_ownership_apply_layer(layer);
    }
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    int16_t slot = layer_ownership_find_slot_for_key(key_pos);

    if (slot < 0) {
        return false;
    }

    noah_runtime_trace_emit(NOAH_TRACE_LAYER_OWNERSHIP, NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_RELEASE, layer_momentary_bindings[slot].layer, layer_ownership_trace_pack_keypos(key_pos));

    return layer_ownership_remove_slot((uint16_t)slot);
}

void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (layer_ownership_debug_snapshot_t){
        .applied_layer_state = layer_state,
        .locked_mask         = layer_locked_mask,
    };

    memcpy(out->momentary_refcounts, layer_momentary_refcounts, sizeof(layer_momentary_refcounts));

    for (uint16_t i = 0; i < ARRAY_SIZE(layer_momentary_bindings); i++) {
        out->bindings[i] = (layer_ownership_binding_snapshot_t){
            .active  = layer_momentary_bindings[i].active,
            .key_pos = layer_momentary_bindings[i].key_pos,
            .layer   = layer_momentary_bindings[i].layer,
        };
    }
}

void layer_ownership_reset_for_test(void) {
    memset(layer_momentary_bindings, 0, sizeof(layer_momentary_bindings));
    memset(layer_momentary_refcounts, 0, sizeof(layer_momentary_refcounts));
    layer_locked_mask = 0;
}
