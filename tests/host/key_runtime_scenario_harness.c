#include "key_runtime_scenario_harness.h"

#include <string.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/key/interaction/handled_key.h"
#include "users/noah/lib/key/ownership/held_action.h"
#include "users/noah/lib/key/runtime/api.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/pointing/policy/pointer_layer_policy.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"
#include "users/noah/lib/state/runtime/runtime_reset.h"
#include "users/noah/lib/key/runtime/core/ownership_state.h"
#include "users/noah/lib/key/runtime/core/runtime.h"
#include "users/noah/noah_runtime.h"

enum {
    KEY_RUNTIME_SCENARIO_MAX_BEHAVIORS = 16,
    KEY_RUNTIME_SCENARIO_MAX_STEPS     = 16,
    KEY_RUNTIME_SCENARIO_MAX_PD_MODES  = 8,
    KEY_RUNTIME_SCENARIO_MAX_EFFECTS   = 32,
};

typedef struct {
    uint16_t            keycode;
    key_behavior_view_t behavior;
} key_runtime_scenario_behavior_entry_t;

typedef struct {
    uint16_t            keycode;
    uint8_t             tap_count;
    key_behavior_step_t step;
} key_runtime_scenario_step_entry_t;

typedef struct {
    uint16_t       keycode;
    pd_mode_mask_t mode;
} key_runtime_scenario_pd_mode_entry_t;

static uint16_t                              key_runtime_scenario_time;
static bool                                  key_runtime_scenario_hold_survives_flush;
static key_runtime_scenario_behavior_entry_t key_runtime_scenario_behaviors[KEY_RUNTIME_SCENARIO_MAX_BEHAVIORS];
static uint8_t                               key_runtime_scenario_behavior_count;
static key_runtime_scenario_step_entry_t     key_runtime_scenario_steps[KEY_RUNTIME_SCENARIO_MAX_STEPS];
static uint8_t                               key_runtime_scenario_step_entry_count;
static key_runtime_scenario_pd_mode_entry_t  key_runtime_scenario_pd_modes[KEY_RUNTIME_SCENARIO_MAX_PD_MODES];
static uint8_t                               key_runtime_scenario_pd_mode_count;
static layer_state_t                         key_runtime_scenario_locked_layers;
static pd_mode_mask_t                        key_runtime_scenario_pd_locked_modes;
static key_feedback_tap_commit_mode_t        key_runtime_scenario_tap_commit_mode;
static key_runtime_scenario_effect_t         key_runtime_scenario_effects[KEY_RUNTIME_SCENARIO_MAX_EFFECTS];
static uint8_t                               key_runtime_scenario_effect_count_value;
static uint8_t                               key_runtime_scenario_split_sync_count_value;

layer_state_t layer_state;

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    (void)layer_num;
    (void)row;
    (void)column;
    return KC_TRNS;
}

static void key_runtime_scenario_log_effect(key_runtime_scenario_effect_t effect) {
    if (key_runtime_scenario_effect_count_value >= ARRAY_SIZE(key_runtime_scenario_effects)) {
        return;
    }

    key_runtime_scenario_effects[key_runtime_scenario_effect_count_value++] = effect;
}

static keyrecord_t key_runtime_scenario_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .type    = KEY_EVENT,
                .key     = key_pos,
                .pressed = pressed,
            },
    };
}

static bool key_runtime_scenario_process_record(uint16_t keycode, keyrecord_t *record) {
    bool keep_processing;

    if (!noah_pre_process_record_user(keycode, record)) {
        return false;
    }

    keep_processing = noah_process_record_user(keycode, record);
    if (!keep_processing) {
        noah_process_record_user_finalize(keycode, record, false);
        return false;
    }

    noah_post_process_record_user(keycode, record);
    return true;
}

static key_behavior_view_t key_runtime_scenario_default_behavior(uint16_t keycode) {
    return (key_behavior_view_t){
        .keycode            = keycode,
        .handled            = IS_QK_MOMENTARY(keycode),
        .is_momentary_layer = IS_QK_MOMENTARY(keycode),
        .is_layer_tap       = false,
        .has_multi_tap      = false,
        .tap_hold_term      = CUSTOM_TAP_HOLD_TERM,
        .longer_hold_term   = CUSTOM_LONGER_HOLD_TERM,
        .multi_tap_term     = CUSTOM_MULTI_TAP_TERM,
        .branch_confirm_term = CUSTOM_TAP_BRANCH_CONFIRM_TERM,
    };
}

