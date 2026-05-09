#include "pending_release_queue.h"

#include "../reducer/state_query.h"

static keypos_t key_runtime_core_pending_release_invalid_keypos(void) {
    return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static bool key_runtime_core_pending_release_keypos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static bool key_runtime_core_pending_release_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static uint16_t key_runtime_core_pending_release_keypos_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * MATRIX_COLS + key_pos.col);
}

static keypos_t key_runtime_core_pending_release_slot_key_pos(const pending_release_slot_t *pending) {
    return pending ? key_runtime_keypos_unpack(pending->packed_key_pos) : key_runtime_core_pending_release_invalid_keypos();
}

static bool key_runtime_core_pending_release_slot_active(const pending_release_slot_t *pending) {
    return pending && (pending->flags & KEY_RUNTIME_PENDING_RELEASE_FLAG_ACTIVE) != 0u;
}

static bool key_runtime_core_pending_release_slot_has_tap_commit_feedback(const pending_release_slot_t *pending) {
    return pending && (pending->flags & KEY_RUNTIME_PENDING_RELEASE_FLAG_TAP_COMMIT_FEEDBACK) != 0u;
}

static pending_release_t key_runtime_core_pending_release_slot_snapshot(const pending_release_slot_t *pending) {
    if (!key_runtime_core_pending_release_slot_active(pending)) {
        return (pending_release_t){0};
    }

    return (pending_release_t){
        .active              = true,
        .tap_commit_feedback = key_runtime_core_pending_release_slot_has_tap_commit_feedback(pending),
        .owner_token_id      = pending->owner_token_id,
        .sequence            = pending->sequence,
        .key_pos             = key_runtime_core_pending_release_slot_key_pos(pending),
        .action              = pending->action,
        .mods                = pending->mods,
    };
}

static bool key_runtime_core_pending_release_slot_matches(const pending_release_slot_t *pending, keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    return key_runtime_core_pending_release_slot_active(pending) && key_runtime_core_pending_release_keypos_equal(key_runtime_core_pending_release_slot_key_pos(pending), key_pos) && pending->action == action && pending->mods.real == mods.real && pending->mods.weak == mods.weak && pending->mods.oneshot == mods.oneshot && pending->mods.oneshot_locked == mods.oneshot_locked;
}

static pending_release_slot_t *key_runtime_core_allocate_pending_release(key_runtime_core_state_t *state) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        pending_release_slot_t *pending = &state->pending_releases[index];

        if (!key_runtime_core_pending_release_slot_active(pending)) {
            *pending       = (pending_release_slot_t){0};
            pending->flags = KEY_RUNTIME_PENDING_RELEASE_FLAG_ACTIVE;
            state->pending_release_count++;
            return pending;
        }
    }

    return NULL;
}

static bool key_runtime_core_pending_release_owner_active(const key_runtime_core_state_t *state, const pending_release_slot_t *pending) {
    if (!(state && pending && key_runtime_core_pending_release_slot_active(pending) && pending->owner_token_id != 0u)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];

        if (token->token_id == pending->owner_token_id && token->active) {
            return true;
        }
    }

    return false;
}

static int16_t key_runtime_core_oldest_pending_release_index(const key_runtime_core_state_t *state) {
    int16_t  selected = -1;
    uint16_t sequence = 0u;

    if (!state) {
        return -1;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        const pending_release_slot_t *pending = &state->pending_releases[index];

        if (!key_runtime_core_pending_release_slot_active(pending) || key_runtime_core_pending_release_owner_active(state, pending) || (selected >= 0 && pending->sequence >= sequence)) {
            continue;
        }

        selected = (int16_t)index;
        sequence = pending->sequence;
    }

    return selected;
}

static int16_t key_runtime_core_pending_release_index_for_order(const key_runtime_core_state_t *state, uint8_t order) {
    uint16_t previous_sequence = 0u;

    if (!state) {
        return -1;
    }

    for (uint8_t current_order = 0u; current_order <= order; current_order++) {
        int16_t  selected = -1;
        uint16_t sequence = 0u;

        for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
            const pending_release_slot_t *pending = &state->pending_releases[index];

            if (!(key_runtime_core_pending_release_slot_active(pending) && pending->sequence > previous_sequence) || (selected >= 0 && pending->sequence >= sequence)) {
                continue;
            }

            selected = (int16_t)index;
            sequence = pending->sequence;
        }

        if (selected < 0) {
            return -1;
        }

        if (current_order == order) {
            return selected;
        }

        previous_sequence = sequence;
    }

    return -1;
}

static uint8_t key_runtime_core_pending_release_count_for_owner_token(const key_runtime_core_state_t *state, uint16_t owner_token_id) {
    uint8_t count = 0;

    if (!(state && owner_token_id != 0u)) {
        return 0u;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        const pending_release_slot_t *pending = &state->pending_releases[index];

        if (key_runtime_core_pending_release_slot_active(pending) && pending->owner_token_id == owner_token_id) {
            count++;
        }
    }

    return count;
}

