// ────────────────────────────────────────────────────────────────────────────
// Held Repeat Scheduling
// ────────────────────────────────────────────────────────────────────────────

#include "held_repeat.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "../../action/action_dispatch.h"
#include "../../pointing/policy/pointer_layer_policy.h"
#include "../../state/shared/runtime_context_internal.h"
#include "../behavior/key_behavior.h"

static inline bool keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static noah_held_repeat_state_t *held_repeat_state(void) {
    return &noah_runtime_context()->held_repeats;
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
    noah_held_repeat_state_t *state = held_repeat_state();

    for (uint16_t i = 0; i < ARRAY_SIZE(state->bindings); i++) {
        if (state->bindings[i].active && keypos_equal(state->bindings[i].key_pos, key_pos)) return (int16_t)i;
    }

    return -1;
}

static int16_t held_repeat_find_free_slot(void) {
    noah_held_repeat_state_t *state = held_repeat_state();

    for (uint16_t i = 0; i < ARRAY_SIZE(state->bindings); i++) {
        if (!state->bindings[i].active) return (int16_t)i;
    }

    return -1;
}

static void held_repeat_remove_slot(uint16_t slot) {
    noah_held_repeat_state_t *state = held_repeat_state();

    pointer_layer_policy_note_action(state->bindings[slot].action, false);
    if (state->active_count != 0u) {
        state->active_count--;
    }
    state->bindings[slot] = (held_repeat_binding_snapshot_t){0};
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
    noah_held_repeat_state_t *state                = held_repeat_state();
    uint16_t                  interval_ms          = held_repeat_interval_from_hz(repeat_hz);
    bool                      anchor_needs_refresh = true;
    bool                      new_binding          = false;

    if (interval_ms == 0) {
        held_repeat_log_invalid_rate(key_pos, action);
        return;
    }

    noah_emit_action_tap_at(key_pos, action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);

    int16_t slot = held_repeat_find_slot_for_key(key_pos);
    if (slot < 0) {
        slot = held_repeat_find_free_slot();
        if (slot < 0) {
            held_repeat_log_binding_overflow(key_pos, action);
            return;
        }
        new_binding = true;
    } else if (state->bindings[slot].action == action) {
        anchor_needs_refresh = false;
    } else if (state->bindings[slot].action != action) {
        pointer_layer_policy_note_action(state->bindings[slot].action, false);
    }

    if (anchor_needs_refresh) {
        pointer_layer_policy_note_action(action, true);
    }

    if (new_binding) {
        state->active_count++;
    }

    state->bindings[slot] = (held_repeat_binding_snapshot_t){
        .active         = true,
        .key_pos        = key_pos,
        .action         = action,
        .interval_ms    = interval_ms,
        .last_fire_time = timer_read(),
    };
}

void held_repeat_tick(void) {
    noah_held_repeat_state_t *state = held_repeat_state();
    uint16_t                  now;

    if (state->active_count == 0u) {
        return;
    }

    now = timer_read();
    for (uint16_t i = 0; i < ARRAY_SIZE(state->bindings); i++) {
        if (!state->bindings[i].active) {
            continue;
        }

        if ((uint16_t)(now - state->bindings[i].last_fire_time) >= state->bindings[i].interval_ms) {
            state->bindings[i].last_fire_time = now;
            noah_emit_action_tap_at(state->bindings[i].key_pos, state->bindings[i].action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
        }
    }
}

void held_repeat_debug_snapshot(held_repeat_debug_snapshot_t *out) {
    noah_held_repeat_state_t *state = held_repeat_state();

    if (!out) {
        return;
    }

    *out = (held_repeat_debug_snapshot_t){0};

    for (uint16_t i = 0; i < ARRAY_SIZE(state->bindings); i++) {
        out->bindings[i] = (held_repeat_binding_snapshot_t){
            .active         = state->bindings[i].active,
            .key_pos        = state->bindings[i].key_pos,
            .action         = state->bindings[i].action,
            .interval_ms    = state->bindings[i].interval_ms,
            .last_fire_time = state->bindings[i].last_fire_time,
        };
    }
}
