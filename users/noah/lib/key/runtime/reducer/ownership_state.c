#include "ownership_state.h"

#include "../planning/scan_planner.h"
#include "../trace/core_trace.h"

#include "../../../pointing/defs/pd_modes.h"
#include "../../../pointing/policy/pd_mode_policy.h"
#include "../feedback.h"

static bool key_runtime_core_ownership_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static uint16_t key_runtime_core_ownership_keypos_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * MATRIX_COLS + key_pos.col);
}

static press_token_t *key_runtime_core_ownership_press_token_state(key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->press_tokens[key_runtime_core_ownership_keypos_index(key_pos)];
}

static keypos_t key_runtime_core_ownership_invalid_keypos(void) {
    return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static keypos_t key_runtime_core_ownership_keypos_from_slot_index(uint16_t index) {
    if (index >= KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY) {
        return key_runtime_core_ownership_invalid_keypos();
    }

    return (keypos_t){
        .row = (uint8_t)(index / MATRIX_COLS),
        .col = (uint8_t)(index % MATRIX_COLS),
    };
}

static bool key_runtime_core_ownership_press_token_slot_index(const key_runtime_core_state_t *state, const press_token_t *token, uint16_t *out) {
    uintptr_t start;
    uintptr_t end;
    uintptr_t ptr;

    if (out) {
        *out = 0u;
    }

    if (!(state && token && out)) {
        return false;
    }

    start = (uintptr_t)&state->press_tokens[0];
    end   = (uintptr_t)&state->press_tokens[KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY];
    ptr   = (uintptr_t)token;
    if (!(ptr >= start && ptr < end && ((ptr - start) % sizeof(state->press_tokens[0])) == 0u)) {
        return false;
    }

    *out = (uint16_t)((ptr - start) / sizeof(state->press_tokens[0]));
    return true;
}

static keypos_t key_runtime_core_ownership_press_token_resolve_key_pos(const key_runtime_core_state_t *state, const press_token_t *token) {
    uint16_t index;

    return key_runtime_core_ownership_press_token_slot_index(state, token, &index) ? key_runtime_core_ownership_keypos_from_slot_index(index) : key_runtime_core_ownership_invalid_keypos();
}

static lease_kind_t key_runtime_core_lease_kind(const lease_t *lease) {
    return lease ? (lease_kind_t)lease->kind : LEASE_KIND_NONE;
}

static keypos_t key_runtime_core_lease_owner_key_pos(const lease_t *lease) {
    return lease ? key_runtime_keypos_unpack(lease->owner_packed_key_pos) : key_runtime_core_ownership_invalid_keypos();
}

static bool key_runtime_core_lease_owner_keypos_equal(const lease_t *lease, keypos_t key_pos) {
    return lease && key_runtime_core_ownership_keypos_equal(key_runtime_core_lease_owner_key_pos(lease), key_pos);
}

static uint8_t key_runtime_core_modifier_mask_for_keycode(uint16_t keycode) {
    if (IS_MODIFIER_KEYCODE(keycode)) {
        return MOD_BIT(keycode);
    }

    if (IS_QK_MOD_TAP(keycode)) {
        return QK_MOD_TAP_GET_MODS(keycode);
    }

    return 0u;
}

static bool key_runtime_core_keycode_owns_layer_on_press(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode);
}

static bool key_runtime_core_keycode_owns_layer_on_hold(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode);
}

static uint8_t key_runtime_core_layer_for_keycode(uint16_t keycode) {
    if (IS_QK_MOMENTARY(keycode)) {
        return QK_MOMENTARY_GET_LAYER(keycode);
    }

    if (IS_QK_LAYER_TAP(keycode)) {
        return QK_LAYER_TAP_GET_LAYER(keycode);
    }

    return UINT8_MAX;
}

static bool key_runtime_core_keycode_owns_modifier_on_press(uint16_t keycode) {
    return IS_MODIFIER_KEYCODE(keycode);
}

static bool key_runtime_core_keycode_owns_modifier_on_hold(uint16_t keycode) {
    return IS_QK_MOD_TAP(keycode);
}

static pd_mode_mask_t key_runtime_core_pd_mode_for_keycode(uint16_t keycode) {
    return pd_mode_for_keycode(keycode);
}

static bool key_runtime_core_pd_mode_keeps_auto_mouse_anchored(pd_mode_mask_t mode) {
    return mode != 0 && pd_mode_policy_mode_keeps_auto_mouse_anchored(mode);
}

static bool key_runtime_core_pd_mode_prefers_typing_layer(pd_mode_mask_t mode) {
    return mode != 0 && pd_mode_policy_mode_prefers_typing_layer(mode);
}

