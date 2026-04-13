// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slots
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot storage helpers for the split key runtime modules.
// This module centralizes slot lookup and lifecycle so higher-level code does
// not depend on the raw shared-state layout.
// ────────────────────────────────────────────────────────────────────────────

#include "../key_runtime_state.h"
#include "../../interaction/key_behavior_lookup.h"

#include <stddef.h>

bool key_runtime_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static bool key_runtime_slot_position_is_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static uint16_t key_runtime_slot_table_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * (uint16_t)MATRIX_COLS + (uint16_t)key_pos.col);
}

active_key_state_t *key_runtime_slot_for_position(keypos_t key_pos) {
    if (!key_runtime_slot_position_is_valid(key_pos)) {
        return NULL;
    }

    return &noah_runtime_shared_state.key.slots_by_position[key_runtime_slot_table_index(key_pos)];
}

active_key_state_t *key_runtime_slot_at(uint8_t index) {
    if (index >= KEY_RUNTIME_SLOT_TABLE_CAPACITY) {
        return NULL;
    }

    return &noah_runtime_shared_state.key.slots_by_position[index];
}

bool key_runtime_slot_idle(const active_key_state_t *slot) {
    return slot != NULL && slot->owner.keycode == KC_NO && !multi_tap_active(&slot->pending_multi_tap);
}

bool key_runtime_slot_active(const active_key_state_t *slot) {
    return slot != NULL && slot->owner.keycode != KC_NO;
}

key_runtime_slot_phase_t key_runtime_slot_phase(const active_key_state_t *slot) {
    if (!slot || slot->owner.keycode == KC_NO) {
        return KEY_RUNTIME_SLOT_PHASE_IDLE;
    }

    return slot->lifecycle.phase == KEY_RUNTIME_SLOT_PHASE_IDLE ? KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW : slot->lifecycle.phase;
}

bool key_runtime_slot_uses_implicit_hold(const active_key_state_t *slot) {
    return slot != NULL && slot->lifecycle.hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
}

bool key_runtime_slot_uses_fallback_hold(const active_key_state_t *slot) {
    return slot != NULL && slot->lifecycle.hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK;
}

bool key_runtime_slot_allows_tap_release(const active_key_state_t *slot) {
    key_runtime_slot_phase_t phase = key_runtime_slot_phase(slot);
    return phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW;
}

bool key_runtime_slot_has_pending_release_hold(const active_key_state_t *slot) {
    return key_runtime_slot_phase(slot) == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING;
}

bool key_runtime_slot_has_active_hold_tier(const active_key_state_t *slot) {
    return key_runtime_slot_phase(slot) == KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE;
}

bool key_runtime_slot_hold_is_complete(const active_key_state_t *slot) {
    return key_runtime_slot_phase(slot) == KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE;
}

bool key_runtime_slot_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos) {
    return key_runtime_slot_active(slot) && slot->owner.keycode == keycode && key_runtime_keypos_equal(slot->owner.key_pos, key_pos);
}

bool key_runtime_slot_owns_key_position(const active_key_state_t *slot, keypos_t key_pos) {
    return (key_runtime_slot_active(slot) && key_runtime_keypos_equal(slot->owner.key_pos, key_pos)) || (key_runtime_slot_has_pending_multi_tap(slot) && key_runtime_keypos_equal(slot->pending_multi_tap.key_pos, key_pos));
}

static uint8_t key_runtime_slot_preview_layer_from_binding(hold_behavior_t hold, key_runtime_slot_hold_strategy_t hold_strategy) {
    if (hold_strategy != KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT) {
        return UINT8_MAX;
    }

    if (!hold.present || hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE || !IS_QK_MOMENTARY(hold.action)) {
        return UINT8_MAX;
    }

    return QK_MOMENTARY_GET_LAYER(hold.action);
}

uint8_t key_runtime_slot_preview_layer_hint(const active_key_state_t *slot) {
    if (!slot) {
        return UINT8_MAX;
    }

    if (slot->semantic.valid) {
        return slot->semantic.preview_layer;
    }

    return key_runtime_slot_preview_layer_from_binding(slot->binding.hold, slot->lifecycle.hold_strategy);
}

bool key_runtime_slot_has_pending_multi_tap(const active_key_state_t *slot) {
    return slot != NULL && multi_tap_active(&slot->pending_multi_tap);
}

bool key_runtime_slot_pending_multi_tap_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos) {
    return slot != NULL && multi_tap_matches(&slot->pending_multi_tap, keycode, key_pos);
}

bool key_runtime_slot_pending_multi_tap_pending_hold(const active_key_state_t *slot) {
    return slot != NULL && multi_tap_pending_hold(&slot->pending_multi_tap);
}

bool key_runtime_slot_pending_multi_tap_expired(const active_key_state_t *slot) {
    return slot != NULL && multi_tap_expired(&slot->pending_multi_tap);
}

multi_tap_t *key_runtime_multi_tap_slot_at(uint8_t index) {
    active_key_state_t *slot = key_runtime_slot_at(index);

    if (!slot) {
        return NULL;
    }

    return &slot->pending_multi_tap;
}

multi_tap_t *key_runtime_multi_tap_for_slot(const active_key_state_t *slot) {
    if (!slot) {
        return NULL;
    }

    return (multi_tap_t *)&slot->pending_multi_tap;
}

