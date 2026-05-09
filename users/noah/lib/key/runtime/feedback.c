// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback
// ────────────────────────────────────────────────────────────────────────────

#include "feedback.h"

#include <string.h>

#include "../behavior/handled_key_policy.h"
#include "../behavior/key_behavior_lookup.h"
#include "../../compat/qmk_combo_origin.h"
#include "../../split/runtime_sync.h"
#include "reducer/ownership_state.h"
#include "reducer/state_query.h"

#ifdef POINTING_DEVICE_ENABLE
#    include "../../pointing/defs/pd_modes.h"
#endif

static key_feedback_semantic_t key_feedback_semantic_for_token(const press_token_t *token);

typedef struct {
    uint8_t  owners[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
    uint32_t sequence[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
    bool     active[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
} key_feedback_broad_owner_builder_t;

static bool key_feedback_sequence_is_newer_or_equal(uint32_t candidate, uint32_t current) {
    return current == 0u || (candidate != 0u && (uint32_t)(candidate - current) < 0x80000000u);
}

static void key_feedback_broad_owner_builder_init(key_feedback_broad_owner_builder_t *builder) {
    if (!builder) {
        return;
    }

    key_feedback_broad_owner_map_clear(builder->owners);
    for (uint8_t index = 0; index < KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE; index++) {
        builder->sequence[index] = 0u;
        builder->active[index]   = false;
    }
}

static void key_feedback_broad_owner_builder_set(key_feedback_broad_owner_builder_t *builder, key_feedback_broad_owner_slot_t slot, keypos_t owner_key_pos, uint32_t sequence) {
    if (!(builder && slot < KEY_FEEDBACK_BROAD_OWNER_COUNT && key_origin_keypos_valid(owner_key_pos))) {
        return;
    }

    if (builder->active[slot] && !key_feedback_sequence_is_newer_or_equal(sequence, builder->sequence[slot])) {
        return;
    }

    builder->active[slot]   = true;
    builder->sequence[slot] = sequence;
    key_feedback_broad_owner_map_set(builder->owners, slot, owner_key_pos);
}

static bool key_feedback_broad_owner_group_slot(key_feedback_semantic_t semantic, key_feedback_broad_owner_slot_t *out_slot) {
    if (!out_slot) {
        return false;
    }

    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_UNRESOLVED_TAP_BRANCH;
            return true;
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_BRANCH_COMMITTED;
            return true;
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_COMMITTED;
            return true;
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_HOLD_ACTIVE;
            return true;
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            *out_slot = KEY_FEEDBACK_BROAD_OWNER_GROUP_LONG_HOLD_ACTIVE;
            return true;
        case KEY_FEEDBACK_SEMANTIC_NONE:
        default:
            return false;
    }
}

static void key_feedback_broad_owner_builder_consider(key_feedback_broad_owner_builder_t *builder, keypos_t owner_key_pos, key_feedback_semantic_t semantic, uint32_t sequence) {
    uint8_t                         bitmap[KEY_ORIGIN_BITMAP_SIZE];
    split_side_mask_t               sides;
    key_feedback_broad_owner_slot_t group_slot;

    if (!(builder && semantic != KEY_FEEDBACK_SEMANTIC_NONE && key_origin_keypos_valid(owner_key_pos))) {
        return;
    }

    if (!key_origin_registry_get_bitmap(owner_key_pos, bitmap)) {
        key_origin_bitmap_fill_single(bitmap, owner_key_pos);
    }

    if (key_origin_bitmap_has_any(bitmap)) {
        key_feedback_broad_owner_builder_set(builder, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, owner_key_pos, sequence);
    }

    sides = key_origin_bitmap_side_mask(bitmap);
    if ((sides & SPLIT_SIDE_MASK_LEFT) != 0u) {
        key_feedback_broad_owner_builder_set(builder, KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF, owner_key_pos, sequence);
    }
    if ((sides & SPLIT_SIDE_MASK_RIGHT) != 0u) {
        key_feedback_broad_owner_builder_set(builder, KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF, owner_key_pos, sequence);
    }

    if (key_feedback_broad_owner_group_slot(semantic, &group_slot)) {
        key_feedback_broad_owner_builder_set(builder, group_slot, owner_key_pos, sequence);
    }
}

static uint8_t key_feedback_semantic_priority(key_feedback_semantic_t semantic) {
    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED:
            return 70u;
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            return 60u;
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
            return 50u;
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
            return 40u;
        case KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH:
            return 10u;
        case KEY_FEEDBACK_SEMANTIC_NONE:
        default:
            return 0u;
    }
}

static key_feedback_semantic_t key_feedback_semantic_max(key_feedback_semantic_t a, key_feedback_semantic_t b) {
    return key_feedback_semantic_priority(a) >= key_feedback_semantic_priority(b) ? a : b;
}

static void key_feedback_apply_semantic_to_bitmap(uint8_t *semantic_map, const uint8_t *bitmap, key_feedback_semantic_t semantic) {
    if (!(semantic_map && bitmap && semantic != KEY_FEEDBACK_SEMANTIC_NONE)) {
        return;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t                key_pos = {.row = row, .col = col};
            key_feedback_semantic_t existing;

            if (!key_origin_bitmap_has_keypos(bitmap, key_pos)) {
                continue;
            }

            existing = key_feedback_semantic_map_get(semantic_map, key_pos);
            key_feedback_semantic_map_set(semantic_map, key_pos, key_feedback_semantic_max(existing, semantic));
        }
    }
}

