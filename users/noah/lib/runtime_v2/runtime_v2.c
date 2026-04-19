// ────────────────────────────────────────────────────────────────────────────
// Runtime V2 Foundation
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_v2.h"
#include "runtime_v2_release_internal.h"

#include <string.h>

#include "../pointing/defs/pd_modes.h"
#include "../pointing/policy/pd_mode_policy.h"
#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/keyboard_mod_ownership.h"
#include "../state/ownership/layer_ownership.h"
#include "../state/runtime/runtime_debug.h"
#include "runtime_v2_trace.h"

__attribute__((weak)) const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

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

static void runtime_v2_release_effect_plan_push(runtime_v2_release_effect_plan_t *plan, key_runtime_effect_t effect) {
    if (!plan) {
        return;
    }

    if (plan->count < ARRAY_SIZE(plan->items)) {
        plan->items[plan->count++] = effect;
        return;
    }

    plan->overflowed = true;
}

static void runtime_v2_release_effect_plan_push_dispatch_action(runtime_v2_release_effect_plan_t *plan, uint16_t action) {
    if (action == KC_NO) {
        return;
    }

    runtime_v2_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                  .kind        = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
                                                  .data.action = action,
                                              });
}

static void runtime_v2_release_effect_plan_push_held_action(runtime_v2_release_effect_plan_t *plan, key_runtime_effect_kind_t kind, keypos_t key_pos, uint16_t action) {
    if (!(plan && action != KC_NO)) {
        return;
    }

    runtime_v2_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                  .kind = kind,
                                                  .data.held_action =
                                                      {
                                                          .key_pos = key_pos,
                                                          .action  = action,
                                                      },
                                              });
}

static void runtime_v2_release_effect_plan_push_release_owned_state(runtime_v2_release_effect_plan_t *plan, keypos_t key_pos) {
    if (!plan) {
        return;
    }

    runtime_v2_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                  .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                                  .data.key_pos = key_pos,
                                              });
}

static void runtime_v2_release_effect_plan_push_layer_release(runtime_v2_release_effect_plan_t *plan, keypos_t key_pos) {
    if (!plan) {
        return;
    }

    runtime_v2_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                  .kind         = KEY_RUNTIME_EFFECT_LAYER_RELEASE,
                                                  .data.key_pos = key_pos,
                                              });
}

static void runtime_v2_release_effect_plan_push_pd_mode_lock_tap(runtime_v2_release_effect_plan_t *plan, pd_mode_mask_t mode) {
    if (!(plan && mode != 0)) {
        return;
    }

    runtime_v2_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                  .kind         = KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP,
                                                  .data.pd_mode = mode,
                                              });
}

static void runtime_v2_release_effect_plan_push_delayed_action(runtime_v2_release_effect_plan_t *plan, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
    if (!(plan && action != KC_NO && repeat_count != 0u)) {
        return;
    }

    runtime_v2_release_effect_plan_push(plan, (key_runtime_effect_t){
                                                  .kind = KEY_RUNTIME_EFFECT_DELAYED_ACTION,
                                                  .data.delayed_action =
                                                      {
                                                          .action       = action,
                                                          .mods         = mods,
                                                          .repeat_count = repeat_count,
                                                      },
                                              });
}

