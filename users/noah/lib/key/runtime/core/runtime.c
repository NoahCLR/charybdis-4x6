// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Foundation
// ────────────────────────────────────────────────────────────────────────────

#include "runtime.h"
#include "effect_plan.h"
#include "feedback_projection.h"
#include "pd_projection.h"
#include "release_internal.h"
#include "tap_series.h"

#include "../../../pointing/defs/pd_modes.h"
#include "../../../pointing/policy/pd_mode_policy.h"
#include "../../../action/action_dispatch.h"
#include "../../../action/action_lifecycle.h"
#include "../../interaction/handled_key_internal.h"
#include "../../interaction/handled_key_policy.h"
#include "../../interaction/key_behavior_lookup.h"
#include "../../ownership/held_action.h"
#include "../../ownership/held_repeat.h"
#include "../feedback.h"
#include "../../../compat/qmk_combo_origin.h"
#include "../../../state/ownership/layer_ownership.h"
#include "trace.h"

__attribute__((weak)) const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

__attribute__((weak)) uint8_t pd_mode_buffered_tap_masked_real_mods(uint16_t keycode) {
    (void)keycode;
    return 0;
}

__attribute__((weak)) key_feedback_tap_commit_mode_t key_feedback_tap_commit_mode(void) {
    return KEY_FEEDBACK_TAP_COMMIT_ALL_TAPS;
}

static press_token_t *key_runtime_core_press_token_state(key_runtime_core_state_t *state, keypos_t key_pos);
static bool key_runtime_core_queue_pending_release_dispatch_for_owner(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, bool tap_commit_feedback, uint16_t owner_token_id);

bool key_runtime_core_keypos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static uint16_t key_runtime_core_keypos_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * MATRIX_COLS + key_pos.col);
}

static bool key_runtime_core_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

typedef enum {
    KEY_RUNTIME_CORE_FEEDBACK_LEVEL_PRESS = 0,
    KEY_RUNTIME_CORE_FEEDBACK_LEVEL_HOLD,
    KEY_RUNTIME_CORE_FEEDBACK_LEVEL_LONG_HOLD,
} key_runtime_core_feedback_level_t;

static bool key_runtime_core_feedback_sequence_is_newer_or_equal(uint32_t candidate, uint32_t current) {
    return current == 0u || (candidate != 0u && (uint32_t)(candidate - current) < 0x80000000u);
}

static void key_runtime_core_press_token_note_feedback_level(key_runtime_core_state_t *state, press_token_t *token, key_runtime_core_feedback_level_t level) {
    if (!(state && token && token->active)) {
        return;
    }

    if (token->feedback_sequence != 0u && token->feedback_level >= (uint8_t)level) {
        return;
    }

    token->feedback_sequence = key_runtime_core_state_next_feedback_sequence(state);
    token->feedback_level    = (uint8_t)level;
}

static void key_runtime_core_press_token_note_hold_feedback(key_runtime_core_state_t *state, press_token_t *token, bool long_hold_level) {
    key_runtime_core_press_token_note_feedback_level(state, token, long_hold_level ? KEY_RUNTIME_CORE_FEEDBACK_LEVEL_LONG_HOLD : KEY_RUNTIME_CORE_FEEDBACK_LEVEL_HOLD);
}

static keypos_t key_runtime_core_invalid_keypos(void) {
    return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static keypos_t key_runtime_core_keypos_from_slot_index(uint16_t index) {
    if (index >= KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY) {
        return key_runtime_core_invalid_keypos();
    }

    return (keypos_t){
        .row = (uint8_t)(index / MATRIX_COLS),
        .col = (uint8_t)(index % MATRIX_COLS),
    };
}

static bool key_runtime_core_press_token_slot_index(const key_runtime_core_state_t *state, const press_token_t *token, uint16_t *out) {
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

static keypos_t key_runtime_core_press_token_resolve_key_pos(const key_runtime_core_state_t *state, const press_token_t *token) {
    uint16_t index;

    return key_runtime_core_press_token_slot_index(state, token, &index) ? key_runtime_core_keypos_from_slot_index(index) : key_runtime_core_invalid_keypos();
}

static bool key_runtime_core_tap_series_slot_index(const key_runtime_core_state_t *state, const tap_series_t *series, uint16_t *out) {
    uintptr_t start;
    uintptr_t end;
    uintptr_t ptr;

    if (out) {
        *out = 0u;
    }

    if (!(state && series && out)) {
        return false;
    }

    start = (uintptr_t)&state->tap_series[0];
    end   = (uintptr_t)&state->tap_series[KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY];
    ptr   = (uintptr_t)series;
    if (!(ptr >= start && ptr < end && ((ptr - start) % sizeof(state->tap_series[0])) == 0u)) {
        return false;
    }

    *out = (uint16_t)((ptr - start) / sizeof(state->tap_series[0]));
    return true;
}

static keypos_t key_runtime_core_tap_series_resolve_key_pos(const key_runtime_core_state_t *state, const tap_series_t *series) {
    uint16_t index;

    return key_runtime_core_tap_series_slot_index(state, series, &index) ? key_runtime_core_keypos_from_slot_index(index) : key_runtime_core_invalid_keypos();
}

static keypos_t key_runtime_core_pending_release_slot_key_pos(const pending_release_slot_t *pending) {
    return pending ? key_runtime_keypos_unpack(pending->packed_key_pos) : key_runtime_core_invalid_keypos();
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
    return key_runtime_core_pending_release_slot_active(pending) && key_runtime_core_keypos_equal(key_runtime_core_pending_release_slot_key_pos(pending), key_pos) && pending->action == action && pending->mods.real == mods.real && pending->mods.weak == mods.weak && pending->mods.oneshot == mods.oneshot && pending->mods.oneshot_locked == mods.oneshot_locked;
}

static lease_kind_t key_runtime_core_lease_kind(const lease_t *lease) {
    return lease ? (lease_kind_t)lease->kind : LEASE_KIND_NONE;
}

static keypos_t key_runtime_core_lease_owner_key_pos(const lease_t *lease) {
    return lease ? key_runtime_keypos_unpack(lease->owner_packed_key_pos) : key_runtime_core_invalid_keypos();
}

static bool key_runtime_core_lease_owner_keypos_equal(const lease_t *lease, keypos_t key_pos) {
    return lease && key_runtime_core_keypos_equal(key_runtime_core_lease_owner_key_pos(lease), key_pos);
}

void key_runtime_core_effect_plan_init(key_runtime_core_effect_plan_t *plan) {
    if (!plan) {
        return;
    }

    *plan = (key_runtime_core_effect_plan_t){0};
}

void key_runtime_core_effect_plan_init_with_sink(key_runtime_core_effect_plan_t *plan, void (*sink)(void *ctx, key_runtime_effect_t effect), void *sink_ctx) {
    key_runtime_core_effect_plan_init(plan);
    if (!plan) {
        return;
    }

    plan->sink     = sink;
    plan->sink_ctx = sink_ctx;
}

static void key_runtime_core_effect_plan_push(key_runtime_core_effect_plan_t *plan, key_runtime_effect_t effect) {
    if (!plan) {
        return;
    }

    if (plan->sink) {
        plan->sink(plan->sink_ctx, effect);
        return;
    }

    if (plan->count < ARRAY_SIZE(plan->items)) {
        plan->items[plan->count++] = effect;
        return;
    }

    plan->overflowed = true;
}

static void key_runtime_core_effect_plan_push_dispatch_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action) {
    if (!(plan && action != KC_NO)) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind                 = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
                                                .data.dispatch_action = {.action = action, .packed_key_pos = key_runtime_keypos_pack(key_pos)},
                                            });
}

static void key_runtime_core_effect_plan_push_held_action(key_runtime_core_effect_plan_t *plan, key_runtime_effect_kind_t kind, keypos_t key_pos, uint16_t action) {
    if (!(plan && action != KC_NO)) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind = kind,
                                                .data.held_action =
                                                    {
                                                        .key_pos = key_pos,
                                                        .action  = action,
                                                    },
                                            });
}

static void key_runtime_core_effect_plan_push_release_owned_state(key_runtime_core_effect_plan_t *plan, keypos_t key_pos) {
    if (!plan) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                                .data.key_pos = key_pos,
                                            });
}

static void key_runtime_core_effect_plan_push_repeat_start(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    if (!(plan && action != KC_NO)) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind = KEY_RUNTIME_EFFECT_REPEAT_START,
                                                .data.repeat =
                                                    {
                                                        .key_pos   = key_pos,
                                                        .action    = action,
                                                        .repeat_hz = repeat_hz,
                                                    },
                                            });
}

static void key_runtime_core_effect_plan_push_layer_press(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint8_t layer) {
    if (!plan) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind = KEY_RUNTIME_EFFECT_LAYER_PRESS,
                                                .data.layer_press =
                                                    {
                                                        .key_pos = key_pos,
                                                        .layer   = layer,
                                                    },
                                            });
}

static void key_runtime_core_effect_plan_push_layer_release(key_runtime_core_effect_plan_t *plan, keypos_t key_pos) {
    if (!plan) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind         = KEY_RUNTIME_EFFECT_LAYER_RELEASE,
                                                .data.key_pos = key_pos,
                                            });
}

static bool key_runtime_core_tap_commit_feedback_mode_allows(uint8_t tap_count) {
    switch (key_feedback_tap_commit_mode()) {
        case KEY_FEEDBACK_TAP_COMMIT_OFF:
            return false;
        case KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS:
            return tap_count > 1u;
        case KEY_FEEDBACK_TAP_COMMIT_ALL_TAPS:
        default:
            return true;
    }
}

bool key_runtime_core_tap_commit_feedback_allowed(uint16_t action, uint8_t tap_count) {
    noah_action_desc_t desc;

    if (action == KC_NO || !key_runtime_core_tap_commit_feedback_mode_allows(tap_count) || pd_mode_lock_action_lookup(action)) {
        return false;
    }

    desc = noah_action_describe(action);
    return !noah_action_desc_is_layer_action(desc) && !noah_action_desc_is_pd_mode_action(desc);
}

static void key_runtime_core_effect_plan_push_feedback_pulse_ex(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, key_feedback_pulse_kind_t kind, uint8_t tap_branch) {
    if (!plan) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
                                                .data.feedback_pulse =
                                                    {
                                                                .key_pos    = key_pos,
                                                                .kind       = kind,
                                                                .tap_branch = tap_branch,
                                                            },
                                            });
}

