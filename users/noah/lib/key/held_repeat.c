// ────────────────────────────────────────────────────────────────────────────
// Held Repeat Scheduling
// ────────────────────────────────────────────────────────────────────────────

#include "held_repeat.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "../action/action_dispatch.h"
#include "../pointing/pointer_layer_policy.h"
#include "key_behavior.h"

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
    uint16_t interval_ms;
    uint16_t last_fire_time;
} held_repeat_binding_t;

// Board-sized ownership tables keep per-key refcount behavior intact even on
// unusually large chords. A free-slot miss now indicates state corruption or a
// broken matrix definition rather than a routine rollover limit.
static held_repeat_binding_t held_repeats[HELD_REPEAT_BINDING_CAPACITY] = {0};

static inline bool keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static void held_repeat_log_binding_overflow(keypos_t key_pos, uint16_t action) {
#ifdef CONSOLE_ENABLE
    uprintf("Held repeat binding table overflow at key (%u,%u) for action 0x%04X; board-sized ownership capacity was exhausted unexpectedly\n", (unsigned int)key_pos.row, (unsigned int)key_pos.col, (unsigned int)action);
#else
    (void)key_pos;
    (void)action;
#endif
}

static int16_t held_repeat_find_slot_for_key(keypos_t key_pos) {
    for (uint16_t i = 0; i < ARRAY_SIZE(held_repeats); i++) {
        if (held_repeats[i].active && keypos_equal(held_repeats[i].key_pos, key_pos)) return (int16_t)i;
    }

    return -1;
}

static int16_t held_repeat_find_free_slot(void) {
    for (uint16_t i = 0; i < ARRAY_SIZE(held_repeats); i++) {
        if (!held_repeats[i].active) return (int16_t)i;
    }

    return -1;
}

static void held_repeat_remove_slot(uint16_t slot) {
    pointer_layer_policy_note_action(held_repeats[slot].action, false);
    held_repeats[slot] = (held_repeat_binding_t){0};
}

static uint16_t held_repeat_interval_from_hz(uint16_t repeat_hz) {
    if (!hold_repeat_rate_valid(repeat_hz)) {
        return 0;
    }

    uint32_t interval_ms = (1000u + (uint32_t)repeat_hz - 1u) / (uint32_t)repeat_hz;
    return interval_ms == 0 ? 1 : (uint16_t)interval_ms;
}

static void held_repeat_log_invalid_rate(keypos_t key_pos, uint16_t action) {
#ifdef CONSOLE_ENABLE
    uprintf("Invalid repeat binding at key (%u,%u) for action 0x%04X; repeat frequency must be between 1 and %u Hz\n", (unsigned int)key_pos.row, (unsigned int)key_pos.col, (unsigned int)action, (unsigned int)KEY_BEHAVIOR_REPEAT_MAX_HZ);
#else
    (void)key_pos;
    (void)action;
#endif
}

bool held_repeat_release_owned_by_key(keypos_t key_pos) {
    int16_t slot = held_repeat_find_slot_for_key(key_pos);

    if (slot < 0) {
        return false;
    }

    held_repeat_remove_slot((uint16_t)slot);
    return true;
}

void held_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    uint16_t interval_ms          = held_repeat_interval_from_hz(repeat_hz);
    bool     anchor_needs_refresh = true;

    if (interval_ms == 0) {
        held_repeat_log_invalid_rate(key_pos, action);
        return;
    }

    action_dispatch(action);

    int16_t slot = held_repeat_find_slot_for_key(key_pos);
    if (slot < 0) {
        slot = held_repeat_find_free_slot();
        if (slot < 0) {
            held_repeat_log_binding_overflow(key_pos, action);
            return;
        }
    } else if (held_repeats[slot].action == action) {
        anchor_needs_refresh = false;
    } else if (held_repeats[slot].action != action) {
        pointer_layer_policy_note_action(held_repeats[slot].action, false);
    }

    if (anchor_needs_refresh) {
        pointer_layer_policy_note_action(action, true);
    }

    held_repeats[slot] = (held_repeat_binding_t){
        .active         = true,
        .key_pos        = key_pos,
        .action         = action,
        .interval_ms    = interval_ms,
        .last_fire_time = timer_read(),
    };
}

void held_repeat_tick(void) {
    for (uint16_t i = 0; i < ARRAY_SIZE(held_repeats); i++) {
        if (!held_repeats[i].active) {
            continue;
        }

        while (timer_elapsed(held_repeats[i].last_fire_time) >= held_repeats[i].interval_ms) {
            held_repeats[i].last_fire_time = (uint16_t)(held_repeats[i].last_fire_time + held_repeats[i].interval_ms);
            action_dispatch(held_repeats[i].action);
        }
    }
}

void held_repeat_debug_snapshot(held_repeat_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (held_repeat_debug_snapshot_t){0};

    for (uint16_t i = 0; i < ARRAY_SIZE(held_repeats); i++) {
        out->bindings[i] = (held_repeat_binding_snapshot_t){
            .active         = held_repeats[i].active,
            .key_pos        = held_repeats[i].key_pos,
            .action         = held_repeats[i].action,
            .interval_ms    = held_repeats[i].interval_ms,
            .last_fire_time = held_repeats[i].last_fire_time,
        };
    }
}

void held_repeat_reset_for_test(void) {
    memset(held_repeats, 0, sizeof(held_repeats));
}