static void runtime_v2_release_effect_plan_push_action_or_pd_mode_lock_tap(runtime_v2_release_effect_plan_t *plan, uint16_t action) {
    const pd_mode_def_t *def = pd_mode_lock_action_lookup(action);

    if (def) {
        runtime_v2_release_effect_plan_push_pd_mode_lock_tap(plan, def->mode_flag);
        return;
    }

    runtime_v2_release_effect_plan_push_dispatch_action(plan, action);
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

static void runtime_v2_tap_series_clear(runtime_v2_state_t *state, tap_series_t *series) {
    if (!(state && series && series->active)) {
        return;
    }

    *series = (tap_series_t){0};
    if (state->tap_series_count != 0u) {
        state->tap_series_count--;
    }
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

static int16_t runtime_v2_oldest_pending_release_index(const runtime_v2_state_t *state) {
    int16_t  selected = -1;
    uint16_t sequence = 0u;

    if (!state) {
        return -1;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PENDING_RELEASE_CAPACITY; index++) {
        const pending_release_t *pending = &state->pending_releases[index];

        if (!pending->active || (selected >= 0 && pending->sequence >= sequence)) {
            continue;
        }

        selected = (int16_t)index;
        sequence = pending->sequence;
    }

    return selected;
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

static bool runtime_v2_owner_has_lease_kind(const runtime_v2_state_t *state, uint16_t owner_token_id, lease_kind_t kind);
static bool runtime_v2_owner_has_runtime_owned_state_lease(const runtime_v2_state_t *state, uint16_t owner_token_id);

static bool runtime_v2_press_token_owned_state_active(const runtime_v2_state_t *state, const press_token_t *token) {
    if (!(state && token && token->active)) {
        return false;
    }

    if (runtime_v2_owner_has_runtime_owned_state_lease(state, token->token_id)) {
        return true;
    }

    if (token->behavior_contract.quick_release_of_immediate_hold_dispatches_tap && !handled_key_hold_semantics_fires_at_threshold(token->behavior_contract.hold)) {
        return true;
    }

    if (runtime_v2_elapsed(token->pressed_at, state->current_time) < token->hold_term_ms) {
        return false;
    }

    return runtime_v2_hold_semantics_owns_state_at_threshold(token->behavior_contract.hold);
}

static void runtime_v2_press_token_commit_hold_phase(press_token_t *token, bool completes_hold) {
    if (!token) {
        return;
    }

    token->slot_phase = completes_hold ? KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE : KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE;
}

static bool runtime_v2_press_token_active_scan_should_mark_release_hold_pending(const press_token_t *token, uint16_t elapsed) {
    if (!(token && token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW)) {
        return false;
    }

    if (token->interaction.contract.hold.release_action != KC_NO && elapsed >= token->interaction.binding.tap_hold_term) {
        return true;
    }

    return !token->interaction.binding.hold.present && token->interaction.contract.long_hold.release_action != KC_NO && elapsed >= token->interaction.binding.longer_hold_term;
}

static void runtime_v2_press_token_refresh_slot_phase_for_scan(const runtime_v2_state_t *state, press_token_t *token, uint16_t now) {
    uint16_t elapsed;

    if (!(state && token && token->active && token->handled_key)) {
        return;
    }

    elapsed = runtime_v2_elapsed(token->pressed_at, now);

    switch (token->slot_phase) {
        case KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                runtime_v2_press_token_commit_hold_phase(token, true);
                return;
            }

            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) && elapsed >= token->interaction.binding.tap_hold_term) {
                runtime_v2_press_token_commit_hold_phase(token, !token->interaction.binding.long_hold.present);
                return;
            }

            if (runtime_v2_press_token_active_scan_should_mark_release_hold_pending(token, elapsed)) {
                token->slot_phase = KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING;
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                runtime_v2_press_token_commit_hold_phase(token, true);
                return;
            }

            if (elapsed >= token->interaction.binding.tap_hold_term) {
                runtime_v2_press_token_commit_hold_phase(token, !token->interaction.binding.long_hold.present);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING:
        case KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                runtime_v2_press_token_commit_hold_phase(token, true);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE:
        case KEY_RUNTIME_SLOT_PHASE_IDLE:
        default:
            return;
    }
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

static lease_t *runtime_v2_find_held_action_lease(runtime_v2_state_t *state, keypos_t key_pos, uint16_t action) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && lease->kind == LEASE_KIND_HELD_ACTION && runtime_v2_keypos_equal(lease->owner_key_pos, key_pos) && lease->data.action == action) {
            return lease;
        }
    }

    return NULL;
}

static lease_t *runtime_v2_find_repeat_lease(runtime_v2_state_t *state, keypos_t key_pos, uint16_t action) {
    if (!state) {
        return NULL;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (lease->active && lease->kind == LEASE_KIND_REPEAT && runtime_v2_keypos_equal(lease->owner_key_pos, key_pos) && lease->data.repeat.action == action) {
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

static bool runtime_v2_layer_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, uint8_t layer) {
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
        .owner_key_pos  = owner_key_pos,
        .data.layer     = layer,
    };
    return true;
}

static bool runtime_v2_modifier_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, uint8_t modifiers, bool physical) {
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
        .owner_key_pos  = owner_key_pos,
        .data.modifier =
            {
                .modifiers = modifiers,
                .physical  = physical,
            },
    };
    return true;
}

static bool runtime_v2_pd_mode_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, pd_mode_mask_t mode) {
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
        .owner_key_pos  = owner_key_pos,
        .data.pd_mode   = mode,
    };
    return true;
}

static bool runtime_v2_pointer_anchor_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, bool keep_typing_surface) {
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
        .owner_key_pos  = owner_key_pos,
        .data.pointer_anchor =
            {
                .layer               = runtime_v2_default_pointer_layer(),
                .keep_typing_surface = keep_typing_surface,
            },
    };
    return true;
}

static bool runtime_v2_held_action_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, uint16_t action) {
    lease_t *lease;

    if (!(state && action != KC_NO)) {
        return false;
    }

    if (runtime_v2_find_held_action_lease(state, owner_key_pos, action)) {
        return false;
    }

    lease = runtime_v2_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active         = true,
        .kind           = LEASE_KIND_HELD_ACTION,
        .owner_token_id = owner_token_id,
        .owner_key_pos  = owner_key_pos,
        .data.action    = action,
    };
    return true;
}

