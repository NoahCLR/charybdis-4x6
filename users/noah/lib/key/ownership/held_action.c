// ────────────────────────────────────────────────────────────────────────────
// Held Action Ownership
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "../../action/action_dispatch.h"
#include "../../action/action_lifecycle.h"
#include "../../state/ownership/keyboard_mod_ownership.h"
#include "../../state/runtime/runtime_context_internal.h"
#include "held_action.h"
#include "held_repeat.h"

static inline bool keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static noah_held_action_state_t *held_action_state(void) {
    return &noah_runtime_context()->held_actions;
}

static bool held_action_is_pure_modifier(uint16_t action) {
    switch (action) {
        case KC_LEFT_CTRL:
        case KC_LEFT_SHIFT:
        case KC_LEFT_ALT:
        case KC_LEFT_GUI:
        case KC_RIGHT_CTRL:
        case KC_RIGHT_SHIFT:
        case KC_RIGHT_ALT:
        case KC_RIGHT_GUI:
            return true;
        default:
            return false;
    }
}

static bool held_action_requires_per_key_dispatch(uint16_t action) {
    return noah_action_desc_requires_owned_dispatch(noah_action_describe(action));
}

static void held_action_log_binding_overflow(const char *kind, keypos_t key_pos, uint16_t action) {
#ifdef CONSOLE_ENABLE
    uprintf("Held %s binding table overflow at key (%u,%u) for action 0x%04X; board-sized ownership capacity was exhausted unexpectedly\n", kind, (unsigned int)key_pos.row, (unsigned int)key_pos.col, (unsigned int)action);
#else
    (void)kind;
    (void)key_pos;
    (void)action;
#endif
}

static int8_t held_modifier_index_for_action(uint16_t action) {
    switch (action) {
        case KC_LEFT_CTRL:
            return 0;
        case KC_LEFT_SHIFT:
            return 1;
        case KC_LEFT_ALT:
            return 2;
        case KC_LEFT_GUI:
            return 3;
        case KC_RIGHT_CTRL:
            return 4;
        case KC_RIGHT_SHIFT:
            return 5;
        case KC_RIGHT_ALT:
            return 6;
        case KC_RIGHT_GUI:
            return 7;
        default:
            return -1;
    }
}

static int16_t held_modifier_find_slot_for_key(keypos_t key_pos) {
    noah_held_action_state_t *state = held_action_state();

    for (uint16_t i = 0; i < ARRAY_SIZE(state->modifiers); i++) {
        if (state->modifiers[i].active && keypos_equal(state->modifiers[i].key_pos, key_pos)) return (int16_t)i;
    }

    return -1;
}

static int16_t held_modifier_find_free_slot(void) {
    noah_held_action_state_t *state = held_action_state();

    for (uint16_t i = 0; i < ARRAY_SIZE(state->modifiers); i++) {
        if (!state->modifiers[i].active) return (int16_t)i;
    }

    return -1;
}

static int16_t held_action_find_slot_for_key(keypos_t key_pos) {
    noah_held_action_state_t *state = held_action_state();

    for (uint16_t i = 0; i < ARRAY_SIZE(state->actions); i++) {
        if (state->actions[i].active && keypos_equal(state->actions[i].key_pos, key_pos)) return (int16_t)i;
    }

    return -1;
}

static int16_t held_action_find_free_slot(void) {
    noah_held_action_state_t *state = held_action_state();

    for (uint16_t i = 0; i < ARRAY_SIZE(state->actions); i++) {
        if (!state->actions[i].active) return (int16_t)i;
    }

    return -1;
}

static uint16_t held_action_refcount(uint16_t action) {
    noah_held_action_state_t *state = held_action_state();
    uint16_t                  count = 0;

    for (uint16_t i = 0; i < ARRAY_SIZE(state->actions); i++) {
        if (state->actions[i].active && state->actions[i].action == action) {
            count++;
        }
    }

    return count;
}

static void held_modifier_remove_slot(uint16_t slot) {
    noah_held_action_state_t *state  = held_action_state();
    uint16_t                  action = state->modifiers[slot].action;
    int8_t                    index  = held_modifier_index_for_action(action);

    state->modifiers[slot].active = false;
    state->modifiers[slot].action = KC_NO;

    if (index < 0 || state->modifier_refcounts[index] == 0) {
        return;
    }

    state->modifier_refcounts[index]--;
    if (state->modifier_refcounts[index] == 0) {
        keyboard_mod_ownership_unregister(action);
    }
}