active_key_state_t *key_runtime_slot_for_multi_tap(const multi_tap_t *mt) {
    if (!mt) {
        return NULL;
    }

    const active_key_state_t *base = &noah_runtime_shared_state.key.slots_by_position[0];
    const multi_tap_t        *min  = &base[0].pending_multi_tap;
    const multi_tap_t        *max  = &base[KEY_RUNTIME_SLOT_TABLE_CAPACITY - 1].pending_multi_tap;

    if (mt < min || mt > max) {
        return NULL;
    }

    return (active_key_state_t *)((char *)mt - offsetof(active_key_state_t, pending_multi_tap));
}

bool key_runtime_multi_tap_slot_active(const multi_tap_t *mt) {
    return mt != NULL && multi_tap_active(mt);
}

multi_tap_t *key_runtime_first_active_multi_tap(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_SLOT_TABLE_CAPACITY; index++) {
        multi_tap_t *mt = key_runtime_multi_tap_slot_at(index);

        if (key_runtime_multi_tap_slot_active(mt)) {
            return mt;
        }
    }

    return NULL;
}

multi_tap_t *key_runtime_find_multi_tap_by_position(keypos_t key_pos) {
    active_key_state_t *slot = key_runtime_slot_for_position(key_pos);

    if (key_runtime_slot_has_pending_multi_tap(slot) && key_runtime_keypos_equal(slot->pending_multi_tap.key_pos, key_pos)) {
        return &slot->pending_multi_tap;
    }

    return NULL;
}

void key_runtime_slot_begin_pending_multi_tap(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, uint8_t tap_repeat_count, uint16_t tap_hold_term, uint16_t multi_tap_term, bool has_more_taps) {
    if (!slot) {
        return;
    }

    multi_tap_begin(&slot->pending_multi_tap, keycode, key_pos, tap_action, tap_repeat_count, tap_hold_term, multi_tap_term, has_more_taps);
}

uint16_t key_runtime_slot_advance_pending_multi_tap(active_key_state_t *slot, uint16_t keycode) {
    if (!slot) {
        return KC_NO;
    }

    handled_key_view_t key = handled_key_lookup_tap_count(keycode, (uint8_t)(slot->pending_multi_tap.count + 1u));

    return multi_tap_advance(&slot->pending_multi_tap, key.tap_action, key.tap_repeat_count, key.has_more_taps, key.tap_resolves_on_press, key.hold, key.long_hold);
}

uint16_t key_runtime_slot_resolve_pending_multi_tap_hold(active_key_state_t *slot, uint8_t *repeat_count) {
    if (!slot) {
        if (repeat_count) {
            *repeat_count = 0;
        }
        return KC_NO;
    }

    return multi_tap_resolve_hold(&slot->pending_multi_tap, repeat_count);
}

key_runtime_slot_pending_multi_tap_flush_t key_runtime_slot_take_pending_multi_tap_flush(active_key_state_t *slot) {
    key_runtime_slot_pending_multi_tap_flush_t flush = {0};
    multi_tap_t                               *mt    = key_runtime_multi_tap_for_slot(slot);

    if (!key_runtime_slot_has_pending_multi_tap(slot) || !mt) {
        return flush;
    }

    flush.handled = true;
    flush.mods    = delayed_action_mods_from_multi_tap(mt);

    flush.action       = mt->tap_repeat_count > 0 ? mt->tap_action : mt->single_action;
    flush.repeat_count = mt->tap_repeat_count > 0 ? mt->tap_repeat_count : mt->count;
    key_runtime_slot_reset_pending_multi_tap(slot);
    return flush;
}

void key_runtime_slot_reset_pending_multi_tap(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    multi_tap_reset(&slot->pending_multi_tap);
}

void key_runtime_slot_reset(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    *slot = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
}

void key_runtime_slot_set_release_hold_pending(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    slot->lifecycle.phase = KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING;
}

void key_runtime_slot_commit_hold_phase(active_key_state_t *slot, bool completes_hold) {
    if (!slot) {
        return;
    }

    slot->lifecycle.phase = completes_hold ? KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE : KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE;
}

void key_runtime_slot_apply_handled_metadata(active_key_state_t *slot, handled_key_view_t key) {
    if (!slot) {
        return;
    }

    slot->semantic = (key_runtime_slot_semantic_state_t){
        .valid              = true,
        .has_multi_tap      = handled_key_has_multi_tap(key),
        .is_momentary_layer = handled_key_is_momentary_layer(key),
        .is_layer_tap       = handled_key_is_layer_tap(key),
        .layer              = handled_key_layer(key),
        .preview_layer      = key_runtime_slot_preview_layer_from_binding(slot->binding.hold, slot->lifecycle.hold_strategy),
        .pd_mode            = handled_key_pd_mode(key),
    };
}

void key_runtime_slot_track(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_phase_t phase, key_runtime_slot_hold_strategy_t hold_strategy) {
    if (!slot) {
        return;
    }

    multi_tap_t pending_multi_tap = slot->pending_multi_tap;

    *slot = (active_key_state_t){
        .timer         = timer_read(),
        .owner.keycode = keycode,
        .owner.key_pos = key_pos,
        .lifecycle =
            {
                .phase               = phase,
                .held_action_keycode = KC_NO,
                .hold_strategy       = hold_strategy,
            },
        .binding =
            {
                .tap_action = tap_action,
                .hold       = hold,
                .long_hold  = long_hold,
            },
        .timing =
            {
                .tap_hold_term    = tap_hold_term,
                .longer_hold_term = longer_hold_term,
                .multi_tap_term   = multi_tap_term,
            },
        .pending_multi_tap = pending_multi_tap,
    };
}