static bool runtime_v2_repeat_lease_activate(runtime_v2_state_t *state, uint16_t owner_token_id, keypos_t owner_key_pos, uint16_t action, uint16_t repeat_hz) {
    lease_t *lease;

    if (!(state && action != KC_NO)) {
        return false;
    }

    if (runtime_v2_find_repeat_lease(state, owner_key_pos, action)) {
        return false;
    }

    lease = runtime_v2_allocate_lease(state);
    if (!lease) {
        return false;
    }

    *lease = (lease_t){
        .active         = true,
        .kind           = LEASE_KIND_REPEAT,
        .owner_token_id = owner_token_id,
        .owner_key_pos  = owner_key_pos,
        .data.repeat =
            {
                .action    = action,
                .repeat_hz = (uint8_t)repeat_hz,
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

        if (lease->active && lease->owner_token_id == owner_token_id && lease->kind != LEASE_KIND_HELD_ACTION && lease->kind != LEASE_KIND_REPEAT) {
            *lease = (lease_t){0};
            if (state->lease_count != 0u) {
                state->lease_count--;
            }
        }
    }
}

static bool runtime_v2_owner_has_lease_kind(const runtime_v2_state_t *state, uint16_t owner_token_id, lease_kind_t kind) {
    if (!(state && owner_token_id != 0u)) {
        return false;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        const lease_t *lease = &state->leases[index];

        if (lease->active && lease->owner_token_id == owner_token_id && lease->kind == kind) {
            return true;
        }
    }

    return false;
}

static bool runtime_v2_owner_has_runtime_owned_state_lease(const runtime_v2_state_t *state, uint16_t owner_token_id) {
    return runtime_v2_owner_has_lease_kind(state, owner_token_id, LEASE_KIND_HELD_ACTION) || runtime_v2_owner_has_lease_kind(state, owner_token_id, LEASE_KIND_REPEAT);
}

static bool runtime_v2_release_runtime_owned_state_leases_for_key(runtime_v2_state_t *state, keypos_t key_pos) {
    bool changed = false;

    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_LEASE_CAPACITY; index++) {
        lease_t *lease = &state->leases[index];

        if (!lease->active || !runtime_v2_keypos_equal(lease->owner_key_pos, key_pos)) {
            continue;
        }

        if (lease->kind == LEASE_KIND_HELD_ACTION || lease->kind == LEASE_KIND_REPEAT) {
            *lease = (lease_t){0};
            if (state->lease_count != 0u) {
                state->lease_count--;
            }
            changed = true;
        }
    }

    return changed;
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
            changed |= runtime_v2_layer_lease_activate(state, token->token_id, token->key_pos, layer);
        }
    }

    if (runtime_v2_keycode_owns_modifier_on_press(token->resolved_keycode)) {
        changed |= runtime_v2_modifier_lease_activate(state, token->token_id, token->key_pos, runtime_v2_modifier_mask_for_keycode(token->resolved_keycode), true);
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
        changed |= runtime_v2_pd_mode_lease_activate(state, token->token_id, token->key_pos, mode);
        if (runtime_v2_pd_mode_keeps_auto_mouse_anchored(mode)) {
            changed |= runtime_v2_pointer_anchor_lease_activate(state, token->token_id, token->key_pos, runtime_v2_pd_mode_prefers_typing_layer(mode));
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
            changed |= runtime_v2_layer_lease_activate(state, token->token_id, token->key_pos, layer);
        }
    }

    if (runtime_v2_keycode_owns_modifier_on_hold(token->resolved_keycode)) {
        changed |= runtime_v2_modifier_lease_activate(state, token->token_id, token->key_pos, runtime_v2_modifier_mask_for_keycode(token->resolved_keycode), false);
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

    if (series->has_more_taps || series->tap_count > 1u || series->hold.present || series->long_hold.present) {
        return;
    }

    runtime_v2_tap_series_clear(state, series);
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

static void runtime_v2_refresh_slot_phases_for_scan(runtime_v2_state_t *state, uint16_t now) {
    if (!state) {
        return;
    }

    for (uint16_t index = 0; index < RUNTIME_V2_PRESS_TOKEN_CAPACITY; index++) {
        runtime_v2_press_token_refresh_slot_phase_for_scan(state, &state->press_tokens[index], now);
    }
}

static void runtime_v2_press_token_begin(runtime_v2_state_t *state, const runtime_key_event_t *event, uint16_t now) {
    press_token_t             *token;
    tap_series_t              *series;
    handled_key_resolution_t   resolution;
    handled_key_materialized_t materialized;
    key_runtime_slot_interaction_t interaction = key_runtime_slot_interaction_default();
    handled_key_resolution_ctx_t ctx;
    uint16_t                   hold_term_ms;
    uint16_t                   longer_hold_term_ms;
    uint8_t                    tap_count = 1u;
    bool                       handled   = false;
    bool                       tap_outcome_available = false;
    bool                       pd_mode_was_locked_on_press = false;
    key_runtime_slot_phase_t   slot_phase = KEY_RUNTIME_SLOT_PHASE_IDLE;

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
        interaction              = key_runtime_slot_interaction_from_materialized(materialized);
        hold_term_ms             = handled_key_resolution_tap_hold_term(resolution);
        longer_hold_term_ms      = handled_key_resolution_longer_hold_term(resolution);
        tap_outcome_available    = handled_key_resolution_has_multi_tap(resolution) || materialized.tap_action != KC_NO;
        pd_mode_was_locked_on_press = materialized.pd_mode != 0 && state->shadow_projection.pd_mode_local_locked == materialized.pd_mode;
        slot_phase               = hold_registers_on_press(materialized.hold) ? KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW : KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW;
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
        .interaction               = interaction,
        .slot_phase                = slot_phase,
    };
    state->press_token_count++;
    runtime_v2_press_token_attach_press_leases(state, token);

    if (series && tap_count > 1u) {
        series->pending_hold = materialized.hold.present || materialized.long_hold.present;
    }
}