static void key_feedback_clear_semantic_from_bitmap(uint8_t *semantic_map, const uint8_t *bitmap, key_feedback_semantic_t semantic) {
    if (!(semantic_map && bitmap && semantic != KEY_FEEDBACK_SEMANTIC_NONE)) {
        return;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};

            if (!key_origin_bitmap_has_keypos(bitmap, key_pos)) {
                continue;
            }

            if (key_feedback_semantic_map_get(semantic_map, key_pos) == semantic) {
                key_feedback_semantic_map_set(semantic_map, key_pos, KEY_FEEDBACK_SEMANTIC_NONE);
            }
        }
    }
}

static void key_feedback_apply_semantic_for_owner(uint8_t *semantic_map, keypos_t owner_key_pos, key_feedback_semantic_t semantic) {
    uint8_t bitmap[KEY_ORIGIN_BITMAP_SIZE];

    if (!(semantic_map && semantic != KEY_FEEDBACK_SEMANTIC_NONE && key_origin_keypos_valid(owner_key_pos))) {
        return;
    }

    if (!key_origin_registry_get_bitmap(owner_key_pos, bitmap)) {
        return;
    }

    key_feedback_apply_semantic_to_bitmap(semantic_map, bitmap, semantic);
}

static void key_feedback_apply_tap_branch_to_bitmap(uint8_t *tap_branch_map, const uint8_t *bitmap, uint8_t tap_branch) {
    if (!(tap_branch_map && bitmap && tap_branch != 0u)) {
        return;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};
            uint8_t  existing;

            if (!key_origin_bitmap_has_keypos(bitmap, key_pos)) {
                continue;
            }

            existing = key_feedback_tap_branch_map_get(tap_branch_map, key_pos);
            key_feedback_tap_branch_map_set(tap_branch_map, key_pos, existing >= tap_branch ? existing : tap_branch);
        }
    }
}

static void key_feedback_apply_tap_branch_for_owner(uint8_t *tap_branch_map, keypos_t owner_key_pos, uint8_t tap_branch) {
    uint8_t bitmap[KEY_ORIGIN_BITMAP_SIZE];

    if (!(tap_branch_map && tap_branch != 0u && key_origin_keypos_valid(owner_key_pos))) {
        return;
    }

    if (!key_origin_registry_get_bitmap(owner_key_pos, bitmap)) {
        return;
    }

    key_feedback_apply_tap_branch_to_bitmap(tap_branch_map, bitmap, tap_branch);
}

static bool key_feedback_tap_branch_is_higher_tier(uint8_t tap_count) {
    return tap_count > 1u;
}

static bool key_feedback_branch_confirm_mode_allows(uint8_t tap_count) {
    switch (key_feedback_branch_confirm_mode()) {
        case KEY_FEEDBACK_BRANCH_CONFIRM_OFF:
            return false;
        case KEY_FEEDBACK_BRANCH_CONFIRM_NON_BASE_TAPS:
            return key_feedback_tap_branch_is_higher_tier(tap_count);
        default:
            return false;
    }
}

static bool key_feedback_tap_series_shows_pending_feedback(const tap_series_t *series) {
    return series && series->active && !series->branch_confirmed && !series->branch_confirming && key_feedback_tap_branch_is_higher_tier(series->tap_count);
}

static bool key_feedback_tap_series_shows_branch_confirmation(const tap_series_t *series) {
    return series && series->active && series->branch_confirming && key_feedback_branch_confirm_mode_allows(series->branch_confirm_tap_count);
}

