#include "pending_release_queue.h"

#include "../reducer/state_query.h"

typedef struct {
    uint8_t previous_index;
    uint8_t index;
} key_runtime_core_pending_release_cursor_t;

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

static bool key_runtime_core_pending_release_index_valid(uint8_t index) {
    return index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY;
}

static key_runtime_core_pending_release_cursor_t key_runtime_core_pending_release_invalid_cursor(void) {
    return (key_runtime_core_pending_release_cursor_t){
        .previous_index = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
        .index          = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
    };
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
        .key_pos             = key_runtime_core_pending_release_slot_key_pos(pending),
        .action              = pending->action,
        .mods                = pending->mods,
    };
}

static bool key_runtime_core_pending_release_slot_matches(const pending_release_slot_t *pending, keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    return key_runtime_core_pending_release_slot_active(pending) && key_runtime_core_pending_release_keypos_equal(key_runtime_core_pending_release_slot_key_pos(pending), key_pos) && pending->action == action && pending->mods.real == mods.real && pending->mods.weak == mods.weak && pending->mods.oneshot == mods.oneshot && pending->mods.oneshot_locked == mods.oneshot_locked;
}

static uint8_t key_runtime_core_find_free_pending_release_index(const key_runtime_core_state_t *state) {
    if (!state) {
        return KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        if (!key_runtime_core_pending_release_slot_active(&state->pending_releases[index])) {
            return (uint8_t)index;
        }
    }

    return KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE;
}