static void runtime_v2_tap_series_note_tap(runtime_v2_state_t *state, const press_token_t *token, uint16_t now) {
    tap_series_t *series;
    bool          reuse_existing;
    uint16_t      single_action;
    uint16_t      tap_action;
    uint8_t       tap_repeat_count;
    bool          has_more_taps;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
    uint16_t      tap_hold_term_ms;
    uint16_t      tap_term_ms;
    uint8_t       tap_count;

    if (!(state && token && token->active)) {
        return;
    }

    series = runtime_v2_tap_series_state(state, token->key_pos);
    if (!series) {
        return;
    }

    reuse_existing = series->active && series->keycode == token->resolved_keycode && runtime_v2_elapsed(series->last_tap_at, now) <= series->tap_term_ms;
    single_action   = reuse_existing ? series->single_action : token->resolved_keycode;
    tap_action      = token->resolved_keycode;
    tap_repeat_count = 0u;
    has_more_taps   = false;
    hold            = hold_behavior_none();
    long_hold       = hold_behavior_none();
    tap_hold_term_ms = runtime_v2_default_hold_term(token->resolved_keycode);
    tap_term_ms     = runtime_v2_default_multi_tap_term();
    tap_count       = (uint8_t)(reuse_existing ? (uint8_t)(series->tap_count + 1u) : 1u);

    if (token->handled_key) {
        uint16_t handled_tap_action = token->interaction.binding.tap_action != KC_NO ? token->interaction.binding.tap_action : token->resolved_keycode;

        single_action    = reuse_existing ? series->single_action : handled_tap_action;
        tap_action       = handled_tap_action;
        tap_repeat_count = token->interaction.binding.tap_repeat_count;
        has_more_taps    = token->interaction.binding.has_more_taps;
        hold             = token->interaction.binding.hold;
        long_hold        = token->interaction.binding.long_hold;
        tap_hold_term_ms = token->interaction.binding.tap_hold_term;
        tap_term_ms      = token->interaction.binding.multi_tap_term;
        if (token->interaction.selection.tap_count != 0u) {
            tap_count = token->interaction.selection.tap_count;
        }
    }

    if (!series->active) {
        state->tap_series_count++;
    }

    *series = (tap_series_t){
        .active           = true,
        .key_pos          = token->key_pos,
        .keycode          = token->resolved_keycode,
        .tap_count        = tap_count,
        .pending_hold     = false,
        .single_action    = single_action,
        .tap_action       = tap_action,
        .tap_repeat_count = tap_repeat_count,
        .has_more_taps    = has_more_taps,
        .hold             = hold,
        .long_hold        = long_hold,
        .tap_hold_term_ms = tap_hold_term_ms,
        .last_action      = tap_action,
        .last_tap_at      = now,
        .tap_term_ms      = tap_term_ms,
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

static bool runtime_v2_pending_multi_tap_release_uses_held_lifecycle(const press_token_t *token, uint16_t action, uint8_t tap_repeat_count, uint16_t elapsed) {
    handled_key_hold_semantics_t semantics;

    if (!(token && token->handled_key)) {
        return false;
    }

    semantics = token->interaction.contract.hold;
    if (semantics.threshold_action == KC_NO || tap_repeat_count != 1u || elapsed < token->interaction.binding.tap_hold_term) {
        return false;
    }

    if (semantics.threshold != HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD || action != semantics.threshold_action) {
        return false;
    }

    return semantics.uses_held_lifecycle;
}

static bool runtime_v2_pending_multi_tap_flush_resolution(const tap_series_t *series, uint16_t *action, uint8_t *repeat_count) {
    uint16_t resolved_action;
    uint8_t  resolved_repeat_count;

    if (!(series && series->active && !series->pending_hold)) {
        return false;
    }

    resolved_action       = series->tap_repeat_count > 0u ? series->tap_action : series->single_action;
    resolved_repeat_count = series->tap_repeat_count > 0u ? series->tap_repeat_count : series->tap_count;
    if (action) {
        *action = resolved_action;
    }
    if (repeat_count) {
        *repeat_count = resolved_repeat_count;
    }
    return true;
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
            runtime_v2_refresh_for_time(state, event_time);
            runtime_v2_refresh_slot_phases_for_scan(state, event_time);
            return;
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

uint8_t runtime_v2_pending_release_count(void) {
    runtime_v2_state_t *state = runtime_v2_state();

    return state ? state->pending_release_count : 0u;
}

bool runtime_v2_take_pending_multi_tap_flush(keypos_t key_pos, uint16_t *action, uint8_t *repeat_count) {
    runtime_v2_state_t *state = runtime_v2_state();
    tap_series_t       *series;

    if (action) {
        *action = KC_NO;
    }
    if (repeat_count) {
        *repeat_count = 0u;
    }

    if (!(state && runtime_v2_blocker_queries_authoritative() && runtime_v2_keypos_valid(key_pos))) {
        return false;
    }

    series = runtime_v2_tap_series_state(state, key_pos);
    if (!runtime_v2_pending_multi_tap_flush_resolution(series, action, repeat_count)) {
        return false;
    }

    runtime_v2_tap_series_clear(state, series);
    return true;
}

bool runtime_v2_reset_pending_multi_tap(keypos_t key_pos) {
    runtime_v2_state_t *state = runtime_v2_state();
    tap_series_t       *series;

    if (!(state && runtime_v2_keypos_valid(key_pos))) {
        return false;
    }

    series = runtime_v2_tap_series_state(state, key_pos);
    if (!(series && series->active)) {
        return false;
    }

    runtime_v2_tap_series_clear(state, series);
    return true;
}

bool runtime_v2_retire_press_token(keypos_t key_pos) {
    runtime_v2_state_t *state = runtime_v2_state();
    press_token_t      *token;

    if (!(state && runtime_v2_keypos_valid(key_pos))) {
        return false;
    }

    token = runtime_v2_press_token_state(state, key_pos);
    if (!(token && token->active)) {
        return false;
    }

    runtime_v2_press_token_cancel(state, token, state->current_time);
    return true;
}

uint8_t runtime_v2_take_pending_release_dispatches(pending_release_t *out, uint8_t capacity) {
    runtime_v2_state_t *state = runtime_v2_state();
    uint8_t             count = 0;

    if (!(state && out && capacity != 0u && runtime_v2_blocker_queries_authoritative()) || runtime_v2_effective_deferred_release_blocker_count(state) != 0u) {
        return 0u;
    }

    while (count < capacity) {
        int16_t          index   = runtime_v2_oldest_pending_release_index(state);
        pending_release_t pending;

        if (index < 0) {
            break;
        }

        pending                    = state->pending_releases[index];
        out[count++]               = pending;
        state->pending_releases[index] = (pending_release_t){0};
        if (state->pending_release_count != 0u) {
            state->pending_release_count--;
        }
        runtime_v2_pending_release_clear_token(state, pending.owner_token_id);
    }

    return count;
}

void runtime_v2_observe_held_action_register(keypos_t key_pos, uint16_t action) {
    runtime_v2_state_t *state = runtime_v2_state();
    press_token_t      *token;

    if (!(state && action != KC_NO && runtime_v2_keypos_valid(key_pos))) {
        return;
    }

    token = runtime_v2_press_token_state(state, key_pos);
    (void)runtime_v2_held_action_lease_activate(state, token ? token->token_id : 0u, key_pos, action);

    if (!(token && token->handled_key)) {
        return;
    }

    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW) {
        if (key_runtime_slot_interaction_uses_fallback_hold(token->interaction)) {
            runtime_v2_press_token_commit_hold_phase(token, true);
            return;
        }

        if (runtime_v2_elapsed(token->pressed_at, state->current_time) >= token->interaction.binding.tap_hold_term) {
            runtime_v2_press_token_commit_hold_phase(token, !token->interaction.binding.long_hold.present);
            return;
        }
    }

    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING || token->slot_phase == KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE) {
        runtime_v2_press_token_commit_hold_phase(token, true);
    }
}

void runtime_v2_observe_held_action_unregister(keypos_t key_pos, uint16_t action) {
    runtime_v2_state_t *state = runtime_v2_state();
    lease_t            *lease;

    if (!(state && action != KC_NO && runtime_v2_keypos_valid(key_pos))) {
        return;
    }

    lease = runtime_v2_find_held_action_lease(state, key_pos, action);
    if (!lease) {
        return;
    }

    *lease = (lease_t){0};
    if (state->lease_count != 0u) {
        state->lease_count--;
    }
}

void runtime_v2_observe_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    runtime_v2_state_t *state = runtime_v2_state();
    press_token_t      *token;

    if (!(state && action != KC_NO && runtime_v2_keypos_valid(key_pos))) {
        return;
    }

    token = runtime_v2_press_token_state(state, key_pos);
    (void)runtime_v2_repeat_lease_activate(state, token ? token->token_id : 0u, key_pos, action, repeat_hz);

    if (!(token && token->handled_key)) {
        return;
    }

    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || token->slot_phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW) {
        runtime_v2_press_token_commit_hold_phase(token, !token->interaction.binding.long_hold.present);
        return;
    }

    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING || token->slot_phase == KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE) {
        runtime_v2_press_token_commit_hold_phase(token, true);
    }
}