static bool key_runtime_core_pd_mode_lock_owns_pointer_toggle(pd_mode_mask_t mode) {
    return mode != 0 && pd_mode_has_trait(mode, PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE);
}

#ifdef AUTO_MOUSE_DEFAULT_LAYER
static uint8_t key_runtime_core_default_pointer_layer(void) {
    return AUTO_MOUSE_DEFAULT_LAYER;
}
#else
static uint8_t key_runtime_core_default_pointer_layer(void) {
    return 0u;
}
#endif

static lease_t *key_runtime_core_find_pd_mode_lease(key_runtime_core_state_t *state, uint16_t owner_token_id, pd_mode_mask_t mode) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_PD_MODE && lease->owner_token_id == owner_token_id && lease->data.pd_mode == mode) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *key_runtime_core_find_pointer_anchor_lease(key_runtime_core_state_t *state, uint16_t owner_token_id) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_POINTER_ANCHOR && lease->owner_token_id == owner_token_id) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *key_runtime_core_find_modifier_lease(key_runtime_core_state_t *state, uint16_t owner_token_id, uint8_t modifiers, bool physical) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_MODIFIER && lease->owner_token_id == owner_token_id && lease->data.modifier.modifiers == modifiers && lease->data.modifier.physical == physical) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *key_runtime_core_find_layer_lease(key_runtime_core_state_t *state, uint16_t owner_token_id, uint8_t layer) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_LAYER && lease->owner_token_id == owner_token_id && lease->data.layer == layer) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *key_runtime_core_find_held_action_lease(key_runtime_core_state_t *state, keypos_t key_pos, uint16_t action) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_HELD_ACTION && key_runtime_core_lease_owner_keypos_equal(lease, key_pos) && lease->data.action == action) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *key_runtime_core_find_repeat_lease(key_runtime_core_state_t *state, keypos_t key_pos, uint16_t action) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_REPEAT && key_runtime_core_lease_owner_keypos_equal(lease, key_pos) && lease->data.repeat.action == action) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *key_runtime_core_allocate_lease(key_runtime_core_state_t *state) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (!lease->active) {
            *lease        = (lease_t){0};
            lease->active = true;
            state->lease_count++;
            return lease;
        }
    }

    return NULL;
}

static bool key_runtime_core_layer_lease_activate(key_runtime_core_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, uint8_t layer) {
    lease_t *lease;

    if (!(state && layer < 32u)) {
        return false;
    }

    if (key_runtime_core_find_layer_lease(state, owner_token_id, layer)) {
        return false;
    }

    lease = key_runtime_core_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active               = true,
        .kind                 = LEASE_KIND_LAYER,
        .owner_token_id       = owner_token_id,
        .owner_packed_key_pos = key_runtime_keypos_pack(owner_key_pos),
        .data.layer           = layer,
    };
    return true;
}

static bool key_runtime_core_modifier_lease_activate(key_runtime_core_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, uint8_t modifiers, bool physical) {
    lease_t *lease;

    if (!(state && modifiers != 0u)) {
        return false;
    }

    if (key_runtime_core_find_modifier_lease(state, owner_token_id, modifiers, physical)) {
        return false;
    }

    lease = key_runtime_core_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active               = true,
        .kind                 = LEASE_KIND_MODIFIER,
        .owner_token_id       = owner_token_id,
        .owner_packed_key_pos = key_runtime_keypos_pack(owner_key_pos),
        .data.modifier =
            {
                .modifiers = modifiers,
                .physical  = physical,
            },
    };
    return true;
}

static bool key_runtime_core_pd_mode_lease_activate(key_runtime_core_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, pd_mode_mask_t mode) {
    lease_t *lease;

    if (!(state && mode != 0)) {
        return false;
    }

    if (key_runtime_core_find_pd_mode_lease(state, owner_token_id, mode)) {
        return false;
    }

    lease = key_runtime_core_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active               = true,
        .kind                 = LEASE_KIND_PD_MODE,
        .owner_token_id       = owner_token_id,
        .owner_packed_key_pos = key_runtime_keypos_pack(owner_key_pos),
        .data.pd_mode         = mode,
    };
    return true;
}

static bool key_runtime_core_pointer_anchor_lease_activate(key_runtime_core_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, bool keep_typing_surface) {
    lease_t *lease;

    if (!state) {
        return false;
    }

    if (key_runtime_core_find_pointer_anchor_lease(state, owner_token_id)) {
        return false;
    }

    lease = key_runtime_core_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active               = true,
        .kind                 = LEASE_KIND_POINTER_ANCHOR,
        .owner_token_id       = owner_token_id,
        .owner_packed_key_pos = key_runtime_keypos_pack(owner_key_pos),
        .data.pointer_anchor =
            {
                .layer               = key_runtime_core_default_pointer_layer(),
                .keep_typing_surface = keep_typing_surface,
            },
    };
    return true;
}