key_behavior_view_t key_runtime_scenario_pressable_handled_key(uint16_t keycode) {
    key_behavior_view_t behavior = key_runtime_scenario_default_behavior(keycode);

    behavior.handled = true;
    return behavior;
}

void key_runtime_scenario_reset(void) {
    key_runtime_scenario_time                   = 1000;
    key_runtime_scenario_hold_survives_flush    = false;
    key_runtime_scenario_behavior_count         = 0;
    key_runtime_scenario_step_entry_count       = 0;
    key_runtime_scenario_pd_mode_count          = 0;
    key_runtime_scenario_locked_layers          = 0;
    key_runtime_scenario_pd_locked_modes        = 0;
    key_runtime_scenario_tap_commit_mode        = KEY_FEEDBACK_TAP_COMMIT_ALL_TAPS;
    key_runtime_scenario_effect_count_value     = 0;
    key_runtime_scenario_split_sync_count_value = 0;

    memset(key_runtime_scenario_behaviors, 0, sizeof(key_runtime_scenario_behaviors));
    memset(key_runtime_scenario_steps, 0, sizeof(key_runtime_scenario_steps));
    memset(key_runtime_scenario_pd_modes, 0, sizeof(key_runtime_scenario_pd_modes));
    memset(key_runtime_scenario_effects, 0, sizeof(key_runtime_scenario_effects));

    noah_runtime_reset_for_test();
}

void key_runtime_scenario_clear_effects(void) {
    key_runtime_scenario_effect_count_value     = 0;
    key_runtime_scenario_split_sync_count_value = 0;
    memset(key_runtime_scenario_effects, 0, sizeof(key_runtime_scenario_effects));
}

void key_runtime_scenario_add_behavior_view(key_behavior_view_t behavior) {
    if (key_runtime_scenario_behavior_count >= ARRAY_SIZE(key_runtime_scenario_behaviors)) {
        return;
    }

    if (behavior.tap_hold_term == 0) {
        behavior.tap_hold_term = CUSTOM_TAP_HOLD_TERM;
    }
    if (behavior.longer_hold_term == 0) {
        behavior.longer_hold_term = CUSTOM_LONGER_HOLD_TERM;
    }
    if (behavior.multi_tap_term == 0) {
        behavior.multi_tap_term = CUSTOM_MULTI_TAP_TERM;
    }
    key_runtime_scenario_behaviors[key_runtime_scenario_behavior_count++] = (key_runtime_scenario_behavior_entry_t){
        .keycode  = behavior.keycode,
        .behavior = behavior,
    };
}

void key_runtime_scenario_add_behavior_step(uint16_t keycode, uint8_t tap_count, key_behavior_step_t step) {
    if (key_runtime_scenario_step_entry_count >= ARRAY_SIZE(key_runtime_scenario_steps)) {
        return;
    }

    key_runtime_scenario_steps[key_runtime_scenario_step_entry_count++] = (key_runtime_scenario_step_entry_t){
        .keycode   = keycode,
        .tap_count = tap_count,
        .step      = step,
    };
}

void key_runtime_scenario_add_pending_multi_tap_behavior(key_behavior_view_t behavior, const key_runtime_scenario_multi_tap_entry_t *entries, uint8_t entry_count) {
    key_runtime_scenario_add_behavior_view(behavior);

    for (uint8_t index = 0; index < entry_count; index++) {
        key_runtime_scenario_add_behavior_step(behavior.keycode, entries[index].tap_count, entries[index].step);
    }
}

void key_runtime_scenario_add_pending_multi_tap_chain(uint16_t keycode, key_behavior_step_t single, const key_runtime_scenario_multi_tap_entry_t *entries, uint8_t entry_count) {
    key_behavior_view_t behavior = key_runtime_scenario_pressable_handled_key(keycode);

    behavior.has_multi_tap = true;
    behavior.single        = single;
    key_runtime_scenario_add_pending_multi_tap_behavior(behavior, entries, entry_count);
}