bool runtime_v2_release_owned_state_by_key(keypos_t key_pos) {
    runtime_v2_state_t *state = runtime_v2_state();

    if (!(state && runtime_v2_keypos_valid(key_pos))) {
        return false;
    }

    return runtime_v2_release_runtime_owned_state_leases_for_key(state, key_pos);
}

bool runtime_v2_resolve_active_release(keypos_t key_pos, runtime_v2_active_release_resolution_t *out) {
    runtime_v2_state_t                    *state = runtime_v2_state();
    press_token_t                         *token;
    key_runtime_slot_release_semantics_t   semantics;
    key_runtime_slot_release_query_t       query;
    uint16_t                               elapsed;

    if (out) {
        *out = (runtime_v2_active_release_resolution_t){0};
    }

    if (!(state && out && runtime_v2_blocker_queries_authoritative() && runtime_v2_keypos_valid(key_pos))) {
        return false;
    }

    token = runtime_v2_press_token_state(state, key_pos);
    if (!(token && token->handled_key && !token->active && token->observed_release_keycode != KC_NO)) {
        return false;
    }

    elapsed   = runtime_v2_elapsed(token->pressed_at, token->released_at);
    semantics = key_runtime_slot_release_semantics_for_phase(token->slot_phase);
    query     = (key_runtime_slot_release_query_t){
        .interaction                     = token->interaction,
        .semantics                       = semantics,
        .elapsed                         = elapsed,
        .held_action_active              = runtime_v2_owner_has_lease_kind(state, token->token_id, LEASE_KIND_HELD_ACTION),
        .repeat_active                   = runtime_v2_owner_has_lease_kind(state, token->token_id, LEASE_KIND_REPEAT),
        .other_press_interrupted         = token->other_press_interrupted,
        .momentary_layer_tap_interrupted = token->momentary_layer_tap_interrupted,
        .pd_mode_was_locked_on_press     = token->pd_mode_was_locked_on_press,
    };

    *out = (runtime_v2_active_release_resolution_t){
        .interaction                     = token->interaction,
        .phase                           = token->slot_phase,
        .elapsed                         = elapsed,
        .held_action_active              = query.held_action_active,
        .repeat_active                   = query.repeat_active,
        .momentary_layer_tap_interrupted = token->momentary_layer_tap_interrupted,
        .quick_tap                       = key_runtime_slot_release_query_quick_tap(&query),
        .quick_immediate_hold            = key_runtime_slot_release_query_quick_immediate_hold(&query),
        .buffered_base_tap               = key_runtime_slot_release_query_buffered_base_tap(&query),
        .lock_tap_mode                   = key_runtime_slot_release_query_lock_tap_mode(&query),
        .decision                        = key_runtime_slot_release_decide(&query),
    };
    return true;
}