static bool key_runtime_core_held_action_lease_activate(key_runtime_core_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, uint16_t action) {
    lease_t *lease;
    uint16_t feedback_started_at;
    uint32_t feedback_sequence;

    if (!(state && action != KC_NO)) {
        return false;
    }

    lease = key_runtime_core_find_held_action_lease(state, owner_key_pos, action);
    if (lease) {
        if (lease->owner_token_id == owner_token_id && key_runtime_core_lease_owner_keypos_equal(lease, owner_key_pos)) {
            return false;
        }

        feedback_started_at         = timer_read();
        feedback_sequence           = key_runtime_core_state_next_feedback_sequence(state);
        lease->owner_token_id       = owner_token_id;
        lease->owner_packed_key_pos = key_runtime_keypos_pack(owner_key_pos);
        lease->feedback_started_at  = feedback_started_at;
        lease->feedback_sequence    = feedback_sequence;
        return true;
    }

    lease = key_runtime_core_allocate_lease(state);
    if (!lease) {
        return false;
    }

    feedback_started_at = timer_read();
    feedback_sequence   = key_runtime_core_state_next_feedback_sequence(state);
    *lease              = (lease_t){
        .active               = true,
        .kind                 = LEASE_KIND_HELD_ACTION,
        .owner_token_id       = owner_token_id,
        .owner_packed_key_pos = key_runtime_keypos_pack(owner_key_pos),
        .feedback_started_at  = feedback_started_at,
        .feedback_sequence    = feedback_sequence,
        .data.action          = action,
    };
    return true;
}

static bool key_runtime_core_repeat_lease_activate(key_runtime_core_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, uint16_t action, uint16_t repeat_hz) {
    lease_t *lease;
    uint16_t feedback_started_at;
    uint32_t feedback_sequence;

    if (!(state && action != KC_NO)) {
        return false;
    }

    lease = key_runtime_core_find_repeat_lease(state, owner_key_pos, action);
    if (lease) {
        if (lease->owner_token_id == owner_token_id && key_runtime_core_lease_owner_keypos_equal(lease, owner_key_pos)) {
            return false;
        }

        feedback_started_at          = timer_read();
        feedback_sequence            = key_runtime_core_state_next_feedback_sequence(state);
        lease->owner_token_id        = owner_token_id;
        lease->owner_packed_key_pos  = key_runtime_keypos_pack(owner_key_pos);
        lease->feedback_started_at   = feedback_started_at;
        lease->feedback_sequence     = feedback_sequence;
        lease->data.repeat.repeat_hz = (uint8_t)repeat_hz;
        return true;
    }

    lease = key_runtime_core_allocate_lease(state);
    if (!lease) {
        return false;
    }

    feedback_started_at = timer_read();
    feedback_sequence   = key_runtime_core_state_next_feedback_sequence(state);
    *lease              = (lease_t){
        .active               = true,
        .kind                 = LEASE_KIND_REPEAT,
        .owner_token_id       = owner_token_id,
        .owner_packed_key_pos = key_runtime_keypos_pack(owner_key_pos),
        .feedback_started_at  = feedback_started_at,
        .feedback_sequence    = feedback_sequence,
        .data.repeat =
            {
                .action    = action,
                .repeat_hz = (uint8_t)repeat_hz,
            },
    };
    return true;
}

static bool key_runtime_core_persistent_layer_lock_update(key_runtime_core_state_t *state, uint8_t layer, bool active) {
    persistent_intent_t *empty_slot = NULL;

    if (!(state && layer < 32u)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PERSISTENT_INTENT_CAPACITY; index++) {
        persistent_intent_t *intent = &state->persistent_intents[index];

        if (intent->active && intent->kind == PERSISTENT_INTENT_KIND_LAYER_LOCK && intent->data.layer == layer) {
            if (active) {
                return false;
            }

            *intent = (persistent_intent_t){0};
            if (state->persistent_intent_count != 0u) {
                state->persistent_intent_count--;
            }
            return true;
        }

        if (!intent->active && !empty_slot) {
            empty_slot = intent;
        }
    }

    if (!(active && empty_slot)) {
        return false;
    }

    *empty_slot = (persistent_intent_t){
        .active     = true,
        .kind       = PERSISTENT_INTENT_KIND_LAYER_LOCK,
        .data.layer = layer,
    };
    state->persistent_intent_count++;
    return true;
}

