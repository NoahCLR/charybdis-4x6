// ────────────────────────────────────────────────────────────────────────────
// Runtime V2 Foundation
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_v2.h"

#include <string.h>

#include "../pointing/policy/pd_mode_policy.h"
#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/keyboard_mod_ownership.h"
#include "../state/ownership/layer_ownership.h"
#include "../state/runtime/runtime_debug.h"
#include "runtime_v2_trace.h"

static uint8_t runtime_v2_refcount_mask(const uint8_t *refcounts, uint8_t count) {
    uint8_t mask = 0;

    if (!refcounts) {
        return 0;
    }

    for (uint8_t index = 0; index < count && index < 8u; index++) {
        if (refcounts[index] != 0u) {
            mask |= (uint8_t)(1u << index);
        }
    }

    return mask;
}

static bool runtime_v2_keypos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static uint16_t runtime_v2_keypos_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * MATRIX_COLS + key_pos.col);
}

static bool runtime_v2_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static uint16_t runtime_v2_default_hold_term(uint16_t keycode) {
    return (IS_QK_LAYER_TAP(keycode) || IS_QK_MOD_TAP(keycode)) ? TAPPING_TERM : CUSTOM_TAP_HOLD_TERM;
}

static uint16_t runtime_v2_default_longer_hold_term(void) {
    return CUSTOM_LONGER_HOLD_TERM;
}

static uint16_t runtime_v2_default_multi_tap_term(void) {
    return CUSTOM_MULTI_TAP_TERM;
}

static press_token_t *runtime_v2_press_token_state(runtime_v2_state_t *state, keypos_t key_pos) {
    if (!(state && runtime_v2_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->press_tokens[runtime_v2_keypos_index(key_pos)];
}

static tap_series_t *runtime_v2_tap_series_state(runtime_v2_state_t *state, keypos_t key_pos) {
    if (!(state && runtime_v2_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->tap_series[runtime_v2_keypos_index(key_pos)];
}

static pending_release_t *runtime_v2_allocate_pending_release(runtime_v2_state_t *state) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PENDING_RELEASE_CAPACITY; index++) {
        pending_release_t *pending = &state->pending_releases[index];

        if (!pending->active) {
            *pending = (pending_release_t){0};
            pending->active = true;
            state->pending_release_count++;
            return pending;
        }
    }

    return NULL;
}

static uint8_t runtime_v2_pending_release_count_for_owner_token(const runtime_v2_state_t *state, uint16_t owner_token_id) {
    uint8_t count = 0;

    if (!(state && owner_token_id != 0u)) {
        return 0u;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PENDING_RELEASE_CAPACITY; index++) {
        const pending_release_t *pending = &state->pending_releases[index];

        if (pending->active && pending->owner_token_id == owner_token_id) {
            count++;
        }
    }

    return count;
}

static uint16_t runtime_v2_elapsed(uint16_t start, uint16_t end) {
    return (uint16_t)(end - start);
}

static layer_state_t runtime_v2_resolution_layers(const runtime_v2_state_t *state) {
    return ((state ? state->shadow_projection.layer_state : 0u) | ((layer_state_t)1u << 0));
}

static bool runtime_v2_press_token_has_pending_hold_series(const runtime_v2_state_t *state, const press_token_t *token) {
    const tap_series_t *series;

    if (!(state && token && token->active)) {
        return false;
    }

    series = runtime_v2_tap_series_state((runtime_v2_state_t *)state, token->key_pos);
    return series && series->active && series->pending_hold && series->keycode == token->physical_keycode;
}

static bool runtime_v2_hold_semantics_owns_state_at_threshold(handled_key_hold_semantics_t semantics) {
    return handled_key_hold_semantics_registers_held(semantics) || handled_key_hold_semantics_repeats_while_held(semantics);
}

static bool runtime_v2_press_token_quick_tap_suppressed(const press_token_t *token) {
    return token && token->behavior_contract.suppress_tap_on_layer_interrupt && token->momentary_layer_tap_interrupted;
}

static bool runtime_v2_press_token_has_pd_mode_quick_lock_candidate(const press_token_t *token) {
    return token && token->behavior_contract.quick_tap_pd_mode_lock != 0 && token->pd_mode_was_locked_on_press &&
           !runtime_v2_press_token_quick_tap_suppressed(token);
}

static bool runtime_v2_press_token_owned_state_active(const runtime_v2_state_t *state, const press_token_t *token) {
    if (!(state && token && token->active)) {
        return false;
    }

    if (token->behavior_contract.quick_release_of_immediate_hold_dispatches_tap && !handled_key_hold_semantics_fires_at_threshold(token->behavior_contract.hold)) {
        return true;
    }

    if (runtime_v2_elapsed(token->pressed_at, state->current_time) < token->hold_term_ms) {
        return false;
    }

    return runtime_v2_hold_semantics_owns_state_at_threshold(token->behavior_contract.hold);
}

static void runtime_v2_press_token_deferred_release_profile(const runtime_v2_state_t *state, const press_token_t *token, bool *before_tap_term, bool *after_tap_term) {
    bool before = false;
    bool after  = false;

    if (!(state && token && token->active && token->handled_key)) {
        goto done;
    }

    if (runtime_v2_press_token_has_pending_hold_series(state, token) || runtime_v2_press_token_owned_state_active(state, token)) {
        goto done;
    }

    if (token->behavior_contract.buffered_base_tap_dispatches_tap) {
        before = true;
        after  = true;
        goto done;
    }

    if (!runtime_v2_press_token_quick_tap_suppressed(token)) {
        before = token->tap_outcome_available || runtime_v2_press_token_has_pd_mode_quick_lock_candidate(token);
    }

    after = token->behavior_contract.nonquick_release_dispatches_tap;

done:
    if (before_tap_term) {
        *before_tap_term = before;
    }

    if (after_tap_term) {
        *after_tap_term = after;
    }
}

static bool runtime_v2_press_token_deferred_release_blocker_tracks_tap_term(const runtime_v2_state_t *state, const press_token_t *token) {
    bool before = false;
    bool after  = false;

    runtime_v2_press_token_deferred_release_profile(state, token, &before, &after);
    return before != after;
}

static bool runtime_v2_press_token_blocks_deferred_release(const runtime_v2_state_t *state, const press_token_t *token) {
    bool before = false;
    bool after  = false;

    if (!(state && token && token->active)) {
        return false;
    }

    runtime_v2_press_token_deferred_release_profile(state, token, &before, &after);
    if (before == after) {
        return before;
    }

    return runtime_v2_elapsed(token->pressed_at, state->current_time) < token->hold_term_ms ? before : after;
}

static uint8_t runtime_v2_effective_deferred_release_blocker_count(const runtime_v2_state_t *state) {
    uint8_t count = 0;

    if (!state) {
        return 0u;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PRESS_TOKEN_CAPACITY; index++) {
        if (runtime_v2_press_token_blocks_deferred_release(state, &state->press_tokens[index])) {
            count++;
        }
    }

    return count;
}

static uint8_t runtime_v2_timed_deferred_release_blocker_count(const runtime_v2_state_t *state) {
    uint8_t count = 0;

    if (!state) {
        return 0u;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PRESS_TOKEN_CAPACITY; index++) {
        if (runtime_v2_press_token_deferred_release_blocker_tracks_tap_term(state, &state->press_tokens[index])) {
            count++;
        }
    }

    return count;
}

static bool runtime_v2_has_foreign_effective_deferred_release_blocker_except(const runtime_v2_state_t *state, keypos_t key_pos) {
    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];

        if (!runtime_v2_press_token_blocks_deferred_release(state, token) || runtime_v2_keypos_equal(token->key_pos, key_pos)) {
            continue;
        }

        return true;
    }

    return false;
}