static key_feedback_semantic_t key_feedback_semantic_for_pulse(key_feedback_pulse_kind_t kind) {
    switch (kind) {
        case KEY_FEEDBACK_PULSE_TAP_COMMITTED:
            return KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED;
        case KEY_FEEDBACK_PULSE_LONG_HOLD:
            return KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY;
        case KEY_FEEDBACK_PULSE_HOLD:
        default:
            return KEY_FEEDBACK_SEMANTIC_HOLD_PENDING;
    }
}

void key_feedback_pulse_arm(key_feedback_pulse_kind_t kind) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!state) {
        return;
    }

    state->feedback_pulse_timer           = timer_read();
    state->feedback_pulse_sequence        = key_runtime_core_state_next_feedback_sequence(state);
    state->feedback_pulse_active          = true;
    state->feedback_pulse_kind            = kind;
    state->feedback_pulse_tap_branch      = 0u;
    state->feedback_pulse_queued          = false;
    state->feedback_pulse_queued_sequence = 0u;
    split_runtime_sync_notify_key_feedback_dirty();
}

static bool key_feedback_pulse_active(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && state->feedback_pulse_active)) {
        return false;
    }

    if (timer_elapsed(state->feedback_pulse_timer) < KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) {
        return true;
    }

    if (state->feedback_pulse_queued) {
        state->feedback_pulse_timer           = timer_read();
        state->feedback_pulse_sequence        = state->feedback_pulse_queued_sequence != 0u ? state->feedback_pulse_queued_sequence : key_runtime_core_state_next_feedback_sequence(state);
        state->feedback_pulse_active          = true;
        state->feedback_pulse_kind            = state->feedback_pulse_queued_kind;
        state->feedback_pulse_key_pos         = state->feedback_pulse_queued_key_pos;
        state->feedback_pulse_tap_branch      = state->feedback_pulse_queued_tap_branch;
        state->feedback_pulse_queued          = false;
        state->feedback_pulse_queued_sequence = 0u;
        return true;
    }

    state->feedback_pulse_active = false;
    return false;
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
    keypos_t key_pos;

    if (!(token && token->active && token->handled_key) || key_feedback_token_uses_implicit_hold(token) || key_feedback_token_uses_fallback_hold(token)) {
        return UINT8_MAX;
    }

    if (!key_runtime_core_press_token_key_pos(token, &key_pos)) {
        return UINT8_MAX;
    }

    if (key_runtime_core_held_action_keycode_at(key_pos) != KC_NO || key_runtime_core_slot_phase_at(key_pos) == KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE || key_runtime_core_slot_phase_at(key_pos) == KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE || !key_feedback_token_allows_tap_release(token)) {
        return UINT8_MAX;
    }

    return token->interaction.contract.hold.preview_layer;
}

static uint8_t key_feedback_semantic_preview_layer(void) {
    keypos_t key_pos;

    if (!key_runtime_core_preview_owner_key_pos(&key_pos)) {
        return UINT8_MAX;
    }

    return key_feedback_preview_layer_for_token(key_runtime_core_press_token_at(key_pos));
}

uint8_t key_feedback_preview_layer(void) {
    key_runtime_core_state_t *state          = key_runtime_core_state();
    uint8_t                   semantic_layer = key_feedback_semantic_preview_layer();

    if (!state) {
        return semantic_layer;
    }

    if (semantic_layer < LAYER_COUNT) {
        state->preview_display_last_semantic_layer = semantic_layer;
        state->preview_display_bridge_active       = false;
        state->preview_display_bridge_layer        = UINT8_MAX;
        return semantic_layer;
    }

    if (!state->preview_display_bridge_active && state->preview_display_last_semantic_layer < LAYER_COUNT && layer_state_cmp(layer_state, state->preview_display_last_semantic_layer)) {
        state->preview_display_bridge_active     = true;
        state->preview_display_bridge_layer      = state->preview_display_last_semantic_layer;
        state->preview_display_bridge_started_at = timer_read();
    }

    state->preview_display_last_semantic_layer = UINT8_MAX;

    if (state->preview_display_bridge_active) {
        if (state->preview_display_bridge_layer < LAYER_COUNT && layer_state_cmp(layer_state, state->preview_display_bridge_layer) && timer_elapsed(state->preview_display_bridge_started_at) < KEY_FEEDBACK_PREVIEW_DISPLAY_BRIDGE_MS) {
            return state->preview_display_bridge_layer;
        }

        state->preview_display_bridge_active = false;
        state->preview_display_bridge_layer  = UINT8_MAX;
    }

    return UINT8_MAX;
}