static bool key_runtime_core_persistent_pd_mode_lock_update(key_runtime_core_state_t *state, pd_mode_mask_t mode, bool active) {
    persistent_intent_t *empty_slot = NULL;
    bool                 changed    = false;

    if (!(state && mode != 0)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PERSISTENT_INTENT_CAPACITY; index++) {
        persistent_intent_t *intent = &state->persistent_intents[index];

        if (intent->active && intent->kind == PERSISTENT_INTENT_KIND_PD_MODE_LOCK) {
            if (intent->data.pd_mode == mode) {
                if (active) {
                    return changed;
                }

                *intent = (persistent_intent_t){0};
                if (state->persistent_intent_count != 0u) {
                    state->persistent_intent_count--;
                }
                return true;
            }

            if (active) {
                *intent = (persistent_intent_t){0};
                if (state->persistent_intent_count != 0u) {
                    state->persistent_intent_count--;
                }
                changed = true;
                continue;
            }
        }

        if (!intent->active && !empty_slot) {
            empty_slot = intent;
        }
    }

    if (!(active && empty_slot)) {
        return changed;
    }

    *empty_slot = (persistent_intent_t){
        .active       = true,
        .kind         = PERSISTENT_INTENT_KIND_PD_MODE_LOCK,
        .data.pd_mode = mode,
    };
    state->persistent_intent_count++;
    return true;
}

static bool key_runtime_core_persistent_pointer_toggle_update(key_runtime_core_state_t *state, pd_mode_mask_t mode, bool active) {
    persistent_intent_t *empty_slot = NULL;
    bool                 changed    = false;

    if (!(state && mode != 0)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PERSISTENT_INTENT_CAPACITY; index++) {
        persistent_intent_t *intent = &state->persistent_intents[index];

        if (intent->active && intent->kind == PERSISTENT_INTENT_KIND_POINTER_TOGGLE) {
            if (intent->data.pointer_toggle.mode == mode) {
                if (active) {
                    return changed;
                }

                *intent = (persistent_intent_t){0};
                if (state->persistent_intent_count != 0u) {
                    state->persistent_intent_count--;
                }
                return true;
            }

            if (active) {
                *intent = (persistent_intent_t){0};
                if (state->persistent_intent_count != 0u) {
                    state->persistent_intent_count--;
                }
                changed = true;
                continue;
            }
        }

        if (!intent->active && !empty_slot) {
            empty_slot = intent;
        }
    }

    if (!(active && empty_slot)) {
        return changed;
    }

    *empty_slot = (persistent_intent_t){
        .active = true,
        .kind   = PERSISTENT_INTENT_KIND_POINTER_TOGGLE,
        .data.pointer_toggle =
            {
                .pointer_layer = key_runtime_core_default_pointer_layer(),
                .mode          = mode,
            },
    };
    state->persistent_intent_count++;
    return true;
}

static bool key_runtime_core_clear_other_pd_mode_intents(key_runtime_core_state_t *state, pd_mode_mask_t keep_mode) {
    bool changed = false;

    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PERSISTENT_INTENT_CAPACITY; index++) {
        persistent_intent_t *intent = &state->persistent_intents[index];

        if (!intent->active) {
            continue;
        }

        if (intent->kind == PERSISTENT_INTENT_KIND_PD_MODE_LOCK && intent->data.pd_mode != keep_mode) {
            *intent = (persistent_intent_t){0};
            if (state->persistent_intent_count != 0u) {
                state->persistent_intent_count--;
            }
            changed = true;
            continue;
        }

        if (intent->kind == PERSISTENT_INTENT_KIND_POINTER_TOGGLE && intent->data.pointer_toggle.mode != keep_mode) {
            *intent = (persistent_intent_t){0};
            if (state->persistent_intent_count != 0u) {
                state->persistent_intent_count--;
            }
            changed = true;
        }
    }

    return changed;
}

void key_runtime_core_release_leases_for_token(key_runtime_core_state_t *state, uint16_t owner_token_id) {
    if (!state) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && lease->owner_token_id == owner_token_id && key_runtime_core_lease_kind(lease) != LEASE_KIND_HELD_ACTION && key_runtime_core_lease_kind(lease) != LEASE_KIND_REPEAT) {
            *lease = (lease_t){0};
            if (state->lease_count != 0u) {
                state->lease_count--;
            }
        }
    }
}

// Held-action and repeat leases outlive key_runtime_core_release_leases_for_token()
// on purpose: their applied bindings are retired by a planned release effect, not
// by clearing the lease. Token replacement happens on the observe path, which has
// no effect plan and no matching physical release, so the replacing token adopts
// them. Its own release then plans the retire by key position, exactly as the
// original token's would have.
void key_runtime_core_adopt_runtime_owned_state_leases(key_runtime_core_state_t *state, uint16_t from_owner_token_id, uint16_t to_owner_token_id) {
    if (!state || from_owner_token_id == 0u || to_owner_token_id == 0u || from_owner_token_id == to_owner_token_id) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t     *lease = &state->leases[index];
        lease_kind_t kind;

        if (!(lease->active && lease->owner_token_id == from_owner_token_id)) {
            continue;
        }

        kind = key_runtime_core_lease_kind(lease);
        if (kind == LEASE_KIND_HELD_ACTION || kind == LEASE_KIND_REPEAT) {
            lease->owner_token_id = to_owner_token_id;
        }
    }
}