static uint8_t runtime_v2_modifier_mask_for_keycode(uint16_t keycode) {
    if (IS_MODIFIER_KEYCODE(keycode)) {
        return MOD_BIT(keycode);
    }

    if (IS_QK_MOD_TAP(keycode)) {
        return QK_MOD_TAP_GET_MODS(keycode);
    }

    return 0u;
}

static bool runtime_v2_keycode_owns_layer_on_press(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode);
}

static bool runtime_v2_keycode_owns_layer_on_hold(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode);
}

static uint8_t runtime_v2_layer_for_keycode(uint16_t keycode) {
    if (IS_QK_MOMENTARY(keycode)) {
        return QK_MOMENTARY_GET_LAYER(keycode);
    }

    if (IS_QK_LAYER_TAP(keycode)) {
        return QK_LAYER_TAP_GET_LAYER(keycode);
    }

    return UINT8_MAX;
}

static bool runtime_v2_keycode_owns_modifier_on_press(uint16_t keycode) {
    return IS_MODIFIER_KEYCODE(keycode);
}

static bool runtime_v2_keycode_owns_modifier_on_hold(uint16_t keycode) {
    return IS_QK_MOD_TAP(keycode);
}

static pd_mode_mask_t runtime_v2_pd_mode_for_keycode(uint16_t keycode) {
    return pd_mode_for_keycode(keycode);
}

static bool runtime_v2_pd_mode_keeps_auto_mouse_anchored(pd_mode_mask_t mode) {
    return mode != 0 && pd_mode_policy_mode_keeps_auto_mouse_anchored(mode);
}

static bool runtime_v2_pd_mode_prefers_typing_layer(pd_mode_mask_t mode) {
    return mode != 0 && pd_mode_policy_mode_prefers_typing_layer(mode);
}

static bool runtime_v2_pd_mode_lock_owns_pointer_toggle(pd_mode_mask_t mode) {
    return mode != 0 && pd_mode_has_trait(mode, PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE);
}

#ifdef AUTO_MOUSE_DEFAULT_LAYER
static uint8_t runtime_v2_default_pointer_layer(void) {
    return AUTO_MOUSE_DEFAULT_LAYER;
}
#else
static uint8_t runtime_v2_default_pointer_layer(void) {
    return 0u;
}
#endif

static lease_t *runtime_v2_find_pd_mode_lease(runtime_v2_state_t *state, uint16_t owner_token_id, pd_mode_mask_t mode) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && lease->kind == LEASE_KIND_PD_MODE && lease->owner_token_id == owner_token_id && lease->data.pd_mode == mode) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *runtime_v2_find_pointer_anchor_lease(runtime_v2_state_t *state, uint16_t owner_token_id) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && lease->kind == LEASE_KIND_POINTER_ANCHOR && lease->owner_token_id == owner_token_id) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *runtime_v2_find_modifier_lease(runtime_v2_state_t *state, uint16_t owner_token_id, uint8_t modifiers, bool physical) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && lease->kind == LEASE_KIND_MODIFIER && lease->owner_token_id == owner_token_id &&
            lease->data.modifier.modifiers == modifiers && lease->data.modifier.physical == physical) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *runtime_v2_find_layer_lease(runtime_v2_state_t *state, uint16_t owner_token_id, uint8_t layer) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && lease->kind == LEASE_KIND_LAYER && lease->owner_token_id == owner_token_id && lease->data.layer == layer) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *runtime_v2_allocate_lease(runtime_v2_state_t *state) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (!lease->active) {
            *lease = (lease_t){0};
            lease->active = true;
            state->lease_count++;
            return lease;
        }
    }

    return NULL;
}