void key_runtime_core_effect_plan_push_feedback_pulse(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, key_feedback_pulse_kind_t kind) {
    key_runtime_core_effect_plan_push_feedback_pulse_ex(plan, key_pos, kind, 0u);
}

void key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, uint8_t tap_count) {
    if (key_runtime_core_tap_commit_feedback_allowed(action, tap_count)) {
        key_runtime_core_effect_plan_push_feedback_pulse(plan, key_pos, KEY_FEEDBACK_PULSE_TAP_COMMITTED);
    }
}

static key_feedback_pulse_kind_t key_runtime_core_hold_feedback_pulse_kind(bool long_hold_level) {
    return long_hold_level ? KEY_FEEDBACK_PULSE_LONG_HOLD : KEY_FEEDBACK_PULSE_HOLD;
}

static void key_runtime_core_effect_plan_push_delayed_action_with_flags(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count, uint8_t flags) {
    if (!(plan && action != KC_NO && repeat_count != 0u)) {
        return;
    }

    repeat_count &= KEY_RUNTIME_DELAYED_ACTION_REPEAT_COUNT_MASK;
    if (repeat_count == 0u) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind = KEY_RUNTIME_EFFECT_DELAYED_ACTION,
                                                .data.delayed_action =
                                                    {
                                                        .action         = action,
                                                        .packed_key_pos = key_runtime_keypos_pack(key_pos),
                                                        .mods           = mods,
                                                        .repeat_count   = (uint8_t)(repeat_count | flags),
                                                    },
                                            });
}

void key_runtime_core_effect_plan_push_delayed_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
    key_runtime_core_effect_plan_push_delayed_action_with_flags(plan, key_pos, action, mods, repeat_count, 0u);
}

static void key_runtime_core_effect_plan_push_deferred_delayed_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count, bool tap_commit_feedback) {
    uint8_t flags = KEY_RUNTIME_DELAYED_ACTION_FLAG_DEFER_UNTIL_RELEASE;

    if (tap_commit_feedback) {
        flags |= KEY_RUNTIME_DELAYED_ACTION_FLAG_TAP_COMMIT_FEEDBACK;
    }

    key_runtime_core_effect_plan_push_delayed_action_with_flags(plan, key_pos, action, mods, repeat_count, flags);
}

void key_runtime_core_project_effect(const key_runtime_effect_t *effect) {
    if (!effect) {
        return;
    }

    switch (effect->kind) {
        case KEY_RUNTIME_EFFECT_DISPATCH_ACTION:
            noah_emit_action_tap_at(key_runtime_effect_dispatch_action_key_pos(effect), effect->data.dispatch_action.action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
            return;
        case KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER:
            key_runtime_core_pd_projection_preempt_held_action(effect->data.held_action.action);
            key_runtime_core_observe_held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
            held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
            return;
        case KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER:
            held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
            key_runtime_core_observe_held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
            return;
        case KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY:
            key_runtime_core_release_owned_state_by_key(effect->data.key_pos);
            held_action_release_owned_by_key(effect->data.key_pos);
            return;
        case KEY_RUNTIME_EFFECT_REPEAT_START:
            key_runtime_core_observe_repeat_start(effect->data.repeat.key_pos, effect->data.repeat.action, effect->data.repeat.repeat_hz);
            held_repeat_start(effect->data.repeat.key_pos, effect->data.repeat.action, effect->data.repeat.repeat_hz);
            return;
        case KEY_RUNTIME_EFFECT_LAYER_PRESS:
            layer_ownership_momentary_press(effect->data.layer_press.key_pos, effect->data.layer_press.layer);
            return;
        case KEY_RUNTIME_EFFECT_LAYER_RELEASE:
            layer_ownership_momentary_release(effect->data.key_pos);
            return;
        case KEY_RUNTIME_EFFECT_FEEDBACK_PULSE:
            key_runtime_core_feedback_projection_project_pulse(effect->data.feedback_pulse.key_pos, (key_feedback_pulse_kind_t)effect->data.feedback_pulse.kind, effect->data.feedback_pulse.tap_branch);
            return;
        case KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP:
            key_runtime_core_pd_projection_project_lock_tap(effect->data.pd_mode_lock_tap.pd_mode, effect->data.pd_mode_lock_tap.key_pos);
            return;
        case KEY_RUNTIME_EFFECT_DELAYED_ACTION:
            for (uint8_t repeat = 0, repeat_count = (uint8_t)(effect->data.delayed_action.repeat_count & KEY_RUNTIME_DELAYED_ACTION_REPEAT_COUNT_MASK); repeat < repeat_count; repeat++) {
                keypos_t key_pos             = key_runtime_effect_delayed_action_key_pos(effect);
                bool     defer_until_release = (effect->data.delayed_action.repeat_count & KEY_RUNTIME_DELAYED_ACTION_FLAG_DEFER_UNTIL_RELEASE) != 0u;
                bool     tap_commit_feedback = (effect->data.delayed_action.repeat_count & KEY_RUNTIME_DELAYED_ACTION_FLAG_TAP_COMMIT_FEEDBACK) != 0u && repeat == (uint8_t)(repeat_count - 1u);

                if (defer_until_release) {
                    key_runtime_core_state_t *state = key_runtime_core_state();
                    press_token_t            *token = state ? key_runtime_core_press_token_state(state, key_pos) : NULL;
                    uint16_t                  owner = token && token->active ? token->token_id : 0u;

                    if (key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, effect->data.delayed_action.action, effect->data.delayed_action.mods, tap_commit_feedback, owner)) {
                        continue;
                    }
                }

                dispatch_delayed_action_at(key_pos, effect->data.delayed_action.action, effect->data.delayed_action.mods);
                if (defer_until_release && tap_commit_feedback) {
                    key_runtime_core_project_effect(&(key_runtime_effect_t){
                        .kind = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
                        .data.feedback_pulse =
                            {
                                .key_pos = key_pos,
                                .kind    = KEY_FEEDBACK_PULSE_TAP_COMMITTED,
                            },
                    });
                }
            }
            return;
        case KEY_RUNTIME_EFFECT_NONE:
        default:
            return;
    }
}

void key_runtime_core_project_pending_release_dispatch(const pending_release_t *pending) {
    if (!pending) {
        return;
    }

    dispatch_delayed_action_at(pending->key_pos, pending->action, pending->mods);
    if (pending->tap_commit_feedback) {
        key_runtime_core_project_effect(&(key_runtime_effect_t){
            .kind = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
            .data.feedback_pulse =
                {
                    .key_pos = pending->key_pos,
                    .kind    = KEY_FEEDBACK_PULSE_TAP_COMMITTED,
                },
        });
    }
}

static uint16_t key_runtime_core_default_hold_term(uint16_t keycode) {
    return (IS_QK_LAYER_TAP(keycode) || IS_QK_MOD_TAP(keycode)) ? TAPPING_TERM : CUSTOM_TAP_HOLD_TERM;
}

static uint16_t key_runtime_core_default_longer_hold_term(void) {
    return CUSTOM_LONGER_HOLD_TERM;
}

static uint16_t key_runtime_core_default_multi_tap_term(void) {
    return CUSTOM_MULTI_TAP_TERM;
}

static press_token_t *key_runtime_core_press_token_state(key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->press_tokens[key_runtime_core_keypos_index(key_pos)];
}

tap_series_t *key_runtime_core_tap_series_state(key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return NULL;
    }

    return &state->tap_series[key_runtime_core_keypos_index(key_pos)];
}