bool key_runtime_core_owner_has_lease_kind(const key_runtime_core_state_t *state, uint16_t owner_token_id, lease_kind_t kind) {
    if (!(state && owner_token_id != 0u)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        const lease_t *lease = &state->leases[index];

        if (lease->active && lease->owner_token_id == owner_token_id && key_runtime_core_lease_kind(lease) == kind) {
            return true;
        }
    }

    return false;
}

bool key_runtime_core_owner_has_runtime_owned_state_lease(const key_runtime_core_state_t *state, uint16_t owner_token_id) {
    return key_runtime_core_owner_has_lease_kind(state, owner_token_id, LEASE_KIND_HELD_ACTION) || key_runtime_core_owner_has_lease_kind(state, owner_token_id, LEASE_KIND_REPEAT);
}

static bool key_runtime_core_release_runtime_owned_state_leases_for_key(key_runtime_core_state_t *state, keypos_t key_pos) {
    bool changed = false;

    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (!lease->active || !key_runtime_core_lease_owner_keypos_equal(lease, key_pos)) {
            continue;
        }

        if (key_runtime_core_lease_kind(lease) == LEASE_KIND_HELD_ACTION || key_runtime_core_lease_kind(lease) == LEASE_KIND_REPEAT) {
            *lease = (lease_t){0};
            if (state->lease_count != 0u) {
                state->lease_count--;
            }
            changed = true;
        }
    }

    return changed;
}

static bool key_runtime_core_release_pd_related_leases_for_token(key_runtime_core_state_t *state, uint16_t owner_token_id) {
    bool changed = false;

    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (!lease->active || lease->owner_token_id != owner_token_id) {
            continue;
        }

        if (key_runtime_core_lease_kind(lease) == LEASE_KIND_PD_MODE || key_runtime_core_lease_kind(lease) == LEASE_KIND_POINTER_ANCHOR) {
            *lease = (lease_t){0};
            if (state->lease_count != 0u) {
                state->lease_count--;
            }
            changed = true;
        }
    }

    return changed;
}

static bool key_runtime_core_pd_mode_leases_activate(key_runtime_core_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, pd_mode_mask_t mode) {
    bool changed = false;

    if (!(state && mode != 0)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_PD_MODE && lease->data.pd_mode != mode) {
            changed |= key_runtime_core_release_pd_related_leases_for_token(state, lease->owner_token_id);
        }
    }

    changed |= key_runtime_core_clear_other_pd_mode_intents(state, mode);
    changed |= key_runtime_core_pd_mode_lease_activate(state, owner_token_id, owner_key_pos, mode);
    if (key_runtime_core_pd_mode_keeps_auto_mouse_anchored(mode)) {
        changed |= key_runtime_core_pointer_anchor_lease_activate(state, owner_token_id, owner_key_pos, key_runtime_core_pd_mode_prefers_typing_layer(mode));
    }

    return changed;
}

static bool key_runtime_core_press_token_attaches_pd_mode_on_press(const press_token_t *token) {
    return token && (!token->handled_key || key_runtime_slot_interaction_uses_implicit_hold(token->interaction));
}

void key_runtime_core_shadow_projection_recompute(key_runtime_core_state_t *state) {
    key_runtime_core_shadow_projection_t projection                  = {0};
    bool                                 pointer_anchor_lease_active = false;

    if (!state) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        const lease_t *lease = &state->leases[index];

        if (!lease->active) {
            continue;
        }

        switch (key_runtime_core_lease_kind(lease)) {
            case LEASE_KIND_LAYER:
                projection.layer_state |= (layer_state_t)1u << lease->data.layer;
                break;
            case LEASE_KIND_MODIFIER:
                projection.keyboard_mod_state.real |= lease->data.modifier.modifiers;
                if (lease->data.modifier.physical) {
                    projection.keyboard_physical_mod_mask |= lease->data.modifier.modifiers;
                } else {
                    projection.keyboard_managed_mod_mask |= lease->data.modifier.modifiers;
                }
                break;
            case LEASE_KIND_PD_MODE:
                projection.pd_mode_local_active = lease->data.pd_mode;
                break;
            case LEASE_KIND_POINTER_ANCHOR:
                pointer_anchor_lease_active = true;
                break;
            default:
                break;
        }
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PERSISTENT_INTENT_CAPACITY; index++) {
        const persistent_intent_t *intent = &state->persistent_intents[index];

        if (!intent->active) {
            continue;
        }

        switch (intent->kind) {
            case PERSISTENT_INTENT_KIND_LAYER_LOCK:
                projection.locked_layer_mask |= (layer_state_t)1u << intent->data.layer;
                projection.layer_state |= (layer_state_t)1u << intent->data.layer;
                break;
            case PERSISTENT_INTENT_KIND_PD_MODE_LOCK:
                projection.pd_mode_local_locked = intent->data.pd_mode;
                projection.pd_mode_local_active = intent->data.pd_mode;
                break;
            case PERSISTENT_INTENT_KIND_POINTER_TOGGLE:
                projection.pointer_toggle_enabled = true;
                break;
            default:
                break;
        }
    }

    projection.pointer_pd_mode_anchor_active = key_runtime_core_pd_mode_keeps_auto_mouse_anchored(projection.pd_mode_local_active);
    projection.pointer_prefers_typing_layer  = key_runtime_core_pd_mode_prefers_typing_layer(projection.pd_mode_local_active);
    projection.pointer_anchor_active         = projection.pointer_toggle_enabled || pointer_anchor_lease_active || projection.pointer_pd_mode_anchor_active;

    state->shadow_projection = projection;
}