static key_feedback_semantic_t key_feedback_semantic_for_token(const press_token_t *token) {
    uint16_t held_action;
    keypos_t key_pos;

    if (!(token && token->active && token->handled_key)) {
        return KEY_FEEDBACK_SEMANTIC_NONE;
    }

    if (key_feedback_token_uses_implicit_hold(token) || key_feedback_token_uses_fallback_hold(token)) {
        return KEY_FEEDBACK_SEMANTIC_NONE;
    }

    if (!key_runtime_core_press_token_key_pos(token, &key_pos)) {
        return KEY_FEEDBACK_SEMANTIC_NONE;
    }

    held_action            = key_runtime_core_held_action_keycode_at(key_pos);
    bool long_hold_reached = token->interaction.binding.long_hold.present && timer_elapsed(token->pressed_at) >= token->interaction.binding.longer_hold_term;

    if (held_action != KC_NO) {
        handled_key_hold_semantics_t active_contract = key_feedback_registered_hold_contract(token->interaction, held_action, long_hold_reached);

        if (!active_contract.keeps_registered_feedback) {
            return KEY_FEEDBACK_SEMANTIC_NONE;
        }

        return long_hold_reached ? KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING : KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING;
    }

    if (key_runtime_core_repeat_active_at(key_pos)) {
        return long_hold_reached ? KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING : KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING;
    }

    if (long_hold_reached && token->interaction.contract.long_hold.keeps_pending_feedback) {
        return KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY;
    }

    if (!long_hold_reached && token->slot_phase == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING && !key_feedback_hold_contract_uses_preview_layer(token->interaction.contract.hold)) {
        return KEY_FEEDBACK_SEMANTIC_HOLD_PENDING;
    }

    if (!key_feedback_hold_contract_uses_preview_layer(token->interaction.contract.hold) && key_feedback_token_allows_tap_release(token) && timer_elapsed(token->pressed_at) >= token->interaction.binding.tap_hold_term && (handled_key_hold_contract_fires_at_threshold(token->interaction.contract.hold) || token->interaction.contract.hold.keeps_pending_feedback)) {
        return KEY_FEEDBACK_SEMANTIC_HOLD_PENDING;
    }

    return KEY_FEEDBACK_SEMANTIC_NONE;
}

void key_feedback_flash_visibility_bitmap_for_semantic_map(const uint8_t *semantic_map, uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (!semantic_map) {
        return;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t                key_pos  = {.row = row, .col = col};
            key_feedback_semantic_t semantic = key_feedback_semantic_map_get(semantic_map, key_pos);

            if (key_feedback_semantic_is_flashing(semantic) && key_runtime_core_flashing_feedback_visible_at(key_pos)) {
                key_origin_bitmap_add_keypos(out_bitmap, key_pos);
            }
        }
    }
}

void key_feedback_flash_visibility_bitmap(uint8_t *out_bitmap) {
    uint8_t semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];

    if (!out_bitmap) {
        return;
    }

    key_feedback_semantic_map(semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(semantic_map, out_bitmap);
}

static uint32_t key_feedback_sequence_for_token_semantic(const press_token_t *token, keypos_t key_pos, key_feedback_semantic_t semantic) {
    uint32_t sequence;

    if (!token) {
        return 0u;
    }

    if (key_feedback_semantic_is_flashing(semantic) && key_runtime_core_flashing_feedback_sequence_at(key_pos, &sequence)) {
        return sequence;
    }

    return token->feedback_sequence;
}

void key_feedback_broad_owner_map(uint8_t *out_map) {
    key_runtime_core_state_t          *state = key_runtime_core_state();
    key_feedback_broad_owner_builder_t builder;

    if (!out_map) {
        return;
    }

    key_feedback_broad_owner_builder_init(&builder);

    if (key_feedback_pulse_active() && state && key_origin_keypos_valid(state->feedback_pulse_key_pos)) {
        key_feedback_broad_owner_builder_consider(&builder, state->feedback_pulse_key_pos, key_feedback_semantic_for_pulse(state->feedback_pulse_kind), state->feedback_pulse_sequence);
    }

    for (uint16_t index = 0; state && index < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY; index++) {
        keypos_t key_pos;

        if (key_feedback_tap_series_shows_pending_feedback(&state->tap_series[index]) && key_runtime_core_tap_series_key_pos(&state->tap_series[index], &key_pos)) {
            key_feedback_broad_owner_builder_consider(&builder, key_pos, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH, state->tap_series[index].feedback_sequence);
        }

        if (key_feedback_tap_series_shows_branch_confirmation(&state->tap_series[index]) && key_runtime_core_tap_series_key_pos(&state->tap_series[index], &key_pos)) {
            key_feedback_broad_owner_builder_consider(&builder, key_pos, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED, state->tap_series[index].feedback_sequence);
        }
    }

    for (uint16_t index = 0; state && index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        press_token_t          *token    = &state->press_tokens[index];
        key_feedback_semantic_t semantic = key_feedback_semantic_for_token(token);
        keypos_t                key_pos;

        if (semantic == KEY_FEEDBACK_SEMANTIC_NONE || !key_runtime_core_press_token_key_pos(token, &key_pos)) {
            continue;
        }

        key_feedback_broad_owner_builder_consider(&builder, key_pos, semantic, key_feedback_sequence_for_token_semantic(token, key_pos, semantic));
    }

    memcpy(out_map, builder.owners, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE);
}