void key_runtime_core_tap_series_clear(key_runtime_core_state_t *state, tap_series_t *series) {
    if (!(state && series && series->active)) {
        return;
    }

    *series = (tap_series_t){0};
    if (state->tap_series_count != 0u) {
        state->tap_series_count--;
    }
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

static uint16_t key_runtime_core_elapsed(uint16_t start, uint16_t end) {
    return (uint16_t)(end - start);
}

static bool key_runtime_core_tap_series_pending_combo_output(const key_runtime_core_state_t *state, const tap_series_t *series) {
    keypos_t key_pos;

    if (!(state && series && series->active)) {
        return false;
    }

    key_pos = key_runtime_core_tap_series_resolve_key_pos(state, series);
    return key_runtime_core_keypos_valid(key_pos) && noah_qmk_combo_origin_pressed_combo_matches(series->keycode, key_pos, series->last_tap_at, series->tap_term_ms);
}

static bool key_runtime_core_tap_series_can_accept_press(const key_runtime_core_state_t *state, const tap_series_t *series, uint16_t keycode, uint16_t now) {
    return series && series->active && !series->branch_confirmed && !series->branch_confirming && series->keycode == keycode && key_behavior_has_more_taps(series->keycode, series->tap_count) &&
           (key_runtime_core_elapsed(series->last_tap_at, now) <= series->tap_term_ms || key_runtime_core_tap_series_pending_combo_output(state, series));
}

static bool key_runtime_core_interaction_has_multi_tap(key_runtime_slot_interaction_t interaction) {
    return (interaction.flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0u;
}

static layer_state_t key_runtime_core_resolution_layers(const key_runtime_core_state_t *state) {
    return (layer_state | (state ? state->shadow_projection.layer_state : 0u) | ((layer_state_t)1u << 0));
}

static bool key_runtime_core_press_token_has_pending_hold_series(const key_runtime_core_state_t *state, const press_token_t *token) {
    const tap_series_t *series;
    keypos_t            key_pos;

    if (!(state && token && token->active)) {
        return false;
    }

    key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
    series  = key_runtime_core_tap_series_state((key_runtime_core_state_t *)state, key_pos);
    return series && series->active && (series->pending_hold || series->branch_confirming) && series->keycode == token->physical_keycode;
}

static bool key_runtime_core_hold_semantics_owns_state_at_threshold(handled_key_hold_semantics_t semantics) {
    return handled_key_hold_semantics_registers_held(semantics) || handled_key_hold_semantics_repeats_while_held(semantics);
}

static bool key_runtime_core_press_token_quick_tap_suppressed(const press_token_t *token) {
    return token && token->interaction.contract.suppress_tap_on_layer_interrupt && token->momentary_layer_tap_interrupted;
}

static bool key_runtime_core_press_token_has_pd_mode_quick_lock_candidate(const press_token_t *token) {
    return token && token->interaction.contract.quick_tap_pd_mode_lock != 0 && token->pd_mode_was_locked_on_press && !key_runtime_core_press_token_quick_tap_suppressed(token);
}

static bool key_runtime_core_owner_has_runtime_owned_state_lease(const key_runtime_core_state_t *state, uint16_t owner_token_id);

static bool key_runtime_core_press_token_owned_state_active(const key_runtime_core_state_t *state, const press_token_t *token) {
    if (!(state && token && token->active)) {
        return false;
    }

    if (key_runtime_core_owner_has_runtime_owned_state_lease(state, token->token_id)) {
        return true;
    }

    if (token->interaction.contract.quick_release_of_immediate_hold_dispatches_tap && !handled_key_hold_semantics_fires_at_threshold(token->interaction.contract.hold)) {
        return true;
    }

    if (key_runtime_core_elapsed(token->pressed_at, state->current_time) < token->hold_term_ms) {
        return false;
    }

    return key_runtime_core_hold_semantics_owns_state_at_threshold(token->interaction.contract.hold);
}

static void key_runtime_core_press_token_commit_hold_phase(key_runtime_core_state_t *state, press_token_t *token, bool completes_hold, bool long_hold_level) {
    if (!token) {
        return;
    }

    key_runtime_core_press_token_note_hold_feedback(state, token, long_hold_level);
    token->slot_phase = completes_hold ? KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE : KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE;
}

static void key_runtime_core_press_token_mark_release_hold_pending(key_runtime_core_state_t *state, press_token_t *token) {
    if (!token) {
        return;
    }

    key_runtime_core_press_token_note_hold_feedback(state, token, false);
    token->slot_phase = KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING;
}

static bool key_runtime_core_press_token_active_scan_should_mark_release_hold_pending(const press_token_t *token, uint16_t elapsed) {
    if (!(token && token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW)) {
        return false;
    }

    if (token->interaction.contract.hold.release_action != KC_NO && elapsed >= token->interaction.binding.tap_hold_term) {
        return true;
    }

    return !token->interaction.binding.hold.present && token->interaction.contract.long_hold.release_action != KC_NO && elapsed >= token->interaction.binding.longer_hold_term;
}

static void key_runtime_core_press_token_refresh_slot_phase_for_scan(key_runtime_core_state_t *state, press_token_t *token, uint16_t now) {
    uint16_t elapsed;

    if (!(state && token && token->active && token->handled_key)) {
        return;
    }

    elapsed = key_runtime_core_elapsed(token->pressed_at, now);

    if (key_runtime_core_press_token_has_pending_hold_series(state, token)) {
        return;
    }

    switch (token->slot_phase) {
        case KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, true, true);
                return;
            }

            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) && elapsed >= token->interaction.binding.tap_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, !token->interaction.binding.long_hold.present, false);
                return;
            }

            if (key_runtime_core_press_token_active_scan_should_mark_release_hold_pending(token, elapsed)) {
                key_runtime_core_press_token_mark_release_hold_pending(state, token);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, true, true);
                return;
            }

            if (elapsed >= token->interaction.binding.tap_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, !token->interaction.binding.long_hold.present, false);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING:
        case KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_press_token_commit_hold_phase(state, token, true, true);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE:
        case KEY_RUNTIME_SLOT_PHASE_IDLE:
        default:
            return;
    }
}

static void key_runtime_core_press_token_deferred_release_profile(const key_runtime_core_state_t *state, const press_token_t *token, bool *before_tap_term, bool *after_tap_term) {
    bool before = false;
    bool after  = false;

    if (!(state && token && token->active && token->handled_key)) {
        goto done;
    }

    if (key_runtime_core_press_token_has_pending_hold_series(state, token) || key_runtime_core_press_token_owned_state_active(state, token)) {
        goto done;
    }

    if (token->interaction.contract.buffered_base_tap_dispatches_tap) {
        before = true;
        after  = true;
        goto done;
    }

    if (!key_runtime_core_press_token_quick_tap_suppressed(token)) {
        before = token->tap_outcome_available || key_runtime_core_press_token_has_pd_mode_quick_lock_candidate(token);
    }

    after = token->interaction.contract.nonquick_release_dispatches_tap;

done:
    if (before_tap_term) {
        *before_tap_term = before;
    }

    if (after_tap_term) {
        *after_tap_term = after;
    }
}

static bool key_runtime_core_press_token_deferred_release_blocker_tracks_tap_term(const key_runtime_core_state_t *state, const press_token_t *token) {
    bool before = false;
    bool after  = false;

    key_runtime_core_press_token_deferred_release_profile(state, token, &before, &after);
    return before != after;
}

static bool key_runtime_core_press_token_blocks_deferred_release(const key_runtime_core_state_t *state, const press_token_t *token) {
    bool before = false;
    bool after  = false;

    if (!(state && token && token->active)) {
        return false;
    }

    key_runtime_core_press_token_deferred_release_profile(state, token, &before, &after);
    if (before == after) {
        return before;
    }

    return key_runtime_core_elapsed(token->pressed_at, state->current_time) < token->hold_term_ms ? before : after;
}

static uint8_t key_runtime_core_effective_deferred_release_blocker_count(const key_runtime_core_state_t *state) {
    uint8_t count = 0;

    if (!state) {
        return 0u;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        if (key_runtime_core_press_token_blocks_deferred_release(state, &state->press_tokens[index])) {
            count++;
        }
    }

    return count;
}

static uint8_t key_runtime_core_timed_deferred_release_blocker_count(const key_runtime_core_state_t *state) {
    uint8_t count = 0;

    if (!state) {
        return 0u;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        if (key_runtime_core_press_token_deferred_release_blocker_tracks_tap_term(state, &state->press_tokens[index])) {
            count++;
        }
    }

    return count;
}

