// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_internal.h"
#include "key_runtime_feedback.h"
#include "key_runtime_index_internal.h"
#include "../interaction/handled_key_policy.h"
#include "../../pointing/defs/pd_modes.h"

#define key_feedback_pulse (key_runtime_shared_state()->feedback)

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#    define KEY_FEEDBACK_FLASH_HALF_PERIOD_MS RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#else
#    define KEY_FEEDBACK_FLASH_HALF_PERIOD_MS 200
#endif

static bool key_feedback_pulse_active(void) {
    if (!key_feedback_pulse.active) {
        return false;
    }

    if (timer_elapsed(key_feedback_pulse.timer) < KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) {
        return true;
    }

    key_feedback_pulse.active = false;
    return false;
}

void key_feedback_pulse_arm(bool long_hold_level) {
    key_feedback_pulse = (key_runtime_feedback_state_t){
        .timer           = timer_read(),
        .active          = true,
        .long_hold_level = long_hold_level,
    };
}

static uint8_t key_feedback_preview_layer_for_slot(const active_key_state_t *slot) {
    if (!key_runtime_slot_active(slot) || key_runtime_slot_uses_implicit_hold(slot) || key_runtime_slot_uses_fallback_hold(slot)) {
        return UINT8_MAX;
    }

    if (slot->lifecycle.held_action_keycode != KC_NO) {
        // Once a held action is actually registered, the preview window is
        // over. Held momentary layers should render only through layer_state
        // so active MO() and locked layers compose identically in RGB.
        return UINT8_MAX;
    }

    if (!key_runtime_slot_allows_tap_release(slot)) {
        return UINT8_MAX;
    }

    return key_runtime_slot_preview_layer_hint(slot);
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

uint8_t key_feedback_preview_layer(void) {
    return key_feedback_preview_layer_for_slot(key_runtime_preview_owner_slot());
}

static uint8_t key_feedback_pack_for_slot(const active_key_state_t *slot) {
    uint8_t                        flags = 0;
    key_runtime_slot_interaction_t interaction;

    if (!key_runtime_slot_active(slot)) {
        return flags;
    }

    interaction                = key_runtime_slot_cached_interaction(slot);
    uint16_t elapsed           = timer_elapsed(slot->timer);
    bool     long_hold_reached = interaction.binding.long_hold.present && elapsed >= interaction.binding.longer_hold_term;

    if (key_runtime_slot_uses_implicit_hold(slot)) {
        return flags;
    }

    // Fallback base holds are internal runtime glue for "tap override, normal
    // hold" semantics. They are not authored hold surfaces, so keep RGB quiet.
    if (key_runtime_slot_uses_fallback_hold(slot)) {
        return flags;
    }

    if (slot->lifecycle.held_action_keycode != KC_NO) {
        handled_key_hold_semantics_t active_contract = key_feedback_registered_hold_contract(interaction, slot->lifecycle.held_action_keycode, long_hold_reached);

        // Held layer and pd-mode actions do not keep a hold overlay once they
        // are active; the layer or pd-mode color itself is the feedback.
        if (!active_contract.keeps_registered_feedback) {
            return flags;
        }

        // PRESS_AND_HOLD_UNTIL_RELEASE stays visibly active while registered.
        // Pack the current flash phase so both halves flash in lockstep — the
        // slave reads this bit from the sync packet instead of computing phase
        // from its own independent clock.
        flags |= KEY_FEEDBACK_FLAG_LEVEL_FLASH;
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (((timer_read() / KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) & 1u) == 0) {
            flags |= KEY_FEEDBACK_FLAG_FLASH_PHASE;
        }
        if (long_hold_reached) {
            flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        }
        return flags;
    }

    if (slot->lifecycle.repeat_binding_active) {
        flags |= KEY_FEEDBACK_FLAG_LEVEL_FLASH;
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (((timer_read() / KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) & 1u) == 0) {
            flags |= KEY_FEEDBACK_FLAG_FLASH_PHASE;
        }
        if (long_hold_reached) {
            flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        }
        return flags;
    }

    if (long_hold_reached && interaction.contract.long_hold.keeps_pending_feedback) {
        // TAP_ON_RELEASE_AFTER_HOLD keeps feedback visible because the action
        // is still pending until release.
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        return flags;
    }

    if (!long_hold_reached && key_runtime_slot_has_pending_release_hold(slot) && !key_feedback_hold_contract_uses_preview_layer(interaction.contract.hold)) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_PENDING;
        return flags;
    }

    // One-shot threshold actions are complete as soon as they fire, so they do
    // not keep a hold color latched after the threshold. Only an authored
    // normal hold tier keeps the pending hold color before it resolves;
    // long-hold-only surfaces stay quiet until the long-hold tier commits.
    if (!key_feedback_hold_contract_uses_preview_layer(interaction.contract.hold) && key_runtime_slot_allows_tap_release(slot) && elapsed >= interaction.binding.tap_hold_term && (handled_key_hold_contract_fires_at_threshold(interaction.contract.hold) || interaction.contract.hold.keeps_pending_feedback)) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_PENDING;
    }

    return flags;
}

uint8_t key_feedback_pack(void) {
    uint8_t flags = 0;

    if (key_feedback_pulse_active()) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (key_feedback_pulse.long_hold_level) {
            flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        }
        return flags;
    }

    // Multi-tap pending: at least one slot still has an open tap window that
    // has not crossed into a pending hold.
    for (uint8_t index = 0; index < key_runtime_pending_multi_tap_slot_count(); index++) {
        active_key_state_t *slot = key_runtime_pending_multi_tap_slot_by_order(index);
        if (key_runtime_slot_has_pending_multi_tap(slot) && !key_runtime_slot_pending_multi_tap_pending_hold(slot)) {
            flags |= KEY_FEEDBACK_FLAG_MULTI_TAP_PENDING;
            break;
        }
    }

    for (uint8_t index = 0; index < key_runtime_active_slot_count(); index++) {
        uint8_t slot_flags = key_feedback_pack_for_slot(key_runtime_active_slot_by_order(index));
        if (slot_flags != 0) {
            return flags | slot_flags;
        }
    }

    return flags;
}

#undef key_feedback_pulse
#undef KEY_FEEDBACK_FLASH_HALF_PERIOD_MS