bool runtime_v2_plan_active_release_effects(keypos_t key_pos, uint16_t keycode, const runtime_v2_active_release_resolution_t *resolution, runtime_v2_release_effect_plan_t *out) {
    key_runtime_slot_release_contract_t contract;

    if (out) {
        *out = (runtime_v2_release_effect_plan_t){
            .settlement = RUNTIME_V2_RELEASE_SLOT_SETTLEMENT_RESET,
        };
    }

    if (!(resolution && out && runtime_v2_keypos_valid(key_pos))) {
        return false;
    }

    contract = key_runtime_slot_release_contract(resolution->interaction);

    if (key_runtime_slot_interaction_is_momentary_layer(resolution->interaction)) {
        runtime_v2_release_effect_plan_push_layer_release(out, key_pos);
    }
    if (resolution->decision.release_owned_state) {
        runtime_v2_release_effect_plan_push_release_owned_state(out, key_pos);
    }

    switch (resolution->decision.outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_TAP:
            switch (contract.tap.outcome) {
                case KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_BUFFER_MULTI_TAP:
                    out->pending_multi_tap_seed = (runtime_v2_pending_multi_tap_seed_t){
                        .active           = true,
                        .keycode          = keycode,
                        .key_pos          = key_pos,
                        .tap_action       = contract.tap.action,
                        .tap_repeat_count = contract.tap.repeat_count,
                        .tap_hold_term    = resolution->interaction.binding.tap_hold_term,
                        .multi_tap_term   = resolution->interaction.binding.multi_tap_term,
                        .has_more_taps    = resolution->interaction.binding.has_more_taps,
                    };
                    return true;
                case KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_DISPATCH_ACTION:
                    runtime_v2_release_effect_plan_push_action_or_pd_mode_lock_tap(out, contract.tap.action);
                    return true;
                case KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_NONE:
                default:
                    return true;
            }
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_ACTION:
            runtime_v2_release_effect_plan_push_action_or_pd_mode_lock_tap(out, resolution->decision.action);
            return true;
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_PD_MODE_LOCK_TAP:
            runtime_v2_release_effect_plan_push_pd_mode_lock_tap(out, resolution->decision.pd_mode_lock_tap);
            return true;
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_NONE:
        default:
            return true;
    }
}