void key_runtime_core_press_token_attach_press_leases(key_runtime_core_state_t *state, const press_token_t *token) {
    bool           changed = false;
    pd_mode_mask_t mode;
    keypos_t       token_key_pos;

    if (!(state && token && token->active)) {
        return;
    }

    token_key_pos = key_runtime_core_ownership_press_token_resolve_key_pos(state, token);

    if (key_runtime_core_keycode_owns_layer_on_press(token->resolved_keycode)) {
        uint8_t layer = key_runtime_core_layer_for_keycode(token->resolved_keycode);

        if (layer != UINT8_MAX) {
            changed |= key_runtime_core_layer_lease_activate(state, token->token_id, token_key_pos, layer);
        }
    }

    if (key_runtime_core_keycode_owns_modifier_on_press(token->resolved_keycode)) {
        changed |= key_runtime_core_modifier_lease_activate(state, token->token_id, token_key_pos, key_runtime_core_modifier_mask_for_keycode(token->resolved_keycode), true);
    }

    mode = key_runtime_core_pd_mode_for_keycode(token->resolved_keycode);
    if (mode != 0) {
        if (key_runtime_core_press_token_attaches_pd_mode_on_press(token)) {
            changed |= key_runtime_core_pd_mode_leases_activate(state, token->token_id, token_key_pos, mode);
        }
    }

    if (changed) {
        key_runtime_core_shadow_projection_recompute(state);
    }
}

void key_runtime_core_press_token_attach_hold_leases(key_runtime_core_state_t *state, const press_token_t *token) {
    bool     changed = false;
    keypos_t token_key_pos;

    if (!(state && token && token->active && token->phase == PRESS_TOKEN_PHASE_HELD)) {
        return;
    }

    token_key_pos = key_runtime_core_ownership_press_token_resolve_key_pos(state, token);

    if (key_runtime_core_keycode_owns_layer_on_hold(token->resolved_keycode)) {
        uint8_t layer = key_runtime_core_layer_for_keycode(token->resolved_keycode);

        if (layer != UINT8_MAX) {
            changed |= key_runtime_core_layer_lease_activate(state, token->token_id, token_key_pos, layer);
        }
    }

    if (key_runtime_core_keycode_owns_modifier_on_hold(token->resolved_keycode)) {
        changed |= key_runtime_core_modifier_lease_activate(state, token->token_id, token_key_pos, key_runtime_core_modifier_mask_for_keycode(token->resolved_keycode), false);
    }

    if (changed) {
        key_runtime_core_shadow_projection_recompute(state);
    }
}

void key_runtime_core_observe_held_action_register(keypos_t key_pos, uint16_t action) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;
    pd_mode_mask_t            mode;

    if (!(state && action != KC_NO && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    token = key_runtime_core_ownership_press_token_state(state, key_pos);
    (void)key_runtime_core_held_action_lease_activate(state, token ? token->token_id : 0u, key_pos, action);
    mode = key_runtime_core_pd_mode_for_keycode(action);
    if (mode != 0) {
        noah_runtime_trace_emit(NOAH_TRACE_KEY_RUNTIME, NOAH_TRACE_KEY_RUNTIME_EVENT_PD_HELD_REGISTER, mode, action);
    }
    if (key_runtime_core_pd_mode_leases_activate(state, token ? token->token_id : 0u, key_pos, mode)) {
        key_runtime_core_shadow_projection_recompute(state);
    }

    if (!(token && token->handled_key)) {
        return;
    }

    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW) {
        if (key_runtime_slot_interaction_uses_fallback_hold(token->interaction)) {
            key_runtime_core_press_token_commit_hold_phase(state, token, true, false);
            return;
        }

        if ((uint16_t)(state->current_time - token->pressed_at) >= token->interaction.binding.tap_hold_term) {
            key_runtime_core_press_token_commit_hold_phase(state, token, !token->interaction.binding.long_hold.present, false);
            return;
        }
    }

    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING) {
        key_runtime_core_press_token_commit_hold_phase(state, token, true, false);
    }
}