static bool runtime_v2_layer_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, uint8_t layer) {
    lease_t *lease;

    if (!(state && layer < 32u)) {
        return false;
    }

    if (runtime_v2_find_layer_lease(state, owner_token_id, layer)) {
        return false;
    }

    lease = runtime_v2_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active         = true,
        .kind           = LEASE_KIND_LAYER,
        .owner_token_id = owner_token_id,
        .data.layer     = layer,
    };
    return true;
}

static bool runtime_v2_modifier_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, uint8_t modifiers, bool physical) {
    lease_t *lease;

    if (!(state && modifiers != 0u)) {
        return false;
    }

    if (runtime_v2_find_modifier_lease(state, owner_token_id, modifiers, physical)) {
        return false;
    }

    lease = runtime_v2_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active         = true,
        .kind           = LEASE_KIND_MODIFIER,
        .owner_token_id = owner_token_id,
        .data.modifier =
            {
                .modifiers = modifiers,
                .physical  = physical,
            },
    };
    return true;
}

static bool runtime_v2_pd_mode_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, pd_mode_mask_t mode) {
    lease_t *lease;

    if (!(state && mode != 0)) {
        return false;
    }

    if (runtime_v2_find_pd_mode_lease(state, owner_token_id, mode)) {
        return false;
    }

    lease = runtime_v2_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active         = true,
        .kind           = LEASE_KIND_PD_MODE,
        .owner_token_id = owner_token_id,
        .data.pd_mode   = mode,
    };
    return true;
}

static bool runtime_v2_pointer_anchor_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, bool keep_typing_surface) {
    lease_t *lease;

    if (!state) {
        return false;
    }

    if (runtime_v2_find_pointer_anchor_lease(state, owner_token_id)) {
        return false;
    }

    lease = runtime_v2_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active         = true,
        .kind           = LEASE_KIND_POINTER_ANCHOR,
        .owner_token_id = owner_token_id,
        .data.pointer_anchor =
            {
                .layer               = runtime_v2_default_pointer_layer(),
                .keep_typing_surface = keep_typing_surface,
            },
    };
    return true;
}

static bool runtime_v2_persistent_layer_lock_update(runtime_v2_state_t *state, uint8_t layer, bool active) {
    persistent_intent_t *empty_slot = NULL;

    if (!(state && layer < 32u)) {
        return false;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PERSISTENT_INTENT_CAPACITY; index++) {
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
        .active = true,
        .kind   = PERSISTENT_INTENT_KIND_LAYER_LOCK,
        .data.layer = layer,
    };
    state->persistent_intent_count++;
    return true;
}