static void key_runtime_core_pending_release_mark_token(key_runtime_core_state_t *state, uint16_t owner_token_id) {
    if (!(state && owner_token_id != 0u)) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token = &state->press_tokens[index];

        if (token->token_id == owner_token_id && !token->active) {
            token->pending_release_emission = true;
            token->phase                    = PRESS_TOKEN_PHASE_RELEASE_PENDING;
            return;
        }
    }
}

static void key_runtime_core_pending_release_clear_token(key_runtime_core_state_t *state, uint16_t owner_token_id) {
    if (!(state && owner_token_id != 0u) || key_runtime_core_pending_release_count_for_owner_token(state, owner_token_id) != 0u) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token = &state->press_tokens[index];

        if (token->token_id == owner_token_id && !token->active) {
            token->pending_release_emission = false;
            if (token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING) {
                token->phase = PRESS_TOKEN_PHASE_RELEASED;
            }
            return;
        }
    }
}

uint8_t key_runtime_core_pending_release_count(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return state ? state->pending_release_count : 0u;
}

bool key_runtime_core_queue_pending_release_dispatch_for_owner(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, bool tap_commit_feedback, uint16_t owner_token_id) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    pending_release_slot_t   *pending;
    uint8_t                   flags = KEY_RUNTIME_PENDING_RELEASE_FLAG_ACTIVE;

    if (!(state && action != KC_NO && key_runtime_core_pending_release_keypos_valid(key_pos))) {
        return false;
    }

    pending = key_runtime_core_allocate_pending_release(state);
    if (!pending) {
        return false;
    }

    if (tap_commit_feedback) {
        flags |= KEY_RUNTIME_PENDING_RELEASE_FLAG_TAP_COMMIT_FEEDBACK;
    }
    *pending = (pending_release_slot_t){
        .owner_token_id = owner_token_id,
        .sequence       = state->next_pending_release_sequence++,
        .action         = action,
        .mods           = mods,
        .packed_key_pos = key_runtime_keypos_pack(key_pos),
        .flags          = flags,
    };
    key_runtime_core_pending_release_mark_token(state, pending->owner_token_id);
    return true;
}

bool key_runtime_core_queue_pending_release_dispatch(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, bool tap_commit_feedback) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token = (state && key_runtime_core_pending_release_keypos_valid(key_pos)) ? &state->press_tokens[key_runtime_core_pending_release_keypos_index(key_pos)] : NULL;

    return key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, action, mods, tap_commit_feedback, token ? token->token_id : 0u);
}

bool key_runtime_core_pending_release_at_order(uint8_t order, pending_release_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    int16_t                   index;

    if (out) {
        *out = (pending_release_t){0};
    }

    if (!(state && out && order < state->pending_release_count)) {
        return false;
    }

    index = key_runtime_core_pending_release_index_for_order(state, order);
    if (index < 0) {
        return false;
    }

    *out = key_runtime_core_pending_release_slot_snapshot(&state->pending_releases[index]);
    return true;
}

uint8_t key_runtime_core_take_pending_release_dispatches(pending_release_t *out, uint8_t capacity) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    uint8_t                   count = 0;

    if (!(state && out && capacity != 0u) || state->pending_release_count == 0u) {
        return 0u;
    }

    if (state->press_token_count != 0u && key_runtime_core_deferred_release_blocker_count() != 0u) {
        return 0u;
    }

    while (count < capacity) {
        int16_t           index = key_runtime_core_oldest_pending_release_index(state);
        pending_release_t pending;

        if (index < 0) {
            break;
        }

        pending                        = key_runtime_core_pending_release_slot_snapshot(&state->pending_releases[index]);
        out[count++]                   = pending;
        state->pending_releases[index] = (pending_release_slot_t){0};
        if (state->pending_release_count != 0u) {
            state->pending_release_count--;
        }
        key_runtime_core_pending_release_clear_token(state, pending.owner_token_id);
    }

    return count;
}

uint8_t key_runtime_core_pending_release_count_for_keypos(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    uint8_t                   count = 0;

    if (!(state && key_runtime_core_pending_release_keypos_valid(key_pos))) {
        return 0u;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        const pending_release_slot_t *pending = &state->pending_releases[index];

        if (key_runtime_core_pending_release_slot_active(pending) && key_runtime_core_pending_release_keypos_equal(key_runtime_core_pending_release_slot_key_pos(pending), key_pos)) {
            count++;
        }
    }

    return count;
}

void key_runtime_core_observe_release_dispatch_deferred(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    (void)key_runtime_core_queue_pending_release_dispatch(key_pos, action, mods, false);
}

void key_runtime_core_observe_release_dispatch_drained(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && action != KC_NO && key_runtime_core_pending_release_keypos_valid(key_pos))) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        pending_release_slot_t pending = state->pending_releases[index];

        if (!key_runtime_core_pending_release_slot_matches(&pending, key_pos, action, mods)) {
            continue;
        }

        state->pending_releases[index] = (pending_release_slot_t){0};
        if (state->pending_release_count != 0u) {
            state->pending_release_count--;
        }
        key_runtime_core_pending_release_clear_token(state, pending.owner_token_id);
        return;
    }
}