void key_runtime_core_observe_held_action_unregister(keypos_t key_pos, uint16_t action) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    lease_t                  *lease;
    pd_mode_mask_t            mode;
    uint16_t                  owner_token_id;
    bool                      changed = false;

    if (!(state && action != KC_NO && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    lease = key_runtime_core_find_held_action_lease(state, key_pos, action);
    if (!lease) {
        return;
    }

    owner_token_id = lease->owner_token_id;
    *lease         = (lease_t){0};
    if (state->lease_count != 0u) {
        state->lease_count--;
    }

    mode = key_runtime_core_pd_mode_for_keycode(action);
    if (mode != 0) {
        noah_runtime_trace_emit(NOAH_TRACE_KEY_RUNTIME, NOAH_TRACE_KEY_RUNTIME_EVENT_PD_HELD_UNREGISTER, mode, action);
    }
    if (mode != 0) {
        if (owner_token_id != 0u) {
            changed |= key_runtime_core_release_pd_related_leases_for_token(state, owner_token_id);
        } else {
            for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
                lease = &state->leases[index];

                if (!lease->active || lease->owner_token_id != 0u || !key_runtime_core_lease_owner_keypos_equal(lease, key_pos)) {
                    continue;
                }

                if (key_runtime_core_lease_kind(lease) == LEASE_KIND_PD_MODE && lease->data.pd_mode != mode) {
                    continue;
                }

                if (key_runtime_core_lease_kind(lease) == LEASE_KIND_PD_MODE || key_runtime_core_lease_kind(lease) == LEASE_KIND_POINTER_ANCHOR) {
                    *lease = (lease_t){0};
                    if (state->lease_count != 0u) {
                        state->lease_count--;
                    }
                    changed = true;
                }
            }
        }
    }
    if (changed) {
        key_runtime_core_shadow_projection_recompute(state);
    }
}

void key_runtime_core_observe_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;

    if (!(state && action != KC_NO && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    token = key_runtime_core_ownership_press_token_state(state, key_pos);
    (void)key_runtime_core_repeat_lease_activate(state, token ? token->token_id : 0u, key_pos, action, repeat_hz);

    if (!(token && token->handled_key)) {
        return;
    }

    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || token->slot_phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW) {
        key_runtime_core_press_token_commit_hold_phase(state, token, !token->interaction.binding.long_hold.present, false);
        return;
    }

    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING) {
        key_runtime_core_press_token_commit_hold_phase(state, token, true, false);
    }
}

bool key_runtime_core_release_owned_state_by_key(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    return key_runtime_core_release_runtime_owned_state_leases_for_key(state, key_pos);
}

bool key_runtime_core_finalize_non_handled_release(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;
    uint8_t                   lease_count_before;
    bool                      changed = false;

    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    token              = key_runtime_core_ownership_press_token_state(state, key_pos);
    lease_count_before = state->lease_count;
    changed            = key_runtime_core_release_runtime_owned_state_leases_for_key(state, key_pos);
    if (!(token && !token->active && !token->handled_key && !token->pending_release_emission)) {
        if (state->lease_count != lease_count_before) {
            key_runtime_core_shadow_projection_recompute(state);
            return true;
        }
        return changed;
    }

    key_runtime_core_release_leases_for_token(state, token->token_id);
    if (state->lease_count != lease_count_before) {
        key_runtime_core_shadow_projection_recompute(state);
        changed = true;
    }

    *token = (press_token_t){0};
    return true;
}

uint16_t key_runtime_core_key_pos_held_action_keycode(const key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return KC_NO;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        const lease_t *lease = &state->leases[index];

        if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_HELD_ACTION && key_runtime_core_lease_owner_keypos_equal(lease, key_pos)) {
            return lease->data.action;
        }
    }

    return KC_NO;
}

bool key_runtime_core_key_pos_repeat_active(const key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        const lease_t *lease = &state->leases[index];

        if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_REPEAT && key_runtime_core_lease_owner_keypos_equal(lease, key_pos)) {
            return true;
        }
    }

    return false;
}

static bool key_runtime_core_flashing_feedback_lease_visible(const lease_t *lease) {
    if (!(lease && lease->active)) {
        return false;
    }

    return ((timer_elapsed(lease->feedback_started_at) / KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) & 1u) == 0u;
}