bool runtime_v2_resolve_pending_multi_tap_release(keypos_t key_pos, uint16_t tap_action, uint8_t tap_repeat_count, bool preserve_chain_available, runtime_v2_pending_multi_tap_release_resolution_t *out) {
    runtime_v2_state_t                  *state = runtime_v2_state();
    press_token_t                       *token;
    tap_series_t                        *series;
    key_runtime_slot_release_semantics_t semantics = {
        .quick_tap_dispatches_tap        = true,
        .nonquick_release_dispatches_tap = true,
    };
    key_runtime_slot_release_decision_t decision;
    uint16_t                            elapsed;

    if (out) {
        *out = (runtime_v2_pending_multi_tap_release_resolution_t){0};
    }

    if (!(state && out && runtime_v2_blocker_queries_authoritative() && runtime_v2_keypos_valid(key_pos))) {
        return false;
    }

    token = runtime_v2_press_token_state(state, key_pos);
    series = runtime_v2_tap_series_state(state, key_pos);
    if (!(token && token->handled_key && !token->active && token->observed_release_keycode != KC_NO)) {
        return false;
    }

    elapsed = runtime_v2_elapsed(token->pressed_at, token->released_at);

    if (token->interaction.contract.hold.release_action != KC_NO ? elapsed >= token->interaction.binding.tap_hold_term
                                                                 : !token->interaction.binding.hold.present && token->interaction.contract.long_hold.release_action != KC_NO && elapsed >= token->interaction.binding.longer_hold_term) {
        semantics.hold_action_mode = KEY_RUNTIME_SLOT_RELEASE_HOLD_ACTION_MODE_SELECT_HOLD_ACTION;
    }

    decision = key_runtime_slot_release_decide(&(key_runtime_slot_release_query_t){
        .interaction = token->interaction,
        .semantics   = semantics,
        .elapsed     = elapsed,
    });

    switch (decision.outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_ACTION:
            if (runtime_v2_pending_multi_tap_release_uses_held_lifecycle(token, decision.action, tap_repeat_count, elapsed)) {
                runtime_v2_tap_series_clear(state, series);
                *out = (runtime_v2_pending_multi_tap_release_resolution_t){
                    .outcome = RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
                    .action  = decision.action,
                };
                return true;
            }

            runtime_v2_tap_series_clear(state, series);
            *out = (runtime_v2_pending_multi_tap_release_resolution_t){
                .outcome      = RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
                .action       = decision.action,
                .repeat_count = decision.action == KC_NO ? 0u : 1u,
            };
            return true;
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_TAP:
            if (runtime_v2_pending_multi_tap_release_uses_held_lifecycle(token, tap_action, tap_repeat_count, elapsed)) {
                runtime_v2_tap_series_clear(state, series);
                *out = (runtime_v2_pending_multi_tap_release_resolution_t){
                    .outcome = RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
                    .action  = tap_action,
                };
                return true;
            }

            if (tap_action == KC_NO && tap_repeat_count == 0u && preserve_chain_available) {
                *out = (runtime_v2_pending_multi_tap_release_resolution_t){
                    .outcome = RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
                };
                return true;
            }

            runtime_v2_tap_series_clear(state, series);
            *out = (runtime_v2_pending_multi_tap_release_resolution_t){
                .outcome      = RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
                .action       = tap_action,
                .repeat_count = tap_repeat_count,
            };
            return true;
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_PD_MODE_LOCK_TAP:
        case KEY_RUNTIME_SLOT_RELEASE_DECISION_OUTCOME_NONE:
        default:
            if (runtime_v2_pending_multi_tap_release_uses_held_lifecycle(token, tap_action, tap_repeat_count, elapsed)) {
                runtime_v2_tap_series_clear(state, series);
                *out = (runtime_v2_pending_multi_tap_release_resolution_t){
                    .outcome = RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
                    .action  = tap_action,
                };
                return true;
            }

            if (tap_action == KC_NO && tap_repeat_count == 0u && preserve_chain_available) {
                *out = (runtime_v2_pending_multi_tap_release_resolution_t){
                    .outcome = RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
                };
                return true;
            }

            runtime_v2_tap_series_clear(state, series);
            *out = (runtime_v2_pending_multi_tap_release_resolution_t){
                .outcome      = RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
                .action       = tap_action,
                .repeat_count = tap_repeat_count,
            };
            return true;
    }
}