static bool key_runtime_core_append_pending_release(key_runtime_core_state_t *state, uint8_t index) {
    pending_release_slot_t *pending;

    if (!(state && key_runtime_core_pending_release_index_valid(index))) {
        return false;
    }

    pending = &state->pending_releases[index];
    if (!key_runtime_core_pending_release_slot_active(pending)) {
        return false;
    }
    pending->next_queue_index = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE;

    if (state->pending_release_count == 0u) {
        if (state->pending_release_head_index != KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE || state->pending_release_tail_index != KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE) {
            return false;
        }
        state->pending_release_head_index = index;
        state->pending_release_tail_index = index;
    } else {
        pending_release_slot_t *tail;

        if (!(key_runtime_core_pending_release_index_valid(state->pending_release_head_index) && key_runtime_core_pending_release_index_valid(state->pending_release_tail_index))) {
            return false;
        }
        tail = &state->pending_releases[state->pending_release_tail_index];
        if (!key_runtime_core_pending_release_slot_active(tail) || tail->next_queue_index != KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE) {
            return false;
        }
        tail->next_queue_index            = index;
        state->pending_release_tail_index = index;
    }

    state->pending_release_count++;
    if (state->pending_release_count > state->pending_release_high_water_mark) {
        state->pending_release_high_water_mark = state->pending_release_count;
    }
    return true;
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

static key_runtime_core_pending_release_cursor_t key_runtime_core_first_eligible_pending_release(const key_runtime_core_state_t *state) {
    key_runtime_core_pending_release_cursor_t cursor = {
        .previous_index = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
        .index          = state ? state->pending_release_head_index : KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
    };

    for (uint16_t visited = 0u; visited < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY && key_runtime_core_pending_release_index_valid(cursor.index); visited++) {
        const pending_release_slot_t *pending = &state->pending_releases[cursor.index];

        if (!key_runtime_core_pending_release_slot_active(pending)) {
            return key_runtime_core_pending_release_invalid_cursor();
        }
        if (!key_runtime_core_pending_release_owner_active(state, pending)) {
            return cursor;
        }
        cursor.previous_index = cursor.index;
        cursor.index          = pending->next_queue_index;
    }

    return key_runtime_core_pending_release_invalid_cursor();
}

static key_runtime_core_pending_release_cursor_t key_runtime_core_pending_release_cursor_for_order(const key_runtime_core_state_t *state, uint8_t order) {
    key_runtime_core_pending_release_cursor_t cursor = {
        .previous_index = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
        .index          = state ? state->pending_release_head_index : KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
    };

    for (uint16_t current_order = 0u; current_order <= order && current_order < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; current_order++) {
        const pending_release_slot_t *pending;

        if (!key_runtime_core_pending_release_index_valid(cursor.index)) {
            return key_runtime_core_pending_release_invalid_cursor();
        }
        pending = &state->pending_releases[cursor.index];
        if (!key_runtime_core_pending_release_slot_active(pending)) {
            return key_runtime_core_pending_release_invalid_cursor();
        }
        if (current_order == order) {
            return cursor;
        }
        cursor.previous_index = cursor.index;
        cursor.index          = pending->next_queue_index;
    }

    return key_runtime_core_pending_release_invalid_cursor();
}

static key_runtime_core_pending_release_cursor_t key_runtime_core_oldest_matching_pending_release(const key_runtime_core_state_t *state, keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    key_runtime_core_pending_release_cursor_t cursor = {
        .previous_index = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
        .index          = state ? state->pending_release_head_index : KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
    };

    for (uint16_t visited = 0u; visited < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY && key_runtime_core_pending_release_index_valid(cursor.index); visited++) {
        const pending_release_slot_t *pending = &state->pending_releases[cursor.index];

        if (!key_runtime_core_pending_release_slot_active(pending)) {
            return key_runtime_core_pending_release_invalid_cursor();
        }
        if (key_runtime_core_pending_release_slot_matches(pending, key_pos, action, mods)) {
            return cursor;
        }
        cursor.previous_index = cursor.index;
        cursor.index          = pending->next_queue_index;
    }

    return key_runtime_core_pending_release_invalid_cursor();
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

static bool key_runtime_core_unlink_pending_release(key_runtime_core_state_t *state, key_runtime_core_pending_release_cursor_t cursor, pending_release_t *out) {
    pending_release_slot_t *pending;
    pending_release_t       snapshot;
    uint8_t                 next_index;

    if (!(state && state->pending_release_count != 0u && key_runtime_core_pending_release_index_valid(cursor.index))) {
        return false;
    }

    pending = &state->pending_releases[cursor.index];
    if (!key_runtime_core_pending_release_slot_active(pending)) {
        return false;
    }
    if (cursor.previous_index == KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE) {
        if (state->pending_release_head_index != cursor.index) {
            return false;
        }
    } else if (!(key_runtime_core_pending_release_index_valid(cursor.previous_index) && key_runtime_core_pending_release_slot_active(&state->pending_releases[cursor.previous_index]) && state->pending_releases[cursor.previous_index].next_queue_index == cursor.index)) {
        return false;
    }

    next_index = pending->next_queue_index;
    if ((state->pending_release_tail_index == cursor.index) != (next_index == KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE)) {
        return false;
    }

    snapshot = key_runtime_core_pending_release_slot_snapshot(pending);
    if (cursor.previous_index == KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE) {
        state->pending_release_head_index = next_index;
    } else {
        state->pending_releases[cursor.previous_index].next_queue_index = next_index;
    }
    if (state->pending_release_tail_index == cursor.index) {
        state->pending_release_tail_index = cursor.previous_index;
    }

    *pending = (pending_release_slot_t){0};
    state->pending_release_count--;
    if (state->pending_release_count == 0u) {
        state->pending_release_head_index = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE;
        state->pending_release_tail_index = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE;
    }
    key_runtime_core_pending_release_clear_token(state, snapshot.owner_token_id);
    if (out) {
        *out = snapshot;
    }
    return true;
}

uint8_t key_runtime_core_pending_release_count(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return state ? state->pending_release_count : 0u;
}

bool key_runtime_core_queue_pending_release_dispatch_for_owner(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, bool tap_commit_feedback, uint16_t owner_token_id) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    pending_release_slot_t   *pending;
    uint8_t                   index;
    uint8_t                   flags = KEY_RUNTIME_PENDING_RELEASE_FLAG_ACTIVE;

    if (!(state && action != KC_NO && key_runtime_core_pending_release_keypos_valid(key_pos))) {
        return false;
    }

    index = key_runtime_core_find_free_pending_release_index(state);
    if (!key_runtime_core_pending_release_index_valid(index)) {
        return false;
    }

    if (tap_commit_feedback) {
        flags |= KEY_RUNTIME_PENDING_RELEASE_FLAG_TAP_COMMIT_FEEDBACK;
    }
    pending  = &state->pending_releases[index];
    *pending = (pending_release_slot_t){
        .owner_token_id   = owner_token_id,
        .action           = action,
        .mods             = mods,
        .next_queue_index = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE,
        .packed_key_pos   = key_runtime_keypos_pack(key_pos),
        .flags            = flags,
    };
    if (!key_runtime_core_append_pending_release(state, index)) {
        *pending = (pending_release_slot_t){0};
        return false;
    }
    key_runtime_core_pending_release_mark_token(state, pending->owner_token_id);
    return true;
}

bool key_runtime_core_queue_pending_release_dispatch(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, bool tap_commit_feedback) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token = (state && key_runtime_core_pending_release_keypos_valid(key_pos)) ? &state->press_tokens[key_runtime_core_pending_release_keypos_index(key_pos)] : NULL;

    return key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, action, mods, tap_commit_feedback, token ? token->token_id : 0u);
}