void key_feedback_semantic_map(uint8_t *out_map) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!out_map) {
        return;
    }

    key_feedback_semantic_map_clear(out_map);

    if (key_feedback_pulse_active() && state && key_origin_keypos_valid(state->feedback_pulse_key_pos)) {
        key_feedback_apply_semantic_for_owner(out_map, state->feedback_pulse_key_pos, key_feedback_semantic_for_pulse(state->feedback_pulse_kind));
    }

    for (uint16_t index = 0; state && index < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY; index++) {
        keypos_t key_pos;

        if (key_feedback_tap_series_shows_pending_feedback(&state->tap_series[index]) && key_runtime_core_tap_series_key_pos(&state->tap_series[index], &key_pos)) {
            key_feedback_apply_semantic_for_owner(out_map, key_pos, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
        }

        if (key_feedback_tap_series_shows_branch_confirmation(&state->tap_series[index]) && key_runtime_core_tap_series_key_pos(&state->tap_series[index], &key_pos)) {
            key_feedback_apply_semantic_for_owner(out_map, key_pos, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
        }
    }

    for (uint16_t index = 0; state && index < KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY; index++) {
        key_feedback_semantic_t semantic = key_feedback_semantic_for_token(&state->press_tokens[index]);
        keypos_t                key_pos;

        if (semantic == KEY_FEEDBACK_SEMANTIC_NONE || !key_runtime_core_press_token_key_pos(&state->press_tokens[index], &key_pos)) {
            continue;
        }

        key_feedback_apply_semantic_for_owner(out_map, key_pos, semantic);
    }

    if (state) {
        uint8_t pressed_combo_bitmap[KEY_ORIGIN_BITMAP_SIZE];

        noah_qmk_combo_origin_pressed_combo_bitmap(pressed_combo_bitmap);
        key_feedback_clear_semantic_from_bitmap(out_map, pressed_combo_bitmap, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    }
}

void key_feedback_tap_branch_map(uint8_t *out_map) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!out_map) {
        return;
    }

    key_feedback_tap_branch_map_clear(out_map);

    for (uint16_t index = 0; state && index < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY; index++) {
        keypos_t key_pos;

        if (key_feedback_tap_series_shows_branch_confirmation(&state->tap_series[index]) && key_runtime_core_tap_series_key_pos(&state->tap_series[index], &key_pos)) {
            key_feedback_apply_tap_branch_for_owner(out_map, key_pos, state->tap_series[index].branch_confirm_tap_count);
        }
    }
}

void combo_feedback_bitmaps(uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap) {
    keypos_t preview_owner_key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS};
    keypos_t pd_owner_key_pos      = {.row = MATRIX_ROWS, .col = MATRIX_COLS};

    if (out_underlay_bitmap) {
        key_origin_bitmap_clear(out_underlay_bitmap);
    }

    if (out_overlay_bitmap) {
        key_origin_bitmap_clear(out_overlay_bitmap);
    }

    if (!(out_underlay_bitmap && out_overlay_bitmap)) {
        return;
    }

    (void)key_runtime_core_preview_owner_key_pos(&preview_owner_key_pos);
#ifdef POINTING_DEVICE_ENABLE
    (void)pd_mode_local_owner_key_pos_snapshot(&pd_owner_key_pos);
#endif
    noah_qmk_combo_origin_active_bitmaps_partitioned(preview_owner_key_pos, pd_owner_key_pos, out_underlay_bitmap, out_overlay_bitmap);
}

void combo_feedback_underlay_bitmap(uint8_t *out_bitmap) {
    uint8_t unused_overlay[KEY_ORIGIN_BITMAP_SIZE];

    combo_feedback_bitmaps(out_bitmap, unused_overlay);
}

void combo_feedback_overlay_bitmap(uint8_t *out_bitmap) {
    uint8_t  unused_underlay[KEY_ORIGIN_BITMAP_SIZE];

    combo_feedback_bitmaps(unused_underlay, out_bitmap);
}