bool runtime_v2_plan_pending_multi_tap_release_effects(keypos_t key_pos, bool is_momentary_layer, const runtime_v2_pending_multi_tap_release_resolution_t *resolution, delayed_action_mods_t mods, runtime_v2_release_effect_plan_t *out) {
    if (out) {
        *out = (runtime_v2_release_effect_plan_t){
            .settlement = RUNTIME_V2_RELEASE_SLOT_SETTLEMENT_RESET,
        };
    }

    if (!(resolution && out && runtime_v2_keypos_valid(key_pos))) {
        return false;
    }

    if (is_momentary_layer) {
        runtime_v2_release_effect_plan_push_layer_release(out, key_pos);
    }

    switch (resolution->outcome) {
        case RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE:
            runtime_v2_release_effect_plan_push_held_action(out, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, key_pos, resolution->action);
            runtime_v2_release_effect_plan_push_held_action(out, KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER, key_pos, resolution->action);
            return true;
        case RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION:
            runtime_v2_release_effect_plan_push_delayed_action(out, resolution->action, mods, resolution->repeat_count);
            return true;
        case RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN:
            out->settlement = RUNTIME_V2_RELEASE_SLOT_SETTLEMENT_CLEAR_ACTIVE_PRESERVE_PENDING_MULTI_TAP;
            return true;
        case RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE:
        default:
            return true;
    }
}

bool runtime_v2_resolve_pending_multi_tap_scan(keypos_t key_pos, runtime_v2_pending_multi_tap_scan_resolution_t *out) {
    runtime_v2_state_t *state  = runtime_v2_state();
    press_token_t      *token;
    tap_series_t       *series;
    uint16_t            elapsed;
    uint16_t            flush_action;
    uint8_t             flush_repeat_count;

    if (out) {
        *out = (runtime_v2_pending_multi_tap_scan_resolution_t){0};
    }

    if (!(state && out && runtime_v2_blocker_queries_authoritative() && runtime_v2_keypos_valid(key_pos))) {
        return false;
    }

    token  = runtime_v2_press_token_state(state, key_pos);
    series = runtime_v2_tap_series_state(state, key_pos);
    if (!(series && series->active)) {
        return false;
    }

    if (!series->pending_hold) {
        if (runtime_v2_elapsed(series->last_tap_at, state->current_time) > series->tap_term_ms &&
            runtime_v2_pending_multi_tap_flush_resolution(series, &flush_action, &flush_repeat_count)) {
            runtime_v2_tap_series_clear(state, series);
            *out = (runtime_v2_pending_multi_tap_scan_resolution_t){
                .outcome      = RUNTIME_V2_PENDING_MULTI_TAP_SCAN_OUTCOME_FLUSH,
                .action       = flush_action,
                .repeat_count = flush_repeat_count,
            };
        }

        return true;
    }

    if (!(token && token->active && token->handled_key && series->keycode == token->resolved_keycode)) {
        return false;
    }

    elapsed = runtime_v2_elapsed(token->pressed_at, state->current_time);
    if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
        runtime_v2_tap_series_clear(state, series);
        *out = (runtime_v2_pending_multi_tap_scan_resolution_t){
            .outcome        = RUNTIME_V2_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD,
            .hold           = token->interaction.binding.long_hold,
            .semantics      = token->interaction.contract.long_hold,
            .completes_hold = true,
            .action         = token->interaction.binding.long_hold.action,
        };
        return true;
    }

    if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) && elapsed >= token->interaction.binding.tap_hold_term) {
        runtime_v2_tap_series_clear(state, series);
        *out = (runtime_v2_pending_multi_tap_scan_resolution_t){
            .outcome        = RUNTIME_V2_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD,
            .hold           = token->interaction.binding.hold,
            .semantics      = token->interaction.contract.hold,
            .completes_hold = !token->interaction.binding.long_hold.present,
            .action         = token->interaction.binding.hold.action,
        };
        return true;
    }

    return true;
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
        .sequence       = state->next_pending_release_sequence++,
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