static bool key_runtime_core_has_foreign_effective_deferred_release_blocker_except(const key_runtime_core_state_t *state, keypos_t key_pos) {
    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];
        keypos_t             token_key_pos;

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        if (!key_runtime_core_press_token_blocks_deferred_release(state, token) || key_runtime_core_keypos_equal(token_key_pos, key_pos)) {
            continue;
        }

        return true;
    }

    return false;
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

        feedback_started_at        = timer_read();
        feedback_sequence          = key_runtime_core_state_next_feedback_sequence(state);
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
    *lease = (lease_t){
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

        feedback_started_at         = timer_read();
        feedback_sequence           = key_runtime_core_state_next_feedback_sequence(state);
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
    *lease = (lease_t){
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

static void key_runtime_core_release_leases_for_token(key_runtime_core_state_t *state, uint16_t owner_token_id) {
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

static bool key_runtime_core_owner_has_runtime_owned_state_lease(const key_runtime_core_state_t *state, uint16_t owner_token_id) {
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

static void key_runtime_core_shadow_projection_recompute(key_runtime_core_state_t *state) {
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

static void key_runtime_core_press_token_attach_press_leases(key_runtime_core_state_t *state, const press_token_t *token) {
    bool           changed = false;
    pd_mode_mask_t mode;
    keypos_t       token_key_pos;

    if (!(state && token && token->active)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);

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

static void key_runtime_core_press_token_attach_hold_leases(key_runtime_core_state_t *state, const press_token_t *token) {
    bool     changed = false;
    keypos_t token_key_pos;

    if (!(state && token && token->active && token->phase == PRESS_TOKEN_PHASE_HELD)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);

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

static void key_runtime_core_press_token_cancel(key_runtime_core_state_t *state, press_token_t *token, uint16_t now) {
    if (!(state && token && token->active)) {
        return;
    }

    key_runtime_core_release_leases_for_token(state, token->token_id);
    key_runtime_core_shadow_projection_recompute(state);
    token->active      = false;
    token->released_at = now;
    token->phase       = PRESS_TOKEN_PHASE_CANCELLED;
    if (state->press_token_count != 0u) {
        state->press_token_count--;
    }
}

static void key_runtime_core_press_token_refresh_phase(key_runtime_core_state_t *state, press_token_t *token, uint16_t now) {
    press_token_phase_t previous_phase;

    if (!(token && token->active)) {
        return;
    }

    previous_phase = token->phase;

    if (token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING || token->phase == PRESS_TOKEN_PHASE_RELEASED || token->phase == PRESS_TOKEN_PHASE_CANCELLED) {
        return;
    }

    if (key_runtime_core_elapsed(token->pressed_at, now) >= token->hold_term_ms) {
        token->phase = PRESS_TOKEN_PHASE_HELD;
    } else if (token->phase == PRESS_TOKEN_PHASE_PRESSED) {
        token->phase = PRESS_TOKEN_PHASE_HOLD_PENDING;
    }

    if (token->phase != previous_phase && token->phase == PRESS_TOKEN_PHASE_HELD) {
        key_runtime_core_press_token_attach_hold_leases(state, token);
    }
}

static void key_runtime_core_tap_series_release_if_expired(tap_series_t *series, uint16_t now, key_runtime_core_state_t *state) {
    uint16_t action;
    uint8_t  repeat_count;

    if (!(series && state && series->active)) {
        return;
    }

    if (series->pending_hold || series->branch_confirmed || series->branch_confirming) {
        return;
    }

    if (key_runtime_core_elapsed(series->last_tap_at, now) <= series->tap_term_ms) {
        return;
    }

    if (!key_runtime_core_pending_multi_tap_flush_resolution(series, &action, &repeat_count)) {
        return;
    }

    if (action == KC_NO && repeat_count == 0u) {
        key_runtime_core_tap_series_clear(state, series);
    }
}

static void key_runtime_core_refresh_for_time(key_runtime_core_state_t *state, uint16_t now) {
    if (!state) {
        return;
    }

    state->current_time = now;

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        key_runtime_core_press_token_refresh_phase(state, &state->press_tokens[index], now);
        key_runtime_core_tap_series_release_if_expired(&state->tap_series[index], now, state);
    }
}

static void key_runtime_core_refresh_slot_phases_for_scan(key_runtime_core_state_t *state, uint16_t now) {
    if (!state) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        key_runtime_core_press_token_refresh_slot_phase_for_scan(state, &state->press_tokens[index], now);
    }
}

static void key_runtime_core_press_token_begin(key_runtime_core_state_t *state, const runtime_key_event_t *event, uint16_t now) {
    press_token_t                 *token;
    tap_series_t                  *series;
    handled_key_resolution_t       resolution;
    handled_key_materialized_t     materialized;
    key_runtime_slot_interaction_t interaction = key_runtime_slot_interaction_default();
    handled_key_resolution_ctx_t   ctx;
    uint16_t                       hold_term_ms;
    uint16_t                       longer_hold_term_ms;
    uint8_t                        tap_count                   = 1u;
    bool                           handled                     = false;
    bool                           tap_outcome_available       = false;
    bool                           pd_mode_was_locked_on_press = false;
    key_runtime_slot_phase_t       slot_phase                  = KEY_RUNTIME_SLOT_PHASE_IDLE;

    if (!(state && event)) {
        return;
    }

    token  = key_runtime_core_press_token_state(state, event->key_pos);
    series = key_runtime_core_tap_series_state(state, event->key_pos);
    if (!token) {
        return;
    }

    if (token->active) {
        state->cancelled_press_count++;
        key_runtime_core_press_token_cancel(state, token, now);
    }

    if (key_runtime_core_tap_series_can_accept_press(state, series, event->keycode, now)) {
        tap_count = (uint8_t)(series->tap_count + 1u);
    }

    resolution          = handled_key_lookup_tap_count(event->keycode, tap_count);
    handled             = handled_key_resolution_is_handled(resolution);
    materialized        = handled_key_materialized_default(resolution);
    hold_term_ms        = key_runtime_core_default_hold_term(event->keycode);
    longer_hold_term_ms = key_runtime_core_default_longer_hold_term();

    if (handled) {
        ctx                         = handled_key_resolution_ctx_make(event->key_pos, key_runtime_core_resolution_layers(state));
        materialized                = handled_key_materialize(resolution, ctx);
        interaction                 = key_runtime_slot_interaction_from_materialized(materialized);
        hold_term_ms                = handled_key_resolution_tap_hold_term(resolution);
        longer_hold_term_ms         = handled_key_resolution_longer_hold_term(resolution);
        tap_outcome_available       = handled_key_resolution_has_multi_tap(resolution) || materialized.tap_action != KC_NO;
        pd_mode_was_locked_on_press = materialized.pd_mode != 0 && state->shadow_projection.pd_mode_local_locked == materialized.pd_mode;
        slot_phase                  = hold_registers_on_press(materialized.hold) ? KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW : KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *other = &state->press_tokens[index];
        keypos_t       other_key_pos;

        other_key_pos = key_runtime_core_press_token_resolve_key_pos(state, other);
        if (!(other->active && !key_runtime_core_keypos_equal(other_key_pos, event->key_pos))) {
            continue;
        }

        other->other_press_interrupted = true;
        if (other->interaction.contract.suppress_tap_on_layer_interrupt) {
            other->momentary_layer_tap_interrupted = true;
        }
    }

    *token = (press_token_t){
        .active                      = true,
        .token_id                    = state->next_token_id++,
        .physical_keycode            = event->keycode,
        .resolved_keycode            = event->keycode,
        .observed_release_keycode    = KC_NO,
        .pressed_at                  = now,
        .hold_term_ms                = hold_term_ms,
        .longer_hold_term_ms         = longer_hold_term_ms,
        .feedback_sequence           = key_runtime_core_state_next_feedback_sequence(state),
        .phase                       = PRESS_TOKEN_PHASE_PRESSED,
        .feedback_level              = KEY_RUNTIME_CORE_FEEDBACK_LEVEL_PRESS,
        .handled_key                 = handled,
        .tap_outcome_available       = tap_outcome_available,
        .pd_mode_was_locked_on_press = pd_mode_was_locked_on_press,
        .interaction                 = interaction,
        .slot_phase                  = slot_phase,
    };
    state->press_token_count++;
    key_runtime_core_press_token_attach_press_leases(state, token);

    if (series && tap_count > 1u) {
        series->pending_hold = materialized.hold.present || materialized.long_hold.present;
    }
}

static void key_runtime_core_tap_series_note_tap(key_runtime_core_state_t *state, const press_token_t *token, uint16_t now) {
    tap_series_t        *series;
    keypos_t             token_key_pos;
    bool                 reuse_existing;
    bool                 needs_series;
    uint16_t             single_action;
    uint16_t             tap_action;
    uint8_t              tap_repeat_count;
    bool                 has_more_taps;
    hold_behavior_t      hold;
    hold_behavior_t      long_hold;
    uint16_t             tap_hold_term_ms;
    uint16_t             branch_confirm_term_ms;
    uint16_t             tap_term_ms;
    uint8_t              tap_count;
    bool                 pending_hold = false;
    keyboard_mod_state_t saved_mod_state;

    if (!(state && token && token->active)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
    series        = key_runtime_core_tap_series_state(state, token_key_pos);
    if (!series) {
        return;
    }

    reuse_existing   = key_runtime_core_tap_series_can_accept_press(state, series, token->resolved_keycode, now);
    single_action    = reuse_existing ? series->single_action : token->resolved_keycode;
    tap_action       = token->resolved_keycode;
    tap_repeat_count = 0u;
    has_more_taps    = false;
    hold             = hold_behavior_none();
    long_hold        = hold_behavior_none();
    tap_hold_term_ms = key_runtime_core_default_hold_term(token->resolved_keycode);
    branch_confirm_term_ms = CUSTOM_TAP_BRANCH_CONFIRM_TERM;
    tap_term_ms      = key_runtime_core_default_multi_tap_term();
    tap_count        = (uint8_t)(reuse_existing ? (uint8_t)(series->tap_count + 1u) : 1u);
    saved_mod_state  = reuse_existing ? series->saved_mod_state
                                      : (keyboard_mod_state_t){
                                            .real           = get_mods(),
                                            .weak           = get_weak_mods(),
                                            .oneshot        = get_oneshot_mods(),
                                            .oneshot_locked = get_oneshot_locked_mods(),
                                        };
    if (!reuse_existing) {
        saved_mod_state.real &= (uint8_t)~pd_mode_buffered_tap_masked_real_mods(token->resolved_keycode);
    }

    if (token->handled_key) {
        uint16_t handled_tap_action = token->interaction.binding.tap_action;

        single_action    = reuse_existing ? series->single_action : handled_tap_action;
        tap_action       = handled_tap_action;
        tap_repeat_count = token->interaction.binding.tap_repeat_count;
        has_more_taps    = token->interaction.binding.has_more_taps;
        hold             = token->interaction.binding.hold;
        long_hold        = token->interaction.binding.long_hold;
        pending_hold     = reuse_existing && (hold.present || long_hold.present);
        tap_hold_term_ms = token->interaction.binding.tap_hold_term;
        branch_confirm_term_ms = token->interaction.binding.branch_confirm_term;
        tap_term_ms      = token->interaction.binding.multi_tap_term;
        if (token->interaction.selection.tap_count != 0u) {
            tap_count = token->interaction.selection.tap_count;
        }
    }

    needs_series = reuse_existing || (token->handled_key && has_more_taps);
    if (!needs_series) {
        return;
    }

    if (!series->active) {
        state->tap_series_count++;
    }

    *series = (tap_series_t){
        .active           = true,
        .keycode          = token->resolved_keycode,
        .tap_count        = tap_count,
        .pending_hold     = pending_hold,
        .single_action    = single_action,
        .tap_action       = tap_action,
        .tap_repeat_count = tap_repeat_count,
        .has_more_taps    = has_more_taps,
        .hold             = hold,
        .long_hold        = long_hold,
        .tap_hold_term_ms = tap_hold_term_ms,
        .branch_confirm_term_ms = branch_confirm_term_ms,
        .last_action      = tap_action,
        .last_tap_at      = now,
        .tap_term_ms      = tap_term_ms,
        .feedback_sequence = key_runtime_core_state_next_feedback_sequence(state),
        .saved_mod_state  = saved_mod_state,
    };
}

static void key_runtime_core_tap_series_note_hold_release(key_runtime_core_state_t *state, const press_token_t *token) {
    tap_series_t *series;
    keypos_t      token_key_pos;

    if (!(state && token)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
    series        = key_runtime_core_tap_series_state(state, token_key_pos);
    if (!(series && series->active && series->pending_hold && series->keycode == token->resolved_keycode)) {
        return;
    }

    series->pending_hold = false;
}

static void key_runtime_core_tap_series_preserve_release(key_runtime_core_state_t *state, keypos_t key_pos) {
    tap_series_t *series;

    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    series = key_runtime_core_tap_series_state(state, key_pos);
    if (!(series && series->active)) {
        return;
    }

    if (series->pending_hold) {
        series->pending_hold = false;
        series->hold         = hold_behavior_none();
        series->long_hold    = hold_behavior_none();
    }
    series->last_tap_at       = state->current_time;
    series->feedback_sequence = key_runtime_core_state_next_feedback_sequence(state);
}

static bool key_runtime_core_branch_confirm_window_active(uint16_t started_at, uint16_t term_ms, uint16_t now) {
    return term_ms != 0u && key_runtime_core_elapsed(started_at, now) < term_ms;
}

bool key_runtime_core_tap_count_uses_branch_confirm(uint8_t tap_count) {
    return tap_count > 1u;
}

static bool key_runtime_core_tap_series_start_branch_confirm(key_runtime_core_state_t *state, tap_series_t *series, key_runtime_tap_series_branch_confirm_kind_t kind, uint8_t tap_count, uint16_t started_at, uint16_t term_ms) {
    if (!(state && series && series->active && kind != KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_NONE && key_runtime_core_tap_count_uses_branch_confirm(tap_count))) {
        return false;
    }

    if (!key_runtime_core_branch_confirm_window_active(started_at, term_ms, state->current_time)) {
        return false;
    }

    series->pending_hold                       = false;
    series->branch_confirming                  = true;
    series->branch_confirm_kind                = (uint8_t)kind;
    series->branch_confirm_tap_count           = tap_count;
    series->branch_confirm_started_at          = started_at;
    series->branch_confirm_duration_ms         = term_ms;
    series->branch_confirm_action              = KC_NO;
    series->branch_confirm_repeat_count        = 0u;
    series->branch_confirm_mods                = (delayed_action_mods_t){0};
    series->branch_confirm_tap_commit_feedback = false;
    series->branch_confirm_action_feedback     = false;
    series->branch_confirm_action_feedback_kind = KEY_FEEDBACK_PULSE_HOLD;
    series->branch_confirm_long_hold_level     = false;
    series->feedback_sequence                  = key_runtime_core_state_next_feedback_sequence(state);
    return true;
}

bool key_runtime_core_tap_series_start_delayed_action_branch_confirm(key_runtime_core_state_t *state, tap_series_t *series, uint8_t tap_count, uint16_t started_at, uint16_t term_ms, uint16_t action, uint8_t repeat_count, delayed_action_mods_t mods, bool tap_commit_feedback, bool action_feedback, key_feedback_pulse_kind_t action_feedback_kind) {
    if (action == KC_NO || repeat_count == 0u) {
        return false;
    }

    if (!key_runtime_core_tap_series_start_branch_confirm(state, series, KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION, tap_count, started_at, term_ms)) {
        return false;
    }

    series->branch_confirm_action               = action;
    series->branch_confirm_repeat_count         = repeat_count;
    series->branch_confirm_mods                 = mods;
    series->branch_confirm_tap_commit_feedback  = tap_commit_feedback;
    series->branch_confirm_action_feedback      = action_feedback;
    series->branch_confirm_action_feedback_kind = (uint8_t)action_feedback_kind;
    return true;
}

static void key_runtime_core_plan_same_key_branch_confirm_interruption(key_runtime_core_state_t *state, tap_series_t *series, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    if (!(state && series && series->active && series->branch_confirming && plan)) {
        return;
    }

    if (series->branch_confirm_action_feedback) {
        key_runtime_core_plan_branch_confirm_delayed_action(state, series, key_pos, plan);
        return;
    }

    key_runtime_core_effect_plan_push_deferred_delayed_action(plan, key_pos, series->branch_confirm_action, series->branch_confirm_mods, series->branch_confirm_repeat_count, series->branch_confirm_tap_commit_feedback);
    key_runtime_core_tap_series_clear(state, series);
}

static bool key_runtime_core_pending_release_matches(const pending_release_slot_t *pending, keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    return key_runtime_core_pending_release_slot_matches(pending, key_pos, action, mods);
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

static void key_runtime_core_press_token_end(key_runtime_core_state_t *state, const runtime_key_event_t *event, uint16_t now) {
    press_token_t  released_token;
    press_token_t *token;

    if (!(state && event)) {
        return;
    }

    token = key_runtime_core_press_token_state(state, event->key_pos);
    if (!(token && token->active)) {
        state->orphan_release_count++;
        return;
    }

    key_runtime_core_press_token_refresh_phase(state, token, now);
    token->observed_release_keycode = event->keycode;
    token->released_at              = now;
    if (event->keycode != token->resolved_keycode) {
        token->release_keycode_mismatched = true;
        state->release_keycode_mismatch_count++;
    }

    released_token = *token;
    if (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW && key_runtime_core_elapsed(token->pressed_at, now) < token->hold_term_ms) {
        key_runtime_core_tap_series_note_tap(state, token, now);
    } else {
        key_runtime_core_tap_series_note_hold_release(state, token);
    }

    *token        = released_token;
    token->active = false;
    token->phase  = PRESS_TOKEN_PHASE_RELEASED;
    if (state->press_token_count != 0u) {
        state->press_token_count--;
    }
}

void key_runtime_core_apply_event(const runtime_event_t *event, uint16_t event_time) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && event)) {
        return;
    }

    switch (event->kind) {
        case RUNTIME_EVENT_KIND_KEY_DOWN:
            key_runtime_core_refresh_for_time(state, event_time);
            key_runtime_core_press_token_begin(state, &event->data.key_event, event_time);
            return;
        case RUNTIME_EVENT_KIND_KEY_UP:
            key_runtime_core_refresh_for_time(state, event_time);
            key_runtime_core_press_token_end(state, &event->data.key_event, event_time);
            return;
        case RUNTIME_EVENT_KIND_TIMER_ADVANCE:
            key_runtime_core_refresh_for_time(state, (uint16_t)(event_time + event->data.timer_advance.advance_ms));
            return;
        case RUNTIME_EVENT_KIND_SCAN:
            key_runtime_core_refresh_for_time(state, event_time);
            key_runtime_core_refresh_slot_phases_for_scan(state, event_time);
            return;
        case RUNTIME_EVENT_KIND_POINTER_REPORT:
        case RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT:
            key_runtime_core_refresh_for_time(state, event_time);
            return;
    }
}

void key_runtime_core_observe_process_record_event(uint16_t keycode, keyrecord_t *record) {
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
    key_runtime_core_apply_event(&event, timer_read());
}

void key_runtime_core_observe_scan_cycle(uint16_t now) {
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, now);
}

bool key_runtime_core_blocker_queries_authoritative(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return state != NULL;
}

bool key_runtime_core_has_any_deferred_release_blocker(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return key_runtime_core_blocker_queries_authoritative() && key_runtime_core_effective_deferred_release_blocker_count(state) != 0u;
}

bool key_runtime_core_has_foreign_deferred_release_blocker_except(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return key_runtime_core_blocker_queries_authoritative() && key_runtime_core_has_foreign_effective_deferred_release_blocker_except(state, key_pos);
}

uint8_t key_runtime_core_pending_release_count(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return state ? state->pending_release_count : 0u;
}

static bool key_runtime_core_queue_pending_release_dispatch_for_owner(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, bool tap_commit_feedback, uint16_t owner_token_id) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    pending_release_slot_t   *pending;
    uint8_t                   flags = KEY_RUNTIME_PENDING_RELEASE_FLAG_ACTIVE;

    if (!(state && action != KC_NO && key_runtime_core_keypos_valid(key_pos))) {
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
    press_token_t            *token = state ? key_runtime_core_press_token_state(state, key_pos) : NULL;

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

bool key_runtime_core_reset_pending_multi_tap(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    tap_series_t             *series;

    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    series = key_runtime_core_tap_series_state(state, key_pos);
    if (!(series && series->active)) {
        return false;
    }

    key_runtime_core_tap_series_clear(state, series);
    return true;
}

uint8_t key_runtime_core_take_pending_release_dispatches(pending_release_t *out, uint8_t capacity) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    uint8_t                   count = 0;

    if (!(state && out && capacity != 0u) || key_runtime_core_effective_deferred_release_blocker_count(state) != 0u) {
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

void key_runtime_core_observe_held_action_register(keypos_t key_pos, uint16_t action) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;
    pd_mode_mask_t            mode;

    if (!(state && action != KC_NO && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    token = key_runtime_core_press_token_state(state, key_pos);
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

        if (key_runtime_core_elapsed(token->pressed_at, state->current_time) >= token->interaction.binding.tap_hold_term) {
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

    token = key_runtime_core_press_token_state(state, key_pos);
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

    token              = key_runtime_core_press_token_state(state, key_pos);
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

bool key_runtime_core_resolve_pending_multi_tap_scan(keypos_t key_pos, key_runtime_core_pending_multi_tap_scan_resolution_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;
    tap_series_t             *series;
    uint16_t                  elapsed;
    uint16_t                  flush_action;
    uint8_t                   flush_repeat_count;

    if (out) {
        *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){0};
    }

    if (!(state && out && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    token  = key_runtime_core_press_token_state(state, key_pos);
    series = key_runtime_core_tap_series_state(state, key_pos);
    if (!(series && series->active)) {
        return false;
    }

    if (!series->pending_hold && token && token->active && token->handled_key && series->keycode == token->resolved_keycode) {
        return true;
    }

    if (!series->pending_hold) {
        if (key_runtime_core_tap_series_pending_combo_output(state, series)) {
            return true;
        }

        if (key_runtime_core_elapsed(series->last_tap_at, state->current_time) > series->tap_term_ms && key_runtime_core_pending_multi_tap_flush_resolution(series, &flush_action, &flush_repeat_count)) {
            uint8_t flush_tap_count = series->tap_count;

            *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){
                .outcome      = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_FLUSH,
                .action       = flush_action,
                .repeat_count = flush_repeat_count,
                .tap_count    = flush_tap_count,
            };
        }

        return true;
    }

    if (!(token && token->active && token->handled_key && series->keycode == token->resolved_keycode)) {
        return false;
    }

    elapsed = key_runtime_core_elapsed(token->pressed_at, state->current_time);
    if (token->interaction.contract.hold.release_action != KC_NO && elapsed >= token->interaction.binding.tap_hold_term && token->slot_phase != KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING) {
        *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){
            .outcome   = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_RELEASE_HOLD_PENDING,
            .tap_count = series->tap_count,
        };
        return true;
    }

    if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
        *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){
            .outcome        = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD,
            .hold           = token->interaction.binding.long_hold,
            .semantics      = token->interaction.contract.long_hold,
            .completes_hold = true,
            .action         = token->interaction.binding.long_hold.action,
        };
        return true;
    }

    if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) && elapsed >= token->interaction.binding.tap_hold_term) {
        *out = (key_runtime_core_pending_multi_tap_scan_resolution_t){
            .outcome        = KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD,
            .hold           = token->interaction.binding.hold,
            .semantics      = token->interaction.contract.hold,
            .completes_hold = !token->interaction.binding.long_hold.present,
            .action         = token->interaction.binding.hold.action,
        };
        return true;
    }

    return true;
}

static bool key_runtime_core_press_token_allows_tap_release(const press_token_t *token) {
    if (!(token && token->active && token->handled_key)) {
        return false;
    }

    return token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || token->slot_phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW;
}

static bool key_runtime_core_press_token_uses_implicit_hold(const press_token_t *token) {
    return token && token->handled_key && key_runtime_slot_interaction_uses_implicit_hold(token->interaction);
}

static bool key_runtime_core_press_token_uses_fallback_hold(const press_token_t *token) {
    return token && token->handled_key && key_runtime_slot_interaction_uses_fallback_hold(token->interaction);
}

static uint8_t key_runtime_core_press_token_preview_layer_hint(const press_token_t *token) {
    return token ? token->interaction.contract.hold.preview_layer : UINT8_MAX;
}

static uint16_t key_runtime_core_key_pos_held_action_keycode(const key_runtime_core_state_t *state, keypos_t key_pos) {
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

static bool key_runtime_core_key_pos_repeat_active(const key_runtime_core_state_t *state, keypos_t key_pos) {
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

bool key_runtime_core_flashing_feedback_started_at(keypos_t key_pos, uint16_t *out_started_at) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    bool                      found = false;
    uint16_t                  newest_started_at = 0;
    uint32_t                  newest_sequence = 0;

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
    key_runtime_core_state_t *state = key_runtime_core_state();
    bool                      found = false;
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

static bool key_runtime_core_press_token_has_runtime_owned_state(const key_runtime_core_state_t *state, const press_token_t *token) {
    keypos_t token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);

    return token && key_runtime_core_key_pos_held_action_keycode(state, token_key_pos) != KC_NO ? true : key_runtime_core_key_pos_repeat_active(state, token ? token_key_pos : (keypos_t){0});
}

static bool key_runtime_core_hold_activation_needs_pulse(hold_behavior_t hold, handled_key_hold_semantics_t semantics, bool pulse_momentary_layer_action) {
    noah_action_desc_t desc = noah_action_describe(hold.action);

    if (semantics.threshold == HANDLED_KEY_HOLD_THRESHOLD_DISPATCH) {
        return true;
    }

    if (noah_action_desc_is_momentary_layer_keycode(desc)) {
        return pulse_momentary_layer_action;
    }

    return false;
}

static void key_runtime_core_effect_plan_append_release_plan(key_runtime_core_effect_plan_t *plan, const key_runtime_core_release_effect_plan_t *release_plan) {
    if (!(plan && release_plan)) {
        return;
    }

    for (uint8_t index = 0; index < release_plan->count; index++) {
        key_runtime_core_effect_plan_push(plan, release_plan->items[index]);
    }
}

static void key_runtime_core_apply_release_settlement(key_runtime_core_state_t *state, keypos_t key_pos, const key_runtime_core_release_effect_plan_t *release_plan) {
    press_token_t *token;
    uint8_t        lease_count_before;

    if (!(state && release_plan && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    switch (release_plan->settlement) {
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_CLEAR_ACTIVE_PRESERVE_PENDING_MULTI_TAP:
            key_runtime_core_tap_series_preserve_release(state, key_pos);
            break;
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_RESET:
            (void)key_runtime_core_reset_pending_multi_tap(key_pos);
            break;
        case KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_NONE:
        default:
            return;
    }

    token = key_runtime_core_press_token_state(state, key_pos);
    if (token) {
        lease_count_before = state->lease_count;
        key_runtime_core_release_leases_for_token(state, token->token_id);
        if (state->lease_count != lease_count_before) {
            key_runtime_core_shadow_projection_recompute(state);
        }
    }
    if (token && !token->active && !token->pending_release_emission) {
        *token = (press_token_t){0};
    }
}

static void key_runtime_core_tap_series_seed(key_runtime_core_state_t *state, const key_runtime_core_pending_multi_tap_seed_t *seed, keyboard_mod_state_t mods) {
    tap_series_t *series;

    if (!(state && seed && seed->active)) {
        return;
    }

    series = key_runtime_core_tap_series_state(state, seed->key_pos);
    if (!series) {
        return;
    }

    if (!series->active) {
        state->tap_series_count++;
    }

    *series = (tap_series_t){
        .active           = true,
        .keycode          = seed->keycode,
        .tap_count        = seed->tap_count == 0u ? 1u : seed->tap_count,
        .pending_hold     = false,
        .single_action    = seed->tap_action,
        .tap_action       = seed->tap_action,
        .tap_repeat_count = seed->tap_repeat_count,
        .has_more_taps    = seed->has_more_taps,
        .hold             = hold_behavior_none(),
        .long_hold        = hold_behavior_none(),
        .tap_hold_term_ms = seed->tap_hold_term,
        .branch_confirm_term_ms = seed->branch_confirm_term,
        .last_action      = seed->tap_action,
        .last_tap_at      = state->current_time,
        .tap_term_ms      = seed->multi_tap_term,
        .feedback_sequence = key_runtime_core_state_next_feedback_sequence(state),
        .saved_mod_state  = mods,
    };
}

static void key_runtime_core_tap_series_update_for_press(key_runtime_core_state_t *state, tap_series_t *series, const press_token_t *token) {
    keypos_t token_key_pos;

    if (!(state && series && token && token->active && token->handled_key)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);

    if (!series->active) {
        state->tap_series_count++;
    }

    series->active           = true;
    series->keycode          = token->resolved_keycode;
    series->tap_count        = token->interaction.selection.tap_count ? token->interaction.selection.tap_count : (uint8_t)(series->tap_count + 1u);
    series->pending_hold     = token->interaction.binding.hold.present || token->interaction.binding.long_hold.present;
    series->single_action    = series->single_action == KC_NO ? token->interaction.binding.tap_action : series->single_action;
    series->tap_action       = token->interaction.binding.tap_action;
    series->tap_repeat_count = token->interaction.binding.tap_repeat_count;
    series->has_more_taps    = token->interaction.binding.has_more_taps;
    series->hold             = token->interaction.binding.hold;
    series->long_hold        = token->interaction.binding.long_hold;
    series->tap_hold_term_ms = token->interaction.binding.tap_hold_term;
    series->branch_confirm_term_ms = token->interaction.binding.branch_confirm_term;
    series->last_action      = token->interaction.binding.tap_action;
    series->last_tap_at      = state->current_time;
    series->tap_term_ms      = token->interaction.binding.multi_tap_term;
    series->feedback_sequence = key_runtime_core_state_next_feedback_sequence(state);
    (void)token_key_pos;
}

static void key_runtime_core_plan_fallback_hold_activation(key_runtime_core_state_t *state, press_token_t *token, key_runtime_core_effect_plan_t *plan) {
    keypos_t token_key_pos;

    if (!(state && token && token->active && token->handled_key && key_runtime_core_press_token_uses_fallback_hold(token))) {
        return;
    }

    if (key_runtime_core_press_token_has_runtime_owned_state(state, token) || token->resolved_keycode == KC_NO) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
    key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, token_key_pos, token->resolved_keycode);
}

static void key_runtime_core_clear_confirmed_tap_series_at(key_runtime_core_state_t *state, keypos_t key_pos) {
    tap_series_t *series = key_runtime_core_tap_series_state(state, key_pos);

    if (series && series->active && series->branch_confirmed) {
        key_runtime_core_tap_series_clear(state, series);
    }
}

static void key_runtime_core_plan_threshold_hold_effects(key_runtime_core_state_t *state, press_token_t *token, hold_behavior_t hold, handled_key_hold_semantics_t semantics, bool completes_hold, bool long_hold_level, key_runtime_core_effect_plan_t *plan) {
    keypos_t token_key_pos;
    key_feedback_pulse_kind_t feedback_kind;

    if (!(state && token && token->active && token->handled_key && hold.action != KC_NO)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
    feedback_kind = key_runtime_core_hold_feedback_pulse_kind(long_hold_level);

    if (key_runtime_core_press_token_has_runtime_owned_state(state, token)) {
        key_runtime_core_effect_plan_push_release_owned_state(plan, token_key_pos);
    }

    switch (semantics.threshold) {
        case HANDLED_KEY_HOLD_THRESHOLD_DISPATCH:
            key_runtime_core_effect_plan_push_dispatch_action(plan, token_key_pos, hold.action);
            key_runtime_core_effect_plan_push_feedback_pulse(plan, token_key_pos, feedback_kind);
            key_runtime_core_press_token_commit_hold_phase(state, token, completes_hold, long_hold_level);
            return;
        case HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD:
            key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, token_key_pos, hold.action);
            if (key_runtime_core_hold_activation_needs_pulse(hold, semantics, false)) {
                key_runtime_core_effect_plan_push_feedback_pulse(plan, token_key_pos, feedback_kind);
            }
            key_runtime_core_press_token_commit_hold_phase(state, token, completes_hold, long_hold_level);
            return;
        case HANDLED_KEY_HOLD_THRESHOLD_REPEAT:
            key_runtime_core_effect_plan_push_repeat_start(plan, token_key_pos, hold.action, hold.repeat_hz);
            key_runtime_core_effect_plan_push_feedback_pulse(plan, token_key_pos, feedback_kind);
            key_runtime_core_press_token_commit_hold_phase(state, token, completes_hold, long_hold_level);
            return;
        case HANDLED_KEY_HOLD_THRESHOLD_NONE:
        default:
            return;
    }
}

static uint16_t key_runtime_core_token_branch_confirm_started_at(const press_token_t *token, bool long_hold_level) {
    uint16_t boundary;

    if (!token) {
        return 0u;
    }

    boundary = long_hold_level ? token->interaction.binding.longer_hold_term : token->interaction.binding.tap_hold_term;
    return (uint16_t)(token->pressed_at + boundary);
}

static bool key_runtime_core_tap_series_start_threshold_branch_confirm(key_runtime_core_state_t *state, tap_series_t *series, const press_token_t *token, key_runtime_tap_series_branch_confirm_kind_t kind, bool long_hold_level) {
    bool started;

    if (!(state && series && token && key_runtime_core_interaction_has_multi_tap(token->interaction))) {
        return false;
    }

    started = key_runtime_core_tap_series_start_branch_confirm(state, series, kind, token->interaction.selection.tap_count, key_runtime_core_token_branch_confirm_started_at(token, long_hold_level), token->interaction.binding.branch_confirm_term);
    if (started) {
        series->branch_confirm_long_hold_level = long_hold_level;
    }
    return started;
}

static void key_runtime_core_plan_branch_confirm_complete(key_runtime_core_state_t *state, tap_series_t *series, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    press_token_t *token;
    uint16_t       elapsed;

    if (!(state && series && series->active && series->branch_confirming && plan)) {
        return;
    }

    if (key_runtime_core_branch_confirm_window_active(series->branch_confirm_started_at, series->branch_confirm_duration_ms, state->current_time)) {
        return;
    }

    switch ((key_runtime_tap_series_branch_confirm_kind_t)series->branch_confirm_kind) {
        case KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION:
            key_runtime_core_plan_branch_confirm_delayed_action(state, series, key_pos, plan);
            return;
        case KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_RELEASE_HOLD_PENDING:
            token = key_runtime_core_press_token_state(state, key_pos);
            if (!(token && token->active && token->handled_key)) {
                key_runtime_core_tap_series_clear(state, series);
                return;
            }

            key_runtime_core_press_token_mark_release_hold_pending(state, token);
            series->branch_confirming = false;
            series->branch_confirmed  = true;
            series->pending_hold      = false;
            series->hold              = hold_behavior_none();
            series->long_hold         = hold_behavior_none();

            elapsed = key_runtime_core_elapsed(token->pressed_at, state->current_time);
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_tap_series_clear(state, series);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
            }
            return;
        case KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_THRESHOLD_HOLD:
            token = key_runtime_core_press_token_state(state, key_pos);
            if (!(token && token->active && token->handled_key)) {
                key_runtime_core_tap_series_clear(state, series);
                return;
            }

            if (series->branch_confirm_long_hold_level) {
                key_runtime_core_tap_series_clear(state, series);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
                return;
            }

            key_runtime_core_tap_series_clear(state, series);
            key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.hold, token->interaction.contract.hold, !token->interaction.binding.long_hold.present, false, plan);
            return;
        case KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_NONE:
        default:
            key_runtime_core_tap_series_clear(state, series);
            return;
    }
}

static void key_runtime_core_plan_pending_multi_tap_scan_for_key(key_runtime_core_state_t *state, keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_pending_multi_tap_scan_resolution_t resolution;
    press_token_t                                       *token;
    tap_series_t                                        *series;
    delayed_action_mods_t                                mods;

    if (!(state && plan)) {
        return;
    }

    series = key_runtime_core_tap_series_state(state, key_pos);
    mods   = series ? series->saved_mod_state : (delayed_action_mods_t){0};
    if (series && series->active && series->branch_confirming) {
        key_runtime_core_plan_branch_confirm_complete(state, series, key_pos, plan);
        return;
    }

    if (!key_runtime_core_resolve_pending_multi_tap_scan(key_pos, &resolution)) {
        return;
    }

    token = key_runtime_core_press_token_state(state, key_pos);
    switch (resolution.outcome) {
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_FLUSH:
            if (key_runtime_core_tap_series_start_delayed_action_branch_confirm(state, series, resolution.tap_count, (uint16_t)(series->last_tap_at + series->tap_term_ms), series->branch_confirm_term_ms, resolution.action, resolution.repeat_count, mods, true, false, KEY_FEEDBACK_PULSE_HOLD)) {
                return;
            }
            key_runtime_core_effect_plan_push_delayed_action(plan, key_pos, resolution.action, mods, resolution.repeat_count);
            key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, key_pos, resolution.action, resolution.tap_count);
            key_runtime_core_tap_series_clear(state, series);
            return;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD:
            if (key_runtime_core_tap_series_start_threshold_branch_confirm(state, series, token, KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_THRESHOLD_HOLD, false)) {
                return;
            }
            key_runtime_core_tap_series_clear(state, series);
            key_runtime_core_plan_threshold_hold_effects(state, token, resolution.hold, resolution.semantics, resolution.completes_hold, false, plan);
            return;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD:
            if (key_runtime_core_tap_series_start_threshold_branch_confirm(state, series, token, KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_THRESHOLD_HOLD, true)) {
                return;
            }
            key_runtime_core_tap_series_clear(state, series);
            key_runtime_core_plan_threshold_hold_effects(state, token, resolution.hold, resolution.semantics, resolution.completes_hold, true, plan);
            return;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_RELEASE_HOLD_PENDING:
            if (key_runtime_core_tap_series_start_threshold_branch_confirm(state, series, token, KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_RELEASE_HOLD_PENDING, false)) {
                return;
            }
            if (token) {
                key_runtime_core_press_token_mark_release_hold_pending(state, token);
                if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && key_runtime_core_elapsed(token->pressed_at, state->current_time) >= token->interaction.binding.longer_hold_term) {
                    key_runtime_core_tap_series_clear(state, series);
                    key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
                    return;
                }
            }
            if (series) {
                series->branch_confirmed = true;
                series->pending_hold     = false;
                series->hold             = hold_behavior_none();
                series->long_hold        = hold_behavior_none();
            }
            return;
        case KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE:
        default:
            return;
    }
}

static void key_runtime_core_plan_active_scan_for_token(key_runtime_core_state_t *state, press_token_t *token, key_runtime_core_effect_plan_t *plan) {
    uint16_t elapsed;
    keypos_t token_key_pos;

    if (!(state && token && token->active && token->handled_key && plan)) {
        return;
    }

    token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);

    if (key_runtime_core_press_token_has_pending_hold_series(state, token)) {
        return;
    }

    elapsed = key_runtime_core_elapsed(token->pressed_at, state->current_time);

    switch (token->slot_phase) {
        case KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW:
            if (key_runtime_core_press_token_uses_fallback_hold(token) && key_runtime_core_key_pos_held_action_keycode(state, token_key_pos) == KC_NO && elapsed >= token->interaction.binding.tap_hold_term) {
                key_runtime_core_plan_fallback_hold_activation(state, token, plan);
                return;
            }

            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_clear_confirmed_tap_series_at(state, token_key_pos);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
                return;
            }

            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) && elapsed >= token->interaction.binding.tap_hold_term) {
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.hold, token->interaction.contract.hold, !token->interaction.binding.long_hold.present, false, plan);
                return;
            }

            if (key_runtime_core_press_token_active_scan_should_mark_release_hold_pending(token, elapsed)) {
                key_runtime_core_press_token_mark_release_hold_pending(state, token);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW:
            if (elapsed >= token->interaction.binding.tap_hold_term) {
                if (!key_runtime_core_press_token_uses_implicit_hold(token)) {
                    key_runtime_core_effect_plan_push_feedback_pulse(plan, token_key_pos, KEY_FEEDBACK_PULSE_HOLD);
                }
                key_runtime_core_press_token_commit_hold_phase(state, token, !token->interaction.binding.long_hold.present, false);
            }
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_clear_confirmed_tap_series_at(state, token_key_pos);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING:
        case KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE:
            if (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.long_hold) && elapsed >= token->interaction.binding.longer_hold_term) {
                key_runtime_core_clear_confirmed_tap_series_at(state, token_key_pos);
                key_runtime_core_plan_threshold_hold_effects(state, token, token->interaction.binding.long_hold, token->interaction.contract.long_hold, true, true, plan);
            }
            return;
        case KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE:
        case KEY_RUNTIME_SLOT_PHASE_IDLE:
        default:
            return;
    }
}

