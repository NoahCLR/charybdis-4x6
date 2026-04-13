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
#include "held_action.h"
#include "held_repeat.h"

// A held pure modifier is owned by the physical switch that started it, not by
// whichever custom key the tap/hold FSM is currently resolving.
typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
} held_modifier_binding_t;

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
} held_action_binding_t;

// Board-sized ownership tables keep per-key refcount behavior intact even on
// unusually large chords. A free-slot miss now indicates state corruption or a
// broken matrix definition rather than a routine rollover limit.
static held_modifier_binding_t held_modifiers[HELD_ACTION_BINDING_CAPACITY] = {0};
static uint8_t                 held_modifier_refcounts[8]                   = {0};
static held_action_binding_t   held_actions[HELD_ACTION_BINDING_CAPACITY]   = {0};

static inline bool keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
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
    for (uint16_t i = 0; i < ARRAY_SIZE(held_modifiers); i++) {
        if (held_modifiers[i].active && keypos_equal(held_modifiers[i].key_pos, key_pos)) return (int16_t)i;
    }

    return -1;
}

static int16_t held_modifier_find_free_slot(void) {
    for (uint16_t i = 0; i < ARRAY_SIZE(held_modifiers); i++) {
        if (!held_modifiers[i].active) return (int16_t)i;
    }

    return -1;
}

static int16_t held_action_find_slot_for_key(keypos_t key_pos) {
    for (uint16_t i = 0; i < ARRAY_SIZE(held_actions); i++) {
        if (held_actions[i].active && keypos_equal(held_actions[i].key_pos, key_pos)) return (int16_t)i;
    }

    return -1;
}

static int16_t held_action_find_free_slot(void) {
    for (uint16_t i = 0; i < ARRAY_SIZE(held_actions); i++) {
        if (!held_actions[i].active) return (int16_t)i;
    }

    return -1;
}

static uint16_t held_action_refcount(uint16_t action) {
    uint16_t count = 0;

    for (uint16_t i = 0; i < ARRAY_SIZE(held_actions); i++) {
        if (held_actions[i].active && held_actions[i].action == action) {
            count++;
        }
    }

    return count;
}

static void held_modifier_remove_slot(uint16_t slot) {
    uint16_t action = held_modifiers[slot].action;
    int8_t   index  = held_modifier_index_for_action(action);

    held_modifiers[slot].active = false;
    held_modifiers[slot].action = KC_NO;

    if (index < 0 || held_modifier_refcounts[index] == 0) {
        return;
    }

    held_modifier_refcounts[index]--;
    if (held_modifier_refcounts[index] == 0) {
        keyboard_mod_ownership_unregister(action);
    }
}

static void held_modifier_register(keypos_t key_pos, uint16_t action) {
    int16_t slot  = held_modifier_find_slot_for_key(key_pos);
    int8_t  index = held_modifier_index_for_action(action);

    if (index < 0) {
        return;
    }

    if (slot >= 0) {
        if (held_modifiers[slot].action == action) {
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

    held_modifiers[slot] = (held_modifier_binding_t){
        .active  = true,
        .key_pos = key_pos,
        .action  = action,
    };

    if (held_modifier_refcounts[index]++ == 0) {
        keyboard_mod_ownership_register(action);
    }
}

static bool held_action_register_owned(keypos_t key_pos, uint16_t action) {
    int16_t slot = held_action_find_slot_for_key(key_pos);

    if (slot >= 0) {
        if (held_actions[slot].action == action) {
            return true;
        }

        uint16_t old_action       = held_actions[slot].action;
        held_actions[slot].active = false;
        held_actions[slot].action = KC_NO;
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

    bool first_binding = held_action_refcount(action) == 0;
    held_actions[slot] = (held_action_binding_t){
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
    int16_t slot = held_action_find_slot_for_key(key_pos);

    if (slot >= 0) {
        uint16_t action           = held_actions[slot].action;
        held_actions[slot].active = false;
        held_actions[slot].action = KC_NO;

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
    if (held_action_is_pure_modifier(action)) {
        return true;
    }

    int16_t slot = held_action_find_slot_for_key(key_pos);
    return slot >= 0 && held_actions[slot].action == action;
}

void held_action_debug_snapshot(held_action_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (held_action_debug_snapshot_t){0};
    memcpy(out->modifier_refcounts, held_modifier_refcounts, sizeof(held_modifier_refcounts));

    for (uint16_t i = 0; i < ARRAY_SIZE(held_modifiers); i++) {
        out->modifiers[i] = (held_action_binding_snapshot_t){
            .active  = held_modifiers[i].active,
            .key_pos = held_modifiers[i].key_pos,
            .action  = held_modifiers[i].action,
        };
    }

    for (uint16_t i = 0; i < ARRAY_SIZE(held_actions); i++) {
        out->actions[i] = (held_action_binding_snapshot_t){
            .active  = held_actions[i].active,
            .key_pos = held_actions[i].key_pos,
            .action  = held_actions[i].action,
        };
    }
}

void held_action_reset_for_test(void) {
    memset(held_modifiers, 0, sizeof(held_modifiers));
    memset(held_modifier_refcounts, 0, sizeof(held_modifier_refcounts));
    memset(held_actions, 0, sizeof(held_actions));
}
