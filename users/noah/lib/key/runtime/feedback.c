// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback
// ────────────────────────────────────────────────────────────────────────────

#include "feedback.h"

#include "../interaction/handled_key_policy.h"
#include "core/runtime.h"

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#    define KEY_FEEDBACK_FLASH_HALF_PERIOD_MS RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#else
#    define KEY_FEEDBACK_FLASH_HALF_PERIOD_MS 200
#endif

typedef struct {
    uint8_t flags;
    uint8_t bitmap[KEY_ORIGIN_BITMAP_SIZE];
} key_feedback_snapshot_t;

static uint8_t key_feedback_pack_for_token(const press_token_t *token);

static void key_feedback_snapshot_set_bitmap_for_key(key_feedback_snapshot_t *snapshot, keypos_t key_pos) {
    if (!snapshot) {
        return;
    }

    if (!key_origin_registry_get_bitmap(key_pos, snapshot->bitmap)) {
        key_origin_bitmap_clear(snapshot->bitmap);
    }
}

void key_feedback_pulse_arm(bool long_hold_level) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!state) {
        return;
    }

    state->feedback_pulse_timer           = timer_read();
    state->feedback_pulse_active          = true;
    state->feedback_pulse_long_hold_level = long_hold_level;
}

static bool key_feedback_pulse_active(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && state->feedback_pulse_active)) {
        return false;
    }

    if (timer_elapsed(state->feedback_pulse_timer) < KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) {
        return true;
    }

    state->feedback_pulse_active = false;
    return false;
}

static key_feedback_snapshot_t key_feedback_snapshot(void) {
    key_feedback_snapshot_t     snapshot = {0};
    key_runtime_core_state_t   *state    = key_runtime_core_state();

    if (key_feedback_pulse_active()) {
        snapshot.flags = KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (state) {
            key_feedback_snapshot_set_bitmap_for_key(&snapshot, state->feedback_pulse_key_pos);
        }
        if (state && state->feedback_pulse_long_hold_level) {
            snapshot.flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        }
        return snapshot;
    }

    for (uint16_t index = 0; state && index < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY; index++) {
        if (state->tap_series[index].active && !state->tap_series[index].pending_hold) {
            snapshot.flags |= KEY_FEEDBACK_FLAG_MULTI_TAP_PENDING;
            key_feedback_snapshot_set_bitmap_for_key(&snapshot, state->tap_series[index].key_pos);
            break;
        }
    }

    for (uint16_t index = 0; state && index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        uint8_t token_flags = key_feedback_pack_for_token(&state->press_tokens[index]);

        if (token_flags != 0u) {
            snapshot.flags |= token_flags;
            if (!key_origin_bitmap_has_any(snapshot.bitmap)) {
                key_feedback_snapshot_set_bitmap_for_key(&snapshot, state->press_tokens[index].key_pos);
            }
            return snapshot;
        }
    }

    return snapshot;
}

static bool key_feedback_token_allows_tap_release(const press_token_t *token) {
    return token && token->active && (token->slot_phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW || token->slot_phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW);
}

static bool key_feedback_token_uses_implicit_hold(const press_token_t *token) {
    return token && token->handled_key && key_runtime_slot_interaction_uses_implicit_hold(token->interaction);
}

static bool key_feedback_token_uses_fallback_hold(const press_token_t *token) {
    return token && token->handled_key && key_runtime_slot_interaction_uses_fallback_hold(token->interaction);
}

static handled_key_hold_semantics_t key_feedback_registered_hold_contract(key_runtime_slot_interaction_t interaction, uint16_t held_action, bool long_hold_reached) {
    if (long_hold_reached && interaction.contract.long_hold.threshold_action == held_action) {
        return interaction.contract.long_hold;
    }

    if (interaction.contract.hold.threshold_action == held_action) {
        return interaction.contract.hold;
    }

    return (handled_key_hold_semantics_t){
        .keeps_registered_feedback = handled_key_hold_action_keeps_registered_feedback(noah_action_describe(held_action)),
    };
}

static bool key_feedback_hold_contract_uses_preview_layer(handled_key_hold_semantics_t semantics) {
    return semantics.preview_layer != UINT8_MAX;
}

static uint8_t key_feedback_preview_layer_for_token(const press_token_t *token) {
    if (!(token && token->active && token->handled_key) || key_feedback_token_uses_implicit_hold(token) || key_feedback_token_uses_fallback_hold(token)) {
        return UINT8_MAX;
    }

    if (key_runtime_core_held_action_keycode_at(token->key_pos) != KC_NO || key_runtime_core_slot_phase_at(token->key_pos) == KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE || key_runtime_core_slot_phase_at(token->key_pos) == KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE || !key_feedback_token_allows_tap_release(token)) {
        return UINT8_MAX;
    }

    return token->interaction.contract.hold.preview_layer;
}

uint8_t key_feedback_preview_layer(void) {
    keypos_t key_pos;

    if (!key_runtime_core_preview_owner_key_pos(&key_pos)) {
        return UINT8_MAX;
    }

    return key_feedback_preview_layer_for_token(key_runtime_core_press_token_at(key_pos));
}

static uint8_t key_feedback_pack_for_token(const press_token_t *token) {
    uint8_t  flags = 0u;
    uint16_t held_action;

    if (!(token && token->active && token->handled_key)) {
        return 0u;
    }

    if (key_feedback_token_uses_implicit_hold(token) || key_feedback_token_uses_fallback_hold(token)) {
        return 0u;
    }

    held_action            = key_runtime_core_held_action_keycode_at(token->key_pos);
    bool long_hold_reached = token->interaction.binding.long_hold.present && timer_elapsed(token->pressed_at) >= token->interaction.binding.longer_hold_term;

    if (held_action != KC_NO) {
        handled_key_hold_semantics_t active_contract = key_feedback_registered_hold_contract(token->interaction, held_action, long_hold_reached);

        if (!active_contract.keeps_registered_feedback) {
            return 0u;
        }

        flags |= KEY_FEEDBACK_FLAG_LEVEL_FLASH;
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (((timer_read() / KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) & 1u) == 0u) {
            flags |= KEY_FEEDBACK_FLAG_FLASH_PHASE;
        }
        if (long_hold_reached) {
            flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        }
        return flags;
    }

    if (key_runtime_core_repeat_active_at(token->key_pos)) {
        flags |= KEY_FEEDBACK_FLAG_LEVEL_FLASH;
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (((timer_read() / KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) & 1u) == 0u) {
            flags |= KEY_FEEDBACK_FLAG_FLASH_PHASE;
        }
        if (long_hold_reached) {
            flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        }
        return flags;
    }

    if (long_hold_reached && token->interaction.contract.long_hold.keeps_pending_feedback) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        return flags;
    }

    if (!long_hold_reached && token->slot_phase == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING && !key_feedback_hold_contract_uses_preview_layer(token->interaction.contract.hold)) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_PENDING;
        return flags;
    }

    if (!key_feedback_hold_contract_uses_preview_layer(token->interaction.contract.hold) && key_feedback_token_allows_tap_release(token) && timer_elapsed(token->pressed_at) >= token->interaction.binding.tap_hold_term && (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) || token->interaction.contract.hold.keeps_pending_feedback)) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_PENDING;
    }

    return flags;
}

uint8_t key_feedback_pack(void) {
    return key_feedback_snapshot().flags;
}

void key_feedback_bitmap(uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return;
    }

    key_origin_bitmap_copy(out_bitmap, key_feedback_snapshot().bitmap);
}

#undef KEY_FEEDBACK_FLASH_HALF_PERIOD_MS