void key_runtime_core_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan)) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token = &state->press_tokens[index];
        keypos_t       token_key_pos;

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        if (!(token->active && !key_runtime_core_keypos_equal(token_key_pos, key_pos))) {
            continue;
        }

        key_runtime_core_plan_fallback_hold_activation(state, token, plan);
        token->other_press_interrupted = true;
        if (token->interaction.contract.suppress_tap_on_layer_interrupt) {
            token->momentary_layer_tap_interrupted = true;
        }
    }
}

void key_runtime_core_flush_active_keys_except(keypos_t key_pos, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan)) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token = &state->press_tokens[index];
        uint16_t       held_action;
        keypos_t       token_key_pos;

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        if (!(token->active && !key_runtime_core_keypos_equal(token_key_pos, key_pos))) {
            continue;
        }

        held_action = key_runtime_core_key_pos_held_action_keycode(state, token_key_pos);
        if (key_runtime_core_press_token_allows_tap_release(token) && held_action == KC_NO && !key_runtime_core_key_pos_repeat_active(state, token_key_pos) && !is_layer_key(token->resolved_keycode) && token->interaction.binding.tap_action != KC_NO) {
            key_runtime_core_effect_plan_push_dispatch_action(plan, token_key_pos, token->interaction.binding.tap_action);
            key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, token_key_pos, token->interaction.binding.tap_action, token->interaction.selection.tap_count);
        } else if (held_action != KC_NO && !held_action_survives_flush(token_key_pos, held_action)) {
            key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER, token_key_pos, held_action);
        } else if (key_runtime_core_key_pos_repeat_active(state, token_key_pos)) {
            key_runtime_core_effect_plan_push_release_owned_state(plan, token_key_pos);
        }

        key_runtime_core_press_token_cancel(state, token, state->current_time);
        key_runtime_core_shadow_projection_recompute(state);
    }
}