bool key_runtime_core_pending_release_at_order(uint8_t order, pending_release_t *out) {
    key_runtime_core_state_t                 *state = key_runtime_core_state();
    key_runtime_core_pending_release_cursor_t cursor;

    if (out) {
        *out = (pending_release_t){0};
    }

    if (!(state && out && order < state->pending_release_count)) {
        return false;
    }

    cursor = key_runtime_core_pending_release_cursor_for_order(state, order);
    if (!key_runtime_core_pending_release_index_valid(cursor.index)) {
        return false;
    }

    *out = key_runtime_core_pending_release_slot_snapshot(&state->pending_releases[cursor.index]);
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
        key_runtime_core_pending_release_cursor_t cursor = key_runtime_core_first_eligible_pending_release(state);

        if (!key_runtime_core_pending_release_index_valid(cursor.index) || !key_runtime_core_unlink_pending_release(state, cursor, &out[count])) {
            break;
        }
        count++;
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
    key_runtime_core_state_t                 *state = key_runtime_core_state();
    key_runtime_core_pending_release_cursor_t cursor;

    if (!(state && action != KC_NO && key_runtime_core_pending_release_keypos_valid(key_pos))) {
        return;
    }

    cursor = key_runtime_core_oldest_matching_pending_release(state, key_pos, action, mods);
    (void)key_runtime_core_unlink_pending_release(state, cursor, NULL);
}

#ifdef NOAH_HOST_TEST_ENV
static bool key_runtime_core_pending_release_structure_valid(const key_runtime_core_state_t *state) {
    uint16_t reachable_count = 0u;
    uint8_t  index;

    if (!state) {
        return false;
    }
    if (state->pending_release_count == 0u) {
        if (state->pending_release_head_index != KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE || state->pending_release_tail_index != KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE) {
            return false;
        }
    } else if (!(key_runtime_core_pending_release_index_valid(state->pending_release_head_index) && key_runtime_core_pending_release_index_valid(state->pending_release_tail_index))) {
        return false;
    }

    index = state->pending_release_head_index;
    while (key_runtime_core_pending_release_index_valid(index)) {
        const pending_release_slot_t *pending;

        if (reachable_count >= KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY) {
            return false;
        }
        pending = &state->pending_releases[index];
        if (!key_runtime_core_pending_release_slot_active(pending)) {
            return false;
        }
        reachable_count++;
        if (index == state->pending_release_tail_index && pending->next_queue_index != KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE) {
            return false;
        }
        index = pending->next_queue_index;
    }
    if (reachable_count != state->pending_release_count) {
        return false;
    }
    if (reachable_count != 0u && state->pending_releases[state->pending_release_tail_index].next_queue_index != KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE) {
        return false;
    }

    for (uint16_t slot_index = 0u; slot_index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; slot_index++) {
        bool    reachable = false;
        uint8_t cursor    = state->pending_release_head_index;

        for (uint16_t visited = 0u; visited < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY && key_runtime_core_pending_release_index_valid(cursor); visited++) {
            if (cursor == slot_index) {
                reachable = true;
                break;
            }
            cursor = state->pending_releases[cursor].next_queue_index;
        }
        if (key_runtime_core_pending_release_slot_active(&state->pending_releases[slot_index]) != reachable) {
            return false;
        }
    }

    return true;
}

bool key_runtime_core_pending_release_validate(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    bool                      valid = key_runtime_core_pending_release_structure_valid(state);

    if (state && !valid && state->pending_release_validation_failure_count != UINT8_MAX) {
        state->pending_release_validation_failure_count++;
    }
    return valid;
}
#endif