static void held_modifier_register(keypos_t key_pos, uint16_t action) {
    noah_held_action_state_t *state = held_action_state();
    int16_t                   slot  = held_modifier_find_slot_for_key(key_pos);
    int8_t                    index = held_modifier_index_for_action(action);

    if (index < 0) {
        return;
    }

    if (slot >= 0) {
        if (state->modifiers[slot].action == action) {
            return;
        }
        held_modifier_remove_slot((uint16_t)slot);
    } else {
        slot = held_modifier_find_free_slot();
        if (slot < 0) {
            held_action_log_binding_overflow("modifier", key_pos, action);
            return;
        }
    }

    state->modifiers[slot] = (held_action_binding_snapshot_t){
        .active  = true,
        .key_pos = key_pos,
        .action  = action,
    };

    if (state->modifier_refcounts[index]++ == 0) {
        keyboard_mod_ownership_register(action);
    }
}

static bool held_action_register_owned(keypos_t key_pos, uint16_t action) {
    noah_held_action_state_t *state = held_action_state();
    int16_t                   slot  = held_action_find_slot_for_key(key_pos);

    if (slot >= 0) {
        if (state->actions[slot].action == action) {
            return true;
        }

        uint16_t old_action         = state->actions[slot].action;
        state->actions[slot].active = false;
        state->actions[slot].action = KC_NO;
        if (held_action_refcount(old_action) == 0 || held_action_requires_per_key_dispatch(old_action)) {
            noah_action_release(key_pos, old_action);
        }
    } else {
        slot = held_action_find_free_slot();
        if (slot < 0) {
            held_action_log_binding_overflow("action", key_pos, action);
            return false;
        }
    }

    bool first_binding   = held_action_refcount(action) == 0;
    state->actions[slot] = (held_action_binding_snapshot_t){
        .active  = true,
        .key_pos = key_pos,
        .action  = action,
    };

    if (first_binding || held_action_requires_per_key_dispatch(action)) {
        noah_action_press(key_pos, action);
    }

    return true;
}

bool held_modifier_release_owned_by_key(keypos_t key_pos) {
    int16_t slot = held_modifier_find_slot_for_key(key_pos);

    if (slot < 0) {
        return false;
    }

    held_modifier_remove_slot((uint16_t)slot);
    return true;
}

static bool held_action_or_modifier_release_owned_by_key(keypos_t key_pos) {
    noah_held_action_state_t *state = held_action_state();
    int16_t                   slot  = held_action_find_slot_for_key(key_pos);

    if (slot >= 0) {
        uint16_t action             = state->actions[slot].action;
        state->actions[slot].active = false;
        state->actions[slot].action = KC_NO;

        if (held_action_refcount(action) == 0 || held_action_requires_per_key_dispatch(action)) {
            noah_action_release(key_pos, action);
        }
        return true;
    }

    return held_modifier_release_owned_by_key(key_pos);
}

bool held_action_release_owned_by_key(keypos_t key_pos) {
    bool released = held_action_or_modifier_release_owned_by_key(key_pos);

    if (held_repeat_release_owned_by_key(key_pos)) {
        released = true;
    }

    return released;
}

void held_action_register(keypos_t key_pos, uint16_t action) {
    if (held_action_is_pure_modifier(action)) {
        held_modifier_register(key_pos, action);
        return;
    }

    if (held_action_register_owned(key_pos, action)) {
        return;
    }

    noah_action_press(key_pos, action);
}

void held_action_unregister(keypos_t key_pos, uint16_t action) {
    if (held_action_is_pure_modifier(action)) {
        held_modifier_release_owned_by_key(key_pos);
        return;
    }

    if (held_action_or_modifier_release_owned_by_key(key_pos)) {
        return;
    }

    noah_action_release(key_pos, action);
}

bool held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    noah_held_action_state_t *state = held_action_state();

    if (held_action_is_pure_modifier(action)) {
        return true;
    }

    int16_t slot = held_action_find_slot_for_key(key_pos);
    return slot >= 0 && state->actions[slot].action == action;
}

void held_action_debug_snapshot(held_action_debug_snapshot_t *out) {
    noah_held_action_state_t *state = held_action_state();

    if (!out) {
        return;
    }

    *out = (held_action_debug_snapshot_t){0};
    memcpy(out->modifier_refcounts, state->modifier_refcounts, sizeof(state->modifier_refcounts));

    for (uint16_t i = 0; i < ARRAY_SIZE(state->modifiers); i++) {
        out->modifiers[i] = (held_action_binding_snapshot_t){
            .active  = state->modifiers[i].active,
            .key_pos = state->modifiers[i].key_pos,
            .action  = state->modifiers[i].action,
        };
    }

    for (uint16_t i = 0; i < ARRAY_SIZE(state->actions); i++) {
        out->actions[i] = (held_action_binding_snapshot_t){
            .active  = state->actions[i].active,
            .key_pos = state->actions[i].key_pos,
            .action  = state->actions[i].action,
        };
    }
}