bool key_runtime_core_handle_handled_key_press(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token;
    tap_series_t             *series;
    uint16_t                  action;
    uint8_t                   repeat_count;
    delayed_action_mods_t     mods;
    keypos_t                  token_key_pos;
    keypos_t                  series_key_pos;

    if (!(state && plan && handled_key_resolution_is_handled(resolution) && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    token  = key_runtime_core_press_token_state(state, key_pos);
    series = key_runtime_core_tap_series_state(state, key_pos);
    if (!(token && token->active && token->handled_key)) {
        return false;
    }

    token_key_pos  = key_runtime_core_press_token_resolve_key_pos(state, token);
    series_key_pos = key_runtime_core_tap_series_resolve_key_pos(state, series);

    if (series && series->active && !key_runtime_core_tap_series_can_accept_press(state, series, keycode, state->current_time)) {
        if (series->branch_confirming && series->branch_confirm_kind == KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION) {
            key_runtime_core_plan_same_key_branch_confirm_interruption(state, series, series_key_pos, plan);
        } else if (key_runtime_core_tap_series_take_flush(series, &action, &repeat_count, &mods)) {
            key_runtime_core_effect_plan_push_deferred_delayed_action(plan, series_key_pos, action, mods, repeat_count, key_runtime_core_tap_commit_feedback_allowed(action, series->tap_count));
            key_runtime_core_tap_series_clear(state, series);
        } else {
            key_runtime_core_tap_series_clear(state, series);
        }
    }

    if (key_runtime_core_tap_series_can_accept_press(state, series, keycode, state->current_time)) {
        key_runtime_core_tap_series_update_for_press(state, series, token);
        if (token->interaction.binding.tap_resolves_on_press && !series->pending_hold) {
            key_runtime_core_effect_plan_push_dispatch_action(plan, token_key_pos, token->interaction.binding.tap_action);
            key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(plan, token_key_pos, token->interaction.binding.tap_action, token->interaction.selection.tap_count);
            key_runtime_core_tap_series_clear(state, series);
        }
        if (key_runtime_slot_interaction_is_momentary_layer(token->interaction)) {
            key_runtime_core_effect_plan_push_layer_press(plan, token_key_pos, token->interaction.layer);
        }
        if (series && series->active && series->pending_hold && hold_registers_on_press(token->interaction.binding.hold)) {
            key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, token_key_pos, token->interaction.binding.hold.action);
        }
        token->slot_phase = (series && series->active && series->pending_hold) ? KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW : KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE;
        return true;
    }

    if (key_runtime_slot_interaction_is_momentary_layer(token->interaction)) {
        key_runtime_core_effect_plan_push_layer_press(plan, token_key_pos, token->interaction.layer);
    }

    if (hold_registers_on_press(token->interaction.binding.hold)) {
        key_runtime_core_effect_plan_push_held_action(plan, KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER, token_key_pos, token->interaction.binding.hold.action);
    }

    return true;
}

bool key_runtime_core_handle_handled_key_release(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, keyboard_mod_state_t keyboard_mod_state, key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t                               *state = key_runtime_core_state();
    press_token_t                                          *token;
    tap_series_t                                           *series;
    key_runtime_core_release_effect_plan_t                  release_plan;
    key_runtime_core_active_release_resolution_t            active_resolution;
    key_runtime_core_pending_multi_tap_release_resolution_t pending_resolution;
    delayed_action_mods_t                                   series_mods;

    if (!(state && plan && handled_key_resolution_is_handled(resolution) && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    token       = key_runtime_core_press_token_state(state, key_pos);
    series      = key_runtime_core_tap_series_state(state, key_pos);
    series_mods = series ? series->saved_mod_state : (delayed_action_mods_t){0};
    if (series && series->active && token && token->handled_key && token->interaction.selection.tap_count > 1u && !token->active && token->observed_release_keycode != KC_NO && series->keycode == token->resolved_keycode && key_runtime_core_resolve_pending_multi_tap_release(key_pos, series->tap_action, series->tap_repeat_count, true, &pending_resolution) && key_runtime_core_plan_pending_multi_tap_release_effects(key_pos, key_runtime_slot_interaction_is_momentary_layer(token->interaction), &pending_resolution, series_mods, &release_plan)) {
        key_runtime_core_apply_release_settlement(state, key_pos, &release_plan);
        key_runtime_core_effect_plan_append_release_plan(plan, &release_plan);
        return true;
    }

    if (key_runtime_core_resolve_active_release(key_pos, &active_resolution) && key_runtime_core_plan_active_release_effects(key_pos, keycode, &active_resolution, &release_plan)) {
        key_runtime_core_apply_release_settlement(state, key_pos, &release_plan);
        key_runtime_core_effect_plan_append_release_plan(plan, &release_plan);
        if (release_plan.pending_multi_tap_seed.active) {
            key_runtime_core_tap_series_seed(state, &release_plan.pending_multi_tap_seed, keyboard_mod_state);
        }
        return true;
    }

    handled_key_materialized_t materialized = handled_key_materialize(resolution, handled_key_resolution_ctx_make(key_pos, key_runtime_core_resolution_layers(state)));
    if ((materialized.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0) {
        key_runtime_core_effect_plan_push_layer_release(plan, key_pos);
    }
    key_runtime_core_effect_plan_push_release_owned_state(plan, key_pos);
    return true;
}

void key_runtime_core_scan(key_runtime_core_effect_plan_t *plan, uint16_t now) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && plan)) {
        return;
    }

    key_runtime_core_refresh_for_time(state, now);

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        key_runtime_core_plan_active_scan_for_token(state, &state->press_tokens[index], plan);
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY; index++) {
        tap_series_t *series = &state->tap_series[index];
        keypos_t      series_key_pos;

        if (!series->active) {
            continue;
        }

        series_key_pos = key_runtime_core_tap_series_resolve_key_pos(state, series);
        key_runtime_core_plan_pending_multi_tap_scan_for_key(state, series_key_pos, plan);
    }
}

