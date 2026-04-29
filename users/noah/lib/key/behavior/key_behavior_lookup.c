// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Lookup
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime lookup helpers for authored key behavior rows.
// ────────────────────────────────────────────────────────────────────────────

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "../../action/action_dispatch.h"
#include "../../pointing/defs/pd_modes.h"
#include "key_behavior_lookup.h"

static const key_behavior_t *key_behavior_config_lookup(uint16_t keycode) {
    for (uint8_t i = 0; i < key_behavior_count; i++) {
        if (key_behaviors[i].keycode == keycode) return &key_behaviors[i];
    }

    return NULL;
}

static key_behavior_step_t key_behavior_step_lookup_in_config(const key_behavior_t *config, uint8_t tap_count) {
    if (!config || tap_count == 0 || tap_count > KEY_BEHAVIOR_MAX_TAP_COUNT) {
        return key_behavior_step_none();
    }

    return config->tap_counts[tap_count - 1];
}

static bool key_behavior_has_more_taps_in_config(const key_behavior_t *config, uint8_t count) {
    if (!config || count >= KEY_BEHAVIOR_MAX_TAP_COUNT) return false;

    for (uint8_t i = count; i < KEY_BEHAVIOR_MAX_TAP_COUNT; i++) {
        if (key_behavior_step_present(config->tap_counts[i])) return true;
    }

    return false;
}

static bool key_behavior_has_multi_tap_in_config(const key_behavior_t *config) {
    return key_behavior_has_more_taps_in_config(config, 1);
}

static bool key_behavior_step_references_foreign_pd_mode(key_behavior_step_t step, pd_mode_mask_t base_mode) {
    pd_mode_mask_t mode;

    if (base_mode == 0) {
        return false;
    }

    if (step.hold.present) {
        mode = pd_mode_for_keycode(step.hold.action);
        if (mode != 0 && mode != base_mode) {
            return true;
        }
    }

    if (step.long_hold.present) {
        mode = pd_mode_for_keycode(step.long_hold.action);
        if (mode != 0 && mode != base_mode) {
            return true;
        }
    }

    return false;
}

static bool key_behavior_future_tap_path_has_foreign_pd_mode_in_config(const key_behavior_t *config, uint8_t count, pd_mode_mask_t base_mode) {
    if (!config || base_mode == 0 || count >= KEY_BEHAVIOR_MAX_TAP_COUNT) {
        return false;
    }

    for (uint8_t index = count; index < KEY_BEHAVIOR_MAX_TAP_COUNT; index++) {
        if (key_behavior_step_references_foreign_pd_mode(config->tap_counts[index], base_mode)) {
            return true;
        }
    }

    return false;
}

static bool key_behavior_keycode_supported(uint16_t keycode) {
    return noah_action_desc_supported_as_behavior_keycode(noah_action_describe(keycode));
}

static bool key_behavior_action_supported(uint16_t action, hold_behavior_mode_t hold_mode) {
    noah_action_desc_t         desc         = noah_action_describe(action);
    noah_action_authored_use_t authored_use = NOAH_ACTION_AUTHORED_USE_TAP;

    if (hold_mode != HOLD_BEHAVIOR_NONE) {
        authored_use = hold_mode == HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE ? NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD : NOAH_ACTION_AUTHORED_USE_HOLD_OTHER;
    }

    return noah_action_desc_supported_as_authored_action(desc, authored_use);
}

static void key_behavior_log_invalid_keycode(uint8_t index, uint16_t keycode) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported key_behaviors[%u].keycode 0x%04X; only MO(...) and LT(...) layer keycodes are supported in the custom runtime\n", (unsigned int)index, (unsigned int)keycode);
#else
    (void)index;
    (void)keycode;
#endif
}

static void key_behavior_log_invalid_action(uint8_t index, uint8_t tap_count, const char *field, uint16_t action, hold_behavior_mode_t hold_mode) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported key_behaviors[%u].tap_counts[%u].%s action 0x%04X; raw QMK layer actions are only supported as PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer))\n", (unsigned int)index, (unsigned int)tap_count, field, (unsigned int)action);
#else
    (void)index;
    (void)tap_count;
    (void)field;
    (void)action;
    (void)hold_mode;
#endif
}

static void key_behavior_log_invalid_repeat_rate(uint8_t index, uint8_t tap_count, const char *field) {
#ifdef CONSOLE_ENABLE
    uprintf("Invalid key_behaviors[%u].tap_counts[%u].%s REPEAT_WHILE_HELD frequency; use a value between 1 and %u Hz\n", (unsigned int)index, (unsigned int)tap_count, field, (unsigned int)KEY_BEHAVIOR_REPEAT_MAX_HZ);
#else
    (void)index;
    (void)tap_count;
    (void)field;
#endif
}

static void key_behavior_log_duplicate_keycode(uint8_t first_index, uint8_t duplicate_index, uint16_t keycode) {
#ifdef CONSOLE_ENABLE
    uprintf("Duplicate key_behaviors rows at indices %u and %u for keycode 0x%04X; later rows are ignored by lookup\n", (unsigned int)first_index, (unsigned int)duplicate_index, (unsigned int)keycode);
#else
    (void)first_index;
    (void)duplicate_index;
    (void)keycode;
#endif
}