void key_runtime_scenario_add_pd_mode(uint16_t keycode, pd_mode_mask_t mode) {
    if (key_runtime_scenario_pd_mode_count >= ARRAY_SIZE(key_runtime_scenario_pd_modes)) {
        return;
    }

    key_runtime_scenario_pd_modes[key_runtime_scenario_pd_mode_count++] = (key_runtime_scenario_pd_mode_entry_t){
        .keycode = keycode,
        .mode    = mode,
    };
}

void key_runtime_scenario_define_pd_mode_key(uint16_t keycode, pd_mode_mask_t mode, bool locked) {
    key_runtime_scenario_add_pd_mode(keycode, mode);
    if (locked) {
        key_runtime_scenario_pd_locked_modes |= mode;
        key_runtime_core_observe_pd_mode_lock_state(mode, true);
    }
}

void key_runtime_scenario_set_pd_locked_modes(pd_mode_mask_t modes) {
    pd_mode_mask_t changed = key_runtime_scenario_pd_locked_modes ^ modes;

    for (pd_mode_mask_t bit = 1u; changed != 0u; bit <<= 1u) {
        if ((changed & bit) == 0u) {
            continue;
        }

        key_runtime_core_observe_pd_mode_lock_state(bit, (modes & bit) != 0u);
        changed &= (pd_mode_mask_t)~bit;
    }

    key_runtime_scenario_pd_locked_modes = modes;
}

void key_runtime_scenario_set_hold_survives_flush(bool survives_flush) {
    key_runtime_scenario_hold_survives_flush = survives_flush;
}

void key_runtime_scenario_set_tap_commit_mode(key_feedback_tap_commit_mode_t mode) {
    key_runtime_scenario_tap_commit_mode = mode;
}

key_feedback_tap_commit_mode_t key_feedback_tap_commit_mode(void) {
    return key_runtime_scenario_tap_commit_mode;
}

void key_runtime_scenario_run(const key_runtime_scenario_step_t *steps, uint8_t step_count) {
    for (uint8_t index = 0; index < step_count; index++) {
        switch (steps[index].kind) {
            case KEY_RUNTIME_SCENARIO_STEP_PRESS: {
                keyrecord_t record = key_runtime_scenario_record(steps[index].data.key_event.key_pos, true);
                (void)key_runtime_scenario_process_record(steps[index].data.key_event.keycode, &record);
                break;
            }
            case KEY_RUNTIME_SCENARIO_STEP_RELEASE: {
                keyrecord_t record = key_runtime_scenario_record(steps[index].data.key_event.key_pos, false);
                (void)key_runtime_scenario_process_record(steps[index].data.key_event.keycode, &record);
                break;
            }
            case KEY_RUNTIME_SCENARIO_STEP_ADVANCE_MS:
                key_runtime_scenario_time = (uint16_t)(key_runtime_scenario_time + steps[index].data.advance_ms);
                break;
            case KEY_RUNTIME_SCENARIO_STEP_SCAN:
                noah_key_runtime_scan();
                break;
        }
    }
}

bool key_runtime_scenario_layer_locked(uint8_t layer) {
    return layer < LAYER_COUNT && (key_runtime_scenario_locked_layers & ((layer_state_t)1u << layer)) != 0;
}

uint16_t key_runtime_scenario_slot_owner_keycode(keypos_t key_pos) {
    return noah_runtime_debug_slot_owner_keycode(key_pos);
}

uint16_t key_runtime_scenario_slot_held_action_keycode(keypos_t key_pos) {
    return noah_runtime_debug_slot_held_action_keycode(key_pos);
}

bool key_runtime_scenario_slot_has_pending_multi_tap(keypos_t key_pos) {
    return noah_runtime_debug_slot_has_pending_multi_tap(key_pos);
}

bool key_runtime_scenario_slot_hold_is_complete(keypos_t key_pos) {
    return noah_runtime_debug_slot_hold_is_complete(key_pos);
}

uint8_t key_runtime_scenario_effect_count(void) {
    return key_runtime_scenario_effect_count_value;
}

