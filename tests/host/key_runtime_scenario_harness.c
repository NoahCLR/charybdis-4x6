#include "key_runtime_scenario_harness.h"

#include <string.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/key/interaction/handled_key.h"
#include "users/noah/lib/key/ownership/held_action.h"
#include "users/noah/lib/key/runtime/key_runtime_process.h"
#include "users/noah/lib/key/runtime/key_runtime_state.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"
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
static key_runtime_scenario_effect_t         key_runtime_scenario_effects[KEY_RUNTIME_SCENARIO_MAX_EFFECTS];
static uint8_t                               key_runtime_scenario_effect_count_value;

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
                .key     = key_pos,
                .pressed = pressed,
            },
    };
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
    };
}

key_behavior_view_t key_runtime_scenario_pressable_handled_key(uint16_t keycode) {
    key_behavior_view_t behavior = key_runtime_scenario_default_behavior(keycode);

    behavior.handled = true;
    return behavior;
}

void key_runtime_scenario_reset(void) {
    key_runtime_scenario_time                = 1000;
    key_runtime_scenario_hold_survives_flush = false;
    key_runtime_scenario_behavior_count      = 0;
    key_runtime_scenario_step_entry_count    = 0;
    key_runtime_scenario_pd_mode_count       = 0;
    key_runtime_scenario_locked_layers       = 0;
    key_runtime_scenario_pd_locked_modes     = 0;
    key_runtime_scenario_effect_count_value  = 0;

    memset(key_runtime_scenario_behaviors, 0, sizeof(key_runtime_scenario_behaviors));
    memset(key_runtime_scenario_steps, 0, sizeof(key_runtime_scenario_steps));
    memset(key_runtime_scenario_pd_modes, 0, sizeof(key_runtime_scenario_pd_modes));
    memset(key_runtime_scenario_effects, 0, sizeof(key_runtime_scenario_effects));

    noah_runtime_reset_for_test();
}

void key_runtime_scenario_clear_effects(void) {
    key_runtime_scenario_effect_count_value = 0;
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
    }
}

void key_runtime_scenario_set_pd_locked_modes(pd_mode_mask_t modes) {
    key_runtime_scenario_pd_locked_modes = modes;
}

void key_runtime_scenario_set_hold_survives_flush(bool survives_flush) {
    key_runtime_scenario_hold_survives_flush = survives_flush;
}

void key_runtime_scenario_run(const key_runtime_scenario_step_t *steps, uint8_t step_count) {
    for (uint8_t index = 0; index < step_count; index++) {
        switch (steps[index].kind) {
            case KEY_RUNTIME_SCENARIO_STEP_PRESS: {
                keyrecord_t record = key_runtime_scenario_record(steps[index].data.key_event.key_pos, true);
                (void)noah_process_record_user(steps[index].data.key_event.keycode, &record);
                break;
            }
            case KEY_RUNTIME_SCENARIO_STEP_RELEASE: {
                keyrecord_t record = key_runtime_scenario_record(steps[index].data.key_event.key_pos, false);
                (void)noah_process_record_user(steps[index].data.key_event.keycode, &record);
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

void key_runtime_scenario_debug_snapshot(noah_runtime_debug_snapshot_t *out) {
    noah_runtime_debug_snapshot(out);
}

static bool key_runtime_scenario_snapshot_slot_copy(keypos_t key_pos, active_key_state_t *out) {
    noah_runtime_debug_snapshot_t snapshot;

    if (!out) {
        return false;
    }

    noah_runtime_debug_snapshot(&snapshot);
    return noah_runtime_debug_slot_copy(&snapshot, key_pos, out);
}

uint16_t key_runtime_scenario_slot_owner_keycode(keypos_t key_pos) {
    active_key_state_t slot = ACTIVE_KEY_STATE_INIT;

    if (!key_runtime_scenario_snapshot_slot_copy(key_pos, &slot)) {
        return KC_NO;
    }

    return slot.owner.keycode;
}

uint16_t key_runtime_scenario_slot_held_action_keycode(keypos_t key_pos) {
    active_key_state_t slot = ACTIVE_KEY_STATE_INIT;

    if (!key_runtime_scenario_snapshot_slot_copy(key_pos, &slot)) {
        return KC_NO;
    }

    return slot.lifecycle.held_action_keycode;
}

bool key_runtime_scenario_slot_has_pending_multi_tap(keypos_t key_pos) {
    active_key_state_t slot = ACTIVE_KEY_STATE_INIT;

    if (!key_runtime_scenario_snapshot_slot_copy(key_pos, &slot)) {
        return false;
    }

    return key_runtime_slot_has_pending_multi_tap(&slot);
}

bool key_runtime_scenario_slot_hold_is_complete(keypos_t key_pos) {
    active_key_state_t slot = ACTIVE_KEY_STATE_INIT;

    if (!key_runtime_scenario_snapshot_slot_copy(key_pos, &slot)) {
        return false;
    }

    return key_runtime_slot_hold_is_complete(&slot);
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

void layer_ownership_debug_snapshot(layer_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (layer_ownership_debug_snapshot_t){0};
}

void layer_ownership_reset_for_test(void) {
    key_runtime_scenario_locked_layers = 0;
}

void held_action_debug_snapshot(held_action_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (held_action_debug_snapshot_t){0};
}

void held_action_reset_for_test(void) {}

void held_repeat_debug_snapshot(held_repeat_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (held_repeat_debug_snapshot_t){0};
}

void held_repeat_reset_for_test(void) {}

void keyboard_mod_ownership_debug_snapshot(keyboard_mod_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (keyboard_mod_ownership_debug_snapshot_t){0};
}

void keyboard_mod_ownership_reset_for_test(void) {}

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

bool action_dispatch_layer_is_locked(uint8_t layer) {
    return key_runtime_scenario_layer_locked(layer);
}

void action_dispatch(uint16_t action) {
    noah_action_desc_t desc = noah_action_describe(action);

    if (noah_action_desc_is_layer_lock(desc)) {
        key_runtime_scenario_locked_layers ^= (layer_state_t)1u << desc.layer;
    }

    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind        = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
        .data.action = action,
    });
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    (void)policy;
    action_dispatch(action);
}

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    key_runtime_scenario_pd_locked_modes ^= mode;
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind         = KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP,
        .data.pd_mode = mode,
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

void key_behavior_validate_all(void) {}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind = KEY_RUNTIME_EFFECT_DELAYED_ACTION,
        .data.delayed_action =
            {
                .action       = action,
                .mods         = mods,
                .repeat_count = 1,
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

void key_feedback_pulse_arm(bool long_hold_level) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind                 = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
        .data.long_hold_level = long_hold_level,
    });
}

void split_runtime_sync(void) {}