static bool runtime_v2_persistent_pd_mode_lock_update(runtime_v2_state_t *state, pd_mode_mask_t mode, bool active) {
    persistent_intent_t *empty_slot = NULL;
    bool                 changed    = false;

    if (!(state && mode != 0)) {
        return false;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PERSISTENT_INTENT_CAPACITY; index++) {
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

static bool runtime_v2_persistent_pointer_toggle_update(runtime_v2_state_t *state, pd_mode_mask_t mode, bool active) {
    persistent_intent_t *empty_slot = NULL;
    bool                 changed    = false;

    if (!(state && mode != 0)) {
        return false;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PERSISTENT_INTENT_CAPACITY; index++) {
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
                .pointer_layer = runtime_v2_default_pointer_layer(),
                .mode          = mode,
            },
    };
    state->persistent_intent_count++;
    return true;
}

static bool runtime_v2_clear_other_pd_mode_intents(runtime_v2_state_t *state, pd_mode_mask_t keep_mode) {
    bool changed = false;

    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PERSISTENT_INTENT_CAPACITY; index++) {
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

static void runtime_v2_release_leases_for_token(runtime_v2_state_t *state, uint16_t owner_token_id) {
    if (!state) {
        return;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && lease->owner_token_id == owner_token_id) {
            *lease = (lease_t){0};
            if (state->lease_count != 0u) {
                state->lease_count--;
            }
        }
    }
}

static void runtime_v2_release_pd_related_leases_for_token(runtime_v2_state_t *state, uint16_t owner_token_id) {
    if (!state) {
        return;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (!lease->active || lease->owner_token_id != owner_token_id) {
            continue;
        }

        if (lease->kind == LEASE_KIND_PD_MODE || lease->kind == LEASE_KIND_POINTER_ANCHOR) {
            *lease = (lease_t){0};
            if (state->lease_count != 0u) {
                state->lease_count--;
            }
        }
    }
}

static void runtime_v2_shadow_projection_recompute(runtime_v2_state_t *state) {
    runtime_v2_shadow_projection_t projection = {0};
    bool                           pointer_anchor_lease_active = false;

    if (!state) {
        return;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        const lease_t *lease = &state->leases[index];

        if (!lease->active) {
            continue;
        }

        switch (lease->kind) {
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

    for (uint16_t index = 0; index < RUNTIME_V2_PERSISTENT_INTENT_CAPACITY; index++) {
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

    projection.pointer_pd_mode_anchor_active = runtime_v2_pd_mode_keeps_auto_mouse_anchored(projection.pd_mode_local_active);
    projection.pointer_prefers_typing_layer  = runtime_v2_pd_mode_prefers_typing_layer(projection.pd_mode_local_active);
    projection.pointer_anchor_active         = projection.pointer_toggle_enabled || pointer_anchor_lease_active || projection.pointer_pd_mode_anchor_active;

    state->shadow_projection = projection;
}

static void runtime_v2_press_token_attach_press_leases(runtime_v2_state_t *state, const press_token_t *token) {
    bool changed = false;
    pd_mode_mask_t mode;

    if (!(state && token && token->active)) {
        return;
    }

    if (runtime_v2_keycode_owns_layer_on_press(token->resolved_keycode)) {
        uint8_t layer = runtime_v2_layer_for_keycode(token->resolved_keycode);

        if (layer != UINT8_MAX) {
            changed |= runtime_v2_layer_lease_activate(state, token->token_id, layer);
        }
    }

    if (runtime_v2_keycode_owns_modifier_on_press(token->resolved_keycode)) {
        changed |= runtime_v2_modifier_lease_activate(state, token->token_id, runtime_v2_modifier_mask_for_keycode(token->resolved_keycode), true);
    }

    mode = runtime_v2_pd_mode_for_keycode(token->resolved_keycode);
    if (mode != 0) {
        for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
            lease_t *lease = &state->leases[index];

            if (lease->active && lease->kind == LEASE_KIND_PD_MODE && lease->data.pd_mode != mode) {
                runtime_v2_release_pd_related_leases_for_token(state, lease->owner_token_id);
                changed = true;
            }
        }

        changed |= runtime_v2_clear_other_pd_mode_intents(state, mode);
        changed |= runtime_v2_pd_mode_lease_activate(state, token->token_id, mode);
        if (runtime_v2_pd_mode_keeps_auto_mouse_anchored(mode)) {
            changed |= runtime_v2_pointer_anchor_lease_activate(state, token->token_id, runtime_v2_pd_mode_prefers_typing_layer(mode));
        }
    }

    if (changed) {
        runtime_v2_shadow_projection_recompute(state);
    }
}

static void runtime_v2_press_token_attach_hold_leases(runtime_v2_state_t *state, const press_token_t *token) {
    bool changed = false;

    if (!(state && token && token->active && token->phase == PRESS_TOKEN_PHASE_HELD)) {
        return;
    }

    if (runtime_v2_keycode_owns_layer_on_hold(token->resolved_keycode)) {
        uint8_t layer = runtime_v2_layer_for_keycode(token->resolved_keycode);

        if (layer != UINT8_MAX) {
            changed |= runtime_v2_layer_lease_activate(state, token->token_id, layer);
        }
    }

    if (runtime_v2_keycode_owns_modifier_on_hold(token->resolved_keycode)) {
        changed |= runtime_v2_modifier_lease_activate(state, token->token_id, runtime_v2_modifier_mask_for_keycode(token->resolved_keycode), false);
    }

    if (changed) {
        runtime_v2_shadow_projection_recompute(state);
    }
}

static void runtime_v2_press_token_cancel(runtime_v2_state_t *state, press_token_t *token, uint16_t now) {
    if (!(state && token && token->active)) {
        return;
    }

    runtime_v2_release_leases_for_token(state, token->token_id);
    runtime_v2_shadow_projection_recompute(state);
    token->active      = false;
    token->released_at = now;
    token->phase       = PRESS_TOKEN_PHASE_CANCELLED;
    if (state->press_token_count != 0u) {
        state->press_token_count--;
    }
}

static void runtime_v2_press_token_refresh_phase(runtime_v2_state_t *state, press_token_t *token, uint16_t now) {
    press_token_phase_t previous_phase;

    if (!(token && token->active)) {
        return;
    }

    previous_phase = token->phase;

    if (token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING || token->phase == PRESS_TOKEN_PHASE_RELEASED || token->phase == PRESS_TOKEN_PHASE_CANCELLED) {
        return;
    }

    if (runtime_v2_elapsed(token->pressed_at, now) >= token->hold_term_ms) {
        token->phase = PRESS_TOKEN_PHASE_HELD;
    } else if (token->phase == PRESS_TOKEN_PHASE_PRESSED) {
        token->phase = PRESS_TOKEN_PHASE_HOLD_PENDING;
    }

    if (token->phase != previous_phase && token->phase == PRESS_TOKEN_PHASE_HELD) {
        runtime_v2_press_token_attach_hold_leases(state, token);
    }
}

static void runtime_v2_tap_series_release_if_expired(tap_series_t *series, uint16_t now, runtime_v2_state_t *state) {
    if (!(series && state && series->active)) {
        return;
    }

    if (series->pending_hold) {
        return;
    }

    if (runtime_v2_elapsed(series->last_tap_at, now) <= series->tap_term_ms) {
        return;
    }

    *series = (tap_series_t){0};
    if (state->tap_series_count != 0u) {
        state->tap_series_count--;
    }
}

static void runtime_v2_refresh_for_time(runtime_v2_state_t *state, uint16_t now) {
    if (!state) {
        return;
    }

    state->current_time = now;

    for (uint16_t index = 0; index < RUNTIME_V2_PRESS_TOKEN_CAPACITY; index++) {
        runtime_v2_press_token_refresh_phase(state, &state->press_tokens[index], now);
        runtime_v2_tap_series_release_if_expired(&state->tap_series[index], now, state);
    }
}

static void runtime_v2_press_token_begin(runtime_v2_state_t *state, const runtime_key_event_t *event, uint16_t now) {
    press_token_t             *token;
    tap_series_t              *series;
    handled_key_resolution_t   resolution;
    handled_key_materialized_t materialized;
    handled_key_resolution_ctx_t ctx;
    uint16_t                   hold_term_ms;
    uint16_t                   longer_hold_term_ms;
    uint8_t                    tap_count = 1u;
    bool                       handled   = false;
    bool                       tap_outcome_available = false;
    bool                       pd_mode_was_locked_on_press = false;

    if (!(state && event)) {
        return;
    }

    token = runtime_v2_press_token_state(state, event->key_pos);
    series = runtime_v2_tap_series_state(state, event->key_pos);
    if (!token) {
        return;
    }

    if (token->active) {
        state->cancelled_press_count++;
        runtime_v2_press_token_cancel(state, token, now);
    }

    if (series && series->active && series->keycode == event->keycode && runtime_v2_elapsed(series->last_tap_at, now) <= series->tap_term_ms) {
        tap_count = (uint8_t)(series->tap_count + 1u);
    }

    resolution           = handled_key_lookup_tap_count(event->keycode, tap_count);
    handled              = handled_key_resolution_is_handled(resolution);
    materialized         = handled_key_materialized_default(resolution);
    hold_term_ms = runtime_v2_default_hold_term(event->keycode);
    longer_hold_term_ms  = runtime_v2_default_longer_hold_term();

    if (handled) {
        ctx                      = handled_key_resolution_ctx_make(event->key_pos, runtime_v2_resolution_layers(state));
        materialized             = handled_key_materialize(resolution, ctx);
        hold_term_ms             = handled_key_resolution_tap_hold_term(resolution);
        longer_hold_term_ms      = handled_key_resolution_longer_hold_term(resolution);
        tap_outcome_available    = handled_key_resolution_has_multi_tap(resolution) || materialized.tap_action != KC_NO;
        pd_mode_was_locked_on_press = materialized.pd_mode != 0 && state->shadow_projection.pd_mode_local_locked == materialized.pd_mode;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *other = &state->press_tokens[index];

        if (!(other->active && !runtime_v2_keypos_equal(other->key_pos, event->key_pos))) {
            continue;
        }

        other->other_press_interrupted = true;
        if (other->behavior_contract.suppress_tap_on_layer_interrupt) {
            other->momentary_layer_tap_interrupted = true;
        }
    }

    *token = (press_token_t){
        .active                   = true,
        .token_id                 = state->next_token_id++,
        .key_pos                  = event->key_pos,
        .physical_keycode         = event->keycode,
        .resolved_keycode         = event->keycode,
        .observed_release_keycode = KC_NO,
        .pressed_at               = now,
        .hold_term_ms             = hold_term_ms,
        .longer_hold_term_ms      = longer_hold_term_ms,
        .phase                    = PRESS_TOKEN_PHASE_PRESSED,
        .behavior_contract        = materialized.contract,
        .handled_key              = handled,
        .tap_outcome_available    = tap_outcome_available,
        .pd_mode_was_locked_on_press = pd_mode_was_locked_on_press,
    };
    state->press_token_count++;
    runtime_v2_press_token_attach_press_leases(state, token);

    if (series && tap_count > 1u) {
        series->pending_hold = true;
    }
}

static void runtime_v2_tap_series_note_tap(runtime_v2_state_t *state, const press_token_t *token, uint16_t now) {
    tap_series_t *series;
    bool          reuse_existing;

    if (!(state && token && token->active)) {
        return;
    }

    series = runtime_v2_tap_series_state(state, token->key_pos);
    if (!series) {
        return;
    }

    reuse_existing = series->active && series->keycode == token->resolved_keycode && runtime_v2_elapsed(series->last_tap_at, now) <= series->tap_term_ms;

    if (!series->active) {
        state->tap_series_count++;
    }

    *series = (tap_series_t){
        .active       = true,
        .key_pos      = token->key_pos,
        .keycode      = token->resolved_keycode,
        .tap_count    = (uint8_t)(reuse_existing ? (uint8_t)(series->tap_count + 1u) : 1u),
        .pending_hold = false,
        .last_action  = token->resolved_keycode,
        .last_tap_at  = now,
        .tap_term_ms  = runtime_v2_default_multi_tap_term(),
    };
}

static void runtime_v2_tap_series_note_hold_release(runtime_v2_state_t *state, const press_token_t *token) {
    tap_series_t *series;

    if (!(state && token)) {
        return;
    }

    series = runtime_v2_tap_series_state(state, token->key_pos);
    if (!(series && series->active && series->pending_hold && series->keycode == token->resolved_keycode)) {
        return;
    }

    series->pending_hold = false;
}

static bool runtime_v2_pending_release_matches(const pending_release_t *pending, keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    return pending && pending->active && runtime_v2_keypos_equal(pending->key_pos, key_pos) && pending->action == action && pending->mods.real == mods.real &&
           pending->mods.weak == mods.weak && pending->mods.oneshot == mods.oneshot && pending->mods.oneshot_locked == mods.oneshot_locked;
}

static void runtime_v2_pending_release_mark_token(runtime_v2_state_t *state, uint16_t owner_token_id) {
    if (!(state && owner_token_id != 0u)) {
        return;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token = &state->press_tokens[index];

        if (token->token_id == owner_token_id && !token->active) {
            token->pending_release_emission = true;
            token->phase                    = PRESS_TOKEN_PHASE_RELEASE_PENDING;
            return;
        }
    }
}

static void runtime_v2_pending_release_clear_token(runtime_v2_state_t *state, uint16_t owner_token_id) {
    if (!(state && owner_token_id != 0u) || runtime_v2_pending_release_count_for_owner_token(state, owner_token_id) != 0u) {
        return;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PRESS_TOKEN_CAPACITY; index++) {
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

static void runtime_v2_press_token_end(runtime_v2_state_t *state, const runtime_key_event_t *event, uint16_t now) {
    press_token_t released_token;
    press_token_t *token;

    if (!(state && event)) {
        return;
    }

    token = runtime_v2_press_token_state(state, event->key_pos);
    if (!(token && token->active)) {
        state->orphan_release_count++;
        return;
    }

    runtime_v2_press_token_refresh_phase(state, token, now);
    token->observed_release_keycode = event->keycode;
    token->released_at              = now;
    if (event->keycode != token->resolved_keycode) {
        token->release_keycode_mismatched = true;
        state->release_keycode_mismatch_count++;
    }

    released_token = *token;
    if (runtime_v2_elapsed(token->pressed_at, now) < token->hold_term_ms) {
        runtime_v2_tap_series_note_tap(state, token, now);
    } else {
        runtime_v2_tap_series_note_hold_release(state, token);
    }

    runtime_v2_release_leases_for_token(state, token->token_id);
    runtime_v2_shadow_projection_recompute(state);
    *token = released_token;
    token->active = false;
    token->phase  = PRESS_TOKEN_PHASE_RELEASED;
    if (state->press_token_count != 0u) {
        state->press_token_count--;
    }
}

void runtime_v2_apply_event(const runtime_event_t *event, uint16_t event_time) {
    runtime_v2_state_t *state = runtime_v2_state();

    if (!(state && event)) {
        return;
    }

    state->input_stream_observed = true;

    switch (event->kind) {
        case RUNTIME_EVENT_KIND_KEY_DOWN:
            runtime_v2_refresh_for_time(state, event_time);
            runtime_v2_press_token_begin(state, &event->data.key_event, event_time);
            return;
        case RUNTIME_EVENT_KIND_KEY_UP:
            runtime_v2_refresh_for_time(state, event_time);
            runtime_v2_press_token_end(state, &event->data.key_event, event_time);
            return;
        case RUNTIME_EVENT_KIND_TIMER_ADVANCE:
            runtime_v2_refresh_for_time(state, (uint16_t)(event_time + event->data.timer_advance.advance_ms));
            return;
        case RUNTIME_EVENT_KIND_SCAN:
        case RUNTIME_EVENT_KIND_POINTER_REPORT:
        case RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT:
            runtime_v2_refresh_for_time(state, event_time);
            return;
    }
}

void runtime_v2_observe_process_record_event(uint16_t keycode, keyrecord_t *record) {
    runtime_event_t event;

    if (!record) {
        return;
    }

    event = (runtime_event_t){
        .kind = record->event.pressed ? RUNTIME_EVENT_KIND_KEY_DOWN : RUNTIME_EVENT_KIND_KEY_UP,
        .data.key_event =
            {
                .keycode = keycode,
                .key_pos = record->event.key,
            },
    };
    runtime_v2_apply_event(&event, timer_read());
}

void runtime_v2_observe_scan_cycle(uint16_t now) {
    runtime_v2_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, now);
}

bool runtime_v2_blocker_queries_authoritative(void) {
    runtime_v2_state_t *state = runtime_v2_state();

    return state && state->input_stream_observed;
}

bool runtime_v2_has_any_deferred_release_blocker(void) {
    runtime_v2_state_t *state = runtime_v2_state();

    return runtime_v2_blocker_queries_authoritative() && runtime_v2_effective_deferred_release_blocker_count(state) != 0u;
}

bool runtime_v2_has_foreign_deferred_release_blocker_except(keypos_t key_pos) {
    runtime_v2_state_t *state = runtime_v2_state();

    return runtime_v2_blocker_queries_authoritative() && runtime_v2_has_foreign_effective_deferred_release_blocker_except(state, key_pos);
}

const press_token_t *runtime_v2_press_token_at(keypos_t key_pos) {
    return runtime_v2_press_token_state(runtime_v2_state(), key_pos);
}

const tap_series_t *runtime_v2_tap_series_at(keypos_t key_pos) {
    return runtime_v2_tap_series_state(runtime_v2_state(), key_pos);
}

const runtime_v2_shadow_projection_t *runtime_v2_shadow_projection(void) {
    runtime_v2_state_t *state = runtime_v2_state();

    return state ? &state->shadow_projection : NULL;
}

uint8_t runtime_v2_pending_release_count_for_keypos(keypos_t key_pos) {
    runtime_v2_state_t *state = runtime_v2_state();
    uint8_t             count = 0;

    if (!(state && runtime_v2_keypos_valid(key_pos))) {
        return 0u;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PENDING_RELEASE_CAPACITY; index++) {
        const pending_release_t *pending = &state->pending_releases[index];

        if (pending->active && runtime_v2_keypos_equal(pending->key_pos, key_pos)) {
            count++;
        }
    }

    return count;
}

uint8_t runtime_v2_deferred_release_blocker_count_for_keypos(keypos_t key_pos) {
    runtime_v2_state_t *state = runtime_v2_state();
    press_token_t      *token = runtime_v2_press_token_state(state, key_pos);

    return runtime_v2_press_token_blocks_deferred_release(state, token) ? 1u : 0u;
}

void runtime_v2_layer_lock_set(uint8_t layer, bool active) {
    runtime_v2_state_t *state = runtime_v2_state();

    if (runtime_v2_persistent_layer_lock_update(state, layer, active)) {
        runtime_v2_shadow_projection_recompute(state);
    }
}

void runtime_v2_pd_mode_lock_set(pd_mode_mask_t mode, bool active) {
    runtime_v2_state_t *state   = runtime_v2_state();
    bool                changed = false;

    if (!(state && mode != 0)) {
        return;
    }

    if (active) {
        for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
            lease_t *lease = &state->leases[index];

            if (lease->active && lease->kind == LEASE_KIND_PD_MODE && lease->data.pd_mode != mode) {
                runtime_v2_release_pd_related_leases_for_token(state, lease->owner_token_id);
                changed = true;
            }
        }

        changed |= runtime_v2_clear_other_pd_mode_intents(state, mode);
    }

    changed |= runtime_v2_persistent_pd_mode_lock_update(state, mode, active);
    changed |= runtime_v2_persistent_pointer_toggle_update(state, mode, active && runtime_v2_pd_mode_lock_owns_pointer_toggle(mode));

    if (changed) {
        runtime_v2_shadow_projection_recompute(state);
    }
}

void runtime_v2_observe_release_dispatch_deferred(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    runtime_v2_state_t *state = runtime_v2_state();
    press_token_t      *token;
    pending_release_t  *pending;

    if (!(state && action != KC_NO && runtime_v2_keypos_valid(key_pos))) {
        return;
    }

    pending = runtime_v2_allocate_pending_release(state);
    if (!pending) {
        return;
    }

    token = runtime_v2_press_token_state(state, key_pos);
    *pending = (pending_release_t){
        .active         = true,
        .owner_token_id = token ? token->token_id : 0u,
        .key_pos        = key_pos,
        .action         = action,
        .mods           = mods,
    };
    runtime_v2_pending_release_mark_token(state, pending->owner_token_id);
}

void runtime_v2_observe_release_dispatch_drained(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    runtime_v2_state_t *state = runtime_v2_state();

    if (!(state && action != KC_NO && runtime_v2_keypos_valid(key_pos))) {
        return;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PENDING_RELEASE_CAPACITY; index++) {
        pending_release_t pending = state->pending_releases[index];

        if (!runtime_v2_pending_release_matches(&pending, key_pos, action, mods)) {
            continue;
        }

        state->pending_releases[index] = (pending_release_t){0};
        if (state->pending_release_count != 0u) {
            state->pending_release_count--;
        }
        runtime_v2_pending_release_clear_token(state, pending.owner_token_id);
        return;
    }
}

void runtime_v2_observe_deferred_release_blocker_profile(keypos_t key_pos, bool active, bool blocks_before_tap_term, bool blocks_after_tap_term) {
    (void)key_pos;
    (void)active;
    (void)blocks_before_tap_term;
    (void)blocks_after_tap_term;
}

projection_snapshot_t runtime_v2_projection_snapshot_capture(void) {
    projection_snapshot_t                  snapshot = {0};
    layer_ownership_debug_snapshot_t       layer_snapshot;
    keyboard_mod_ownership_debug_snapshot_t mod_snapshot;
    pointer_layer_policy_debug_snapshot_t  pointer_snapshot;
    runtime_v2_state_t                    *state = runtime_v2_state();

    memset(&layer_snapshot, 0, sizeof(layer_snapshot));
    memset(&mod_snapshot, 0, sizeof(mod_snapshot));
    memset(&pointer_snapshot, 0, sizeof(pointer_snapshot));

    layer_ownership_debug_snapshot(&layer_snapshot);
    keyboard_mod_ownership_debug_snapshot(&mod_snapshot);
    pointer_layer_policy_debug_snapshot(layer_state, &pointer_snapshot);

    snapshot.layer_state                  = layer_state;
    snapshot.locked_layer_mask            = layer_snapshot.locked_mask;
    snapshot.keyboard_mod_state           = mod_snapshot.live_state;
    snapshot.keyboard_managed_mod_mask    = runtime_v2_refcount_mask(mod_snapshot.managed_refcounts, KEYBOARD_MOD_OWNERSHIP_MOD_COUNT);
    snapshot.keyboard_physical_mod_mask   = runtime_v2_refcount_mask(mod_snapshot.physical_refcounts, KEYBOARD_MOD_OWNERSHIP_MOD_COUNT);
    snapshot.pd_mode_local_active         = pd_mode_local_active_snapshot();
    snapshot.pd_mode_local_locked         = pd_mode_local_locked_snapshot();
    snapshot.pd_mode_display_active       = pd_mode_display_active_snapshot();
    snapshot.pd_mode_display_locked       = pd_mode_display_locked_snapshot();
    snapshot.pointer_anchor_active        = pointer_snapshot.auto_mouse_anchored;
    snapshot.pointer_pd_mode_anchor_active = pointer_snapshot.pd_mode_anchor_active;
    snapshot.pointer_prefers_typing_layer = pointer_snapshot.prefers_typing_layer;
    snapshot.pointer_toggle_enabled       = pointer_snapshot.auto_mouse_toggle_enabled;
    snapshot.pointer_sniping_layer_active = pointer_snapshot.sniping_layer_active;
    snapshot.pointer_key_tracker          = pointer_snapshot.auto_mouse_key_tracker;
    snapshot.pointer_layer                = pointer_snapshot.auto_mouse_layer;
    snapshot.active_slot_count            = noah_runtime_debug_active_slot_count();
    snapshot.pending_multi_tap_slot_count = noah_runtime_debug_pending_multi_tap_slot_count();
    snapshot.deferred_release_count       = noah_runtime_debug_deferred_release_count();
    snapshot.deferred_release_blocker_count = noah_runtime_debug_deferred_release_blocker_count();
    snapshot.deferred_release_timed_blocker_count = noah_runtime_debug_deferred_release_timed_blocker_count();

    if (state) {
        snapshot.v2_shadow_layer_state     = state->shadow_projection.layer_state;
        snapshot.v2_shadow_locked_layer_mask = state->shadow_projection.locked_layer_mask;
        snapshot.v2_shadow_keyboard_mod_state = state->shadow_projection.keyboard_mod_state;
        snapshot.v2_shadow_keyboard_managed_mod_mask = state->shadow_projection.keyboard_managed_mod_mask;
        snapshot.v2_shadow_keyboard_physical_mod_mask = state->shadow_projection.keyboard_physical_mod_mask;
        snapshot.v2_shadow_pd_mode_local_active = state->shadow_projection.pd_mode_local_active;
        snapshot.v2_shadow_pd_mode_local_locked = state->shadow_projection.pd_mode_local_locked;
        snapshot.v2_shadow_pointer_anchor_active = state->shadow_projection.pointer_anchor_active;
        snapshot.v2_shadow_pointer_pd_mode_anchor_active = state->shadow_projection.pointer_pd_mode_anchor_active;
        snapshot.v2_shadow_pointer_prefers_typing_layer = state->shadow_projection.pointer_prefers_typing_layer;
        snapshot.v2_shadow_pointer_toggle_enabled = state->shadow_projection.pointer_toggle_enabled;
        snapshot.v2_press_token_count     = state->press_token_count;
        snapshot.v2_tap_series_count      = state->tap_series_count;
        snapshot.v2_lease_count           = state->lease_count;
        snapshot.v2_pending_release_count = state->pending_release_count;
        snapshot.v2_deferred_release_blocker_count = runtime_v2_effective_deferred_release_blocker_count(state);
        snapshot.v2_deferred_release_timed_blocker_count = runtime_v2_timed_deferred_release_blocker_count(state);
        snapshot.v2_persistent_intent_count = state->persistent_intent_count;
        snapshot.v2_release_keycode_mismatch_count = state->release_keycode_mismatch_count;
        snapshot.v2_orphan_release_count = state->orphan_release_count;
        snapshot.v2_cancelled_press_count = state->cancelled_press_count;
        state->last_projection            = snapshot;
    }

    return snapshot;
}

bool runtime_v2_projection_snapshot_equal(const projection_snapshot_t *lhs, const projection_snapshot_t *rhs) {
    if (!(lhs && rhs)) {
        return lhs == rhs;
    }

    return lhs->layer_state == rhs->layer_state &&
           lhs->locked_layer_mask == rhs->locked_layer_mask &&
           lhs->keyboard_mod_state.real == rhs->keyboard_mod_state.real &&
           lhs->keyboard_mod_state.weak == rhs->keyboard_mod_state.weak &&
           lhs->keyboard_mod_state.oneshot == rhs->keyboard_mod_state.oneshot &&
           lhs->keyboard_mod_state.oneshot_locked == rhs->keyboard_mod_state.oneshot_locked &&
           lhs->keyboard_managed_mod_mask == rhs->keyboard_managed_mod_mask &&
           lhs->keyboard_physical_mod_mask == rhs->keyboard_physical_mod_mask &&
           lhs->pd_mode_local_active == rhs->pd_mode_local_active &&
           lhs->pd_mode_local_locked == rhs->pd_mode_local_locked &&
           lhs->pd_mode_display_active == rhs->pd_mode_display_active &&
           lhs->pd_mode_display_locked == rhs->pd_mode_display_locked &&
           lhs->pointer_anchor_active == rhs->pointer_anchor_active &&
           lhs->pointer_pd_mode_anchor_active == rhs->pointer_pd_mode_anchor_active &&
           lhs->pointer_prefers_typing_layer == rhs->pointer_prefers_typing_layer &&
           lhs->pointer_toggle_enabled == rhs->pointer_toggle_enabled &&
           lhs->pointer_sniping_layer_active == rhs->pointer_sniping_layer_active &&
           lhs->pointer_key_tracker == rhs->pointer_key_tracker &&
           lhs->pointer_layer == rhs->pointer_layer &&
           lhs->active_slot_count == rhs->active_slot_count &&
           lhs->pending_multi_tap_slot_count == rhs->pending_multi_tap_slot_count &&
           lhs->deferred_release_count == rhs->deferred_release_count &&
           lhs->deferred_release_blocker_count == rhs->deferred_release_blocker_count &&
           lhs->deferred_release_timed_blocker_count == rhs->deferred_release_timed_blocker_count &&
           lhs->v2_shadow_layer_state == rhs->v2_shadow_layer_state &&
           lhs->v2_shadow_locked_layer_mask == rhs->v2_shadow_locked_layer_mask &&
           lhs->v2_shadow_keyboard_mod_state.real == rhs->v2_shadow_keyboard_mod_state.real &&
           lhs->v2_shadow_keyboard_mod_state.weak == rhs->v2_shadow_keyboard_mod_state.weak &&
           lhs->v2_shadow_keyboard_mod_state.oneshot == rhs->v2_shadow_keyboard_mod_state.oneshot &&
           lhs->v2_shadow_keyboard_mod_state.oneshot_locked == rhs->v2_shadow_keyboard_mod_state.oneshot_locked &&
           lhs->v2_shadow_keyboard_managed_mod_mask == rhs->v2_shadow_keyboard_managed_mod_mask &&
           lhs->v2_shadow_keyboard_physical_mod_mask == rhs->v2_shadow_keyboard_physical_mod_mask &&
           lhs->v2_shadow_pd_mode_local_active == rhs->v2_shadow_pd_mode_local_active &&
           lhs->v2_shadow_pd_mode_local_locked == rhs->v2_shadow_pd_mode_local_locked &&
           lhs->v2_shadow_pointer_anchor_active == rhs->v2_shadow_pointer_anchor_active &&
           lhs->v2_shadow_pointer_pd_mode_anchor_active == rhs->v2_shadow_pointer_pd_mode_anchor_active &&
           lhs->v2_shadow_pointer_prefers_typing_layer == rhs->v2_shadow_pointer_prefers_typing_layer &&
           lhs->v2_shadow_pointer_toggle_enabled == rhs->v2_shadow_pointer_toggle_enabled &&
           lhs->v2_press_token_count == rhs->v2_press_token_count &&
           lhs->v2_tap_series_count == rhs->v2_tap_series_count &&
           lhs->v2_lease_count == rhs->v2_lease_count &&
           lhs->v2_pending_release_count == rhs->v2_pending_release_count &&
           lhs->v2_deferred_release_blocker_count == rhs->v2_deferred_release_blocker_count &&
           lhs->v2_deferred_release_timed_blocker_count == rhs->v2_deferred_release_timed_blocker_count &&
           lhs->v2_persistent_intent_count == rhs->v2_persistent_intent_count &&
           lhs->v2_release_keycode_mismatch_count == rhs->v2_release_keycode_mismatch_count &&
           lhs->v2_orphan_release_count == rhs->v2_orphan_release_count &&
           lhs->v2_cancelled_press_count == rhs->v2_cancelled_press_count;
}

#if defined(NOAH_RUNTIME_TRACE_ENABLE)

void runtime_v2_trace_capture_projection(void) {
    projection_snapshot_t snapshot = runtime_v2_projection_snapshot_capture();

    runtime_v2_trace_record_projection_snapshot(&snapshot);
}

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