const key_runtime_scenario_effect_t *key_runtime_scenario_effect_at(uint8_t index) {
    if (index >= key_runtime_scenario_effect_count_value) {
        return NULL;
    }

    return &key_runtime_scenario_effects[index];
}

uint16_t key_runtime_scenario_now(void) {
    return key_runtime_scenario_time;
}

uint8_t key_runtime_scenario_split_sync_count(void) {
    return key_runtime_scenario_split_sync_count_value;
}

uint16_t timer_read(void) {
    return key_runtime_scenario_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(key_runtime_scenario_time - last);
}

uint32_t timer_read32(void) {
    return key_runtime_scenario_time;
}

uint32_t timer_elapsed32(uint32_t last) {
    return (uint32_t)(key_runtime_scenario_time - last);
}

uint8_t get_mods(void) {
    return 0;
}

uint8_t get_weak_mods(void) {
    return 0;
}

uint8_t get_oneshot_mods(void) {
    return 0;
}

uint8_t get_oneshot_locked_mods(void) {
    return 0;
}

void set_mods(uint8_t mods) {
    (void)mods;
}

void set_weak_mods(uint8_t mods) {
    (void)mods;
}

void set_oneshot_mods(uint8_t mods) {
    (void)mods;
}

void set_oneshot_locked_mods(uint8_t mods) {
    (void)mods;
}

void clear_mods(void) {}
void clear_weak_mods(void) {}
void clear_oneshot_mods(void) {}
void clear_oneshot_locked_mods(void) {}
void add_mods(uint8_t mods) {
    (void)mods;
}
void del_mods(uint8_t mods) {
    (void)mods;
}
void send_keyboard_report(void) {}

bool noah_synthetic_record_active(void) {
    return false;
}

bool macro_dispatch(uint16_t keycode) {
    (void)keycode;
    return false;
}

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    (void)mods;
    return 0;
}

static void action_dispatch_at(keypos_t key_pos, uint16_t action) {
    noah_action_desc_t desc = noah_action_describe(action);

    if (noah_action_desc_is_layer_lock(desc)) {
        key_runtime_scenario_locked_layers ^= (layer_state_t)1u << desc.layer;
    }

    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind                 = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
        .data.dispatch_action = {.action = action, .packed_key_pos = key_runtime_keypos_pack(key_pos)},
    });
}

void action_dispatch(uint16_t action) {
    action_dispatch_at((keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS}, action);
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    noah_emit_action_tap_at((keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS}, action, policy);
}

void noah_emit_action_tap_at(keypos_t key_pos, uint16_t action, noah_emit_policy_t policy) {
    (void)policy;
    action_dispatch_at(key_pos, action);
}

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void pd_mode_service_active_dpi_sync(void) {}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    key_runtime_scenario_pd_locked_modes ^= mode;
    key_runtime_core_observe_pd_mode_lock_state(mode, (key_runtime_scenario_pd_locked_modes & mode) != 0u);
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind                  = KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP,
        .data.pd_mode_lock_tap = {.pd_mode = mode, .key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS}},
    });
    return true;
}

bool pd_mode_toggle_lock_state_at(pd_mode_mask_t mode, keypos_t key_pos) {
    key_runtime_scenario_pd_locked_modes ^= mode;
    key_runtime_core_observe_pd_mode_lock_state(mode, (key_runtime_scenario_pd_locked_modes & mode) != 0u);
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind                  = KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP,
        .data.pd_mode_lock_tap = {.pd_mode = mode, .key_pos = key_pos},
    });
    return true;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t index = 0; index < key_runtime_scenario_pd_mode_count; index++) {
        if (key_runtime_scenario_pd_modes[index].keycode == keycode) {
            return key_runtime_scenario_pd_modes[index].mode;
        }
    }

    return 0;
}

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    return (key_runtime_scenario_pd_locked_modes & mode) != 0;
}

uint8_t pd_mode_active_keyboard_event_masked_real_mods(void) {
    return 0u;
}

pd_mode_mask_t pd_mode_local_active_snapshot(void) {
    return 0u;
}

pd_mode_mask_t pd_mode_local_locked_snapshot(void) {
    return key_runtime_scenario_pd_locked_modes;
}

pd_mode_mask_t pd_mode_display_active_snapshot(void) {
    return 0u;
}