bool key_runtime_core_settle_pending_fallback_hold(key_runtime_core_effect_plan_t *plan) {
    key_runtime_core_state_t *state       = key_runtime_core_state();
    bool                      settled_any = false;

    if (!(state && plan)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t *token  = &state->press_tokens[index];
        uint8_t        before = plan->count;

        key_runtime_core_plan_fallback_hold_activation(state, token, plan);
        if (plan->count != before) {
            settled_any = true;
        }
    }

    return settled_any;
}

const press_token_t *key_runtime_core_press_token_at(keypos_t key_pos) {
    return key_runtime_core_press_token_state(key_runtime_core_state(), key_pos);
}

const tap_series_t *key_runtime_core_tap_series_at(keypos_t key_pos) {
    return key_runtime_core_tap_series_state(key_runtime_core_state(), key_pos);
}

bool key_runtime_core_press_token_key_pos(const press_token_t *token, keypos_t *out) {
    key_runtime_core_state_t *state   = key_runtime_core_state();
    keypos_t                  key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out && token && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    *out = key_pos;
    return true;
}

bool key_runtime_core_tap_series_key_pos(const tap_series_t *series, keypos_t *out) {
    key_runtime_core_state_t *state   = key_runtime_core_state();
    keypos_t                  key_pos = key_runtime_core_tap_series_resolve_key_pos(state, series);

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out && series && key_runtime_core_keypos_valid(key_pos))) {
        return false;
    }

    *out = key_pos;
    return true;
}