static uint8_t key_behavior_error_count_increment(uint8_t error_count) {
    return error_count < UINT8_MAX ? (uint8_t)(error_count + 1u) : UINT8_MAX;
}

static uint8_t key_behavior_validate_unique_keycodes(void) {
    uint8_t error_count = 0u;

    for (uint8_t i = 0; i < key_behavior_count; i++) {
        for (uint8_t j = (uint8_t)(i + 1u); j < key_behavior_count; j++) {
            if (key_behaviors[i].keycode == key_behaviors[j].keycode) {
                key_behavior_log_duplicate_keycode(i, j, key_behaviors[i].keycode);
                error_count = key_behavior_error_count_increment(error_count);
            }
        }
    }

    return error_count;
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    return key_behavior_step_lookup_in_config(key_behavior_config_lookup(keycode), tap_count);
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    return key_behavior_has_more_taps_in_config(key_behavior_config_lookup(keycode), count);
}

bool key_behavior_future_tap_path_has_foreign_pd_mode(uint16_t keycode, uint8_t count, pd_mode_mask_t base_mode) {
    return key_behavior_future_tap_path_has_foreign_pd_mode_in_config(key_behavior_config_lookup(keycode), count, base_mode);
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    const key_behavior_t *config    = key_behavior_config_lookup(keycode);
    noah_action_desc_t    desc      = noah_action_describe(keycode);
    bool                  custom_lt = config && noah_action_desc_uses_authored_layer_tap_contract(desc);

    uint16_t tap_term = CUSTOM_TAP_HOLD_TERM;
    if (config && config->tap_hold_term) {
        tap_term = config->tap_hold_term;
    } else if (custom_lt) {
        tap_term = TAPPING_TERM;
    }

    uint16_t longer_term = config && config->longer_hold_term ? config->longer_hold_term : CUSTOM_LONGER_HOLD_TERM;
    uint16_t multi_term  = config && config->multi_tap_term ? config->multi_tap_term : CUSTOM_MULTI_TAP_TERM;
    uint16_t branch_term = config && config->branch_confirm_term.set ? config->branch_confirm_term.term : CUSTOM_TAP_BRANCH_CONFIRM_TERM;

    return (key_behavior_view_t){
        .config              = config,
        .keycode             = keycode,
        .handled             = config || noah_action_desc_is_runtime_handled_keycode(desc),
        .is_momentary_layer  = noah_action_desc_is_momentary_layer_keycode(desc) || custom_lt,
        .is_layer_tap        = custom_lt,
        .has_multi_tap       = key_behavior_has_multi_tap_in_config(config),
        .tap_hold_term       = tap_term,
        .longer_hold_term    = longer_term,
        .multi_tap_term      = multi_term,
        .branch_confirm_term = branch_term,
        .single              = config ? config->tap_counts[0] : key_behavior_step_none(),
    };
}

uint8_t key_behavior_validate_all(void) {
    uint8_t error_count = key_behavior_validate_unique_keycodes();

    for (uint8_t i = 0; i < key_behavior_count; i++) {
        const key_behavior_t *config = &key_behaviors[i];

        if (!key_behavior_keycode_supported(config->keycode)) {
            key_behavior_log_invalid_keycode(i, config->keycode);
            error_count = key_behavior_error_count_increment(error_count);
        }

        for (uint8_t tap_index = 0; tap_index < KEY_BEHAVIOR_MAX_TAP_COUNT; tap_index++) {
            key_behavior_step_t step = config->tap_counts[tap_index];

            if (step.tap.present && !key_behavior_action_supported(step.tap.action, HOLD_BEHAVIOR_NONE)) {
                key_behavior_log_invalid_action(i, tap_index, "tap", step.tap.action, HOLD_BEHAVIOR_NONE);
                error_count = key_behavior_error_count_increment(error_count);
            }

            if (step.hold.present && !key_behavior_action_supported(step.hold.action, step.hold.mode)) {
                key_behavior_log_invalid_action(i, tap_index, "hold", step.hold.action, step.hold.mode);
                error_count = key_behavior_error_count_increment(error_count);
            }
            if (step.hold.present && step.hold.mode == HOLD_BEHAVIOR_REPEAT_WHILE_HELD && !hold_repeat_rate_valid(step.hold.repeat_hz)) {
                key_behavior_log_invalid_repeat_rate(i, tap_index, "hold");
                error_count = key_behavior_error_count_increment(error_count);
            }

            if (step.long_hold.present && !key_behavior_action_supported(step.long_hold.action, step.long_hold.mode)) {
                key_behavior_log_invalid_action(i, tap_index, "long_hold", step.long_hold.action, step.long_hold.mode);
                error_count = key_behavior_error_count_increment(error_count);
            }
            if (step.long_hold.present && step.long_hold.mode == HOLD_BEHAVIOR_REPEAT_WHILE_HELD && !hold_repeat_rate_valid(step.long_hold.repeat_hz)) {
                key_behavior_log_invalid_repeat_rate(i, tap_index, "long_hold");
                error_count = key_behavior_error_count_increment(error_count);
            }
        }
    }

    return error_count;
}