pd_mode_mask_t pd_mode_display_locked_snapshot(void) {
    return key_runtime_scenario_pd_locked_modes;
}

bool pd_mode_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait) {
    (void)mode;
    (void)trait;
    return false;
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    for (uint8_t index = 0; index < key_runtime_scenario_behavior_count; index++) {
        if (key_runtime_scenario_behaviors[index].keycode == keycode) {
            return key_runtime_scenario_behaviors[index].behavior;
        }
    }

    key_behavior_view_t behavior = key_runtime_scenario_default_behavior(keycode);
    if (pd_mode_for_keycode(keycode) != 0) {
        behavior.handled = true;
    }
    return behavior;
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    for (uint8_t index = 0; index < key_runtime_scenario_step_entry_count; index++) {
        if (key_runtime_scenario_steps[index].keycode == keycode && key_runtime_scenario_steps[index].tap_count == tap_count) {
            return key_runtime_scenario_steps[index].step;
        }
    }

    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    for (uint8_t index = 0; index < key_runtime_scenario_step_entry_count; index++) {
        if (key_runtime_scenario_steps[index].keycode == keycode && key_runtime_scenario_steps[index].tap_count > count && key_behavior_step_present(key_runtime_scenario_steps[index].step)) {
            return true;
        }
    }

    return false;
}

bool key_behavior_future_tap_path_has_foreign_pd_mode(uint16_t keycode, uint8_t count, pd_mode_mask_t base_mode) {
    (void)keycode;
    (void)count;
    (void)base_mode;
    return false;
}

uint8_t key_behavior_validate_all(void) {
    return 0u;
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    dispatch_delayed_action_at((keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS}, action, mods);
}

void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind = KEY_RUNTIME_EFFECT_DELAYED_ACTION,
        .data.delayed_action =
            {
                .action         = action,
                .packed_key_pos = key_runtime_keypos_pack(key_pos),
                .mods           = mods,
                .repeat_count   = 1,
            },
    });
}

void held_action_register(keypos_t key_pos, uint16_t action) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind = KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER,
        .data.held_action =
            {
                .key_pos = key_pos,
                .action  = action,
            },
    });
}

void held_action_unregister(keypos_t key_pos, uint16_t action) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind = KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER,
        .data.held_action =
            {
                .key_pos = key_pos,
                .action  = action,
            },
    });
}

void held_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind = KEY_RUNTIME_EFFECT_REPEAT_START,
        .data.repeat =
            {
                .key_pos   = key_pos,
                .action    = action,
                .repeat_hz = repeat_hz,
            },
    });
}

void held_repeat_tick(void) {}

bool held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
    return key_runtime_scenario_hold_survives_flush;
}

bool held_action_release_owned_by_key(keypos_t key_pos) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
        .data.key_pos = key_pos,
    });
    return true;
}

bool held_repeat_release_owned_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

bool held_modifier_release_owned_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind = KEY_RUNTIME_EFFECT_LAYER_PRESS,
        .data.layer_press =
            {
                .key_pos = key_pos,
                .layer   = layer,
            },
    });
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind         = KEY_RUNTIME_EFFECT_LAYER_RELEASE,
        .data.key_pos = key_pos,
    });
    return true;
}

void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->applied_layer_state = layer_state;
}

void keyboard_mod_ownership_debug_snapshot(keyboard_mod_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
}

void pointer_layer_policy_debug_snapshot(layer_state_t state, pointer_layer_policy_debug_snapshot_t *out) {
    (void)state;

    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
}

void key_feedback_pulse_arm(key_feedback_pulse_kind_t kind) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
        .data.feedback_pulse =
            {
                .kind = kind,
            },
    });
}

void key_feedback_pulse_observe(keypos_t key_pos, key_feedback_pulse_kind_t kind, uint8_t tap_branch) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
        .data.feedback_pulse =
            {
                .key_pos    = key_pos,
                .kind       = kind,
                .tap_branch = tap_branch,
            },
    });
}

void split_runtime_sync(void) {
    key_runtime_scenario_split_sync_count_value++;
}

void split_runtime_sync_request(void) {
    split_runtime_sync();
}