uint8_t key_runtime_core_active_press_token_count(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    return state ? state->press_token_count : 0u;
}

bool key_runtime_core_active_press_token_key_pos(uint8_t order, keypos_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    uint8_t                   seen  = 0u;

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];

        if (!token->active) {
            continue;
        }

        if (seen++ == order) {
            *out = key_runtime_core_press_token_resolve_key_pos(state, token);
            return true;
        }
    }

    return false;
}

uint8_t key_runtime_core_pending_multi_tap_count(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    return state ? state->tap_series_count : 0u;
}

bool key_runtime_core_pending_multi_tap_key_pos(uint8_t order, keypos_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    uint8_t                   seen  = 0u;

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY; index++) {
        const tap_series_t *series = &state->tap_series[index];

        if (!series->active) {
            continue;
        }

        if (seen++ == order) {
            *out = key_runtime_core_tap_series_resolve_key_pos(state, series);
            return true;
        }
    }

    return false;
}

bool key_runtime_core_has_other_active_press_token(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!state) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];
        keypos_t             token_key_pos;

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        if (token->active && !key_runtime_core_keypos_equal(token_key_pos, key_pos)) {
            return true;
        }
    }

    return false;
}

uint16_t key_runtime_core_owner_keycode_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return (token && token->active) ? token->resolved_keycode : KC_NO;
}

uint16_t key_runtime_core_tap_action_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return (token && token->active && token->handled_key) ? token->interaction.binding.tap_action : KC_NO;
}

uint16_t key_runtime_core_held_action_keycode_at(keypos_t key_pos) {
    return key_runtime_core_key_pos_held_action_keycode(key_runtime_core_state(), key_pos);
}

bool key_runtime_core_repeat_active_at(keypos_t key_pos) {
    return key_runtime_core_key_pos_repeat_active(key_runtime_core_state(), key_pos);
}

key_runtime_slot_phase_t key_runtime_core_slot_phase_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return (token && token->active) ? token->slot_phase : KEY_RUNTIME_SLOT_PHASE_IDLE;
}

bool key_runtime_core_momentary_layer_tap_interrupted_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return token ? token->momentary_layer_tap_interrupted : false;
}

uint8_t key_runtime_core_pending_multi_tap_tap_count_at(keypos_t key_pos) {
    const tap_series_t *series = key_runtime_core_tap_series_at(key_pos);
    return (series && series->active) ? series->tap_count : 0u;
}

bool key_runtime_core_pending_multi_tap_holding_at(keypos_t key_pos) {
    const tap_series_t *series = key_runtime_core_tap_series_at(key_pos);
    return series ? series->pending_hold : false;
}

bool key_runtime_core_has_pending_multi_tap_at(keypos_t key_pos) {
    const tap_series_t *series = key_runtime_core_tap_series_at(key_pos);
    return series && series->active;
}

bool key_runtime_core_hold_is_complete_at(keypos_t key_pos) {
    const press_token_t *token = key_runtime_core_press_token_at(key_pos);
    return token && token->active && token->slot_phase == KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE;
}

uint8_t key_runtime_core_deferred_release_blocker_count(void) {
    return key_runtime_core_effective_deferred_release_blocker_count(key_runtime_core_state());
}

uint8_t key_runtime_core_deferred_release_timed_blocker_count(void) {
    return key_runtime_core_timed_deferred_release_blocker_count(key_runtime_core_state());
}

bool key_runtime_core_preview_owner_key_pos(keypos_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];
        keypos_t             token_key_pos;

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        if (!(token->active && token->handled_key) || key_runtime_core_press_token_uses_implicit_hold(token) || key_runtime_core_press_token_uses_fallback_hold(token) || key_runtime_core_key_pos_held_action_keycode(state, token_key_pos) != KC_NO || key_runtime_core_key_pos_repeat_active(state, token_key_pos) || !key_runtime_core_press_token_allows_tap_release(token) || key_runtime_core_press_token_preview_layer_hint(token) == UINT8_MAX) {
            continue;
        }

        *out = token_key_pos;
        return true;
    }

    return false;
}

bool key_runtime_core_pending_fallback_key_pos(keypos_t *out) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (out) {
        *out = (keypos_t){0};
    }

    if (!(state && out)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        const press_token_t *token = &state->press_tokens[index];
        keypos_t             token_key_pos;

        if (!(token->active && token->handled_key) || !key_runtime_core_press_token_uses_fallback_hold(token) || key_runtime_core_press_token_has_runtime_owned_state(state, token) || token->resolved_keycode == KC_NO) {
            continue;
        }

        token_key_pos = key_runtime_core_press_token_resolve_key_pos(state, token);
        *out          = token_key_pos;
        return true;
    }

    return false;
}

const key_runtime_core_shadow_projection_t *key_runtime_core_shadow_projection(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return state ? &state->shadow_projection : NULL;
}

uint8_t key_runtime_core_pending_release_count_for_keypos(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    uint8_t                   count = 0;

    if (!(state && key_runtime_core_keypos_valid(key_pos))) {
        return 0u;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        const pending_release_slot_t *pending = &state->pending_releases[index];

        if (key_runtime_core_pending_release_slot_active(pending) && key_runtime_core_keypos_equal(key_runtime_core_pending_release_slot_key_pos(pending), key_pos)) {
            count++;
        }
    }

    return count;
}

uint8_t key_runtime_core_deferred_release_blocker_count_for_keypos(keypos_t key_pos) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    press_token_t            *token = key_runtime_core_press_token_state(state, key_pos);

    return key_runtime_core_press_token_blocks_deferred_release(state, token) ? 1u : 0u;
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

void key_runtime_core_observe_release_dispatch_deferred(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    (void)key_runtime_core_queue_pending_release_dispatch(key_pos, action, mods, false);
}

void key_runtime_core_observe_release_dispatch_drained(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && action != KC_NO && key_runtime_core_keypos_valid(key_pos))) {
        return;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        pending_release_slot_t pending = state->pending_releases[index];

        if (!key_runtime_core_pending_release_matches(&pending, key_pos, action, mods)) {
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