static bool key_runtime_core_feedback_sequence_is_newer_or_equal(uint32_t candidate, uint32_t current) {
    return current == 0u || (candidate != 0u && (uint32_t)(candidate - current) < 0x80000000u);
}

bool key_runtime_core_flashing_feedback_started_at(keypos_t key_pos, uint16_t *out_started_at) {
    key_runtime_core_state_t *state             = key_runtime_core_state();
    bool                      found             = false;
    uint16_t                  newest_started_at = 0;
    uint32_t                  newest_sequence   = 0;

    if (!(state && out_started_at && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        const lease_t *lease = &state->leases[index];
        lease_kind_t   kind;

        if (!(lease->active && key_runtime_core_lease_owner_keypos_equal(lease, key_pos))) {
            continue;
        }

        kind = key_runtime_core_lease_kind(lease);
        if (kind != LEASE_KIND_HELD_ACTION && kind != LEASE_KIND_REPEAT) {
            continue;
        }

        if (!found || key_runtime_core_feedback_sequence_is_newer_or_equal(lease->feedback_sequence, newest_sequence)) {
            newest_started_at = lease->feedback_started_at;
            newest_sequence   = lease->feedback_sequence;
            found             = true;
        }
    }

    if (!found) {
        return false;
    }

    *out_started_at = newest_started_at;
    return true;
}

bool key_runtime_core_flashing_feedback_sequence_at(keypos_t key_pos, uint32_t *out_sequence) {
    key_runtime_core_state_t *state           = key_runtime_core_state();
    bool                      found           = false;
    uint32_t                  newest_sequence = 0;

    if (!(state && out_sequence && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        const lease_t *lease = &state->leases[index];
        lease_kind_t   kind;

        if (!(lease->active && key_runtime_core_lease_owner_keypos_equal(lease, key_pos))) {
            continue;
        }

        kind = key_runtime_core_lease_kind(lease);
        if (kind != LEASE_KIND_HELD_ACTION && kind != LEASE_KIND_REPEAT) {
            continue;
        }

        if (!found || key_runtime_core_feedback_sequence_is_newer_or_equal(lease->feedback_sequence, newest_sequence)) {
            newest_sequence = lease->feedback_sequence;
            found           = true;
        }
    }

    if (!found) {
        return false;
    }

    *out_sequence = newest_sequence;
    return true;
}

bool key_runtime_core_flashing_feedback_visible_at(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        const lease_t *lease = &state->leases[index];
        lease_kind_t   kind;

        if (!(lease->active && key_runtime_core_lease_owner_keypos_equal(lease, key_pos))) {
            continue;
        }

        kind = key_runtime_core_lease_kind(lease);
        if ((kind == LEASE_KIND_HELD_ACTION || kind == LEASE_KIND_REPEAT) && key_runtime_core_flashing_feedback_lease_visible(lease)) {
            return true;
        }
    }

    return false;
}

uint16_t key_runtime_core_held_action_keycode_at(keypos_t key_pos) {
    return key_runtime_core_key_pos_held_action_keycode(key_runtime_core_state(), key_pos);
}

bool key_runtime_core_repeat_active_at(keypos_t key_pos) {
    return key_runtime_core_key_pos_repeat_active(key_runtime_core_state(), key_pos);
}

const key_runtime_core_shadow_projection_t *key_runtime_core_shadow_projection(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return state ? &state->shadow_projection : NULL;
}

void key_runtime_core_layer_lock_set(uint8_t layer, bool active) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (key_runtime_core_persistent_layer_lock_update(state, layer, active)) {
        key_runtime_core_shadow_projection_recompute(state);
    }
}

void key_runtime_core_observe_pd_mode_lock_state(pd_mode_mask_t mode, bool active) {
    key_runtime_core_state_t *state   = key_runtime_core_state();
    bool                      changed = false;

    if (!(state && mode != 0)) {
        return;
    }

    if (active) {
        for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
            lease_t *lease = &state->leases[index];

            if (lease->active && key_runtime_core_lease_kind(lease) == LEASE_KIND_PD_MODE && lease->data.pd_mode != mode) {
                changed |= key_runtime_core_release_pd_related_leases_for_token(state, lease->owner_token_id);
            }
        }

        changed |= key_runtime_core_clear_other_pd_mode_intents(state, mode);
    }

    changed |= key_runtime_core_persistent_pd_mode_lock_update(state, mode, active);
    changed |= key_runtime_core_persistent_pointer_toggle_update(state, mode, active && key_runtime_core_pd_mode_lock_owns_pointer_toggle(mode));

    if (changed) {
        key_runtime_core_shadow_projection_recompute(state);
    }
}
