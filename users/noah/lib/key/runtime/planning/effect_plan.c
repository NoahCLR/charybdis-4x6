// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Effect Plan Construction
// ────────────────────────────────────────────────────────────────────────────

#include "effect_plan.h"
#include "release_internal.h"

#include "../../../action/action_dispatch.h"
#include "../../../pointing/defs/pd_modes.h"

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

void key_runtime_core_effect_plan_push_dispatch_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action) {
    if (!(plan && action != KC_NO)) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind                 = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
                                                .data.dispatch_action = {.action = action, .packed_key_pos = key_runtime_keypos_pack(key_pos)},
                                            });
}

void key_runtime_core_effect_plan_push_held_action(key_runtime_core_effect_plan_t *plan, key_runtime_effect_kind_t kind, keypos_t key_pos, uint16_t action) {
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

void key_runtime_core_effect_plan_push_release_owned_state(key_runtime_core_effect_plan_t *plan, keypos_t key_pos) {
    if (!plan) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                                .data.key_pos = key_pos,
                                            });
}

void key_runtime_core_effect_plan_push_repeat_start(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
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

void key_runtime_core_effect_plan_push_layer_press(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint8_t layer) {
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

void key_runtime_core_effect_plan_push_layer_release(key_runtime_core_effect_plan_t *plan, keypos_t key_pos) {
    if (!plan) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind         = KEY_RUNTIME_EFFECT_LAYER_RELEASE,
                                                .data.key_pos = key_pos,
                                            });
}

void key_runtime_core_effect_plan_push_pd_mode_lock_state(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, pd_mode_mask_t mode, bool locked) {
    if (!(plan && mode != 0)) {
        return;
    }

    key_runtime_core_effect_plan_push(plan, (key_runtime_effect_t){
                                                .kind = KEY_RUNTIME_EFFECT_PD_MODE_LOCK_STATE,
                                                .data.pd_mode_lock_state =
                                                    {
                                                        .pd_mode = mode,
                                                        .key_pos = key_pos,
                                                        .locked  = locked,
                                                    },
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

void key_runtime_core_effect_plan_push_deferred_delayed_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count, bool tap_commit_feedback) {
    uint8_t flags = KEY_RUNTIME_DELAYED_ACTION_FLAG_DEFER_UNTIL_RELEASE;

    if (tap_commit_feedback) {
        flags |= KEY_RUNTIME_DELAYED_ACTION_FLAG_TAP_COMMIT_FEEDBACK;
    }

    key_runtime_core_effect_plan_push_delayed_action_with_flags(plan, key_pos, action, mods, repeat_count, flags);
}

void key_runtime_core_effect_plan_append_release_plan(key_runtime_core_effect_plan_t *plan, const key_runtime_core_release_effect_plan_t *release_plan) {
    if (!(plan && release_plan)) {
        return;
    }

    for (uint8_t index = 0; index < release_plan->count; ++index) {
        key_runtime_core_effect_plan_push(plan, release_plan->items[index]);
    }
}
